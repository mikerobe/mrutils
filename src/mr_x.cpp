#include "mr_x.h"
#ifdef _MR_CPPLIB_X_H_INCLUDE

namespace {
    int get_byte_order (void) {
        union {
            char c[sizeof(short)];
            short s;
        } order;

        order.s = 1;
        if ((1 == order.c[0])) {
            return LSBFirst;
        } else {
            return MSBFirst;
        }
    }
}

unsigned mrutils::x11::getColorAt(Display* disp, Window win, const int x, const int y) {
    unsigned bg;

    /* Determine the background color */
    Pixmap pixmap; bg = WhitePixel(disp, 0);
    if ((pixmap = XCreatePixmap(disp, win, 1, 1, DefaultDepth(disp, 0)))) {
        GC gc;
        if((gc = XCreateGC(disp, win, 0, NULL))) {
            XImage *image;
            XCopyArea(disp, win, pixmap, gc, x, y, 1, 1, 0, 0);
            if ((image = XGetImage(disp, pixmap, 0, 0, 1, 1, -1, ZPixmap))) {
                bg = XGetPixel(image, 0, 0);
                XDestroyImage(image);
            }
            XFreeGC(disp, gc);
        }
        XFreePixmap(disp, pixmap);
    }

    return bg;
}

bool mrutils::x11::findTermWindow(Display * disp, Window* win, Window* parent, int * pxWidth, int * pxHeight) {
    char *windowid;

    if ((windowid = getenv("WINDOWID")) != NULL) {
        *win = (Window) atoi(windowid);
    } else { 
        int revert;
        XGetInputFocus(disp, win, &revert);
    }

    if (*win == 0) return false;

    // search for the appropriate child window
    XWindowAttributes attr;
    Window root, *children;
    unsigned nchildren;
    
    XGetWindowAttributes(disp, *win, &attr);
    *pxWidth = attr.width; *pxHeight = attr.height;

    for(;;) {
        Window p_window;

        XQueryTree(disp, *win, &root, parent, &children, &nchildren);
        p_window = *win;

        for (unsigned i = 0; i < nchildren; i++) {
            XGetWindowAttributes(disp, children[i], &attr);

            /* maybe text window */
            if (attr.width > *pxWidth * 0.7 && attr.height > *pxHeight * 0.7) {
                *pxWidth = attr.width;
                *pxHeight = attr.height;
                *win = children[i];
            }
        }

        if (p_window == *win) break;
    }
    
    return true;
}

