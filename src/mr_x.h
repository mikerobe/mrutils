#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__) 
#define _MR_CPPLIB_X_H
#endif

#ifndef _MR_CPPLIB_X_H
#define _MR_CPPLIB_X_H
#define _MR_CPPLIB_X_H_INCLUDE

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "mr_packets.h"


namespace mrutils {
    namespace x11 {

        /**
         * Note: there's an x11 color table here:
         * /usr/X11/share/X11/rgb.txt
         */
        inline unsigned findColor(const char * name) {
            static Display *display = XOpenDisplay(NULL);
            static int screen = DefaultScreen(display);
            static Colormap default_cmap = DefaultColormap(display, screen);

            XColor exact_def;

            if (!XParseColor(display,default_cmap,name,&exact_def)) return 0;
            if (!XAllocColor(display,default_cmap,&exact_def)) return 0;
            return exact_def.pixel;
        }

        XImage *create_image_from_buffer (Display *dis, int screen, uint8_t *buf, int width, int height, uint32_t *& ximgData, int targetWidth = 0, int targetHeight = 0);

        bool findTermWindow(Display* disp, Window* win, Window* parent, int * pxWidth, int * pxHeight);

        /**
         * Returns the color of the pixel in the window at (x,y)
         * (as pixel type not rgb... xcolor.pixel)
         */
        unsigned getColorAt(Display* disp, Window win, const int x, const int y);
    }
}

#endif
