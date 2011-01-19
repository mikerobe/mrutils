#ifndef _MR_CPPLIB_SOCKET_H
#define _MR_CPPLIB_SOCKET_H

#include "mr_exports.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)                                                                  
    #define _MR_IS_WINDOWS
    #include <WinSock2.h>
    #pragma comment(lib,"Ws2_32.lib")
    #define SHUT_RDWR 2
    #define ioctl ioctlsocket
    #define sockaddr_un sockaddr_in

    #define UINT64_MAX 18446744073709551615ULL
#else
    #define _MR_IS_UNIX
    #define SOCKET int
    #define ioctlsocket ioctl
    #define closesocket close
    #define TIMEVAL struct timeval
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h> // for multicast
    #include <netdb.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/un.h>
#endif

typedef unsigned long long uint64_t;
typedef   signed long long  int64_t;
typedef unsigned int       uint32_t;
typedef   signed int        int32_t;
typedef unsigned short     uint16_t;
typedef   signed short      int16_t;
typedef unsigned char      uint8_t;
typedef   signed char       int8_t;

#ifndef htonll
    #ifdef _BIG_ENDIAN
        #define htonull(x)   (x)
        #define ntohull(x)   (x)
        #define htonll(x)   (x)
        #define ntohll(x)   (x)
    #else
        #define htonull(x)   ((((uint64_t)htonl(x)) << 32) + htonl(x >> 32))
        #define ntohull(x)   ((((uint64_t)ntohl(x)) << 32) + ntohl(x >> 32))
        #define htonll(x)   ((((int64_t)htonl(x)) << 32) + htonl(x >> 32))
        #define ntohll(x)   ((((int64_t)ntohl(x)) << 32) + ntohl(x >> 32))
    #endif
#endif

#include <string>
#include <sstream>
#include <iostream>
#include <signal.h>

#if defined(_MR_IMAP_H) && !defined(MR_GNUTLS)
    // gnutls required if using imap
    #define MR_GNUTLS
#endif

#if defined(MR_GNUTLS)
    #define ssize_t long
    #include <gnutls/gnutls.h>
    #include <gnutls/extra.h>
#endif

// TODO: this may not work on windows
// write macro
#define MR_SOCKET_WRITE \
    if (!connected || size == 0) return 0;\
    const char * const end = start + size;\
    if (blocking) {\
        for(int adv;start!=end;start+=adv) if (0 >= (adv = send(start,end-start))) return (connected = false, adv);\
    } else {\
        int adv = send(start,end-start);\
        if (adv <= 0 || end != (start += adv))\
            for (;;) {\
                FD_SET(s_,&fdswrite_); \
                if (0 >= (adv = select(selmax_,NULL,&fdswrite_,NULL,NULL))\
                  ||0 >= (adv = send(start,end-start))) return (connected = false, adv);\
                if (end == (start+=adv)) break;\
            }\
    }

#define MR_SOCKET_WRITE_ELEM \
    const char * start = (char*)&elem;\
    MR_SOCKET_WRITE

// read macro
#define MR_SOCKET_READ \
    if (!connected||size==0) return 0; \
    const char * const end = start + size;\
    if (blocking) {\
        for(int adv;start!=end;start+=adv) if (0 >= (adv = recv(start,end-start))) return (connected = false, adv);\
    } else {\
        int adv = recv(start,end-start);\
        if (adv <= 0 || end != (start += adv))\
            for (;;) {\
                FD_SET(s_,&fdsread_); \
                if (0 >= (adv = select(selmax_,&fdsread_,NULL,NULL,NULL))\
                  ||0 >= (adv = recv(start,end-start))) return (connected = false, adv);\
                if (end == (start += adv)) break;\
            }\
    }

#define MR_SOCKET_READ_ELEM \
    char * start = (char*)&elem;\
    MR_SOCKET_READ

namespace mrutils {

    namespace sockets {
        static inline bool getIP(const char * const name, char * buffer, int size) {
            hostent * host = gethostbyname(name);
            in_addr addr = *((in_addr*)host -> h_addr);
            return (NULL != inet_ntop(AF_INET, &addr, buffer, size));
        }

