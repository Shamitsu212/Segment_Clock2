#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <ctime>
#include <cstring>
#include <iostream>
#include <algorithm>

using namespace std;

/*

    (1)
  ───────
(0)     (2)
 │       │
 │       │
    (3)
  ───────
(6)     (4)
 │       │
 │       │
    (5)
  ───────

  
*/
const int DIGIT_SEGMENTS[10][7] = {
    { 1,1,1,0,1,1,1 }, // 0
    { 0,0,1,0,1,0,0 }, // 1
    { 0,1,1,1,0,1,1 }, // 2
    { 0,1,1,1,1,1,0 }, // 3
    { 1,0,1,1,1,0,0 }, // 4
    { 1,1,0,1,1,1,0 }, // 5
    { 1,1,0,1,1,1,1 }, // 6
    { 0,1,1,0,1,0,0 }, // 7
    { 1,1,1,1,1,1,1 }, // 8
    { 1,1,1,1,1,1,0 }  // 9
};

void drawSegment(Display* d, Window w, GC gc, int x, int y, int digitW, int digitH, int segIdx) {

    int segThick = max(2, min(digitW, digitH) / 12); // толщина сегмента
    int horizLen = digitW - 2 * segThick;            // длина горизонтального сегмента

    if (horizLen < segThick) horizLen = segThick;
    // высота вертикального сегмента (между горизонталями)

    int vertLen = (digitH - 3 * segThick) / 2;
    if (vertLen < segThick) vertLen = segThick;

    switch (segIdx) {
        case 0: 
            XFillRectangle(d, w, gc, x, y + segThick, segThick, vertLen);
            break;
        case 1: 
            XFillRectangle(d, w, gc, x + segThick, y, horizLen, segThick);
            break;
        case 2: 
            XFillRectangle(d, w, gc, x + digitW - segThick, y + segThick, segThick, vertLen);
            break;
        case 3: 
            XFillRectangle(d, w, gc, x + segThick, y + segThick + vertLen, horizLen, segThick);
            break;
        case 4: 
            XFillRectangle(d, w, gc, x + digitW - segThick, y + segThick + vertLen + segThick, segThick, vertLen);
            break;
        case 5: 
            XFillRectangle(d, w, gc, x + segThick, y + digitH - segThick, horizLen, segThick);
            break;
        case 6: 
            XFillRectangle(d, w, gc, x, y + segThick + vertLen + segThick, segThick, vertLen);
            break;
    }
}

void drawDigit(Display* d, Window w, GC gc, int digit, int x, int y, int digitW, int digitH) {
    if (digit < 0 || digit > 9) return;
    for (int s = 0; s < 7; ++s) {
        if (DIGIT_SEGMENTS[digit][s]) {
            drawSegment(d, w, gc, x, y, digitW, digitH, s);
        }
    }
}

// Рисует сегментное двоеточие
void drawColon(Display* d, Window w, GC gc, int cx, int y, int size) {
    int dotW = max(2, size / 10);
    int dotH = max(2, size / 10);
    int topY  = y + size * 3 / 10 - dotH/2;
    int botY  = y + size * 7 / 10 - dotH/2;
    XFillRectangle(d, w, gc, cx - dotW/2, topY, dotW, dotH);
    XFillRectangle(d, w, gc, cx - dotW/2, botY, dotW, dotH);
}

int main() {
    Display* display = XOpenDisplay(nullptr);

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    int winW = 1000, winH = 320;
    Window window = XCreateSimpleWindow(display, root, 50, 50, winW, winH, 1,
                                        BlackPixel(display, screen),
                                        BlackPixel(display, screen));
    XStoreName(display, window, "Clock");
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);

    GC gc = XCreateGC(display, window, 0, nullptr);

    XColor green, exact;
    if (XAllocNamedColor(display, DefaultColormap(display, screen), "lime", &green, &exact)) {
        XSetForeground(display, gc, green.pixel);
    } else {
        XSetForeground(display, gc, WhitePixel(display, screen));
    }

    while (true) {
        while (XPending(display)) {
            XEvent ev;
            XNextEvent(display, &ev);
            if (ev.type == ConfigureNotify) {
                winW = ev.xconfigure.width;
                winH = ev.xconfigure.height;
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape || ks == XK_q || ks == XK_Q) {
                    XCloseDisplay(display);
                    return 0;
                }
            }
        }

        XWindowAttributes wa;
        XGetWindowAttributes(display, window, &wa);
        winW = wa.width;
        winH = wa.height;

        const int blocks = 8;
        int base = min(winW, winH);
        int margin = max(6, int(base * 0.05f)); 

        int digitH = winH - 2 * margin;
        if (digitH < 20) digitH = 20;

        int spacing = margin;
        int blockW = (winW - spacing * (blocks + 1)) / blocks;

        int minBlockW = max(10, int(digitH * 0.45));
        if (blockW < minBlockW) {
            spacing = max(2, (winW - minBlockW * blocks) / (blocks + 1));
            blockW = (winW - spacing * (blocks + 1)) / blocks;
        }

        int totalUsed = blocks * blockW + (blocks + 1) * spacing;
        int startX = max(0, (winW - totalUsed) / 2);
        int x = startX + spacing;
        int y = margin;

        XClearWindow(display, window);

        time_t now = time(nullptr);
        tm* t = localtime(&now);
        int digitsArr[6] = {
            t->tm_hour / 10, t->tm_hour % 10,
            t->tm_min / 10,  t->tm_min % 10,
            t->tm_sec / 10,  t->tm_sec % 10
        };

        auto drawBlockDigit = [&](int d) {
            int innerPad = max(2, blockW / 12);
            int dx = x + innerPad;
            int dw = max(8, blockW - 2 * innerPad);
            drawDigit(display, window, gc, d, dx, y, dw, digitH);
            x += blockW + spacing;
        };

        auto drawBlockColon = [&]() {
            int cx = x + blockW / 2;
            drawColon(display, window, gc, cx, y, digitH);
            x += blockW + spacing;
        };

        drawBlockDigit(digitsArr[0]);
        drawBlockDigit(digitsArr[1]);
        drawBlockColon();
        drawBlockDigit(digitsArr[2]);
        drawBlockDigit(digitsArr[3]);
        drawBlockColon();
        drawBlockDigit(digitsArr[4]);
        drawBlockDigit(digitsArr[5]);

        XFlush(display);
        usleep(120000); 
    }

    XCloseDisplay(display);
    return 0;
}
