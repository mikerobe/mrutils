#ifndef _MR_CPPLIB_SELINTERRUPT_H
#define _MR_CPPLIB_SELINTERRUPT_H

#include "mr_files.h"
#include "mr_sockets.h"

namespace mrutils {

/**
  * This uses pipe on unix and an actual socket on windows
  * since select doesn't work properly
  *
  * on windows, 
  * the first available port is used, starting at portStart
  */
class _API_ SelectInterrupt {
    public:
        SelectInterrupt()
        : readFD(-1), good_(false), writeFD(-1)
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
         ,server(NULL), client(NULL), serverWrite(NULL), setWrite(false)
        #endif
        {}

        ~SelectInterrupt() {
            if (good_) {
                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                    delete server; delete client; delete serverWrite;
                #else
                    MR_CLOSE(readFD);
                    MR_CLOSE(writeFD);
                #endif
            }
        }

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        inline void serverAccept(void *) {
            serverWrite = server->accept();
            if (serverWrite != NULL) {
                writeFD = serverWrite->s_;
                setWrite = 1;
            }
        }
        #endif

        bool init();

       /**
        * Returns false if interrupted
        * This is a convenience function -- shouldn't be used in high perf
        */
        inline bool sleep(const unsigned milliTime) {
            TIMEVAL tv;
            tv.tv_sec = milliTime / 1000;
            tv.tv_usec = 1000 * (milliTime % 1000);
            fd_set fds; FD_ZERO(&fds); FD_SET(readFD,&fds);
            return (0==select(maxFD,&fds,NULL,NULL,&tv));
        }

    public:
        inline void signal() {
            char c = 0; 

            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                ::send(writeFD,&c,1,0);
            #else
                _UNUSED const int i = ::write(writeFD,&c,1);
            #endif
        }

        inline void read() {
            char c; 
            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                ::recv(readFD,&c,1,0);
            #else
                _UNUSED const int i = ::read(readFD,&c,1);
            #endif
        }

    public:
        int readFD, maxFD;
        bool good_;

    private:
        int writeFD;

    private:    
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            mrutils::Socket * server, * client;
            mrutils::Socket * serverWrite;
            volatile bool setWrite;
        #endif


};


}

#endif
