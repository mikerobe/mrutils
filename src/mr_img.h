#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_IMG_H
#endif

#ifndef _MR_CPPLIB_IMG_H
#define _MR_CPPLIB_IMG_H
#define _MR_CPPLIB_IMG_H_INCLUDE

#include "mr_exports.h"
#include "mr_packets.h"
#include "mr_bufferedreader.h"
#include "mr_threads.h"
#include <iostream>
#include <X11/Xlib.h>

namespace mrutils {

class _API_ ImageDecode {
    public:
        enum format_t {
            AUTO
           ,JPG
           ,PNG
        };

        static const char * formatExtension[];

    public:
        ImageDecode()
        : originalImg((uint8_t*)malloc(32768))
         ,originalEnd(originalImg)
         ,rawImg(NULL), width(0), height(0)
         ,ximg(NULL), ximgData(NULL)
         ,xwidth(0), xheight(0)
         ,originalEOB(originalImg + 32768)
         ,imgMutex(mrutils::mutexCreate())
        {}
        
        ~ImageDecode() {
            if (rawImg != NULL) delete rawImg;
            if (originalImg != NULL) delete originalImg;
            if (ximg != NULL) delete ximg;
            if (ximgData != NULL) delete ximgData;
            mrutils::mutexDelete(imgMutex);
        }
        
    public:
       /******************
        * Main methods
        ******************/

       /**
        * Returns true if ImageDecoder knows how to decode this image
        * format
        */
        static inline bool knownFormat(const char * buffer) {
            return (isJPG((uint8_t*)buffer) || isPNG((uint8_t*)buffer));
        }

        inline bool decode(mrutils::BufferedReader& reader
            ,format_t format = AUTO) {
            bool ret = false;

            mrutils::mutexAcquire(imgMutex);

                originalEnd = originalImg;

                bool read8 = format == AUTO;
                if (read8) {
                    if (doRead8(reader)) {
                        if (isJPG(originalImg))
                            format = JPG;
                        else if (isPNG(originalImg))
                            format = PNG;
                    }
                }

                this->format = format;

                switch (format) {
                    case JPG:
                        ret = decodeJPG(reader, read8);
                        break;
                    case PNG:
                        ret = decodePNG(reader, read8);
                        break;
                    default: break;
                }

            mrutils::mutexRelease(imgMutex);

            return ret;
        }

        inline bool saveOriginal(const char * path) {
            bool goodWrite = true;

            mrutils::mutexAcquire(imgMutex);
                if (originalEnd == originalImg) {
                    mrutils::mutexRelease(imgMutex);
                    return false;
                }

                int fd = MR_OPENW(path);
                if (fd < 0) { mrutils::mutexRelease(imgMutex); return false; }

                goodWrite = (originalEnd - originalImg) == 
                    MR_WRITE(fd,originalImg,originalEnd - originalImg);

                MR_CLOSE(fd);
            mrutils::mutexRelease(imgMutex);

            return goodWrite;
        }

    public:
        uint8_t * originalImg; 
        uint8_t * originalEnd;
        uint8_t * rawImg;
        unsigned width, height;
        format_t format;

        // these can (optionally) be used to store xlib data
        XImage * ximg; unsigned * ximgData;
        int xwidth, xheight;

    public:
        uint8_t * originalEOB;

    private:
        bool decodeJPG(mrutils::BufferedReader& reader, bool read8);
        bool decodePNG(mrutils::BufferedReader& reader, bool read8);
        static bool isJPG(uint8_t * buffer);
        static bool isPNG(uint8_t * buffer);
        bool doRead8(mrutils::BufferedReader& reader);

        MUTEX imgMutex;
};

}

#endif
