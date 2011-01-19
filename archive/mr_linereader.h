#ifndef _LINEREADER_H
#define _LINEREADER_H

#define ZLIB_WINAPI

#include "mr_exports.h"
#include <iostream>

#include "mr_strutils.h"
#include "mr_sockets.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #include "mr_files.h"
#else
    #include "unzip.h"
#endif

#ifndef USE_VULCAN
    #include "mr_gzipreader.h"
#endif

//#define ZIP_BUFF_SIZE 32769U
//#define ZIP_BUFF_SIZE 5242880
///static const unsigned int ZIP_BUFF_SIZE = 5242880;

namespace mrutils {

class _API_ LineReader {
    static const unsigned int ZIP_BUFF_SIZE = 32769;

    public:
        LineReader(char const * const path )
        : path(path), fd(-1), opened(false)
         ,isText(false), isZip(false), isGZip(false)
         ,isFF(false), filePosition(0)
         ,suspended(false), sock(NULL)
        {}

        LineReader(Socket& sock)
        : path("socket"), fd(-1), opened(true)
         ,isText(true), isZip(false), isGZip(false)
         ,isFF(true), filePosition(0)
         ,suspended(false), line(NULL), zipEnd(NULL)
         ,sock(&sock)
        {}

        ~LineReader();

        /**
         * Opens the target file; returns if successful. 
         * */
        bool open();

        bool changePath(const char * newPath) {
            this->path = newPath;
            opened = false;
            return true;
        }

        /**
         * Returns true if open() has already been called. 
         * */
        inline bool haveOpened() { return opened; }

        inline void close() {
            opened = false;
            if (isText) {
                if (fd > 2) {
                    MR_CLOSE(fd);
                    fd = -1;
                }
            } else if (isGZip) {
                #ifndef USE_VULCAN
                    gzipReader.close();
                #endif
            } else if (isZip) {
                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                #else
                    unzCloseCurrentFile( zipFile );
                #endif
            }
        }

        /**
         * This closes the file handle and caches its current
         * position. A call to resume, reopens the file handle
         * and seeks to the current position in the file. 
         * */
        void suspend();
        inline bool isSuspended() { return suspended; }
        bool resume();

        /**
         * Reads the next line in the file if there is one. 
         * */
        bool nextLine(); 

        char * line;
        std::string path;
    private:
        bool isZip, isGZip, isFF, isText;
        bool opened, suspended;

        // plain file method
        int fd;// FILE * fp;
        static const int lineBufferSize = 16384;
        char lineBuffer[16384];
        off_t filePosition;

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        #else
            unzFile zipFile;
        #endif

        #ifndef USE_VULCAN
        mrutils::GZipReader gzipReader;
        #endif

        char zipBuffer[ZIP_BUFF_SIZE];
        char * zipEnd;

        bool readBuf();

        mrutils::Socket * sock;
};

}

#endif
