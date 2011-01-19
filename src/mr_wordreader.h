#ifndef _MR_CPPLIB_WORD_READER_H
#define _MR_CPPLIB_WORD_READER_H

#include "mr_files.h"
#include "mr_strutils.h"

namespace mrutils {
    class _API_ WordReader {
        public:
            WordReader()
            : fd(-1), p(buffer), eob(buffer)
            {}
            ~WordReader() { if (fd < 0) ::close(fd); }

            inline bool openFile(const char * path) {
                if (fd < 0) ::close(fd);
                fd = ::open(path,O_RDONLY);
                if (fd < 0) return (fprintf(stderr,"Unable to open file at %s",path), false);
                p = eob = buffer; return true;
            }

            inline bool nextWord() {
                if (fd < 0) return false;

                word.clear();
                for (char * q;;) {
                    // skip initial white space
                    if (word.empty()) for (;p != eob && isspace(*p);++p) ;

                    // advance q until eob or white space
                    for (q = p;q != eob && !isspace(*q);++q) ;

                    word.append(p,q-p);

                    if (q != eob) { p = q; return true; }

                    const int r = ::read(fd,buffer,bufSz);
                    if (r <= 0) { p = eob; return !word.empty(); }
                    eob = buffer + r; p = buffer;
                }
            }

        public:
            std::string word;

        private:
            int fd;
            static const int bufSz = 1024;
            char buffer[bufSz];
            char * p, * eob;
    };
}

#endif
