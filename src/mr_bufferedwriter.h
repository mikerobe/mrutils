#ifndef MR_BUFFEREDWRITER_H
#define MR_BUFFEREDWRITER_H

#include "mr_sockets.h"
#include "mr_files.h"
#include "mr_except.h"

namespace mrutils {

class _API_ BufferedWriter {
    public:
        BufferedWriter(const int bufSize = 32768)
        : bufSize(bufSize)
         ,socket(NULL)
         ,fd(-1)
         ,buffer(new char[bufSize])
         ,EOB(buffer+bufSize)
         ,eob(buffer)
         ,waitSecs(-1)
         ,selmax_(1), interruptFD(-1)
         ,noAutoFlush(false)
        {
            buffers.push_back(buffer);
        }

        ~BufferedWriter() {
            flush(); close();
            for (std::vector<char*>::iterator it = buffers.begin()
                ;it != buffers.end(); ++it) delete[] *it;
        }

    public:
       /******************
        * Socket options
        ******************/

        /**
          * Pass -1 to wait forever
          */
        inline void setSocketWaitTime(int seconds) 
        { this->waitSecs = seconds; }

        /**
          * Whenever there is data to read on this 
          * fd, will immediately return false on 
          * any send requests.
          */
        inline void setSocketInterruptFD(int fd) { 
            interruptFD = fd; 
            if (fd >= selmax_) selmax_ = fd+1;
        }

        /**
         * Set this option to require an explicit call to flush() to
         * transmit data; the buffer size is then effectively unlimited
         */
        inline void setNoAutoFlush(bool tf = true) { 
            if (tf == this->noAutoFlush) return;
            if (!buffereobs.empty()) flush();
            this->noAutoFlush = tf; 
        }

        inline bool use(Socket& socket) {
            close();
            this->socket = &socket;
            selmax_ = 1 + (socket.s_>interruptFD?socket.s_:interruptFD);
            return true;
        }

        inline bool open(const char * path) {
            close();
            return (0 < (fd = MR_OPENW(path)));
        }

    public:
       /**************
        * Writing data -- without exceptions
        **************/

        inline bool flush() {
            for (size_t i = 0; i < buffereobs.size(); ++i) 
                if (!send(buffers[i],buffereobs[i])) return false;
            if (!send(buffer,eob)) return false;
            resetBuffers();
            return true;
        }

        template <class T>
        inline bool write(const T& elem) { return write((char*)&elem,sizeof(elem)); }
        inline bool write(const char * const str) { return write(str,strlen(str)); }
        inline bool write(const std::string& str) { return write(str.c_str(),str.size()); }
        inline bool writeLine(const char * const str) { return write(str,strlen(str)+1); }
        inline bool writeLine(char * const str) { return write(str,strlen(str)+1); }
        inline bool writeLine(const std::string& str) { return write(str.c_str(),str.size()+1); }
        inline bool write(const char * const str, const int size) {
            if (size > EOB - eob) {
                if (!flushOrExpand()) return false;
                if (size > bufSize) return send(str,str+size);
            } memcpy(eob,str,size); eob += size; return true;
        }

        inline bool write(const uint16_t e) { const uint16_t ep = htons(e);  return write((char*)&ep,2); }
        inline bool write(const uint32_t e) { const uint32_t ep = htonl(e);  return write((char*)&ep,4); }
        inline bool write(const uint64_t e) { const uint64_t ep = htonll(e); return write((char*)&ep,8); }
        inline bool write(const  int16_t e) { const  int16_t ep = htons(e);  return write((char*)&ep,2); }
        inline bool write(const  int32_t e) { const  int32_t ep = htonl(e);  return write((char*)&ep,4); }
        inline bool write(const  int64_t e) { const  int64_t ep = htonll(e); return write((char*)&ep,8); }

        inline bool writeFile(const char * path, int size) {
            int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
            if (fd < 0) return false;

            for (int r
                ;size > 0
                 && (EOB - eob >= 512 || flushOrExpand())
                 && 0 < (r = MR_READ(fd,eob,EOB - eob))
                ;size -= r, eob += r) ;
            MR_CLOSE(fd);
            return (size == 0);
        }

    public:
       /**************
        * Writing data -- WITH exceptions
        **************/

        template<class T>
        inline friend mrutils::BufferedWriter& operator<<(mrutils::BufferedWriter& b, const T& elem) throw (mrutils::ExceptionNoSuchData) 
        { b.put(elem); return b; }

        inline friend mrutils::BufferedWriter& operator<<(mrutils::BufferedWriter& b, const std::string& elem) throw (mrutils::ExceptionNoSuchData)
        { b.putLine(elem); return b; }