XImage * mrutils::x11::create_image_from_buffer (Display *dis, int screen, uint8_t *buf, int width, int height, uint32_t *& ximgData, int targetWidth, int targetHeight) {
    int depth = DefaultDepth (dis, screen);
    Visual *vis = DefaultVisual (dis, screen);;

    double rRatio = vis->red_mask / 255.0;
    double gRatio = vis->green_mask / 255.0;
    double bRatio = vis->blue_mask / 255.0;
    const int numBufBytes = 3 * width * height;

    // always downscale
    if (targetWidth == 0 || targetWidth > width) targetWidth = width;
    if (targetHeight == 0 || targetHeight > height) targetHeight = height;

    if (depth < 24) {
        fprintf (stderr, "This program does not support displays with a depth less than 24.");
        return NULL;				
    }
        
    if (targetWidth == width && targetHeight == height) {
        size_t numNewBufBytes = sizeof(uint32_t) * (targetWidth * targetHeight);
        ximgData = (uint32_t*)realloc (ximgData, numNewBufBytes);

        for (int i = 0, outIndex = 0; i < numBufBytes;) {
            unsigned r = static_cast<unsigned>(buf[i] * rRatio) & vis->red_mask;   ++i;
            unsigned g = static_cast<unsigned>(buf[i] * gRatio) & vis->green_mask; ++i;
            unsigned b = static_cast<unsigned>(buf[i] * bRatio) & vis->blue_mask;  ++i;
            ximgData[outIndex++] = r | g | b;
        }		
    } else {
        size_t numNewPixels = targetWidth * targetHeight;
        size_t numNewBufBytes = sizeof(uint32_t) * numNewPixels;
        ximgData = (uint32_t*) realloc(ximgData, numNewBufBytes);

        const double w = (double)width/targetWidth, h = (double)height/targetHeight, area = w*h;
        rRatio /= area; gRatio /= area; bRatio /= area;
        const size_t uTargetWidth = (size_t) targetWidth;

        for (size_t outIndex = 0, x = 0, y = 0; outIndex < numNewPixels; ++outIndex) {
            int lowerX = x * w, lowerY = y * h, upperX = (x+1)*w, upperY = (y+1)*h;
            if (upperX >= width) upperX = width-1; if (upperY >= height) upperY = height-1;
            double fracX[2] = { 1 - (x*w-lowerX), (x+1)*w - upperX };
            double fracY[2] = { 1 - (y*h-lowerY), (y+1)*h - upperY };

            double rgb[3] = {0,0,0};
            for (int i = 0; i < 3; ++i) {
                for (int row = lowerY; row <= upperY; ++row) {
                    const double yfrac = row == lowerY ? fracY[0] : row == upperY ? fracY[1] : 1;
                    for (int col = lowerX; col <= upperX; ++col) {
                        const double frac = yfrac * (col == lowerX ? fracX[0] : col == upperX ? fracX[1] : 1);
                        rgb[i] += frac * buf[3*(row * width + col) + i];
                    }
                }
            }

            ximgData[outIndex] = 
              static_cast<unsigned>(rgb[0] * rRatio) & vis->red_mask
            | static_cast<unsigned>(rgb[1] * gRatio) & vis->green_mask
            | static_cast<unsigned>(rgb[2] * bRatio) & vis->blue_mask;

            if (++x == uTargetWidth) { x = 0; ++y; }
        }



        /** First implementation: uses a double, scans original grid
        size_t numNewPixels = targetWidth * targetHeight;
        size_t numNewBufBytes = sizeof(double) * numNewPixels;
        const double w = (double)width/targetWidth, h = (double)height/targetHeight, area = w*h;

        double * data = (double*) realloc(ximgData, numNewBufBytes);
        ximgData = (uint32_t*)data; memset(data,0,numNewBufBytes);

        for (int i = 0, x = 0, y = 0; i < numBufBytes; i += 3) {
            const unsigned xs[2] = { x/w, (x+1)/w };
            const unsigned ys[2] = { y/h, (y+1)/h };

            double fracX = std::min(xs[0]*w+w-x,1.);
            double fracY = std::min(ys[0]*h+h-y,1.);

            unsigned newGridPos[4] = {
                    xs[0] + ys[0]*targetWidth
                   ,xs[1] + ys[0]*targetWidth
                   ,xs[1] + ys[1]*targetWidth
                   ,xs[0] + ys[1]*targetWidth };

            if (newGridPos[0] < numNewPixels) {
                data[newGridPos[0]] += fracX * fracY * buf[i];

                if (newGridPos[1] < numNewPixels && newGridPos[1] != newGridPos[0]) 
                    data[newGridPos[1]] += (1.-fracX) * fracY * buf[i];

                if (newGridPos[2] < numNewPixels && newGridPos[2] != newGridPos[1]) 
                    data[newGridPos[2]] += (1.-fracX) * (1.-fracY) * buf[i];

                if (newGridPos[3] < numNewPixels && newGridPos[3] != newGridPos[0]) 
                    data[newGridPos[3]] += fracX * (1.-fracY) * buf[i];
            }

            if (++x == width) { x = 0; ++y; }
        }

        // now divide by area and set to uint32_t
        uint32_t * p = (uint32_t*)data;
        for (size_t i = 0; i < numNewPixels; ++i, ++p) *p = static_cast<uint32_t>(data[i] / area * rRatio) & vis->red_mask;
        */
    }



    XImage * img = XCreateImage (dis, 
        CopyFromParent, depth, 
        ZPixmap, 0, 
        (char *) ximgData,
        targetWidth, targetHeight,
        32, 0
    );

    XInitImage (img);
    /*Set the client's byte order, so that XPutImage knows what to do with the data.*/
    /*The default in a new X image is the server's format, which may not be what we want.*/
    if ((LSBFirst == get_byte_order ())) {
        img->byte_order = LSBFirst;
    } else {
        img->byte_order = MSBFirst;
    }
    
    /*The bitmap_bit_order doesn't matter with ZPixmap images.*/
    img->bitmap_bit_order = MSBFirst;

    return img;
}		

#endif
