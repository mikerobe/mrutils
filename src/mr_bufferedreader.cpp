#include "mr_bufferedreader.h"
#ifdef OK_OUTSIDE_LIBS
    #include "mr_gzipreader.h"
#endif

#include <pantheios/pantheios.hpp>
#include <pantheios/inserters.hpp>


// minimum amt to read at end of buffer for nextLine
#define SOCKET_MIN_READ 32

#define STRIPCR(p) \
    if (stripCR && p > buffer && *p=='\n' && *(p-1)=='\r') *(p-1) = 0

#define CHECK_POS\
    for (;pos < eob;++pos)  {\
        if (*pos == '\0') strlen=pos-line;\
        else if (*pos == '\n') {\
            if (stripCR && pos != buffer && *(pos-1) == '\r') {\
                *(pos-1) = '\0';\
                strlen = pos - line - 1;\
            } else { *pos = '\0'; strlen = pos-line; }\
        } else continue;\
        ++pos; return true;\
    } 

#define CHECK_FILE \
    if (type == FT_NONE) return false; \
    if (suspended_) { if (!resume()) return false; } \

mrutils::BufferedReader::BufferedReader(const char * path, const int bufSize, char * useBuffer)
: strlen(0)
 ,bufSize(bufSize)
 ,type(FT_NONE)
 ,buffer(useBuffer != NULL ? useBuffer : (char*)malloc(bufSize))
 ,line(buffer)
 ,EOB(buffer+bufSize)
 ,pos(buffer), eob(buffer)
 ,socket(NULL)
 ,filePosition(0) ,suspended_(false)
 ,externalBuffer(useBuffer != NULL)
 ,openedSocket(false)
#ifdef OK_OUTSIDE_LIBS
 ,gzipReader(NULL)
#endif
 ,fd(-1), waitSecs(-1), recvData(NULL)
 ,selmax_(1), interruptFD(-1)
{ 
    FD_ZERO(&fds);
    if (*path) open(path);
}

mrutils::BufferedReader::~BufferedReader() {
    close(); 
    if (!externalBuffer) free(buffer);
#ifdef OK_OUTSIDE_LIBS
    if (gzipReader != NULL) delete gzipReader;
#endif
}

bool mrutils::BufferedReader::open(const char * const path_, bool clearData) {
    close(clearData);

    this->path.assign(path_);

    const char * p = path_+(this->path.size()-1);

    while (p > path_ && *p != '.') --p;
    if (*p++ != '.') {
        if (*(p-1) == '-') {
            // stdin
            type = FT_STDIN;
            fd = 0;
            return true;
        } else {
            // assume it's text
            type = FT_TXT;
        }
    } else {
        if (0==mrutils::stricmp(p, "gz")) {
            type = FT_GZ;
        } else {
            type = FT_TXT;
        }
    }

    switch (type) {
        case FT_GZ:
            #ifndef OK_OUTSIDE_LIBS
                pantheios::log(pantheios::critical,
                    __PRETTY_FUNCTION__," gzip not supported");
                type = FT_NONE;
                return false;
            #else
                if (gzipReader == NULL) gzipReader = new GZipReader();
                else gzipReader->reset();

                if (!gzipReader->reader.openPlain(path.c_str())) {
                    type = FT_NONE;
                    return false;
                } else return true;
            #endif
        case FT_TXT:
            fd = MR_OPEN(path.c_str(), O_RDONLY | O_BINARY);
            if (fd < 0) {
                type = FT_NONE; // not open
                return false;
            } else return true;
        default:
            break;
    }

    // unhandled type
    return false;
}

bool mrutils::BufferedReader::suspend() {
    if (suspended_) return true;

    switch (type) {
        case FT_TXT:
            if (fd > 2) {
                filePosition = MR_LSEEK(fd, 0, SEEK_CUR);
                MR_CLOSE(fd);
                suspended_ = true;
            }
            return true;

        case FT_GZ:
            #ifdef OK_OUTSIDE_LIBS
                return gzipReader->reader.suspend();
            #endif
            break;
        default:
            break;
    }

    pantheios::log(pantheios::critical,
        __PRETTY_FUNCTION__," can't suspend receive type");
    return false;
}

