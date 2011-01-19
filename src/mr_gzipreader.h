#ifndef _MR_CPPLIB_GZIPREADER_H
#define _MR_CPPLIB_GZIPREADER_H

#include "mr_exports.h"
#include <iostream>
#include <string>
#include <string.h>
#include <stdlib.h>
#include "mr_files.h"
#include "fastdelegate.h"
#include "mr_bufferedreader.h"

namespace mrutils {
    class _API_ GZipReader {
        friend class mrutils::BufferedReader;
        
        public:
            GZipReader(mrutils::BufferedReader& reader);
            GZipReader();
            ~GZipReader();

        public:
            int read( char* buffer, unsigned int size);
            void reset();

            private: mrutils::BufferedReader * reader_; public:

            mrutils::BufferedReader & reader;

        private:
            bool skip(unsigned n);
            bool readmore();

            bool readHeader();

            void readerWillSwapBuffer(char * newBuffer);

        private:
            bool parsedHeader;
            void * strm;
    };
}

#endif
