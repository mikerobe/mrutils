#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUIEDITBOX_H
#endif

#ifndef _MR_CPPLIB_GUIEDITBOX_H
#define _MR_CPPLIB_GUIEDITBOX_H
#define _MR_CPPLIB_GUIEDITBOX_H_INCLUDE

#include "mr_termline.h"

namespace mrutils {

class GuiEditBox {
    public:
        GuiEditBox(const char * title = "Edit", int cols = 0);
        ~GuiEditBox();

    public:
        char * editText(const char * text);

        inline void freeze() {
            termLine.freeze();
            frozen = true;
        }

        void thaw();


    private:
        void init();
        void redraw();

    private:
        bool init_;
        public: int winRows, winCols; private:
        void * win;
        bool frozen;

        std::string title;

        public: mrutils::TermLine termLine; private:
};

}

#endif
