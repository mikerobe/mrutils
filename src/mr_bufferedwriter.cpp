#include <errno.h>
#include "mr_bufferedwriter.h"
#include <pantheios/pantheios.hpp>
#include <pantheios/inserters.hpp>

#ifndef O_BINARY
#   define O_BINARY 0
#endif

extern "C"
void * threadRun(void *data)
{
    mrutils::BufferedWriter &writer =
            *static_cast<mrutils::BufferedWriter*>(data);
    writer.writeThread();
    return NULL;
}

namespace mrutils {

void BufferedWriter::flush()
        throw (mrutils::ExceptionNoSuchData)
{
    if (m_done)
        return;

    switch (m_mode)
    {
        case MODE_ASYNC:
            break;
        case MODE_SYNC:
            if (m_fd != -1)
                pushOutput(m_head, m_head->m_size);
            break;
    }
}

void BufferedWriter::finish()
        throw (mrutils::ExceptionNoSuchData)
{
    if (m_done)
        return;

    m_done = true;

    switch (m_mode)
    {
        case MODE_ASYNC:
            if (int const ret = pthread_join(m_writeThread,NULL))
            {
                pthread_attr_destroy(&m_writeThreadAttr);

                std::stringstream ss;
                ss << __PRETTY_FUNCTION__ << " error joining thread "
                    << m_writeThread << ": [" << ret << "] ["
                    << strerror(errno) << "]";
                throw mrutils::ExceptionNoSuchData(ss.str());
            }

            pthread_attr_destroy(&m_writeThreadAttr);
            clearBuffers();
            break;

        case MODE_SYNC:
            if (m_fd != -1 && m_state != STATE_INVALID)
                pushOutput(m_head, m_head->m_size);
            break;
    }
}

void BufferedWriter::start()
        throw (mrutils::ExceptionNoSuchData)
{
    if (!m_done)
        return;

    m_state = STATE_OK;
    m_done = false;

    switch (m_mode)
    {
        case MODE_SYNC:
            break;
        case MODE_ASYNC:
            pthread_attr_init(&m_writeThreadAttr);
            pthread_attr_setdetachstate(&m_writeThreadAttr,
                    PTHREAD_CREATE_JOINABLE);

            if (0 != pthread_create(&m_writeThread, &m_writeThreadAttr,
                    &::threadRun, static_cast<void*>(this)))
            {
                std::stringstream ss;
                ss << __PRETTY_FUNCTION__ << " error starting thread: "
                    << strerror(errno);
                throw mrutils::ExceptionNoSuchData(ss.str());
            }

            break;
    }
}

void BufferedWriter::clearBuffers()
{
    for (struct buffer *itBuffer = m_head->m_next;
            itBuffer != NULL;)
    {
        struct buffer *const next = itBuffer->m_next;
        delete itBuffer;
        itBuffer = next;
    }
    m_tail = m_head;
    m_tail->reset();
}

void BufferedWriter::putFile(char const *const restrict path)
            throw (mrutils::ExceptionNoSuchData)
{
    int fd = open(path, O_RDONLY|O_BINARY);
    if (fd <= 0)
    {
        throw mrutils::ExceptionNoSuchData(
            static_cast<std::stringstream&>(std::stringstream().seekp(0)
                << __PRETTY_FUNCTION__ << " can't open file at \"" << path
            << "\" " << strerror(errno)).str(),1);
    }

    try
    {
        for (;;)
        {
            buffer::fileResult result = m_tail->readFile(fd);
            if (buffer::FILE_COMPLETE == result)
            {
                break;
            } else if (buffer::FILE_FULL == result)
            {
                switch (m_mode)
                {
                    case MODE_SYNC:
                        pushOutput(m_tail, m_tail->m_size);
                        break;
                    case MODE_ASYNC:
                        m_tail->m_next = nextBuffer();
                        m_tail->m_state = buffer::STATE_FULL;
                        m_tail = m_tail->m_next;
                        break;
                }
            } else
            {
                m_state = STATE_INVALID;
                throw mrutils::ExceptionNoSuchData(
                    static_cast<std::stringstream&>(std::stringstream().seekp(0)
                        << __PRETTY_FUNCTION__
                        << " error while reading file at " << path
                    ).str());
            }
        }
    } catch (...)
    {
        ::close(fd);
        throw;
    }

    ::close(fd);
}

BufferedWriter::buffer::fileResult
BufferedWriter::buffer::readFileRight(int const fd, size_t rhs)
{
    size_t bytesRead = 0;

    if (rhs > 0) do
    {
        int const r = ::read(fd, m_data + m_tail, rhs);
        if (r < 0)
            return FILE_ERROR;
        if (r == 0)
        {
            __sync_add_and_fetch(&m_size, bytesRead);
            return FILE_COMPLETE;
        }

        bytesRead += r;
        rhs -= r;
        m_tail += r;
    } while (rhs > 0);

    return FILE_FULL;
}

BufferedWriter::buffer::fileResult
BufferedWriter::buffer::readFile(int const fd)
{
    size_t const left = m_capacity - m_size;
    size_t const rhs = std::min(left, m_capacity - m_tail);
    size_t const lhs = left - rhs;

    switch (fileResult result = readFileRight(fd, rhs))
    {
        case FILE_FULL:
            break;
        case FILE_ERROR:
        case FILE_COMPLETE:
            return result;
    }

    m_tail = 0;
    __sync_add_and_fetch(&m_size, rhs);

    switch (fileResult result = readFileRight(fd, lhs))
    {
        case FILE_FULL:
            break;
        case FILE_ERROR:
        case FILE_COMPLETE:
            return result;
    }

    __sync_add_and_fetch(&m_size, lhs);
    return FILE_FULL;
}


size_t BufferedWriter::buffer::write(char const *const restrict data,
        size_t size)
{
    size_t const left = m_capacity - m_size;

    if (size > left)
        size = left;

    if (size == 0)
        return 0;

    size_t const written1 = std::min(size, m_capacity - m_tail);
    memcpy(m_data + m_tail, data, written1);

    size_t const total = size;
    size -= written1;

    if (size > 0)
    {
        memcpy(m_data, data + written1, size);
        m_tail = size;
    } else
    {
        m_tail += written1;
    }

    __sync_add_and_fetch(&m_size, total);
    return total;
}

void BufferedWriter::write(char const * restrict data, size_t size)
        throw (mrutils::ExceptionNoSuchData)
{
    if (size == 0)
        return;

    if (m_state == STATE_INVALID)
    {
        throw mrutils::ExceptionNoSuchData("BufferedWriter has been invalidated.");
    }

    switch (m_mode)
    {
        case MODE_SYNC:
            for (;;)
            {
                size_t const written = m_tail->write(data, size);
                if (written == size)
                    return;

                data += written;
                size -= written;
                pushOutput(m_tail, m_tail->m_size);
            }
            break;

        case MODE_ASYNC:
            for (;;)
            {
                size_t const written = m_tail->write(data, size);
                if (written == size)
                    return;

                data += written;
                size -= written;
                struct buffer *buffer = nextBuffer();
                m_tail->m_next = buffer;
                m_tail->m_state = buffer::STATE_FULL;
                m_tail = buffer;
            }
            break;
    }

}

void BufferedWriter::writeThread()
{
    try {
        struct buffer *buffer = m_head;
        for (;;)
        {
            // spin lock until there's data
            while (buffer->m_size == 0)
            {
                if (m_done)
                {
                    if (buffer->m_state != buffer::STATE_FULL
                            && buffer->m_size == 0)
                    { return; }
                }

                if (buffer->m_state == buffer::STATE_FULL
                        && buffer->m_size == 0)
                {
                    struct buffer *const next = buffer->m_next;
                    buffer->m_state = buffer::STATE_EMPTY;
                    buffer = next;
                }
            }

            pushOutput(buffer, buffer->m_size);
        }
    } catch (...)
    { }
}

struct BufferedWriter::buffer * BufferedWriter::nextBuffer()
{
    if (m_head->m_state == buffer::STATE_EMPTY)
    {
        struct buffer *buffer = m_head;
        m_head = m_head->m_next;
        buffer->reset();
        return buffer;
    } else
    {
        return new buffer(m_capacity);
    }
}

void BufferedWriter::pushOutput(struct buffer *buffer, size_t size)
        throw (mrutils::ExceptionNoSuchData)
{
    pantheios::log(pantheios::debug,
            __PRETTY_FUNCTION__," ", pantheios::integer(size));

    if (size == 0)
        return;

    if (m_state == STATE_INVALID)
    {
        throw mrutils::ExceptionNoSuchData(
            "BufferedWriter: can't push; state already invalid.");
    }

    try
    {
        size_t const rhs = std::min(size, buffer->m_capacity - buffer->m_head);
        size_t const lhs = size - rhs;

        pushRight(buffer, rhs);
        if (lhs > 0)
        {
            buffer->m_head = 0;
            pushRight(buffer, lhs);
        }

        __sync_sub_and_fetch(&buffer->m_size, size);

    } catch (mrutils::ExceptionNoSuchData const &e)
    {
        m_state = STATE_INVALID;
        throw;
    }
}

void BufferedWriter::pushRight(struct buffer *buffer, size_t size)
        throw (mrutils::ExceptionNoSuchData)
{
#   define FUNCTION "BufferedWriter::pushRight"
    if (size == 0)
        return;

    do
    {
        if (!canPush())
        {
            throw mrutils::ExceptionNoSuchData(FUNCTION
                " can't push (timeout, disconnect, or interruption).");
        }

        int const written = ::write(m_fd,
                buffer->m_data + buffer->m_head, size);

        pantheios::log(pantheios::debug,
                __PRETTY_FUNCTION__, " wrote ", pantheios::integer(written),
                " to ", pantheios::integer(m_fd));

        if (written <= 0)
        {
            throw mrutils::ExceptionNoSuchData(FUNCTION
                " error while pushing (disconnect).");
        }

        size -= written;
        buffer->m_head += written;

    } while (size > 0);

#   undef FUNCTION
}

bool BufferedWriter::canPush() const
{
    fd_set fdsread;
    fd_set fdswrite;
    int const selmax = 1 + std::max(m_interruptFD, m_fd);

    FD_ZERO(&fdsread);
    FD_ZERO(&fdswrite);

    struct timeval waitTime = {0, 0};
    if (m_waitSeconds >= 0)
        waitTime.tv_sec = m_waitSeconds;

    if (m_interruptFD >= 0)
        FD_SET(m_interruptFD, &fdsread);

    FD_SET(m_fd, &fdswrite);

    int const ret = select(selmax, &fdsread, &fdswrite, NULL,
            0 <= m_waitSeconds ? &waitTime : NULL);

    if (ret < 0)
    {
        pantheios::log(pantheios::error,
                __PRETTY_FUNCTION__, " error ", pantheios::integer(errno),
                ": ", strerror(errno));
        return false;
    }

    if (ret == 0)
        return false;

    if (m_interruptFD >= 0 && FD_ISSET(m_interruptFD, &fdsread))
        return false;

    return true;
}

}
