#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUI_SELECT_H
#endif

#ifndef _MR_CPPLIB_GUI_SELECT_H
#define _MR_CPPLIB_GUI_SELECT_H
#define _MR_CPPLIB_GUI_SELECT_H_INCLUDE

#include "mr_termline.h"
#include "mr_colchooser.h"

namespace mrutils {

class GuiSelect {
    public:
        GuiSelect(int columns = 1, int y0 = 0, int x0 = 0, int rows = 0, int cols = 0, bool border = false, bool termOnTop = false);
        ~GuiSelect();

    public:
       /******************
        * Gui
        ******************/

        inline void show() {
            termLine.show();
            colChooser.show();
        }

        inline void freeze() {
            termLine.freeze();
            colChooser.freeze();
        }

        void thaw();

    public:
       /******************
        * Main get
        ******************/

        int get(const char * passChars = NULL);

        char * line;

        TermLine termLine;
        ColChooser colChooser;

    public:
       /**************
        * Call functions
        **************/

        inline bool quit() { quit_ = true; return false; }

    private:
        int winRows, winCols;
        void * win;

        inline bool lineSearchEnd(bool keepSearch) { return !keepSearch; }
        inline bool open() { return false; }

        bool quit_;

    
};

}

#endif
