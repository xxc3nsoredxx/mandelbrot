#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

volatile sig_atomic_t interrupted = 0;

void handler () {
    interrupted = 1;
}

unsigned int color (char r, char g, char b) {
    unsigned int ret = 0;
    ret |= r << (16) & 0xFF0000;
    ret |= g << (8) & 0xFF00;
    ret |= b & 0xFF;
    return ret;
}

unsigned int position (int row, int col, int xres) {
    return (row * xres) + (row * 10) + col;
}

void paint (unsigned int *fb, unsigned int pos, unsigned int color) {
    *(fb + pos) = color;
}

int main () {
    int fb_file;
    struct fb_var_screeninfo info;
    unsigned long pixels;
    unsigned int *fb;
    sigset_t mask;
    struct sigaction usr_action;
    unsigned int row;
    unsigned int col;

    const char *CSI = "\x1B[";

    /* Attempt to open the framebuffer */
    fb_file = open ("/dev/fb0", O_RDWR);
    if (fb_file == -1) {
        write (2, "Error opening framebuffer.\n", 27);
        return -1;
    }

    /* Attempt to get information about the screen */
    if (ioctl (fb_file, FBIOGET_VSCREENINFO, &info)) {
        write (2, "Error getting screen info.\n", 27);
        close (fb_file);
        return -1;
    }

    /* Calculate the pixel count */
    pixels = info.xres * info.yres;

    /* Attempt to mmap the framebuffer */
    fb = mmap (0, (pixels + (info.yres * 10)) * 4, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    if ((long)fb == (long)MAP_FAILED) {
        write (2, "Error mapping framebuffer to memory.\n", 37);
        close (fb_file);
        return -1;
    }

    /* Set up the SIGINT handler */
    sigfillset (&mask);
    usr_action.sa_handler = handler;
    usr_action.sa_mask = mask;
    usr_action.sa_flags = 0;
    sigaction (SIGINT, &usr_action, NULL);

    /* Hide cursor */
    write (1, CSI, 2);
    write (1, "?25l", 4);

    /* Draw to the screen */
    for (row = 0; row < info.yres; row++) {
        for (col = 0; col < info.xres; col++) {
            paint (fb, position (row, col, info.xres), color (88, 88, 8));
        }
    }

    /* Close the framebuffer */
    munmap (fb, (pixels + (info.yres * 10)) * 4);
    close (fb_file);

    /* Wait for SIGINT */
    while (!interrupted);

    /* Show cursor */
    write (1, CSI, 2);
    write (1, "?25h", 4);

    return 0;
}
