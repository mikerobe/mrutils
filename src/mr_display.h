#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_DISPLAY_H
#endif

#ifndef _MR_CPPLIB_DISPLAY_H
#define _MR_CPPLIB_DISPLAY_H
#define _MR_CPPLIB_DISPLAY_H_INCLUDE

#include <sstream>
#include "mr_strutils.h"
#include "mr_gui.h"
#include "mr_numerical.h"

namespace mrutils {

class Display {
    public:
        Display(int y0 = 0, int x0 = 0, int rows = 0, int cols = 0);
        ~Display();

    public:
       /**********************
        * Main methods
        **********************/

        void clear();
        void update();

        void show();
        inline void freeze()
        { frozen = true; }
        void thaw();

        // call these for adding header & content
        // header() << "header text";
        // content() << "content text";
        Display& header();
        inline mrutils::stringstream& content() { return mainText; }


        inline Display& operator << (_UNUSED const Display& d) { return *this; }
        template <class T> inline Display& operator << (const T& d) {
            builder << d;
            return *this;
        }

    private:
        void build(mrutils::stringstream&, bool header=true);

    private:
        mrutils::stringstream mainText;
        mrutils::stringstream builder;

        bool init_;
        public: int winRows, winCols; private:
        public: void * win; private:
        bool frozen; 

        int line;
};

}

#endif
