#ifndef _MR_UTILS_PROG_H
#define _MR_UTILS_PROG_H

#include "mr_exports.h"
#include <iosfwd>
#include <vector>
#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

namespace mrutils {

    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #elif defined(__CYGWIN__)
    #else
        class _API_ StackTrace {
            public:
                StackTrace();

                void print(std::ostream& os = std::cout);
                void print(FILE *fp);

            private:
                static const int MAX_TRACES = 62;
                void* trace_[MAX_TRACES];
                int count_;

                // this is for windows
                static void * context;
        };
    #endif

    class _API_ Debug {
        public:
            static void breakNow();
    };

   /**
    * Outputs the mac address to the out buffer.
    */
    #if defined(__APPLE__) || defined(__CYGWIN__) || defined(__linux__)
        bool getMACAddress(char * out,const char * interface="en0");
    #endif

    static inline int numProcessors() {
        long nprocs = -1;
        //long nprocs_max = -1;

        #ifdef _WIN32
            #ifndef _SC_NPROCESSORS_ONLN
                SYSTEM_INFO info;
                GetSystemInfo(&info);
                #define sysconf(a) info.dwNumberOfProcessors
                #define _SC_NPROCESSORS_ONLN
            #endif
        #endif

        #ifdef _SC_NPROCESSORS_ONLN
            nprocs = sysconf(_SC_NPROCESSORS_ONLN);
            if (nprocs < 1) {
                return 1; // err
            } else {
                return nprocs;
            }
        #else
            // can't determine
            return 1;
        #endif
    }

}

#endif
