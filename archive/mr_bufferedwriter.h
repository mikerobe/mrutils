#pragma once

#include <algorithm>
#include <list>

#include "mr_sockets.h"
#include "mr_threads.h"
#include "mr_files.h"
#include "mr_except.h"

namespace mrutils {

class _API_ BufferedWriter {
    struct buffer {
        inline buffer() :
            sob(NULL),
            eob(NULL),
            EOB(NULL),
            refCount(NULL)
        {}

        inline explicit buffer(size_t size) :
            sob(new char[size]),
            eob(sob),
            EOB(sob+size),
            refCount(NULL)
        {}

        inline ~buffer()
        { decrementRefCount(); }

        inline buffer(buffer const& copy)
        { setEqual(copy); }

        inline buffer & operator=(buffer const &rhs)
        {
            if (this != &rhs) {
                decrementRefCount();
                setEqual(rhs);
            }

            return *this;
        }

        inline size_t size() const
        { return (eob - sob); }

        inline bool empty() const
        { return (0 == size()); }

        inline size_t left() const
        { return (EOB - eob); }

        inline void copy(char const *const str, size_t size)
        {
            memcpy(eob, str, size);
            eob += size;
        }

        inline void renew() {
            if (NULL == refCount || 1 == *refCount) {
                eob = sob;
            } else {
                size_t const sz = EOB - sob;
                --*refCount;
                refCount = NULL;
                eob = sob = new char[sz];
                EOB = sob + sz;
            }
        }

        /**
         * sends data based on the writer's mode (file or socket)
         */
        inline bool send(BufferedWriter *writer) const
        { return send(writer, sob, eob); }

        static bool send(BufferedWriter *writer, char const *const sob, char const *const eob);

        char *sob;
        char *eob;
        char *EOB;
    private:
        inline void decrementRefCount()
        {
            if (NULL == refCount || 0 == --*refCount) {
                delete[] sob;
                delete refCount;
            }
        }

        inline void setEqual(buffer const &rhs)
        {
            sob = rhs.sob;
            eob = rhs.eob;
            EOB = rhs.EOB;
            refCount = rhs.refCount;

            if (NULL == refCount) {
                rhs.refCount = refCount = new int(2);
            } else {
                ++*refCount;
            }
        }

        mutable int * refCount;
    };
friend class buffer;

public:
    enum mode_t {
        MODE_NORMAL, //< normal behavior: fills the buffer, waits for delivery on each flush
        MODE_ASYNC, //< asynchronous, writes to memory, sends asynchronously
        MODE_EXPANDING //< writes everything to memory, expanding as necessary, then flush explicitly
    };

    explicit BufferedWriter(size_t const bufSize = 32768)
    : bufSize(bufSize)
     ,socket(NULL)
     ,fd(-1)
     ,current(bufSize)
     ,waitSecs(-1)
     ,interruptFD(-1)
     ,mode(MODE_NORMAL)
     ,mutex(mrutils::mutexCreate())
     ,threadIsActive(false)
     ,m_good(true)
    {}

