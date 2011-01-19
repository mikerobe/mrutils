#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_DIALOG_H
#endif

#ifndef _MR_CPPLIB_DIALOG_H
#define _MR_CPPLIB_DIALOG_H
#define _MR_CPPLIB_DIALOG_H_INCLUDE

#include <iostream>
#include <stdarg.h>
#include "mr_numerical.h"
#include "mr_strutils.h"
#include "mr_signal.h"

namespace mrutils {

class Dialog {
    public:
        Dialog(const char * title = "Dialog"
            ,int rows = 0, int cols = 0);
        ~Dialog();

    public:
       /********************
        * Window, construction, formatting
        ********************/

        void hide();

        void buttons(const char * name, ...);

        inline Dialog& center() { center_ = true; return *this; }

    public:
       /********************
        * Adding data
        ********************/

        inline Dialog& next() {
            if (building) addBuiltLine();
            building = true;
            return *this;
        }

        template <class T>
        inline Dialog& operator << (const T& d) {
            builder << d;
            return *this;
        }

        inline Dialog& operator << (_UNUSED const Dialog& d) { return *this; }

    public:
       /********************
        * Getting data
        ********************/

        int get(bool obtainMutex = true);

    private:
        void init();
        void addBuiltLine();
        void highlight(int button);

    private:
        bool init_;
        public: int winRows, winCols; private:
        void * win;
        bool frozen;

        bool center_;
        int line;

        bool building;
        mrutils::stringstream builder;
        int button;
        std::vector<std::string> buttons_;
        std::vector<int> buttonStarts;

};
}

#endif