        static inline bool isSSL(const short port) {
            static const short sslPorts[] = {
                261
               ,443
               ,448
               ,465
               ,563
               ,614
               ,636
               ,989
               ,990
               ,992
               ,993
               ,994
               ,995
            }; static const int size = sizeof(sslPorts)/sizeof(*sslPorts);
            static const short * end = sslPorts + size;
            
            const short * start = sslPorts; int count = size;
            for (int step;step = count/2, count>0;) {
                const short * const it = start + step; 
                if (*it<port) { start=it+1; count-=step+1;  }
                else count=step;
            } return (start != end && *start == port);
        }
    }

class _API_ Socket {
    friend class SocketServer; 

    public:
        enum type_t {
            SOCKET_STREAM      = 0
           ,SOCKET_UNIX        = 1
           ,SOCKET_MULTICAST   = 2
        };

    public:
        Socket(short port, type_t type = SOCKET_STREAM) 
        : s_(-1), selmax_(0), serverIP("localhost"), port(port), connected(false), type(type), addrLen(sizeof(addr))
         ,blocking(true), isServer(false), setToBlock(blocking)
         ,isSecure(false), isSRP(false)
        { create(); }

        Socket(const char * server, short port, type_t type = SOCKET_STREAM)
        : s_(-1), selmax_(0), serverIP(server), port(port), connected(false), type(type), addrLen(sizeof(addr))
         ,blocking(true), isServer(false), setToBlock(blocking)
         ,isSecure(false), isSRP(false)
        { create(); }

        ~Socket() {
            ::shutdown(s_,SHUT_RDWR);
            ::closesocket(s_);

            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                // now called once for the whole program
                //WSACleanup();
            #else 
                if (type == SOCKET_UNIX) unlink(addrUnix.sun_path);
            #endif
        }

    public:
       /****************************
        * Socket settings
        ****************************/

        bool setBlocking(bool tf = true);

    public:
        /***********************
         * Init routines
         ***********************/

         bool initClient(const int seconds = 10, const int stopFD = -1);
         bool initServer(bool reuse = false);
         inline void close() { connected = false; reopenStream(); }

         // for socket_stream. closes & opens non-blocking
         private: void reopenStream(); public:

    public:
       /****************************
        * Secure socket 
        ****************************/

        /**
          * If server disposition:
          * Uses SRP and gnutls to negotiate a secure server connection
          * a is tpasswd file, b is tpasswd.conf file
          *
          * If client disposition:
          * Negotiates TLS handshake with server. If username and password
          * are provided, assumes SRP authentication, else will rely on certs
          * a is username, b is password
          */
        #ifdef MR_GNUTLS
        bool makeSecure(const char * a = NULL, const char * b = NULL);
        void makeUnsecure();
        #endif

    public:
       /***************************
        * Send/Recv wrappers if secure
        ***************************/

        inline int send(const char * start, int size) {
            #ifdef MR_GNUTLS
                return (isSecure
                    ? gnutls_record_send(session,start,size)
                    : ::send(s_, start, size, 0));
            #else
                return ::send(s_,start,size,0);
            #endif
        }
        inline int recv(char * start, int size) {
            #ifdef MR_GNUTLS
                return (isSecure
                    ? gnutls_record_recv(session,start,size)
                    : ::recv(s_, start, size, 0));
            #else
                return ::recv(s_,start,size,0);
            #endif
        }
        

    public:
       /***************************
        * Multicast packet send/receive
        ***************************/

        inline int readPacket(char * buffer, int size) {
            if (!connected) return 0;
            return ::recvfrom(s_, buffer, size, 0
                ,(struct sockaddr*)&addr
                ,&addrLen);
        }

        inline int writePacket(char * buffer, int size) {
            if (!connected) return 0;
            return ::sendto(s_, buffer, size, 0
                ,(struct sockaddr*)&addr
                ,addrLen);
        }

    public:
       /*****************************
        * Convenience writers
        *****************************/