    inline ~BufferedWriter() {
        flush();
        close();
        joinWriter();
        mrutils::mutexDelete(mutex);
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
    inline void setSocketInterruptFD(int fd)
    { interruptFD = fd; }

    inline bool use(Socket& socket)
    {
        flush();
        close();
        this->socket = &socket;
        return true;
    }

    inline bool open(const char * path)
    {
        flush();
        close();
        return (0 < (fd = MR_OPENW(path)));
    }

    void setMode(mode_t mode);

    inline void close()
    {
        if (fd != -1)
            MR_CLOSE(fd), fd = -1;

        socket = NULL;
        resetBuffers();
        m_good = true;
    }


public: // core writing logic
    bool flush();

    bool write(const char * str, size_t size);

    inline void put(const char * const str, const size_t size)
            throw (mrutils::ExceptionNoSuchData)
    {
        if (size > current.left())
        {
            if (!flushOrExpand())
            {
                throw mrutils::ExceptionNoSuchData("BufferedWriter disconnected");
            }

            if (size > bufSize) {
                if (!buffer::send(this,str,str+size)) {
                    throw mrutils::ExceptionNoSuchData("BufferedWriter disconnected");
                }
                return;
            }
        }

        current.copy(str,size);
    }

    /**
     * same as @see writeFile but throws an exception instead of
     * returning false when it's unable to open the file or is
     * disconnected during write
     */
    void putFile(const char * path, int size);

    bool writeFile(const char * path, int size);


public: // writing data without return code
    template <class T>
    inline bool write(const T& elem) { return write((char*)&elem,sizeof(elem)); }
    inline bool write(const char * const str) { return write(str,strlen(str)); }
    inline bool write(const std::string& str) { return write(str.c_str(),str.size()); }
    inline bool writeLine(const char * const str) { return write(str,strlen(str)+1); }
    inline bool writeLine(char * const str) { return write(str,strlen(str)+1); }
    inline bool writeLine(const std::string& str) { return write(str.c_str(),str.size()+1); }

    inline bool write(const uint16_t e) { const uint16_t ep = htons(e);  return write((char*)&ep,2); }
    inline bool write(const uint32_t e) { const uint32_t ep = htonl(e);  return write((char*)&ep,4); }
    inline bool write(const uint64_t e) { const uint64_t ep = htonll(e); return write((char*)&ep,8); }
    inline bool write(const  int16_t e) { const  int16_t ep = htons(e);  return write((char*)&ep,2); }
    inline bool write(const  int32_t e) { const  int32_t ep = htonl(e);  return write((char*)&ep,4); }
    inline bool write(const  int64_t e) { const  int64_t ep = htonll(e); return write((char*)&ep,8); }

public: // writing data with exceptions
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

    inline void put(const uint16_t e) throw (mrutils::ExceptionNoSuchData) { const uint16_t ep = htons(e);  put((char*)&ep,2); }
    inline void put(const uint32_t e) throw (mrutils::ExceptionNoSuchData) { const uint32_t ep = htonl(e);  put((char*)&ep,4); }
    inline void put(const uint64_t e) throw (mrutils::ExceptionNoSuchData) { const uint64_t ep = htonll(e); put((char*)&ep,8); }
    inline void put(const  int16_t e) throw (mrutils::ExceptionNoSuchData) { const  int16_t ep = htons(e);  put((char*)&ep,2); }
    inline void put(const  int32_t e) throw (mrutils::ExceptionNoSuchData) { const  int32_t ep = htonl(e);  put((char*)&ep,4); }
    inline void put(const  int64_t e) throw (mrutils::ExceptionNoSuchData) { const  int64_t ep = htonll(e); put((char*)&ep,8); }

public:
    const size_t bufSize;
    mrutils::Socket * socket;
    int fd;

private:
    BufferedWriter(const BufferedWriter&);
    BufferedWriter& operator=(const BufferedWriter&);

    /**
     * clears all but the first buffer
     */
    inline void resetBuffers()
    {
        buffers.erase(buffers.begin(),buffers.end());
        current.renew();
    }

    inline bool flushOrExpand() {
        if (!m_good)
            return false;

        switch(mode) {
            case MODE_NORMAL:
                return flush();
            case MODE_ASYNC:
                mrutils::mutexAcquire(mutex);
                if (!threadIsActive) {
                    mrutils::mutexRelease(mutex);
                    return false;
                }
                buffers.push_back(current);
                current.renew();
                signal.send();
                mrutils::mutexRelease(mutex);
                break;
            case MODE_EXPANDING:
                buffers.push_back(current);
                current.renew();
                break;
        }
        return true;
    }

    void joinWriter();

    void writerThreadFn(void*);

    inline void startWriter()
    {
        writerThread = mrutils::threadRun(
            fastdelegate::MakeDelegate(this,
            &BufferedWriter::writerThreadFn),
            NULL,false);
        threadIsActive = true;
    }

private:
    buffer current;

    typedef std::list<buffer> buffers_t;
    buffers_t buffers;

    int waitSecs;
    int interruptFD;
    mode_t mode;

    mrutils::thread_t writerThread;
    mrutils::mutex_t mutex;
    volatile bool threadIsActive;
    mrutils::Signal signal;

    bool m_good; ///< set to false when a transmit fails
};
}
