#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUI_SCROLL_H
#endif

#ifndef _MR_CPPLIB_GUI_SCROLL_H
#define _MR_CPPLIB_GUI_SCROLL_H
#define _MR_CPPLIB_GUI_SCROLL_H_INCLUDE

#include <iostream>
#include "fastdelegate.h"
#include "mr_gui.h"
#include "mr_bufferedreader.h"

namespace mrutils {
class GuiScroll {
    public:
        GuiScroll(int y0 = 0, int x0 = 0, int rows = 0, int cols = 0, int header=0);
        ~GuiScroll();

        void close();

    public:
       /***************
        * Main methods
        ***************/

        bool readFrom(const char * path = "-");

    private:
        void init();

    private:
        bool createdScreen; bool init_; 
        public: int winRows, winCols; private:
        bool frozen;

        public: MUTEX mutex; private:

        void * pad; FILE * tty; int header;
        mrutils::BufferedReader reader;
};
}

#endif
