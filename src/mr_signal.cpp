#include "mr_signal.h"

namespace mrutils {
namespace SignalHandler {

    namespace {
        MUTEX mutexStdin = mrutils::mutexCreate();

        ATOMIC_INT twiceKill = false;
        ATOMIC_INT called = false;
        ATOMIC_INT singleCharMode = false;

        fd_set fds_;
        char buffer[2048];
        const char *eoB = buffer+2048;
        char *eob = buffer;
        char *sob = buffer;
        char *sobGet = buffer;
        TIMEVAL waitZero = {0,0};
        ATOMIC_INT trapped = false;
        int stdinFD = 0;

        void _onSignal(_UNUSED int signal) { 
            // allow this signal to be used again
            ::signal(SIGQUIT, _onSignal);
            ::signal(SIGINT, _onSignal);

            if (ATOMIC_GET_AND_SET(called,1) && twiceKill) {
                ::exit(1);
            }
        }

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            inline bool CtrlHandlerTrap( DWORD fdwCtrlType )  { return true; }
        #endif
    }


    void setTwiceKill(bool tf) { 
        if (tf) ATOMIC_GET_AND_SET(twiceKill,1); 
        else ATOMIC_GET_AND_SET(twiceKill,0); 
    }

    void trap() {
        if (!ATOMIC_GET_AND_SET(trapped,1)) {
            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandlerTrap, true);
            #endif

            ::signal(SIGQUIT, _onSignal);
            ::signal(SIGINT, _onSignal);

            FD_ZERO(&fds_); 
        }
    }

    void lockStdin() { mrutils::mutexAcquire(mutexStdin); }
    void unlockStdin() { mrutils::mutexRelease(mutexStdin); }

    bool inSingleCharMode() { return singleCharMode; }

    void setSingleCharStdin(int fd) {
        if (singleCharMode) return;

        priv::destroy.init(fd);

        stdinFD = fd;
        FD_ZERO(&fds_); 

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            HANDLE const stdIn = GetStdHandle( STD_INPUT_HANDLE ); 

            SetConsoleMode( stdIn, 
                priv::destroy.tio & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT) 
                ); 
        #else
            struct termios tio; tcgetattr(fd,&tio);
            tio.c_lflag = 0; // no echo, non-canonical
            tio.c_cc[VTIME]    = 0;
            tio.c_cc[VMIN]     = 1; // chars to wait for
            tcflush(fd, TCIFLUSH); 
            tcsetattr(fd,TCSANOW,&tio);
        #endif

        singleCharMode = true;
    }

    bool get() { 
        if (ATOMIC_GET_AND_SET(called,0)) return true;

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        #else
            if (!singleCharMode) return false;

            FD_SET(stdinFD,&fds_); memset(&waitZero,0,sizeof(TIMEVAL));
            if (select(stdinFD+1,&fds_,NULL,NULL,&waitZero) > 0) {
                int read = MR_READ(stdinFD, eob, eoB - eob);
                eob += read;
            }

            while (eob > sobGet) {
                switch (*sobGet++) {
                    case 3:
                    case 28:
                        *(sobGet-1) = 31;
                        return true;
                }
            }
        #endif

        return false;
    }

    int getchNonBlocking(bool obtainMutex, const int seconds, const int microseconds) {
        if (obtainMutex) mrutils::mutexAcquire(mutexStdin);

        while (eob > sob) {
            switch (*sob) {
                case 3:
                case 28:
                    if (trapped) _onSignal(SIGINT);
                    else raise(SIGINT);
                    ++sob;
                    *sob = 31;
                    break;

                case 31:
                    ++sob;
                    break;

                default:
                    char c= *sob++;
                    if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                    return c;
            }
        }

        int c = 0;

        // no data on buffer...
        // see if there's a character on stdin...
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            if (_kbhit()) {
        #else
            FD_SET(stdinFD,&fds_); 
            waitZero.tv_sec = seconds; waitZero.tv_usec = microseconds;
            if (select(stdinFD+1,&fds_,NULL,NULL,&waitZero) > 0) {
        #endif
            int read = MR_READ(stdinFD, eob, eoB - eob);
            eob += read;

            c = getchNonBlocking(false,0);
        }

        if (obtainMutex) mrutils::mutexRelease(mutexStdin);
        return c;
    }

    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        void directSetCalled(bool tf) {
            if (tf) {
                ATOMIC_GET_AND_SET(called,1);
            } else ATOMIC_GET_AND_SET(called,0);
        }
    #endif

    int getch(bool obtainMutex) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            if (obtainMutex) mrutils::mutexAcquire(mutexStdin);

            int c;

            switch (c = _getch()) {
                case 3:
                case 28:
                    if (trapped) _onSignal(SIGINT);
                    else raise(SIGINT);
                    return -1;
            }

            if (obtainMutex) mrutils::mutexRelease(mutexStdin);
            return c;
        #else
            while (eob > sob) {
                switch (*sob) {
                    case 3:
                    case 28:
                        if (trapped) _onSignal(SIGINT);
                        else raise(SIGINT);
                        ++sob;
                        *sob = 31;
                        break;

                    case 31:
                        ++sob;
                        break;

                    default:
                        char c= *sob++;
                        if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                        return c;
                }
            }
            
            // read everything possible into the buffer
            eob = buffer; sobGet = buffer; sob = buffer; 
            char c; for (;;) {
                int read = MR_READ(stdinFD, eob, eoB - eob);
                if (read == 0) {
                    if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                    return 0;
                } else if (read < 0) continue; // interrupted
                eob += read; c = *sob++;
                break;
            }

            switch (c) {
                case 3: 
                case 28:
                    if (trapped) _onSignal(SIGINT);
                    else raise(SIGINT);
                    *(sob-1) = 31;
                    c = getch(false);
                    if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                    return c;

                case 31: // internal
                    c = getch(false);
                    if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                    return c;

                default:
                    if (obtainMutex) mrutils::mutexRelease(mutexStdin);
                    return c;
            }
        #endif
    }

    priv::destroy_t priv::destroy;

    priv::destroy_t::~destroy_t() {
        if (init_) { mrutils::SignalHandler::restoreTerm(); }
    }

    void restoreTerm() {
        if (priv::destroy.func != NULL) (*priv::destroy.func)();

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            SetConsoleMode(stdIn, priv::destroy.tio); 
        #else
            tcsetattr(priv::destroy.fd,TCSANOW,&priv::destroy.tio);
        #endif

        priv::destroy.init_ = false;
    }

}}
