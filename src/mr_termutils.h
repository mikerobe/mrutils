#ifndef _MR_CPPLIB_TERMUTIL_H
#define _MR_CPPLIB_TERMUTIL_H

#include "mr_exports.h"
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <time.h>
#include <map>
#include "mr_signal.h"
#include "mr_circbuf.h"
#include "mr_signal.h"

namespace mrutils {

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#else 
    static inline bool tty_getsize(int *rows, int *cols) {
        struct winsize wins;
        int tty = open(ttyname(STDOUT_FILENO), O_RDWR); 
        if (ioctl(tty, TIOCGWINSZ, &wins) >= 0 && wins.ws_row != 0 && wins.ws_col != 0) 
        { *rows = wins.ws_row; *cols = wins.ws_col; }
        return true;
    }
#endif

class _API_ Progress {
    public:
        Progress(int width = 40)
        : printed(false), width(width), startTime(time(NULL))
        {}

        void update(double prog) {
            using namespace std;

            int w = (int)(prog * width);
            if (w > width) w = width;
            if (w < 0) w = 0;

            if (!printed) printed = true;
            else cout << "\r";

            double seconds = difftime(time(NULL), startTime);
            int min = (int)(seconds/60);
            seconds -= 60 * min;

            cout << setfill('0') << setw(2) << min << ":" 
                 << setfill('0') << setw(2) << seconds << " ";

            cout << "[" << right << setfill('=') << setw(w) << "";
            cout << ">" << setfill(' ') << setw(width-w) << "";
            cout << "] " << (int)(100*prog) << "%" << flush;
        }


    private:
        bool printed;
        int width;
        time_t startTime;
};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
class _API_ BufferedTerm {
    public:
        typedef fastdelegate::FastDelegate0<bool> callFunc;
        typedef fastdelegate::FastDelegate1<const char *> liveFunc;
        typedef fastdelegate::FastDelegate1<bool,bool> liveEndFunc;

    private:
        static const int maxLength = 64;

    public:
        BufferedTerm(int history = 10, const char * prompt = ">")
        : prompt(prompt), init_(false), EOB(line+sizeof(line)-1)
         ,eob(line), p(line)
         ,statusMutex(mrutils::mutexCreate())
         ,enteringText(false)
         ,historyFromTail(-1), history(history)
        {
            for (int i = 0; i < maxLength; ++i) {
                utilBackspace[i] = 8;
                utilSpace[i] = ' ';
            }
        }

        ~BufferedTerm() {
            mrutils::mutexDelete(statusMutex);
        }

        inline void init() {
            if (init_) return;
            init_ = true;
            SignalHandler::setSingleCharStdin();
            SignalHandler::trap();
            SignalHandler::setTwiceKill();
        }

    public:
       /*************************
        * Main methods
        *************************/

        // passChars is ignored in windows
        bool nextLine(const char * passChars = NULL);

        void appendText(const char * text) {}
        void assignText(const char * text) {}

        char line[maxLength];
        char * eob;
        char * EOB;

    public:
       /************************
        * Status & prompt
        ************************/

        inline void setPrompt(const char * prompt) { }
        inline void setStatus(const char * status) { 
            mrutils::mutexAcquire(statusMutex);
                if (enteringText) {
                    if (eob == line) {
                        clearStatus();
                        if (strlen(status) > maxLength - prompt.size() - 1)
                            this->status.assign(status, maxLength - prompt.size() - 1);
                        else 
                            this->status.assign(status);
                        writeStatus();
                        fflush(stdout);
                        mrutils::mutexRelease(statusMutex);
                        return;
                    }
                } 

                if (strlen(status) > maxLength - prompt.size() - 1)
                    this->status.assign(status, maxLength - prompt.size() - 1);
                else 
                    this->status.assign(status);
            mrutils::mutexRelease(statusMutex);
        }
        inline void lockStatus(const char * status) { }
        inline void unlockStatus() { }

    public:
       /*************************
        * File logging -- not supported
        *************************/

        void setToFile(bool tf = true) {}
        inline void setLogPath(const char * path) {}
        inline const char * getLogPath() { return ""; }