bool mrutils::BufferedReader::resume() {
    if (!suspended_) return true;

    switch (type) {
        case FT_TXT:
            fd = MR_OPEN( path.c_str(), O_RDONLY |O_BINARY );
            if (fd == -1) return false;
            MR_LSEEK(fd, filePosition, SEEK_CUR);

            return true;

        case FT_GZ:
            #ifdef OK_OUTSIDE_LIBS
                return gzipReader->reader.resume();
            #endif
            break;
        default:
            break;
    }

    // unsupported/unknown type && suspended...
    return false;
}

bool mrutils::BufferedReader::skipMore(int size) {
    CHECK_FILE

    int dif = size - (eob - pos);
    switch (type) {
        case FT_NONE:
            return false;

        case FT_TXT: {
            MR_LSEEK(fd, dif, SEEK_CUR);
            eob = buffer; pos = buffer; 
        } break;

        case FT_STDIN:
        case FT_GZ:
        case FT_RECV:
        case FT_SOCKET: {
            for (;;) {
                int ret = 0;

                switch (type) {
                    case FT_GZ:
                        #ifndef OK_OUTSIDE_LIBS
                            return false;
                        #else
                            ret = gzipReader->read(buffer, bufSize);
                        #endif
                        break;
                    case FT_RECV:
                        ret = recvFn(recvData, buffer, bufSize);
                        break;
                    case FT_STDIN:
                        ret = MR_READ(fd, buffer, bufSize);
                        break;
                    case FT_SOCKET:
                    default: // to prevent warning
                        if (waitSecs < 0) {
                            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                            if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, NULL) <= 0)
                                return false;
                            if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) return false;
                            ret = socket->recv(buffer, bufSize);
                        } else {
                            waitTime.tv_sec = waitSecs; waitTime.tv_usec = 0;
                            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                            if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, &waitTime) <= 0)
                                return false;
                            else {
                                if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) return false;
                                ret = socket->recv(buffer, bufSize);
                            }
                        }
                        break;
                }

                if (ret <= 0)
                {
                    if (ret < 0)
                    {
                        pantheios::logprintf(pantheios::error,
                                "%s:%d socket read error %d (%s)",
                                __FILE__, __LINE__, errno,
                                strerror(errno));
                    }

                    // no more data
                    return false;
                }

                if (ret >= dif) {
                    eob = buffer + ret;
                    pos = buffer + dif;
                    break;
                } else dif -= ret;
            }
        } break;
    } 

    return true;
}

int mrutils::BufferedReader::nextLineRecvHelper(bool stripCR) {
    char * p = eob;
    while (eob < EOB - SOCKET_MIN_READ) {
        // receive data
        int ret = recvFn(recvData, eob, EOB - eob);
        if (ret <= 0) { strlen = 0; return -1; }
        eob += ret;

        // search for \n or 0
        for(;p < eob;++p) {
            if (*p == '\n' || *p == 0) {
                STRIPCR(p);
                *p++ = 0;
                line = pos; pos = p;
                strlen = pos - line - 1;
                return 1;
            }
        }
    }

    strlen = 0; return false;
}

int mrutils::BufferedReader::nextLineSocketHelper(bool stripCR) {
    char * p = eob;
    while (eob < EOB - SOCKET_MIN_READ) {
        // receive data
        int ret;
        if (waitSecs < 0) {
            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
            if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, NULL) <= 0)
            { strlen = 0; return -1; }
            if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) return -1;
            ret = socket->recv(eob, EOB - eob);
        } else {
            waitTime.tv_sec = waitSecs; waitTime.tv_usec = 0;
            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
            if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, &waitTime) <= 0)
            { strlen = 0; return -1; }
            else {
                if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) 
                { strlen = 0; return -1; }
                ret = socket->recv(eob, EOB - eob);
            }
        }

        if (ret <= 0)
        {
            if (ret < 0)
            {
                pantheios::logprintf(pantheios::error,
                        "%s:%d socket read error %d (%s)",
                        __FILE__, __LINE__, errno,
                        strerror(errno));
            }

            strlen = 0;
            return -1;
        }

        eob += ret;

        // search for \n or 0
        for (;p < eob;++p) {
            if (*p == '\n' || *p == 0) {
                STRIPCR(p);
                *p++ = 0;
                line = pos; pos = p;
                strlen = pos - line - 1;
                return 1;
            }
        }
    }

    return 0;
}

