#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_VIEWER_H
#endif

#ifndef _MR_CPPLIB_VIEWER_H
#define _MR_CPPLIB_VIEWER_H
#define _MR_CPPLIB_VIEWER_H_INCLUDE

#include "mr_colchooser.h"
#include "mr_termline.h"
#include "mr_display.h"

namespace mrutils {

class GuiViewer {
    public:
        GuiViewer(int columns = 1, int listRows = 10
            ,int y0 = 0, int x0 = 0, int rows = 0, int cols = 0);

    public:
       /*******************
        * Init & setup
        *******************/

        inline void show() {
            if (frozen) {
                thaw();
            } else {
                colChooser.show();
                display.show();
                termLine.show();
            }
        }

        inline void freeze() { 
            colChooser.freeze();
            display.freeze();
            termLine.freeze();
            frozen = true;
        }

        inline void thaw() {
            colChooser.thaw();
            display.thaw();
            termLine.thaw();
            frozen = false;
        }

    public:
       /*******************
        * Main methods
        *******************/

        int get();
        inline void clear() {
            colChooser.clear();
            display.clear();
        }

        ColChooser colChooser;
        Display display;
        TermLine termLine;

    private:
        inline bool quit() { quit_ = true; return false; }
        inline bool open() { return false; }

    private:
        bool quit_;
        public: volatile bool frozen; private:
};

}

#endif
