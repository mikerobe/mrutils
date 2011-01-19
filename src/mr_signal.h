#ifndef _MR_CPPLIB_SIGNAL_H
#define _MR_CPPLIB_SIGNAL_H

#include "mr_exports.h"
#include "mr_threads.h"
#include <signal.h>
#include <iostream>
#include "mr_files.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define SIGQUIT SIGBREAK
    #include <conio.h>
#else
    #include <sys/types.h>
    #include <termios.h>
#endif
#include "mr_sockets.h"

namespace mrutils {

    namespace SignalHandler {
        _API_ void trap();

        /**
          * check if the signal has been invoked
          */
        _API_ bool get();

        /**
         * Call this to make it so that two signals
         * in a row before any is picked up kills
         * the process
         * */
        _API_ void setTwiceKill(bool tf = true);

        /**
         * An FD is passed since you might want to be reading 
         * from /dev/tty instead if there's a piped stdin.
         */
        _API_ void setSingleCharStdin(int fd = 0);

        _API_ bool inSingleCharMode();

        /**
          * Will obtain a lock to read stdin, forcing any other
          * current readers to break & wait.
          */
        _API_ void lockStdin();
        _API_ void unlockStdin();

        /**
         * Assumes that setSingleCharStdin has been called
         * and so checks for signal callers on input
         * */
        _API_ int getch(bool obtainMutex = true);

        /** 
          * Returns 0 if stdin is blank
          */
        _API_ int getchNonBlocking(bool obtainMutex = true, const int seconds = 0, const int microseconds = 0);

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            /**
              * Used by bufferedterm so that ^C will clear the line
              */
            void directSetCalled(bool tf);
        #endif

        void restoreTerm();

       /**
        * This allows restoring the term on exit
        */
        namespace priv {
            struct destroy_t {
                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                    DWORD tio; 
                #else
                    struct termios tio;
                    int fd; // if using tty
                #endif

                bool init_;


                inline void init(int fd) {
                    init_ = true;

                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                        HANDLE const stdIn = GetStdHandle( STD_INPUT_HANDLE ); 
                        GetConsoleMode( stdIn, &tio ); 
                    #else
                        this->fd = fd;
                        tcgetattr(fd,&tio);
                    #endif

                }

                destroy_t() :init_(false), func(NULL) {}
                ~destroy_t();

                // this is called before the term is reset 
                typedef void (*closeFunc)();
                closeFunc func;
            } extern destroy;
        }
    }

}



#endif