bool mrutils::BufferedReader::fetchNextLine(bool stripCR) {
    CHECK_FILE

    char * p = eob;

    switch (type) { 
        case FT_NONE:
            strlen = 0; return false;

        case FT_TXT: {
            MR_LSEEK(fd, pos-eob, SEEK_CUR);
            line = buffer; eob = buffer; pos = buffer; 
            int ret = MR_READ(fd, buffer, bufSize);
            if (ret <= 0) { strlen = 0; return false; }
            eob += ret;
            CHECK_POS

            if (eob >= EOB)
            {
                pantheios::logprintf(pantheios::critical,
                        "%s %s:%d buffer overflow (eob>=EOB)",
                        __PRETTY_FUNCTION__,__FILE__,__LINE__);
                strlen = 0; return false;
            } else {
                if (eob == buffer) {
                    strlen = 0; return false;
                } else {
                    pos = eob; *eob = 0; 
                    strlen = pos - line - 1;
                    return true;
                }
            }
        } break;

        case FT_GZ: 
            #ifndef OK_OUTSIDE_LIBS
                strlen = 0; return false;
            #else 
                // copy current stub to beginning
                for (p=buffer; pos != eob; *p++ = *pos++){}
                line = buffer; pos = buffer; eob = p;

                do {
                    int ret = gzipReader->read(eob, EOB - eob);
                    if (ret <= 0) break;
                    eob += ret;
                } while (eob != EOB);

                CHECK_POS

                // if here, found no \n in the whole buffer. then on last line
                if (eob >= EOB) {
                    pantheios::logprintf(pantheios::critical,
                            "%s %s:%d buffer overflow (eob>=EOB)",
                            __PRETTY_FUNCTION__,__FILE__,__LINE__);
                    strlen = 0; return false;
                } else {
                    if (eob == buffer) {
                        // no data
                        strlen = 0; return false;
                    } else {
                        pos = eob; *eob = 0; 
                        strlen = eob - line;
                        return true;
                    }
                }
            #endif
            break;
        case FT_STDIN: 
            // copy current stub to beginning
            for (p=buffer; pos != eob; *p++ = *pos++){}
            line = buffer; pos = buffer; eob = p;
            do {
                int ret = MR_READ(fd, eob, EOB - eob);
                if (ret <= 0) break;
                eob += ret;

                // search for '\n'
                CHECK_POS
            } while (eob != EOB);

            // if here, found no \n in the whole buffer. then on last line
            if (eob >= EOB) {
                pantheios::logprintf(pantheios::critical,
                        "%s %s:%d buffer overflow (eob>=EOB)",
                        __PRETTY_FUNCTION__,__FILE__,__LINE__);
                strlen = 0; return false;
            } else {
                if (eob == buffer) {
                    // no data
                    strlen = 0; return false;
                } else {
                    pos = eob; *eob = 0;
                    strlen = eob - line;
                    return true;
                }
            }
            break;

        case FT_RECV: {
            int ret = nextLineRecvHelper(stripCR);
            if (ret < 0) return false;
            if (ret > 0) return true;

            // copy from pos to beginning
            for (p=buffer; pos != eob; *p++ = *pos++){}
            line = buffer; pos = buffer; eob = p;

            // loop & keep reading until find a \n or out of space
            ret = nextLineRecvHelper(stripCR);
            if (ret < 0) return false;
            if (ret > 0) return true;
            pantheios::logprintf(pantheios::critical,
                    "%s %s:%d buffer overflow (eob>=EOB)",
                    __PRETTY_FUNCTION__,__FILE__,__LINE__);
            return false;
        } break;

        case FT_SOCKET: {
            int ret = nextLineSocketHelper(stripCR);
            if (ret < 0) return false;
            if (ret > 0) return true;

            // copy from pos to beginning
            p = buffer;
            while (pos != eob) *p++ = *pos++;
            line = buffer; pos = buffer; eob = p;

            // loop & keep reading until find a \n or out of space
            ret = nextLineSocketHelper(stripCR);
            if (ret < 0) return false;
            if (ret > 0) return true;
            pantheios::logprintf(pantheios::critical,
                    "%s %s:%d buffer overflow (eob>=EOB)",
                    __PRETTY_FUNCTION__,__FILE__,__LINE__);
            return false;
        } break;
    }
    
    return false;
}