    public:
       /**********************
        * Letter function calls -- not supported
        **********************/

        inline void assignFunction(char c, callFunc callFn) {}

        inline void assignSearch(char startChar, liveFunc liveFn
            ,liveEndFunc endFn, char endChar = 0, bool skipChar=true, bool checkFns=false) { }

    private:
        bool parseChar(int c);

        inline void writeStatus() {
            if (status.size() == 0) return;
            fputs(status.c_str(),stdout); 
            moveCursor(-1 * (int)status.size());
        }

        inline void clearStatus() {
            if (status.size() == 0) return;
            fwrite(utilSpace,1,status.size(),stdout);
            fwrite(utilBackspace,1,status.size(),stdout);
        }

        inline void moveCursor(int rel, const char * chars = NULL) {
            if (rel == 0) return;
            if (rel < 0) {
                fwrite(utilBackspace,1,-rel,stdout); 
            } else { 
                // rewrite characters 
                fwrite(chars,1,rel,stdout);
            }
        }
        
    private:
        std::string prompt, status;
        bool init_;
        MUTEX statusMutex;
        bool enteringText;
        char * p;

        // filled with char 8
        char utilBackspace[maxLength];

        // filled with spaces
        char utilSpace[maxLength];

        // history
        mrutils::CircBuf<std::string> history;
        int historyFromTail;
};

#else 

/**
 * Emulates a terminal input with a history.
 * NOTE:
 *  Side effect of creating one of these is a 
 *  non-blocking, non-echo, non-canonical stdin.
 * */
class BufferedTerm {
    protected:
        static const int C_PREV    = 16;
        static const int C_NEXT    = 14;
        static const int C_DEL     = 8; 
        static const int C_CLLINE  = 21;   // <c-u>
        static const int C_DELWRD  = -120; // <m-backspace>
        static const int M_D       = -28;  // <m-d>
        static const int C_A       = 1;
        static const int C_E       = 5;
        static const int C_K       = 11;
        static const int C_F       = 6;
        static const int C_B       = 2;
        static const int M_F       = -26; // forward word
        static const int M_B       = -30; // back word
        static const int C_D       = 4; // delete character
        static const int C_W       = 23; // delete word

    public:
        typedef fastdelegate::FastDelegate0<bool> callFunc;
        typedef fastdelegate::FastDelegate1<const char *> liveFunc;
        typedef fastdelegate::FastDelegate1<bool,bool> liveEndFunc;

    private:
        struct searchFunc {
            liveFunc searchFn;
            liveEndFunc endFn;
            char endChar;
            bool skipChar;
            bool checkFns;
        };

    public:
        BufferedTerm(int history = 10, const char * prompt = ">")
        : eob(line), EOB(&line[sizeof(line)])
         ,obtainMutex(true), passChars(NULL)
         ,tty(NULL)
         ,append(false), pipeToFile(false), p(line)
         ,checkFns(true)
         ,smartTerm(getenv("TERM") != NULL && getenv("TERM")[0] != 0)
         ,gotMutex(false), started(false)
         ,writeMutex(mrutils::mutexCreate())
         ,statusWriteMutex(mrutils::mutexCreate())
         ,statusReadMutex(mrutils::mutexCreate())
         ,init_(false)
         ,prompt(prompt), maxLength(128)
         ,fromTail(0), history(history)
         ,searchChar(0)
        {}

        ~BufferedTerm() {
            if (tty != NULL) fclose(tty);
        }

        inline void init() {
            if (init_) return;
            init_ = true;
            SignalHandler::trap();
            if (!mrutils::SignalHandler::inSingleCharMode()) {
                tty = fopen("/dev/tty","r");
                mrutils::SignalHandler::setSingleCharStdin(fileno(tty));
            }
        }

        inline void setMaxLineLength(int length) 
        { this->maxLength = length; }

    public:
       /************************
        * Status & prompt
        ************************/

        inline void setPrompt(const char * prompt) { 
            this->prompt = prompt; 
        }

