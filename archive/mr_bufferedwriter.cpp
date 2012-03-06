#include "mr_bufferedwriter.h"

bool mrutils::BufferedWriter::writeFile(const char * path, int size)
{
    int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
    if (fd < 0) return false;

    for (int r
        ;size > 0
         && (current.left() >= 512 || flushOrExpand())
         && 0 < (r = MR_READ(fd,current.eob,current.left()))
        ;size -= r, current.eob += r) ;

    MR_CLOSE(fd);
    return (size == 0);
}

void mrutils::BufferedWriter::putFile(const char * path, int size)
{
    int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
    if (fd < 0)
    {
        mrutils::stringstream ss;
        ss << "BufferedWriter can't open file to put at " << path;
        throw mrutils::ExceptionNoSuchData(ss.c_str());
    }

    for (int r
        ;size > 0
         && (current.left() >= 512 || flushOrExpand())
         && 0 < (r = MR_READ(fd,current.eob,current.left()))
        ;size -= r, current.eob += r) ;

    MR_CLOSE(fd);

    if (size != 0)
    {
        mrutils::stringstream ss;
        ss << "BufferedWriter disconnected while putting file at " << path
        << ". Left to transmit: " << size;
        throw mrutils::ExceptionNoSuchData(ss.c_str());
    }
}

bool mrutils::BufferedWriter::buffer::send(mrutils::BufferedWriter *writer,
    char const *const sob, char const *const eob)
{
    if (eob == sob)
        return true;

    if (writer->fd != -1) {
        int const sz = eob - sob;
        return sz == MR_WRITE(writer->fd,sob,sz);
    }

    if (writer->socket != NULL) {
        TIMEVAL waitTime;
        fd_set fdsread;
        fd_set fdswrite;
        int const selmax = std::max(writer->interruptFD, writer->socket->s_) + 1;

        FD_ZERO(&fdsread);
        FD_ZERO(&fdswrite);

        for (char const *start = sob;;) {
            FD_SET(writer->socket->s_,&fdswrite);
            if (writer->interruptFD > 0)
                FD_SET(writer->interruptFD,&fdsread); 

            if (0 <= writer->waitSecs) {
                waitTime.tv_sec = writer->waitSecs;
                waitTime.tv_usec = 0;
            }

            if (0 >= select(selmax, &fdsread, &fdswrite, NULL,
                    0 <= writer->waitSecs ? &waitTime : NULL))
            {
                writer->m_good = false;
                return false;
            }

            if (0 <= writer->interruptFD && FD_ISSET(writer->interruptFD,&fdsread))
            {
                writer->m_good = false;
                return false;
            }

            int const ret = writer->socket->send(start, eob - start);
            if (0 >= ret)
            {
                char buf[1024];
                snprintf(buf,sizeof(buf),"%s:%d socket write error",
                        __FILE__, __LINE__);
                buf[sizeof(buf)-1] = '\0';
                perror(buf);
                writer->m_good = false;
                return false;
            }
            if (eob == (start += ret))
                return true;
        }
    }

    return false;
}

void mrutils::BufferedWriter::joinWriter()
{
    if (threadIsActive) {
        mrutils::mutexAcquire(mutex);
        threadIsActive = false;
        mrutils::mutexRelease(mutex);
        signal.send();
        mrutils::threadJoin(writerThread);
        resetBuffers();
    }
}

void mrutils::BufferedWriter::setMode(mrutils::BufferedWriter::mode_t mode)
{
    if (this->mode == mode)
        return;

    switch(this->mode) {
        case MODE_NORMAL:
            break;
        case MODE_EXPANDING:
            if (MODE_NORMAL == mode) {
                flush();
            }
            break;
        case MODE_ASYNC:
            flush();
            joinWriter();
            break;
    }

    switch(mode) {
        case MODE_NORMAL:
        case MODE_EXPANDING:
            break;
        case MODE_ASYNC:
            startWriter();
            break;
    }

    this->mode = mode;
}

void mrutils::BufferedWriter::writerThreadFn(void*)
{
    std::vector<BufferedWriter::buffer> myBuffers;

    for (;;) {
        signal.wait();

        bool doExit = false;

        mrutils::mutexAcquire(mutex);
            myBuffers.reserve(buffers.size());
            std::copy(buffers.begin(), buffers.end(),
                std::back_inserter(myBuffers));
            resetBuffers();
            doExit = !threadIsActive;
        mrutils::mutexRelease(mutex);

        bool good = std::vector<buffer>::difference_type(myBuffers.size()) == 
            std::count_if(myBuffers.begin(), myBuffers.end(),
                std::bind2nd(std::mem_fun_ref(&buffer::send),this));
        myBuffers.clear();

        if (doExit)
            break;

        if (!good) {
            mrutils::mutexAcquire(mutex);
            threadIsActive = false;
            mrutils::mutexRelease(mutex);
            break;
        }
    }
}

bool mrutils::BufferedWriter::write(const char * str, size_t size)
{
    size_t left = current.left();

    if (size > left) {
        switch(mode) {
            case MODE_NORMAL:
                if (!flushOrExpand())
                    return false;
                if (size > bufSize)
                    return buffer::send(this,str,str+size);
                break;

            case MODE_EXPANDING:
            case MODE_ASYNC:
                for (;size > left;) {
                    if (!write(str,left))
                        return false;
                    size -= left;
                    str += left;
                    flushOrExpand();
                    left = current.left();
                }
                break;
        }
    }

    current.copy(str, size);
    return true;
}

bool mrutils::BufferedWriter::flush()
{
    if (!m_good)
    {
        switch (mode)
        {
            case MODE_NORMAL:
            case MODE_ASYNC:
                return false;
            case MODE_EXPANDING:
                resetBuffers();
                return false;
        }
    }



    bool good = false;

    switch(mode) {
        case MODE_NORMAL:
            good = current.send(this);
            current.renew();
            break;

        case MODE_EXPANDING:
            good = 
                buffers_t::difference_type(buffers.size()) ==
                std::count_if(buffers.begin(), buffers.end(),
                   std::bind2nd(std::mem_fun_ref(&buffer::send),this))
                && current.send(this);
            resetBuffers();
            break;

        case MODE_ASYNC:
            if (!current.empty())
                good = flushOrExpand();
            else
                good = true;
            break;
    }
 
    return good;
}
