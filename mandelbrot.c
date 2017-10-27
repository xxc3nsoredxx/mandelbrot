#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

/* Global variables */
volatile sig_atomic_t interrupted = 0;
struct fb_var_screeninfo info;
struct fb_fix_screeninfo finfo;
double domain_min;
double domain_max;
double range_min;
double range_max;

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

/* Turns x,y into an absolute position */
unsigned int position (double x_pos, double y_pos) {
    return (y_pos * (finfo.line_length / (info.bits_per_pixel / 8))) + x_pos;
}

/* Turns a complex point into an absolute position */
unsigned int c_position (double re, double im) {
    double domain_normal;
    double range_normal;
    double domain_percent;
    double range_percent;

    /* Return -1 if the position is outside the domain/range */
    if (re < domain_min || re > domain_max) return -1;
    else if (im < range_min || im > range_max) return -1;

    /* Shift the domain/range so that it starts at 0 */
    domain_normal = domain_max - domain_min;
    range_normal = range_max - range_min;

    /* Fix the point */
    re -= domain_min;
    im -= range_min;

    /* Calculate how far in the point is compared the domain/range */
    domain_percent = re / domain_normal;
    range_percent = 1 - (im / range_normal);

    return position (range_percent * info.yres_virtual,
                     domain_percent * info.xres_virtual);
}

/* Draws the pixel to the screen */
void paint (unsigned int *fb, unsigned int pos, unsigned int color) {
    if (pos != -1) *(fb + pos) = color;
}

int main () {
    int fb_file;
    unsigned int *fb;
    unsigned int *buf;
    sigset_t mask;
    struct sigaction usr_action;
    double y_pos;
    double x_pos;

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

    /* Hide cursor */
    write (1, CSI, 2);
    write (1, "?25l", 4);

    /* Clear the screen */
    for (x_pos = 0; x_pos < info.xres_virtual; x_pos++) {
        for (y_pos = 0; y_pos < info.yres_virtual; y_pos++) {
            paint (buf, position (x_pos, y_pos), color (255, 255, 255, 0));
        }
    }

    /* Copy buffer into screen */
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