        inline void setStatus(const char * status) {
            if (mrutils::mutexTryAcquire(statusWriteMutex)) {
                mrutils::mutexAcquire(statusReadMutex);
                // string's assign function doesn't work as expected -- size will always be max chars
                    if (strlen(status) > maxLength - prompt.size() - 1)
                        this->status.assign(status, maxLength - prompt.size() - 1);
                    else 
                        this->status.assign(status);
                mrutils::mutexRelease(statusReadMutex);
                mrutils::mutexRelease(statusWriteMutex);

                if (started && mrutils::mutexTryAcquire(writeMutex)) {
                    clearToEOL();
                    writeStatus();
                    mrutils::mutexRelease(writeMutex);
                }
            }
        }

        inline void lockStatus(const char * status) {
            mrutils::mutexAcquire(statusWriteMutex);
                mrutils::mutexAcquire(statusReadMutex);
                    this->status.assign(status, maxLength - prompt.size() - 1);
                mrutils::mutexRelease(statusReadMutex);

                mrutils::mutexAcquire(writeMutex);
                clearToEOL(); writeStatus();
                mrutils::mutexRelease(writeMutex);
        }

        inline void unlockStatus() {
            mrutils::mutexRelease(statusWriteMutex);
        }

    public:
       /**********************
        * Letter function calls
        **********************/

        inline void assignFunction(char c, callFunc callFn)
        { callFns[c] = callFn; }

        /**
         * CheckFns to enable checking function keys DURING the search.
         */
        inline void assignSearch(char startChar, liveFunc liveFn
            ,liveEndFunc endFn, char endChar = 0, bool skipChar=1, bool checkFns=false) { 
            struct searchFunc s = {liveFn, endFn, (endChar?endChar:startChar), skipChar, checkFns};
            searchFns[startChar] = s;
        }

    public:
       /*************************
        * File logging
        *************************/

        void setToFile(bool tf = true);

        inline void setLogPath(const char * path) 
        { filePath = path; }

        inline const char * getLogPath() 
        { return filePath.c_str(); }


    public:
       /*************************
        * Main methods
        *************************/

        bool nextLine(const char * passChars = NULL);

        /**
          * Used principally for tab completion
          */
        void appendText(const char * text);
        void assignText(const char * text);

        char line[1024];
        char * eob;
        char * EOB;
        bool obtainMutex;

    private:
        const char * passChars;
        bool parseChar(int c);
        inline void writeStatus() {
            mrutils::mutexAcquire(statusReadMutex);
                fputs(status.c_str(), stdout);
                moveCursor(-status.size());
            mrutils::mutexRelease(statusReadMutex);
            fflush(stdout);
        }

    protected:
       /************************
        * Printing
        ************************/

        virtual inline void fputs(const char * str, _UNUSED FILE* ignored) {
            ::fputs(str, stdout);
            if (pipeToFile) ::fputs(str, oldStdout);
        }
        virtual inline void fflush(_UNUSED FILE* ignored) {
            ::fflush(stdout);
            if (pipeToFile) ::fflush(oldStdout);
        }
        virtual inline void fputc(char c, _UNUSED FILE* ignored) {
            ::fputc(c, stdout);
            if (pipeToFile) ::fputc(c, oldStdout);
        }
        virtual inline void moveCursor(int amt) {
            if (amt > 0) { // right
                fprintf(stdout,"\033[%dC", amt); 
            } else if (amt < 0) { // left
                fprintf(stdout,"\033[%dD", -amt); 
            }
        }
        virtual inline void clearToEOL() {
            fputs("\033[K",stdout);
        }


    private:
        FILE * tty;
        bool append;
        FILE * oldStdout;
        int oldStdoutFD;
        bool pipeToFile;
        std::string filePath;
        char * p;
        bool checkFns;
        bool smartTerm;

        bool gotMutex, started;
        MUTEX writeMutex;

        MUTEX statusWriteMutex;
        MUTEX statusReadMutex;
        std::string status;

        bool init_;
        std::string prompt; int maxLength;
        int fromTail;
        mrutils::CircBuf<std::string> history;

        std::map<char, callFunc> callFns;
        std::map<char, searchFunc> searchFns;
        std::map<char, searchFunc>::iterator searchIt;
        std::map<char,callFunc>::iterator callIt;

        char searchChar;
};

#endif



}

#endif
