#ifndef _MR_CPPLIB_FAST_FILE_H
#define _MR_CPPLIB_FAST_FILE_H

#include "mr_exports.h"
#include <string>
#include "mr_fastfileserver.h"
#include "mr_sockets.h"
#include "mr_strutils.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #include <direct.h>
    #define _MR_IS_WINDOWS
#else
    #include <unistd.h>
#endif

namespace mrutils {

class FastFile {
    public:
        FastFile() 
        : client(NULL)
        {}

        ~FastFile() { 
            if (client != NULL) delete client; 
        }

        inline bool open(const char * path) {
            int len;

            char fullPath[1024];
            #ifdef _MR_IS_WINDOWS
                _getcwd(fullPath,sizeof(fullPath));
            #else
                getcwd(fullPath,sizeof(fullPath));
            #endif
            len = strlen(fullPath);
            fullPath[len++] = '/';
            strcpy(&fullPath[len], path);

            formatPath(fullPath);

            char * p = strchr(fullPath,':');
            if (p != NULL) {
                len = p - fullPath;
                *p++ = '\0';
            } else len = strlen(fullPath);

            #ifdef _MR_IS_WINDOWS
                client = new SocketClient(
                        "localhost",(p == NULL || atoi(p) == 0?FastFileServer::DEFLT_PORT:atoi(p)));
            #else 
                client = new SocketClient(
                        (p == NULL || atoi(p) == 0?FastFileServer::DEFLT_PORT:atoi(p)),
                        true);
            #endif

            if (!init()) {
                client = NULL;
                return false;
            }

            client->write(len);
            client->write(fullPath,len);

            return true;
        }
        
        template<class T>
        inline int read( T* buffer, unsigned int size) {
            client->write(size);

            int avail;
            if (client->read(avail) <= 0 || avail <= 0) return 0;

            client->read((unsigned char*)buffer, avail);

            return avail;
        }

    private:
        inline bool init() {
            if (!client->init()) {
                std::cerr << "FastFile unable to init connection to server "
                          << client->server() << ":" << client->port() << std::endl;
                return false;
            }
            return true;
        }
        
    private:
        SocketClient *client;
};

}

#endif
