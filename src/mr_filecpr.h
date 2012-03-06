#ifndef _MR_CPPLIB_FILE_COPY_RECURSIVE_H
#define _MR_CPPLIB_FILE_COPY_RECURSIVE_H

#include "mr_exports.h"
#include "mr_bufferedreader.h"
#include "mr_bufferedwriter.h"
#include "mr_strutils.h"
#include "mr_files.h"

namespace mrutils {

/**
 * Allows the source to be a socket or local.
 */
class _API_ FileCopyRecursive {
    public:
        FileCopyRecursive(bool replaceSymLinks = 0
            ,int replace = 1, FILE * verboseOut = NULL
            )
        : reader(NULL), writer(NULL)
         ,replaceSymLinks(replaceSymLinks)
         ,replace(replace), verboseOut(verboseOut)
         ,buffer((char*)malloc(bufSize))
        {}

        ~FileCopyRecursive() {
            free(buffer);
        }

        inline void setSourceSocket(mrutils::BufferedReader& reader
            ,mrutils::BufferedWriter& writer) {
            this->reader = &reader;
            this->writer = &writer;
        }

    public:
        inline bool cp(const char * src, const char * dest) {
            endSrc  =mrutils::replaceCopy(srcPath,src,'\\','/');
            endDest =mrutils::replaceCopy(destPath,dest,'\\','/');

            bool sourceIsDir; unsigned srcMod;

            if (!init(&sourceIsDir,&srcMod)) return false;

            if (!sourceIsDir) {
                if (okToCopy(srcMod)) {
                    return cp(srcPath,destPath,srcMod);
                } else return true;
            } else return run();
        }

    private:
        bool init(bool *sourceIsDir, unsigned * srcMod);
        bool run();

        char fileStat(const char * path, bool isSrc, unsigned * modTime);
        bool listDir(const char * path, bool isSrc
            ,std::vector<std::string>& files
            ,std::vector<char>& types
            ,std::vector<unsigned>& modTimes
            );

        bool cp(const char * src, const char * dest, int mtime);

        inline bool okToCopy(int srcMod) {
            switch (replace) {
                case 0:
                    return !fileExists(destPath);
                case 1: {
                    struct MR_STAT s;
                    return (0!=MR_STAT(destPath, &s) || s.st_mtime < srcMod);
                    }
            }
            return true;
        }

        inline int readlink(const char * src, char ** buffer, int size) {
            if (reader != NULL) {
                // remote
                writer->write("filereadlink ",strlen("filereadlink "));
                *writer << src;
                writer->flush();
                if (!reader->nextLine()) return -1;
                *buffer = reader->line;
                return reader->strlen;
            } else {
                #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                    return -1;
                #else 
                    return ::readlink(src,*buffer,size);
                #endif
            }
        }
    
    private:
        mrutils::BufferedReader * reader;
        mrutils::BufferedWriter * writer;
        bool replaceSymLinks;

        char srcPath [1024];
        char destPath[1024];

        int replace; 
        FILE * verboseOut;

        char * endSrc;
        char * endDest;

        static const unsigned bufSize = 5*1024*1024; // 5MB
        char * buffer;

};

}


#endif
