#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#define MAX_ITER 100

typedef struct {
    double re;
    double im;
} complex_t;

/* Global variables */
volatile sig_atomic_t interrupted = 0;
struct fb_var_screeninfo info;
struct fb_fix_screeninfo finfo;
double domain_min;
double domain_max;
double range_min;
double range_max;
double domain_normal;
double range_normal;

void handler () {
    interrupted = 1;
}

/* Creates a color from r, g, b, and a values */
unsigned int color (char r, char g, char b, char a) {
    unsigned int ret = 0;
    ret |= a << 24;
    ret |= r << 16;
    ret |= g << 8;
    ret |= b;
    return ret;
}

/* Turns row,col into an absolute position */
unsigned int position (unsigned int row, unsigned int col) {
    return (row * (finfo.line_length / (info.bits_per_pixel / 8))) + col;
}

/* Turns a complex point into an absolute position */
unsigned int c_position (complex_t c) {
    double domain_percent;
    double range_percent;

    /* Return -1 if the position is outside the domain/range */
    if (c.re < domain_min || c.re > domain_max) return -1;
    else if (c.im < range_min || c.im > range_max) return -1;

    /* Fix the point */
    c.re -= domain_min;
    c.im -= range_min;

    /* Calculate how far in the point is compared the domain/range */
    domain_percent = c.re / domain_normal;
    range_percent = 1 - (c.im / range_normal);

    return position ((unsigned int)(range_percent * info.yres_virtual),
                     (unsigned int)(domain_percent * info.xres_virtual));
}

double mag (complex_t c) {
    return sqrt ((c.re * c.re) + (c.im * c.im));
}

complex_t mult (complex_t a, complex_t b) {
    complex_t ret = {
        .re = 0,
        .im = 0
    };

    ret.re = (a.re * b.re) - (a.im * b.im);
    ret.im = (a.im * b.re) + (a.re * b.im);

    return ret;
}

complex_t add (complex_t a, complex_t b) {
    complex_t ret = {
        .re = 0,
        .im = 0
    };

    ret.re = a.re + b.re;
    ret.im = a.im + b.im;

    return ret;
}

unsigned int mandelbrot (complex_t c) {
    complex_t z = {
        .re = 0,
        .im = 0
    };
    unsigned int ret = 0;

    while (mag (z) <= 2 && ret < MAX_ITER) {
        z = mult (z, z);
        z = add (z, c);
        ret++;
    }

    return ret;
}

/* Draws the pixel to the screen */
void paint (unsigned int *buf, unsigned int pos, unsigned int color) {
    if (pos != (unsigned int)-1) *(buf + pos) = color;
}

int main () {
    int fb_file;
    unsigned int *fb;
    unsigned int *buf;
    sigset_t mask;
    struct sigaction usr_action;
    double x_inc;
    double y_inc;
    complex_t pos;

    const char *CSI = "\x1B[";

    /* Attempt to open the framebuffer */
    fb_file = open ("/dev/fb0", O_RDWR);
    if (fb_file == -1) {
        write (2, "Error opening framebuffer.\n", 27);
        return -1;
    }

    /* Attempt to get information about the screen */
    if (ioctl (fb_file, FBIOGET_VSCREENINFO, &info)) {
        write (2, "Error getting screen var info.\n", 31);
        close (fb_file);
        return -1;
    }
    if (ioctl (fb_file, FBIOGET_FSCREENINFO, &finfo)) {
        write (2, "Error getting screen fix info.\n", 31);
        close (fb_file);
        return -1;
    }

    /* Attempt to mmap the framebuffer */
    fb = mmap (0, finfo.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    if ((long)fb == (long)MAP_FAILED) {
        write (2, "Error mapping framebuffer to memory.\n", 37);
        close (fb_file);
        return -1;
    }

    /* Attempt to create a temp screen buffer */
    buf = calloc (finfo.smem_len, 1);
    if (!buf) {
        write (2, "Error creating temp buffer.\n", 28);
        munmap (fb, finfo.smem_len);
        close (fb_file);
        return -1;
    }

    /* Set up the SIGINT handler */
    sigfillset (&mask);
    usr_action.sa_handler = handler;
    usr_action.sa_mask = mask;
    usr_action.sa_flags = 0;
    sigaction (SIGINT, &usr_action, NULL);

    /* Set up the domain and range of the graph */
    domain_min = -2;
    domain_max = 1;
    range_min = -1;
    range_max = 1;

    /* Shift the domain/range so that it starts at 0 */
    domain_normal = domain_max - domain_min;
    range_normal = range_max - range_min;

    /* Calculate x and y increments */
    x_inc = domain_normal / info.xres_virtual;
    y_inc = range_normal / info.yres_virtual;

    /* Hide cursor */
    write (1, CSI, 2);
    write (1, "?25l", 4);

    /* Clear the screen (copies the empty buffer) */
    memcpy (fb, buf, finfo.smem_len);

    /* Test every point */
    for (pos.im = range_min + y_inc; pos.im < range_max; pos.im += y_inc) {
        for (pos.re = domain_min + x_inc; pos.re < domain_max; pos.re += x_inc) {
            if (mandelbrot (pos) == MAX_ITER)
                paint (buf, c_position (pos), color (0, 0, 0, 0));
            else
                paint (buf, c_position (pos), color (255, 255, 255, 0));
        }
    }
    memcpy (fb, buf, finfo.smem_len);

    /* Close the framebuffer */
    free (buf);
    munmap (fb, finfo.smem_len);
    close (fb_file);

    /* Wait for SIGINT */
    while (!interrupted);

    /* Show cursor */
    write (1, CSI, 2);
    write (1, "?25h", 4);

    return 0;
}
