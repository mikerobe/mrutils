#include "mr_guiimgterm.h"
#include <X11/Xutil.h>
#include "mr_img.h"
#include "mr_x.h"
#ifdef _MR_CPPLIB_IMGTERM_H_INCLUDE

mrutils::ImgTerm::ImgTerm(int y0, int x0, int rows, int cols, bool border)
: good(false)
 ,pxWidth(0), pxHeight(0)
 ,pxLine(0), pxCol(0)
 ,fg(0), bg(0)
 ,paddingTop(0), paddingLeft(0), paddingBottom(0), paddingRight(0)
 ,disp(XOpenDisplay(NULL))
 ,screen(DefaultScreen(disp))
 ,border(border)
{
    // total window dimensions
    int sLines =0, sCols = 0;
    mrutils::tty_getsize(&sLines,&sCols);

    winRows = rows > 0?rows :sLines + rows;
    winCols = cols > 0?cols :sCols  + cols;

    // identify the window
    if (!mrutils::x11::findTermWindow(disp,&win,&parent,&pxWidth,&pxHeight)) {
        std::cerr << "Error: " <<  __func__ << " can't find x11 window" << std::endl;
        return;
    }

    // background color
    bg = mrutils::x11::getColorAt(disp,win,0,0);
    fg = ~bg;

    pxLine = pxHeight / sLines;
    pxCol  = pxWidth / sCols;

    paddingTop = (pxHeight - pxLine * sLines)/2;
    paddingBottom = (pxHeight - pxLine*sLines) - paddingTop;
    paddingLeft = (pxWidth - pxCol * sCols)/2;
    paddingRight = (pxWidth - pxCol*sCols) - paddingLeft;

    offset_y = pxLine * y0 + (y0 > 0?paddingTop:0);
    offset_x = pxCol * x0 + (x0 > 0?paddingLeft:0);

    pxHeight -= offset_y; if (winRows < sLines - y0) pxHeight -= paddingBottom + (sLines-y0-winRows)*pxLine;
    pxWidth  -= offset_x; if (winCols < sCols  - x0) pxWidth  -= paddingRight + (sCols-x0-winCols)*pxCol;

    // window graphics
    if (NULL == (gc = XCreateGC(disp, parent, 0, NULL))) return;

    good = true;

    // Create global pixmap & fill with bg
    pixmap = XCreatePixmap(disp, parent, pxWidth, pxHeight, DefaultDepth(disp, 0));
    XSetForeground(disp, gc, bg);
    XFillRectangle(disp, pixmap, gc, offset_x, offset_y, pxWidth, pxHeight);

    if (border) {
        drawRectangle(WhitePixel(disp,0),0,0,pxWidth,pxHeight);
        ++offset_x; ++offset_y; pxWidth -= 2; pxHeight -= 2;
    }
}

mrutils::ImgTerm::~ImgTerm() {
    if (good) {
        // erase the border with bg
        if (border) { --offset_x; --offset_y; pxWidth += 2; pxHeight += 2; }
        fill();
        XFreePixmap(disp, pixmap);
    }

    if (disp) XCloseDisplay(disp);
}

void mrutils::ImgTerm::drawEllipse(unsigned color, int x, int y, int w, int h) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XDrawArc(disp, pixmap, gc, 0,0,w-1,h-1,0,23040);
    XCopyArea(disp, pixmap, win, gc, x, y, w, h, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::drawRectangle(unsigned color, int x, int y, int w, int h) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XDrawRectangle(disp, pixmap, gc, x, y, w-1, h-1);
    XCopyArea(disp, pixmap, win, gc, x, y, w, h, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::fillEllipse(unsigned color, int x, int y, int w, int h) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XFillArc(disp, pixmap, gc, 0,0,w-1,h-1,0,23040);
    XCopyArea(disp, pixmap, win, gc, x, y, w, h, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::vline(unsigned color, int x, int y, int len) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XDrawLine(disp, pixmap, gc, x, y, x, y+len);
    XCopyArea(disp, pixmap, win, gc, x, y, 1, len, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::hline(unsigned color, int x, int y, int len) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XDrawLine(disp, pixmap, gc, x, y, x+len, y);
    XCopyArea(disp, pixmap, win, gc, x, y, len, 1, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::fillRectangle(unsigned color, int x, int y, int w, int h) {
    if (!good) return;
    if (color == 0) color = bg;

    XSetForeground(disp, gc, color);
    XFillRectangle(disp, pixmap, gc, x, y, w, h);
    XCopyArea(disp, pixmap, win, gc, x, y, w, h, offset_x+x, offset_y+y);

    XSync(disp, false);
}

void mrutils::ImgTerm::showImage(mrutils::ImageDecode& img, int x, int y, int w, int h) {
    if (!good) return;

    if (img.ximg == NULL || w != img.xwidth || h != img.xheight) {
        if (img.ximg != NULL) delete img.ximg;
        img.ximg = mrutils::x11::create_image_from_buffer(disp, screen, img.rawImg, img.width, img.height, img.ximgData, w, h);
        img.xwidth = w; img.xheight = h;
    }

    XPutImage(disp, win, gc, img.ximg, 0, 0, offset_x+x, offset_y+y, img.width, img.height);
    XFlush(disp); 
}


#endif
