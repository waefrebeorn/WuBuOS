/*
 * wubu_x11_recorder.c  --  WuBuOS X11 Window Recorder
 * Records X11 window to MP4 using ffmpeg pipe
 * Compile: gcc -std=c11 -O2 -lX11 -o wubu_x11_recorder wubu_x11_recorder.c
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

static Display *dpy = NULL;
static Window target_win = None;
static int width = 0, height = 0;
static FILE *ffmpeg = NULL;
static volatile int running = 1;

void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

Window find_window(Display *dpy, const char *name) {
    Window root = DefaultRootWindow(dpy);
    Window parent, *children;
    unsigned int nchildren;
    
    if (XQueryTree(dpy, root, &root, &parent, &children, &nchildren)) {
        for (unsigned int i = 0; i < nchildren; i++) {
            char *win_name = NULL;
            if (XFetchName(dpy, children[i], &win_name)) {
                if (win_name && strstr(win_name, name)) {
                    XFree(win_name);
                    Window found = children[i];
                    XFree(children);
                    return found;
                }
                XFree(win_name);
            }
        }
        XFree(children);
    }
    return None;
}

int main(int argc, char **argv) {
    const char *window_name = "WuBuOS";
    const char *output_file = "/home/wubu/wubuos_demo.mp4";
    int fps = 30;
    int duration = 30;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) window_name = argv[++i];
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output_file = argv[++i];
        else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) fps = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) duration = atoi(argv[++i]);
    }
    
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }
    
    /* Find target window */
    printf("Searching for window: %s\n", window_name);
    for (int attempt = 0; attempt < 30 && target_win == None; attempt++) {
        target_win = find_window(dpy, window_name);
        if (target_win == None) {
            usleep(100000);
        }
    }
    
    if (target_win == None) {
        fprintf(stderr, "Window not found: %s\n", window_name);
        XCloseDisplay(dpy);
        return 1;
    }
    
    /* Get window geometry */
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, target_win, &attr);
    width = attr.width;
    height = attr.height;
    printf("Recording %dx%d -> %s (%d fps, %d sec)\n", width, height, output_file, fps, duration);
    
    /* Build ffmpeg command */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pix_fmt bgra -s %dx%d -r %d -i - "
        "-c:v libx264 -preset ultrafast -crf 18 -pix_fmt yuv420p "
        "-movflags +faststart %s 2>/dev/null",
        width, height, fps, output_file);
    
    ffmpeg = popen(cmd, "w");
    if (!ffmpeg) {
        fprintf(stderr, "Failed to start ffmpeg\n");
        XCloseDisplay(dpy);
        return 1;
    }
    
    /* Allocate frame buffer */
    size_t frame_size = width * height * 4;
    uint8_t *frame = malloc(frame_size);
    if (!frame) {
        fprintf(stderr, "Failed to allocate frame buffer\n");
        pclose(ffmpeg);
        XCloseDisplay(dpy);
        return 1;
    }
    
    XImage *ximage = XGetImage(dpy, target_win, 0, 0, width, height, AllPlanes, ZPixmap);
    if (!ximage) {
        fprintf(stderr, "XGetImage failed\n");
        free(frame);
        pclose(ffmpeg);
        XCloseDisplay(dpy);
        return 1;
    }
    
    struct timespec frame_time = {0, 1000000000L / fps};
    int frames = 0;
    int max_frames = fps * duration;
    
    printf("Recording started...\n");
    
    while (running && frames < max_frames) {
        XGetSubImage(dpy, target_win, 0, 0, width, height, AllPlanes, ZPixmap, ximage, 0, 0);
        
        /* Convert to BGRA for ffmpeg */
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                unsigned long pixel = XGetPixel(ximage, x, y);
                int idx = (y * width + x) * 4;
                frame[idx + 0] = (pixel >> 0) & 0xFF;   /* B */
                frame[idx + 1] = (pixel >> 8) & 0xFF;   /* G */
                frame[idx + 2] = (pixel >> 16) & 0xFF;  /* R */
                frame[idx + 3] = 0xFF;                   /* A */
            }
        }
        
        fwrite(frame, 1, frame_size, ffmpeg);
        fflush(ffmpeg);
        frames++;
        
        if (frames % 30 == 0) {
            printf("  Frame %d/%d\n", frames, max_frames);
        }
        
        nanosleep(&frame_time, NULL);
    }
    
    printf("Recording complete: %d frames\n", frames);
    
    XDestroyImage(ximage);
    free(frame);
    pclose(ffmpeg);
    XCloseDisplay(dpy);
    
    printf("Video saved to: %s\n", output_file);
    return 0;
}