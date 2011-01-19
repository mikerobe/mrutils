#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_IMGTERM_H
#endif

#ifndef _MR_CPPLIB_IMGTERM_H
#define _MR_CPPLIB_IMGTERM_H
#define _MR_CPPLIB_IMGTERM_H_INCLUDE

#include "mr_termutils.h"
#include "mr_xwin.h"

namespace mrutils {

class ImgTerm {
    public:
        ImgTerm(int y0=0, int x0=0, int rows=0, int cols=0, bool border = false);
        ~ImgTerm();

    public:
       /***************
        * Main methods
        ***************/

        void fillEllipse(unsigned color, int x = 0, int y = 0, int w = 10, int h = 10);
        void fillRectangle(unsigned color, int x = 0, int y = 0, int w = 10, int h = 10);

        void drawEllipse(unsigned color, int x = 0, int y = 0, int w = 10, int h = 10);
        void drawRectangle(unsigned color, int x = 0, int y = 0, int w = 10, int h = 10);

        void vline(unsigned color, int x = 0, int y = 0, int len = 10);
        void hline(unsigned color, int x = 0, int y = 0, int len = 10);

        inline void fill(unsigned color = 0) {
            if (!good) return;
            fillRectangle(color,0,0,pxWidth,pxHeight); 
        }

        void showImage(mrutils::ImageDecode& img, int x = 0, int y = 0, int w = 0, int h = 0);

    public:
        bool good;
        int pxWidth, pxHeight;
        int pxLine, pxCol;
        unsigned fg, bg;
        int winRows, winCols;
        int paddingTop, paddingLeft, paddingBottom, paddingRight;

    private:
        Display *disp; const int screen;
        const bool border;
        Window win, parent; GC gc; 

        Pixmap pixmap;
        int offset_x, offset_y;
};

}

#endif