        template <class T>
        inline int write(const T& elem) { static const size_t size = sizeof(elem); MR_SOCKET_WRITE_ELEM; return size;}
        inline int write(const int elem_) { int elem = htonl(elem_); static const size_t size = sizeof(int); MR_SOCKET_WRITE_ELEM; return size; }
        inline int write(const unsigned elem_) { unsigned elem = htonl(elem_); static const size_t size = sizeof(unsigned); MR_SOCKET_WRITE_ELEM; return size; }
        inline int write(const uint64_t elem_) { uint64_t elem = htonull(elem_); static const size_t size = sizeof(uint64_t); MR_SOCKET_WRITE_ELEM; return size; }
        inline int write(const int64_t elem_) { int64_t elem = htonll(elem_); static const size_t size = sizeof(int64_t); MR_SOCKET_WRITE_ELEM; return size; }
        inline int writeLine(const char * start) { const size_t size = strlen(start) + 1; MR_SOCKET_WRITE; return size; }
        inline int write(const char * start) { const size_t size = strlen(start); MR_SOCKET_WRITE; return size; }
        inline int write(char * start) { const size_t size = strlen(start); MR_SOCKET_WRITE; return size; }
        inline int write(const char * start, const int size) { MR_SOCKET_WRITE; return size; } 
        inline int write(char * start, const int size) { MR_SOCKET_WRITE; return size; } 

    public:
       /*****************************
        * Convenience readers
        *****************************/

        template <class T>
        inline int read(T& elem) { size_t size = sizeof(elem); MR_SOCKET_READ_ELEM; return size; }
        inline int read(int& elem) { static const size_t size = sizeof(int); MR_SOCKET_READ_ELEM; elem = ntohl(elem); return size; }
        inline int read(unsigned& elem) { static const size_t size = sizeof(unsigned); MR_SOCKET_READ_ELEM; elem = ntohl(elem); return size; }
        inline int read(int64_t& elem) { static const size_t size = sizeof(int64_t); MR_SOCKET_READ_ELEM; elem = ntohll(elem); return size; }
        inline int read(uint64_t& elem) { static const size_t size = sizeof(uint64_t); MR_SOCKET_READ_ELEM; elem = ntohull(elem); return size; }
        inline int read(char * start, const int size) { MR_SOCKET_READ; return size; }

    public:
       /************************
        * Utilities
        ************************/

        inline Socket* accept(const int secTimeout = -1) {
            if (!waitRead(secTimeout)) return false;
            int s = ::accept(s_, NULL, NULL);
            if (s < 0) return NULL;
            return new Socket(serverIP.c_str(), ntohs(addr.sin_port), s, type, true);
        }

        inline int waitWrite(const int secTimeout = -1) {
            if (secTimeout < 0) {
                FD_SET(s_,&fdswrite_); 
                return select(selmax_, (fd_set*)NULL, &fdswrite_, (fd_set*)NULL, NULL);
            } else {
                waitTime.tv_sec = secTimeout; waitTime.tv_usec = 0;
                FD_SET(s_,&fdswrite_); 
                return select(selmax_, (fd_set*)NULL, &fdswrite_, (fd_set*)NULL, &waitTime);
            }
        }

        inline int waitRead(const int secTimeout = -1) {
            if (secTimeout < 0) {
                FD_SET(s_,&fdsread_); 
                return select(selmax_, &fdsread_, (fd_set*)NULL,(fd_set*)NULL, NULL);
            } else {
                waitTime.tv_sec = secTimeout; waitTime.tv_usec = 0;
                FD_SET(s_,&fdsread_); 
                return select(selmax_, &fdsread_, (fd_set*)NULL,(fd_set*)NULL, &waitTime);
            }
        }

    public:
        int s_, selmax_;
        const std::string serverIP;
        const short port;
        bool connected;

    private:
        Socket(const char * server, short port, int s__, type_t type, bool isServer)
        : s_(s__), selmax_(s__+1), serverIP(server), port(port), connected(true), type(type), addrLen(sizeof(addr))
         ,blocking(true), isServer(isServer), setToBlock(blocking)
         ,isSecure(false), isSRP(false)
        {
            FD_ZERO(&fdsread_); FD_ZERO(&fdswrite_);
        }

        void create();

        type_t type;
        sockaddr_in addr; socklen_t addrLen;
        sockaddr_un addrUnix;
        bool blocking; bool isServer;

        fd_set fdsread_; fd_set fdswrite_;
        TIMEVAL waitTime;

    private: // during initClient
        bool setToBlock; 

    private:
        #ifdef MR_GNUTLS
            /*** Secure socket data ***/
            public: gnutls_session_t session; private:
            gnutls_certificate_credentials_t xcred;
            gnutls_srp_client_credentials_t srp_client_cred;
            gnutls_srp_server_credentials_t srp_server_cred;
        #endif
        public: bool isSecure; private:
        bool isSRP;

    private:
        Socket(const Socket& );
        Socket& operator=(const Socket& o);
};

}


#endif
