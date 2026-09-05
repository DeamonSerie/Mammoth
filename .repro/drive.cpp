/* XTest driver: replays the user's stamping workflow against Mammoth.
 * argv[1] = path to the app's DebugLog (for coordinate calibration).
 * Coordinates are framebuffer-relative; a live calibration step measures
 * the app's fb origin via its own "[Mouse] onMove" logs. */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

extern "C" {
int XTestFakeKeyEvent(Display*, unsigned int, int, unsigned long);
int XTestFakeButtonEvent(Display*, unsigned int, int, unsigned long);
int XTestFakeMotionEvent(Display*, int, int, int, unsigned long);
}

/* ---- helpers ---- */
static Display* d;
static int ox = 0, oy = 0;       /* client window origin in root coords */
static int cx = 0, cy = 0;       /* empirical fb-origin correction */

static void mm(int x, int y) {
    XTestFakeMotionEvent(d, -1, ox + x + cx, oy + y + cy, CurrentTime); XFlush(d);
}
static void btn(int b, int down) { XTestFakeButtonEvent(d, b, down, CurrentTime); XFlush(d); }
static void click(int b) { btn(b, 1); usleep(90000); btn(b, 0); usleep(200000); }
static void rawkey(KeySym ks, int down) {
    int c = XKeysymToKeycode(d, ks);
    if (!c) { fprintf(stderr, "no keycode for %lu\n", ks); return; }
    XTestFakeKeyEvent(d, c, down, CurrentTime); XFlush(d);
}
static void chord(KeySym k) {
    rawkey(XK_Control_L, 1); usleep(60000);
    rawkey(k, 1); usleep(80000); rawkey(k, 0);
    rawkey(XK_Control_L, 0); usleep(180000);
}
static void tap(KeySym k) { rawkey(k, 1); usleep(80000); rawkey(k, 0); usleep(180000); }
static void drag(int x1, int y1, int x2, int y2) {
    mm(x1, y1); usleep(200000); btn(1, 1); usleep(200000);
    for (int i = 1; i <= 8; i++) { mm(x1 + (x2 - x1) * i / 8, y1 + (y2 - y1) * i / 8); usleep(45000); }
    usleep(150000); btn(1, 0); usleep(200000);
}
static void step(const char* s) { fprintf(stderr, "STEP %s\n", s); fflush(stderr); }

/* ---- calibration ---- */
/* Read app's newest "[Mouse] onMove pos=(x,y)" to measure its fb origin. */
static void calibrate(const char* logPath, int probeFbX, int probeFbY) {
    mm(probeFbX, probeFbY);
    usleep(400000);
    FILE* f = fopen(logPath, "r");
    if (!f) { fprintf(stderr, "CAL: cannot open %s\n", logPath); return; }
    char line[512];
    long rx = -999999, ry = -999999;
    while (fgets(line, sizeof(line), f)) {
        float px, py;
        if (sscanf(line, "[Mouse] onMove pos=(%f,%f)", &px, &py) == 2) { rx = (long)px; ry = (long)py; }
    }
    fclose(f);
    if (rx == -999999) { fprintf(stderr, "CAL: no onMove in log\n"); return; }
    cx = probeFbX - (int)rx; cy = probeFbY - (int)ry;
    fprintf(stderr, "CAL: aimed fb(%d,%d) app saw (%ld,%ld) -> correction (%d,%d)\n",
            probeFbX, probeFbY, rx, ry, cx, cy);
}

/* ---- window finding (grabrect.c style) ---- */
static Window findByName(Window w, const char* name) {
    char* title = NULL;
    if (XFetchName(d, w, &title) && title) {
        bool match = strstr(title, name) != NULL;
        XFree(title);
        if (match) return w;
    }
    Window root, parent; Window* children; unsigned int n;
    if (XQueryTree(d, w, &root, &parent, &children, &n)) {
        for (unsigned int i = 0; i < n; i++) {
            Window r = findByName(children[i], name);
            if (r) { XFree(children); return r; }
        }
        if (children) XFree(children);
    }
    return 0;
}

int main(int argc, char** argv) {
    d = XOpenDisplay(NULL);
    if (!d) { fprintf(stderr, "NO DISPLAY\n"); return 2; }

    const char* logPath = (argc > 1) ? argv[1] : "";

    /* find the client window nested under a parent named Mammoth */
    Window win = 0, root = DefaultRootWindow(d);
    Window frame = findByName(root, "Mammoth");
    if (frame) {
        Window* ch; unsigned int n;
        if (XQueryTree(d, frame, &root, &root, &ch, &n)) { win = (n > 0) ? ch[0] : frame; }
        if (ch) XFree(ch);
    }
    if (!win) win = findByName(root, "Mammoth"); /* fallback */

    if (!win) { fprintf(stderr, "NO WINDOW\n"); return 3; }
    XRaiseWindow(d, win);
    XWindowAttributes a; XGetWindowAttributes(d, win, &a);
    Window child; XTranslateCoordinates(d, win, root, 0, 0, &ox, &oy, &child);
    fprintf(stderr, "CLIENT at %d,%d size %dx%d\n", ox, oy, a.width, a.height);
    XSetInputFocus(d, win, RevertToParent, CurrentTime); XFlush(d);
    usleep(500000);

    if (logPath[0]) calibrate(logPath, 640, 360);

    /**** user's workflow ****/
    step("strokes");
    drag(330, 240, 470, 300);
    drag(360, 320, 480, 260);
    drag(340, 380, 430, 420);

    /* Select tool: row y[128..154] x[10..130] -> click center (60,141) */
    step("select-tool");
    mm(60, 141); usleep(150000); click(1); usleep(300000);

    /* Marquee over strokes */
    step("marquee");
    drag(310, 225, 495, 335);

    /* Stamp: copy -> float, drag away, Enter places on NEW layer */
    step("copy-paste float");
    chord(XK_c); usleep(400000);
    chord(XK_v); usleep(500000);
    drag(400, 280, 660, 400);
    tap(XK_Return); usleep(450000);

    /* Move tool: row y[98..124] x[10..130] -> click center (60,111) */
    step("move-tool");
    mm(60, 111); usleep(150000); click(1); usleep(300000);

    /* Drag with marquee still over original lines; active layer = fresh stamp layer */
    step("move-drag-over-empty-stamp-layer");
    drag(400, 280, 520, 300);
    drag(450, 290, 380, 270);
    usleep(400000);

    step("undo x2");
    chord(XK_z); usleep(350000);
    chord(XK_z); usleep(350000);

    fprintf(stderr, "drive done\n");
    XCloseDisplay(d);
    return 0;
}