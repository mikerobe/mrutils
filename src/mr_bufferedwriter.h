#pragma once
#include <fcntl.h>
#include "mr_threads.h"
#include "mr_except.h"

#define restrict __restrict__
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

namespace mrutils {

class _API_ BufferedWriter
{
    struct buffer
    {
        enum state
        {
            STATE_EMPTY,
            STATE_IN_USE,
            STATE_FULL
        };

        buffer(size_t capacity = 32768) :
            m_next(NULL),
            m_state(STATE_IN_USE),
            m_size(0),
            m_data(new char[capacity]),
            m_capacity(capacity),
            m_head(0),
            m_tail(0)
        {}

        ~buffer()
        {
            delete[] m_data;
        }

        inline void reset()
        {
            m_next = NULL;
            m_state = STATE_IN_USE;
            m_size = 0;
            m_head = m_tail = 0;
        }

        /**
         * writes as much as possible to the buffer,
         * returning the amount written
         */
        size_t write(char const* restrict const data, size_t size);

        enum fileResult
        {
            FILE_ERROR = -1,
            FILE_FULL = 0,
            FILE_COMPLETE = 1
        };

        /**
         * reads from the specified fd and returns the result type,
         * modifying m_tail and m_size appropriately.
         */
        fileResult readFile(int const fd);

        buffer *m_next;
        volatile enum state m_state;
        volatile int m_size;

        char *m_data;
        size_t const m_capacity;
        size_t m_head, m_tail;

    private:
        /**
         * Helper of @see readFile.
         */
        fileResult readFileRight(int const fd, size_t rhs);
    };

    enum state
    {
        STATE_OK, //< read/write operations are OK
        STATE_INVALID //< further output is invalid
    };

public:
    enum mode
    {
        MODE_SYNC, //< synchronous, blocks when out of space
        MODE_ASYNC //< asynchronous (writing on a separate thread), no blocking
    };

    BufferedWriter(size_t const capacity = 32768) :
        m_mode(MODE_SYNC),
        m_done(true),
        m_state(STATE_OK),
        m_head(new buffer(capacity)),
        m_tail(m_head),
        m_capacity(capacity),
        m_fd(-1),
        m_interruptFD(-1),
        m_waitSeconds(-1)
    {}

    ~BufferedWriter()
    {
        try
        {
            finish();
        } catch (mrutils::ExceptionNoSuchData const &)
        {}
    }

    inline void setInterruptFD(int const interruptFD)
    { m_interruptFD = interruptFD; }

    /**
     * Max time in seconds to wait for write.
     */
    inline void setMaxWaitSecs(int waitSecs)
    { m_waitSeconds = waitSecs; }

    inline void clearInterruptFD()
    { m_interruptFD = -1; }

    inline void setFD(int const fd)
            throw (mrutils::ExceptionNoSuchData)
    {
        finish();
        m_fd = fd;
        start();
    }

    inline void setMode(enum mode const mode)
            throw (mrutils::ExceptionNoSuchData)
    {
        if (m_mode == mode)
            return;
        finish();
        m_mode = mode;
        start();
    }

    void flush() throw (mrutils::ExceptionNoSuchData);

    /**
     * Thread function for asynchronous writes.
     */
    void writeThread();

public: // functions for pushing data

    void putFile(char const * const restrict path)
            throw (mrutils::ExceptionNoSuchData);

    void write(char const * restrict data, size_t size)
            throw (mrutils::ExceptionNoSuchData);

    template<typename T>
    inline BufferedWriter& operator<<(T const &elem)
            throw (mrutils::ExceptionNoSuchData)
    {
        write(reinterpret_cast<char const *>(&elem), sizeof(T));
        return *this;
    }

    inline BufferedWriter& operator<<(std::string const &elem)
            throw (mrutils::ExceptionNoSuchData)
    {
        write(elem.c_str(), elem.size()+1);
        return *this;
    }

    inline BufferedWriter& operator<<(char * const elem)
            throw (mrutils::ExceptionNoSuchData)
    {
        write(elem, strlen(elem)+1);
        return *this;
    }

    inline BufferedWriter& operator<<(char const *const elem)
            throw (mrutils::ExceptionNoSuchData)
    {
        write(elem, strlen(elem)+1);
        return *this;
    }

#define X(X_TYPE, X_FUNC)\
    inline BufferedWriter& operator<<(X_TYPE const elem)\
            throw (mrutils::ExceptionNoSuchData)\
    {\
        X_TYPE const ep = X_FUNC(elem);\
        write(reinterpret_cast<char const *>(&ep), sizeof(X_TYPE));\
        return *this;\
    }
    X(uint16_t, htons)
    X(int16_t, htons)
    X(uint32_t, htonl)
    X(int32_t, htonl)
    X(uint64_t, htonll)
    X(int64_t, htonll)
#undef X

private:
    /**
     * Removes tail buffers, sets m_head to m_tail.
     */
    void clearBuffers();

    /**
     * Checks if the m_head is empty and returns that (updating m_head)
     * otherwise creates a new buffer and returns that
     */
    struct buffer * nextBuffer();

    /**
     * Writes the output from the head of buffer.
     */
    void pushOutput(struct buffer *buffer, size_t size)
            throw (mrutils::ExceptionNoSuchData);

    /**
     * @see pushOutput helper. pushes only to the right hand side of
     * buffer->m_head.
     */
    void pushRight(struct buffer *buffer, size_t size)
            throw (mrutils::ExceptionNoSuchData);

    /**
     * Returns true if pushOutput can push data. False indicates that
     * it's either timed out, been disconnected or been interrupted.
     */
    bool canPush() const;

    /**
     * joins the write thread
     */
    void finish() throw (mrutils::ExceptionNoSuchData);

    /**
     * launches the write thread
     */
    void start() throw (mrutils::ExceptionNoSuchData);


private:
    enum mode m_mode;
    volatile bool m_done;
    volatile enum state m_state;
    struct buffer *m_head, *m_tail;
    size_t const m_capacity;
    int m_fd, m_interruptFD;
    pthread_t m_writeThread;
    pthread_attr_t m_writeThreadAttr;
    int m_waitSeconds;

private:
    BufferedWriter(BufferedWriter const &);
    BufferedWriter & operator=(BufferedWriter const &);
};

}