int mrutils::BufferedReader::readMore(int size)
{
    if (type == FT_NONE) return false;
    if (suspended_ && !resume()) return false;
    if (size > bufSize)
    {
        pantheios::log(pantheios::error,
            __PRETTY_FUNCTION__, " can't read more than ",
            pantheios::integer(bufSize));
        return false;
    }

    switch (type) {
        case FT_NONE:
            return false;

        case FT_TXT: {
            MR_LSEEK(fd, pos-eob, SEEK_CUR);
            line = buffer; eob = buffer; pos = buffer; 
            int ret = MR_READ(fd, buffer, bufSize);
            if (ret < size) size = ret;
            eob += ret; 
            pos += size;
            return size;
        } break;

        case FT_GZ: 
            #ifndef OK_OUTSIDE_LIBS
                return 0;
            #endif
        case FT_STDIN: {
                // copy current stub to beginning
                char * p = buffer;
                while (pos != eob) *p++ = *pos++;
                line = buffer; pos = buffer; eob = p;
                
                switch (type) {
                    #ifdef OK_OUTSIDE_LIBS
                        case FT_GZ:
                            do {
                                int ret = gzipReader->read(eob, EOB - eob);
                                if (ret <= 0) break;
                                eob += ret;
                            } while (eob != EOB);
                            break;
                    #endif
                    default:
                        do {
                            int ret = MR_READ(fd, eob, EOB - eob);
                            if (ret <= 0) break;
                            eob += ret;
                        } while (eob != EOB);
                }

                if (eob - buffer >= size) {
                    pos += size;
                    return size;
                } else {
                    pos = eob;
                    return (eob - buffer);
                }
        } break;

        case FT_RECV:
        case FT_SOCKET:
        {
            char * targetEnd = pos + size;

            if (targetEnd > EOB)
            {
                // not enough buffer for block
                // copy current stub to beginning
                char * p = buffer;
                while (pos != eob) *p++ = *pos++;
                line = buffer; pos = buffer; eob = p;
                targetEnd = buffer + size;
            }

            switch (type) {
                case FT_RECV: {
                    do {
                        int ret = recvFn(recvData, eob, EOB - eob);
                        if (ret <= 0) { line = pos; size = eob - pos; pos = eob; return size; }
                        eob += ret;
                    } while (eob < targetEnd);
                } break;

                default: // to prevent warning
                case FT_SOCKET: {
                    do {
                        int ret = 0;
                        if (waitSecs < 0)
                        {
                            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                            if (int const ret = select(selmax_, &fds,NULL,NULL,NULL))
                            {
                                if (ret < 0)
                                {
                                    pantheios::logprintf(pantheios::error,
                                        "%s:%d %s",
                                        __FILE__, __LINE__, strerror(errno));
                                    return 0;
                                }
                            } else
                            {
                                pantheios::log(pantheios::error,
                                        __FILE__, ": ", pantheios::integer(__LINE__),
                                        " select returned 0 with no wait...");
                                return 0;
                            }

                            if (interruptFD >= 0 && FD_ISSET(interruptFD,&fds))
                            {
                                pantheios::log(pantheios::notice,
                                        __FILE__, ": ", pantheios::integer(__LINE__),
                                        " interrupted by interruptFD",
                                        pantheios::integer(interruptFD));
                                return 0;
                            }

                            ret = socket->recv(eob, EOB - eob);

                        } else {
                            waitTime.tv_sec = waitSecs; waitTime.tv_usec = 0;
                            FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                            if (int const ret = select(selmax_, &fds,NULL,NULL,
                                    &waitTime))
                            {
                                if (ret < 0)
                                {
                                    pantheios::log(pantheios::error,
                                        __FILE__, pantheios::integer(__LINE__),
                                        " ", strerror(errno));
                                    return 0;
                                }
                            } else
                            {
                                pantheios::log(pantheios::error,
                                    __FILE__, pantheios::integer(__LINE__),
                                    " select timed out after ",
                                    pantheios::integer(waitSecs), "seconds");
                                return 0;
                            }

                            if (interruptFD >= 0 && FD_ISSET(interruptFD,&fds))
                            {
                                pantheios::log(pantheios::notice,
                                        __FILE__, pantheios::integer(__LINE__),
                                        " interrupted by interruptFD",
                                        pantheios::integer(interruptFD));
                                return 0;
                            }

                            ret = socket->recv(eob, EOB - eob);
                        }

                        if (ret < 0)
                        {
                            pantheios::log(pantheios::error,
                                __FILE__, pantheios::integer(__LINE__),
                                " ", strerror(errno));
                        }

                        if (ret == 0)
                        { // then peer has closed connection
                            pantheios::log(pantheios::debug,
                                __PRETTY_FUNCTION__,
                                " recv disconnected with [",
                                pantheios::integer(EOB-eob),
                                "] remaining on buffer, [",
                                pantheios::integer(targetEnd - eob),
                                "] remaining to read of ",
                                pantheios::integer(size));
                            line = pos;
                            size = eob - pos;
                            pos = eob;
                            return size;
                        }

                        eob += ret;

                        pantheios::log(pantheios::debug,
                            __PRETTY_FUNCTION__,
                            " got ", pantheios::integer(eob-pos),
                            "/", pantheios::integer(size));
                    } while (eob < targetEnd);
                } break;
            }



            pantheios::log(pantheios::debug,
                    __PRETTY_FUNCTION__,
                    " done getting ", pantheios::integer(size));

            line = pos; pos += size;
            return size;
        } break;
    }

    return 0;
}

