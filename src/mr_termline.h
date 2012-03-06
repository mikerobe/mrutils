#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_TERMLINE_H
#endif

#ifndef _MR_CPPLIB_TERMLINE_H
#define _MR_CPPLIB_TERMLINE_H
#define _MR_CPPLIB_TERMLINE_H_INCLUDE

#include "mr_termutils.h"
#include "mr_gui.h"
#include "fastdelegate.h"
#include "mr_numerical.h"

namespace mrutils {

class TermLine {
    public:
        TermLine(int y0 = 0, int x0 = 0, int rows = 0, int cols = 0,bool termOnTop=false);
        ~TermLine();

    public:
       /********************
        * Appearance
        ********************/

        void setHideCursor(bool tf = true);
        void setBold(bool tf = true);

    public:
       /********************
        * Status & prompts
        ********************/


        void setStatus(const char * status);
        void lockStatus(const char * status);
        void unlockStatus();

        void setPrompt(const char * prompt);
        void setObtainMutex(bool tf = true);

        void appendText(const char * text);
        void assignText(const char * text);

    public:
       /**********************
        * Letter function calls
        **********************/

        void assignFunction(char c, BufferedTerm::callFunc callFn);
        void assignSearch(char c, BufferedTerm::searchFunc const &);

    public:
       /**********************
        * Main methods
        **********************/

        void show();
        bool nextLine(const char * passChars = NULL);

        void freeze();
        void thaw();


    private:
        bool init_;
        public: int winRows, winCols; private:
        public: void * win; private:

        class CursesTerm;
        CursesTerm* cursesTerm;

        public: char * line; char *& eob; private:
};

}

#endif
