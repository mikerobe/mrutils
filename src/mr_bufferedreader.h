#ifndef MR_BUFFEREDREADER_H
#define MR_BUFFEREDREADER_H

#include "mr_exports.h"
#include "mr_files.h"
#include "mr_sockets.h"
#include "fastdelegate.h"
#include "mr_except.h"

namespace mrutils {

class GZipReader;

class _API_ BufferedReader {
    public:
    enum fileType {
        FT_TXT
       ,FT_STDIN
       ,FT_GZ
       ,FT_SOCKET
       ,FT_RECV
       ,FT_NONE
    };

    public:
        typedef fastdelegate::FastDelegate3<void*,char*,int,int> recvFunc;

    public:
        BufferedReader(const char * path = "", const int bufSize=32768, char * useBuffer = NULL);
        ~BufferedReader();

    public:
       /******************
        * Settings
        ******************/

        /**
          * If no messages are received over the socket 
          * for x seconds, then assumed dead && read returns
          * false
          *
          * Pass -1 to wait forever
          */
        inline void setSocketWaitTime(int seconds) 
        { this->waitSecs = seconds; }

        /**
          * Whenever there is data to read on this 
          * fd, will immediately return false on 
          * any read requests.
          */
        inline void setSocketInterruptFD(int fd) { 
            interruptFD = fd; 
            if (fd >= selmax_) selmax_ = fd+1;
        }

    public:
       /******************
        * Opening
        ******************/

        inline bool open(recvFunc recvFn, void* data, bool clearData = true) {
            close(clearData);
            this->recvFn = recvFn;
            this->recvData = data;
            type = FT_RECV;
            return true;
        }

        inline bool open(const char * const server, int port, int seconds = 10, bool clearData = true) {
            close(clearData);

            socket = new mrutils::Socket(server, port, mrutils::Socket::SOCKET_STREAM);
            openedSocket = true;
            if (!((mrutils::Socket*)socket)->initClient(seconds)) return false;

            selmax_ = std::max(socket->s_+1,selmax_);

            type = FT_SOCKET;

            return true;
        }

        inline bool use(Socket& socket, bool clearData = true) {
            close(clearData);
            openedSocket = false;
            selmax_ = 1 + (socket.s_>interruptFD?socket.s_:interruptFD);
            type = FT_SOCKET;

            this->socket = &socket;
            return true;
        }

       /**
        * This switches the buffer being used to read data
        * and copies any current data to the beginning
        * new buffer must be the same size
        * The current line is preserved
        */
        void switchBuffer(char * newBuffer);

        bool open(const char * const path_, bool clearData = true);

    public:
       /******************
        * Closing
        ******************/

        inline void close(bool clearData = true) {
            if (clearData) pos = eob = line = buffer;
            if (type == FT_NONE) return;

            suspended_ = false;

            if (fd > 0) { MR_CLOSE(fd); fd = -1; }

            if (openedSocket && socket != NULL) {
                delete socket; socket = NULL;
            }

            FD_ZERO(&fds); 
            if (interruptFD > 0) {
                selmax_ = interruptFD+1;
            } else selmax_ = 1;

            type = FT_NONE;
        }


    public:
       /******************
        * Reading data
        ******************/

        inline bool skip(int size) {
            int dif = size - (eob - pos);
            if (dif <= 0) {
                pos += size; 
                return true; 
            } else return skipMore(size);
        }

        /**
          * This can allow for arbitary data sizes. 
          * Returns the amount read (which is the size
          * requested, up to bufSize)
          */
        inline int read(const int size) {
            if (eob - pos >= size) {
                line = pos; pos += size; 
                return size; 
            } else return readMore(size);
        }

       /**
        * Inline returns a type (int, long, unsigned, etc.)
        * Throws an exception if it can't get enough data
        */
        template<class T> inline T get() throw (mrutils::ExceptionNoSuchData) {
            if (sizeof(T) != read(sizeof(T))) {
                throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data");
            } return *(T*)line;
        }

       /**
        * getLine is a wrapper around nextLine that returns the line itself
        * and throws an exception if there's an error (rather than returning 
        * a bool)
        */
        inline char * getLine() throw (mrutils::ExceptionNoSuchData) {
            if (!nextLine()) throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't read line");
            return line;
        }
        inline char * getLineStripCR() throw (mrutils::ExceptionNoSuchData) {
            if (!nextLineStripCR()) throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't read line");
            return line;
        }

        /**
          * Waits for data to be available.
          * Then makes a single call to read and returns
          * the amount of unread data on the buffer.
          */
        int readOnce();

