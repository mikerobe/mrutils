#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__) 
#define _MR_CPPLIB_XWIN_H
#endif

#ifndef _MR_CPPLIB_XWIN_H
#define _MR_CPPLIB_XWIN_H
#define _MR_CPPLIB_XWIN_H_INCLUDE

#include "mr_exports.h"
#include <iostream>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "mr_img.h"
#include "mr_threads.h"

namespace mrutils {

class _API_ XWin {
    public:
        XWin();

        ~XWin();

    public:
       /************
        * Init
        ************/

        void init(unsigned width, unsigned height, int y = 0, int x = 0);

        inline void show(bool threadedInteraction = true) { 
            this->startThread = threadedInteraction;
            show_(true); 
        }

        void hide();

        void resize(int width, int height);
        void setName(const char * name);

        /**
          * If this is true, then when the user closes the window
          * it will reappear when new draw requests happen
          */
        inline void setPersist(bool tf = true)
        { this->persist = tf; }

        /**
          * Called to capture key inputs after show(false)
          */
        int getKey();

        /**
          * Loops, quits when user types q 
          */
        void waitQuit();

        inline bool persistExit() {
            bool ret = false;
            mrutils::mutexAcquire(winMutex);
                ret = hidden;
                persist = false;
            mrutils::mutexRelease(winMutex);
            return ret;
        }

    public:
       /**********************
        * Displaying content
        **********************/

        inline bool showImage(mrutils::ImageDecode& img) 
        { return showImage_(img,true); }

        inline void registerSave(mrutils::ImageDecode& img) {
            mrutils::mutexAcquire(winMutex);
                saveImg = &img;
            mrutils::mutexRelease(winMutex);
        }
        inline void unregisterSave(_UNUSED mrutils::ImageDecode& img) {
            mrutils::mutexAcquire(winMutex);
                saveImg = NULL;
            mrutils::mutexRelease(winMutex);
        }

    private:
        /**
          * This is for handling the user interaction
          * with the window
          */
        void threadedGUI(void *);

        bool showImage_(mrutils::ImageDecode& img, bool getLock);
        void show_(bool getLock);

    private:
        bool init_, hidden, createdWin, startThread, startedThread;
        bool persist; bool redrawNextExpose;
        int screen;
        Display * dis; Window win;
        GC graphicsContext;
        int y, x; unsigned width, height;

        mrutils::ImageDecode * saveImg;
        XImage * ximg; unsigned * ximgData;

        mutex_t winMutex;
        unsigned imgResizes;
};

}


#endif