        inline friend mrutils::BufferedWriter& operator<< (mrutils::BufferedWriter& b, const char* const elem) throw(mrutils::ExceptionNoSuchData)
        { b.putLine(elem); return b; }

        inline friend mrutils::BufferedWriter& operator<< (mrutils::BufferedWriter& b, char* const elem) throw(mrutils::ExceptionNoSuchData)
        { b.putLine(elem); return b; }

        template <class T>
        inline void put(const T& elem) throw (mrutils::ExceptionNoSuchData) { put((char*)&elem,sizeof(elem)); }
        inline void put(const char * const str) throw (mrutils::ExceptionNoSuchData) { put(str,strlen(str)); }
        inline void put(const std::string& str) throw (mrutils::ExceptionNoSuchData) { put(str.c_str(),str.size()); }
        inline void putLine(const char * const str) throw (mrutils::ExceptionNoSuchData) { put(str,strlen(str)+1); }
        inline void putLine(char * const str) throw (mrutils::ExceptionNoSuchData) { put(str,strlen(str)+1); }
        inline void putLine(const std::string& str) throw (mrutils::ExceptionNoSuchData) { put(str.c_str(),str.size()+1); }
        inline void put(const char * const str, const int size) throw (mrutils::ExceptionNoSuchData) {
            if (size > EOB - eob) {
                if (!flushOrExpand()) { throw mrutils::ExceptionNoSuchData("BufferedWriter disconnected"); }
                if (size > bufSize) {
                    if (send(str,str+size)) { throw mrutils::ExceptionNoSuchData("BufferedWriter disconnected"); }
                    return;
                }
            } memcpy(eob,str,size); eob += size;
        }

        inline void put(const uint16_t e) throw (mrutils::ExceptionNoSuchData) { const uint16_t ep = htons(e);  put((char*)&ep,2); }
        inline void put(const uint32_t e) throw (mrutils::ExceptionNoSuchData) { const uint32_t ep = htonl(e);  put((char*)&ep,4); }
        inline void put(const uint64_t e) throw (mrutils::ExceptionNoSuchData) { const uint64_t ep = htonll(e); put((char*)&ep,8); }
        inline void put(const  int16_t e) throw (mrutils::ExceptionNoSuchData) { const  int16_t ep = htons(e);  put((char*)&ep,2); }
        inline void put(const  int32_t e) throw (mrutils::ExceptionNoSuchData) { const  int32_t ep = htonl(e);  put((char*)&ep,4); }
        inline void put(const  int64_t e) throw (mrutils::ExceptionNoSuchData) { const  int64_t ep = htonll(e); put((char*)&ep,8); }

        inline void putFile(const char * path, int size) {
            int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
            if (fd < 0) throw mrutils::ExceptionNoSuchData("BufferedWriter can't open file to put");

            for (int r
                ;size > 0
                 && (EOB - eob >= 512 || flushOrExpand())
                 && 0 < (r = MR_READ(fd,eob,EOB - eob))
                ;size -= r, eob += r) ;
            MR_CLOSE(fd);
            if (size != 0) throw mrutils::ExceptionNoSuchData("BufferedWriter disconnected while putting file");
        }

    public:
        const int bufSize;
        mrutils::Socket * socket;
        int fd;

    private:
        BufferedWriter(const BufferedWriter&);
        BufferedWriter& operator=(const BufferedWriter&);

        /**
         * clears all but the first buffer
         */
        inline void resetBuffers() {
            if (!buffereobs.empty()) {
                for (std::vector<char*>::iterator it = buffers.begin()+1
                    ;it != buffers.end(); ++it) delete[] *it;
                buffers.resize(1); buffereobs.clear();
                buffer = buffers[0];
                EOB = buffer + bufSize;
            }

            eob = buffer;
        }
        
        inline void close() { 
            if (fd != -1) { MR_CLOSE(fd); fd = -1; }
            socket = NULL;

            resetBuffers();

            FD_ZERO(&fdsread_); FD_ZERO(&fdswrite_);
        }

        inline bool flushOrExpand() {
            if (noAutoFlush) {
                buffereobs.push_back(eob);
                buffers.push_back(eob = buffer = new char[bufSize]);
                EOB = buffer + bufSize;
                return true;
            } else return flush();
        }

        bool send(const char * start, const char * const end);

    private:
        // current buffer
        char * buffer;
        const char * EOB;
        char * eob; 

        std::vector<char *> buffers;
        std::vector<char *> buffereobs;

        int waitSecs;
        TIMEVAL waitTime;
        fd_set fdsread_; fd_set fdswrite_;
        int selmax_,interruptFD;
        bool noAutoFlush;
};
}
#endif