        /**
          * Returns the amount of unread data sitting
          * on the buffer
          */
        inline int unreadOnBuffer() 
        { return (eob - pos); }
        
        inline bool nextLineStripCR() {
            for (char * p = pos;p != eob;++p) {
                if (*p == '\0') strlen = p - pos;
                else if (*p == '\n') {
                    if (p != buffer && *(p-1) == '\r') {
                        *(p-1) = '\0';
                        strlen = p-pos-1;
                    } else { *p = '\0'; strlen = p-pos; }
                } else continue;
                line = pos; pos = p+1;
                return true;
            } return fetchNextLine(true);
        }

        inline bool nextLine() {
            for (char * p = pos;p != eob;++p) {
                if (*p == '\0') {}
                else if (*p == '\n') *p = '\0';
                else continue;
                strlen = p - pos;
                line = pos; pos = p+1;
                return true;
            } return fetchNextLine(false);
        }

        /**
          * this is faster than calling strlen() since we already
          * know how much data there is.
          */
        int strlen;


    public:
       /******************
        * Suspending 
        ******************/

        /**
          * This closes the file handle and caches its current
          * position. A call to resume, reopens the file handle
          * and seeks to the current position in the file. 
          */
        bool suspend();
        inline bool suspended() const { return suspended_; }
        bool resume();
        

    private:
        int readMore(int size);
        bool skipMore(int size);
        int nextLineSocketHelper(bool stripCR);
        int nextLineRecvHelper(bool stripCR);
        bool fetchNextLine(bool stripCR);

        inline bool openPlain(const char * path_) {
            close(); this->path.assign(path_);
            type = FT_TXT;
            fd = MR_OPEN(path.c_str(), O_RDONLY | O_BINARY);
            if (fd < 0) return (type = FT_NONE, false);
            else return true;
        }

    public:
        const int bufSize;
        std::string path;
        fileType type;

        char * buffer, *line, *EOB, *pos, *eob;
        mrutils::Socket * socket;

    private:
        off_t filePosition;
        bool suspended_;

        bool externalBuffer;

        bool openedSocket;

        mrutils::GZipReader* gzipReader;

        int fd, waitSecs; void * recvData;
        recvFunc recvFn;

        // for waiting on the socket
        TIMEVAL waitTime;
        fd_set fds; int selmax_;
        int interruptFD;
};

template<> inline double mrutils::BufferedReader::get<double>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(double) != read(sizeof(double))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    double t; memcpy(&t,line,sizeof(double));
    return t;
}

template<> inline int16_t mrutils::BufferedReader::get<int16_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(int16_t) != read(sizeof(int16_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    int16_t t = *(int16_t*)line; t = ntohs(t);
    return t;
}

template<> inline uint16_t mrutils::BufferedReader::get<uint16_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(uint16_t) != read(sizeof(uint16_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    uint16_t t = *(uint16_t*)line; t = ntohs(t);
    return t;
}

template<> inline int32_t mrutils::BufferedReader::get<int32_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(int32_t) != read(sizeof(int32_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    int32_t t = *(int32_t*)line; t = ntohl(t);
    return t;
}

template<> inline uint32_t mrutils::BufferedReader::get<uint32_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(uint32_t) != read(sizeof(uint32_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    uint32_t t = *(uint32_t*)line; t = ntohl(t);
    return t;
}

template<> inline int64_t mrutils::BufferedReader::get<int64_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(int64_t) != read(sizeof(int64_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    #ifdef _BIG_ENDIAN
        return (
            ( ((uint64_t)(*(uint32_t*)(line+4))) << 32)
            + (uint64_t)(*(uint32_t*)line);
            );
    #else 
        return (
             ( ((uint64_t)ntohl(*(uint32_t*)line)) << 32 )
            + (uint64_t)ntohl(*(uint32_t*)(line+4))
            );
    #endif
}

template<> inline uint64_t mrutils::BufferedReader::get<uint64_t>() throw (mrutils::ExceptionNoSuchData) {
    if (sizeof(uint64_t) != read(sizeof(uint64_t))) { throw mrutils::ExceptionNoSuchData("mrutils::BufferedReader can't get data"); } 
    #ifdef _BIG_ENDIAN
        return (
            ( ((uint64_t)(*(uint32_t*)(line+4))) << 32)
            + (uint64_t)(*(uint32_t*)line);
            );
    #else 
        return (
             ( ((uint64_t)ntohl(*(uint32_t*)line)) << 32 )
            + (uint64_t)ntohl(*(uint32_t*)(line+4))
            );
    #endif
}


}

#endif