int mrutils::BufferedReader::readOnce() {
    CHECK_FILE

    if (eob == EOB) {
        if (pos == eob) {
            // then start over
            line = buffer; pos = buffer; eob = buffer;
        } else return (eob - pos);
    }

    int ret;

    switch (type) {
        case FT_NONE: return false;

        case FT_STDIN:
        case FT_TXT:
            ret = MR_READ(fd,eob,EOB-eob);
            if (ret > 0) eob += ret;
            break;
        case FT_GZ:
            #ifdef OK_OUTSIDE_LIBS
            ret = gzipReader->read(eob, EOB - eob);
            if (ret > 0) eob += ret;
            #endif
            break;
        case FT_RECV:
            ret = recvFn(recvData, eob, EOB - eob);
            if (ret > 0) eob += ret;
            break;
        case FT_SOCKET:
            if (waitSecs < 0) {
                FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, NULL) > 0) {
                    if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) break;
                    ret = socket->recv(eob, EOB - eob);
                    if (ret > 0) eob += ret;
                }
            } else {
                waitTime.tv_sec = waitSecs; waitTime.tv_usec = 0;
                FD_SET(socket->s_,&fds); if (interruptFD > 0) FD_SET(interruptFD,&fds); 
                if (select(selmax_, &fds, (fd_set*)NULL,(fd_set*)NULL, &waitTime) > 0) {
                    if (interruptFD > 0 && FD_ISSET(interruptFD,&fds)) break;
                    ret = socket->recv(eob, EOB - eob);
                    if (ret > 0) eob += ret;
                }
            }
            break;
    }

    return (eob - pos);
}

void mrutils::BufferedReader::switchBuffer(char * newBuffer) {
#ifdef OK_OUTSIDE_LIBS
    if (gzipReader != NULL) {
        gzipReader->readerWillSwapBuffer(newBuffer);
    }
#endif
    memcpy(newBuffer,line,eob - line);
    eob = newBuffer + (eob - line);
    EOB = newBuffer + bufSize;
    pos = newBuffer + (pos - line);
    line = newBuffer;

    if (!externalBuffer) free(buffer);
    buffer = newBuffer;
    externalBuffer = true;
}


int mrutils::BufferedReader::cmp(char const *rhs, size_t len)
{
    for (int r = 0; 0 < (r = readOnce(), read(r));)
    {
        for (char *p = line; p != line+r; ++p, ++rhs, --len)
        {
            if (len == 0)
                return *p == 0 ? 0 : +1;

            if (*p < *rhs)
                return -1;
            if (*p > *rhs)
                return +1;
        }
    }

    return 0;
}
