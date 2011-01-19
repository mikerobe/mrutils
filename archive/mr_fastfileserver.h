#ifndef _MR_CPPLIB_FAST_FILE_SRV_H
#define _MR_CPPLIB_FAST_FILE_SRV_H
#define _CRT_SECURE_NO_DEPRECATE

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#else
    #include <unistd.h>
#endif

#include "mr_exports.h"
#include <iostream>
#include "mr_sockets.h"
#include "mr_threads.h"
#include "mr_numerical.h"
#include <stdio.h>
#include <vector>
#include <map>
#include <string>

namespace mrutils {

/**
 * Takes a file path, reads the whole 
 * file into memory, then provides a 
 * socket server to serve up the data. 
 * */
class _API_ FastFileServer {
    friend class FastFile; 

    static const unsigned SIZE = 32768U;
    public: static const int DEFLT_PORT = 6500;

    private:
        struct FileData {
            typedef std::vector<unsigned char *> Data;
            typedef std::vector<int> DataSize;

            ~FileData() {
                for (Data::iterator it = data.begin(); it != data.end(); ++it) 
                    delete *it;
            }

            Data data;
            DataSize dataSize;

        };

        struct ClientData {
            FileData *file;
            Socket *client;

            ClientData(FileData* file, Socket* client)
            : file(file), client(client) {}
            
            ~ClientData() {
                delete client; 
            }
        };


    typedef std::map<std::string,FileData*> Files;

    public:
        FastFileServer(int port = DEFLT_PORT) 
        { 
            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                server() = new SocketServer((port==0?DEFLT_PORT:port), 10);
            #else 
                server() = new SocketServer((port==0?DEFLT_PORT:port), 10, true);
            #endif

            if (!init_() && !(init_() = server()->init())) {
                std::cerr << 
                    "FastFileServer unable to open socket server on port " 
                    << server()->port() << std::endl;
            }

            mrutils::threadRun(FastFileServer::listen, NULL);
        }

        ~FastFileServer() {
            for (Files::iterator it = files().begin(); it != files().end(); ++it)
                delete it->second;
            delete server();
        }

        bool open(const char * path);

        static void listen(void *) {
            if (!init_()) return;

            char path[1024];
            int size;

            for (;;) {                                                                         
                Socket* _c = server()->accept();
                if (_c == NULL) return;

                if (_c->read(size) <= 0) continue;
                _c->read(path,size);
                path[size] = '\0';

                Files::iterator it = files().find(path);

                if (it == files().end()) {
                    std::cerr << "Client tried to read unknown file " 
                              << path << std::endl;
                    _c->write(0);
                    continue;
                } else {
                    mrutils::threadRun(FastFileServer::runClient, 
                            new ClientData(it->second, _c));
                }
            }
        }

        static void runClient(void* cd) {                                                                                                                                             
            Socket &client = *(static_cast<ClientData*>(cd)->client);
            FileData::Data& data = static_cast<ClientData*>(cd)->file->data;
            FileData::DataSize& dataSize = static_cast<ClientData*>(cd)->file->dataSize;
            
            int n = 0;
            unsigned char * ptr = (data.empty()?NULL:data.front());
            int left = (dataSize.empty()?0:dataSize.front());

            unsigned sent = 0;

            int sendData = 0, toSend = 0;
            while (client.read(sendData) > 0 && sendData > 0) {
                if (left == 0) { 
                    client.write(0);
                } else {
                    toSend = MIN(sendData,left);
                    sent += toSend;

                    client.write(toSend); client.write(ptr,toSend);
                    left -= toSend; ptr += toSend;

                    if (left == 0 && ++n != data.size()) {
                        left = dataSize[n];
                        ptr = data[n];
                    }
                }
            }

            delete static_cast<ClientData*>(cd);
        }

    private:
        unsigned int in(FILE* fp, unsigned char *buf);

        /*****************************
         * Accessor functions for statics (windows...)
         *****************************/

         static Files& files();
         static SocketServer*& server();
         static bool& init_();

    private:
        static Files files_;
        static SocketServer *server_;
        static bool init__;
};

}

#endif
