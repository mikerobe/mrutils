#include "mr_img.h"
#ifdef _MR_CPPLIB_IMG_H_INCLUDE
#include "mr_numerical.h"
#include <fstream>
#include <sstream>

#include <unistd.h>

#include <jpeglib.h>
#include <jerror.h>
#include <png.h>

#ifndef png_jmpbuf
#  define png_jmpbuf(png_ptr)   ((png_ptr)->jmpbuf)
#endif

#define ORIGCP(size) \
    if (mgr.img->originalEnd + size > mgr.img->originalEOB) {\
        int newSize = size + 2 * (mgr.img->originalEOB - mgr.img->originalImg);\
        int pos = mgr.img->originalEnd - mgr.img->originalImg;\
        mgr.img->originalImg = (uint8_t*)realloc(mgr.img->originalImg, newSize);\
        mgr.img->originalEnd = mgr.img->originalImg + pos;\
        mgr.img->originalEOB = mgr.img->originalImg + newSize;\
    } \
    memcpy(mgr.img->originalEnd,mgr.reader->line,size);\
    mgr.img->originalEnd += size

const char * mrutils::ImageDecode::formatExtension[] = {
    "xxx", "jpg", "png" };

bool mrutils::ImageDecode::doRead8(mrutils::BufferedReader& reader) {
    if (!reader.read(8)) return false; 
    memcpy(originalImg,reader.line,8);
    originalEnd += 8;
    return true;
}

bool mrutils::ImageDecode::isJPG(uint8_t * originalImg) {
    return (originalImg[0] == 0xFF && originalImg[1] == 0xD8);
}

bool mrutils::ImageDecode::isPNG(uint8_t * originalImg) {
    return png_check_sig(originalImg, 8);
}

/********************************************
 ******************  PNG   ******************
 ********************************************/

namespace {
    struct pngManager {
        mrutils::BufferedReader * reader; mrutils::ImageDecode * img;
    };
}

#define alpha_composite(composite, fg, alpha, bg) {               \
    uint16_t temp = ((uint16_t)(fg)*(uint16_t)(alpha) +                          \
                (uint16_t)(bg)*(uint16_t)(255 - (uint16_t)(alpha)) + (uint16_t)128);  \
    (composite) = (uint8_t)((temp + (temp >> 8)) >> 8);               \
}

static void pngRead(png_structp png_ptr, png_bytep data, png_size_t length) {
    pngManager &mgr = *(pngManager*)png_get_io_ptr(png_ptr);
    
    // TODO: this is unfortunate -- copy everything from reader
    // into both the local image store and this mystery buffer
    for (int len = (int)length; len > 0;) {
        int r = mgr.reader->read(len);
        if (r <= 0) { png_error(png_ptr,"out of data"); return; }
        memcpy(data,mgr.reader->line,r); ORIGCP(r); 
        len -= r; data += r;
    }
}

bool mrutils::ImageDecode::decodePNG(mrutils::BufferedReader& reader, bool read8) {
    pngManager mgr; mgr.reader = &reader; mgr.img = this;

    // read 8 byte header
    if (!read8) {
        if (!reader.read(8)) return false; ORIGCP(8);
    }

    if (!png_check_sig(originalImg, 8)) return false;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return false;
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) { png_destroy_read_struct(&png_ptr, NULL, NULL); return false; }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

    png_set_read_fn(png_ptr, &mgr, pngRead);

    png_set_sig_bytes(png_ptr, 8);  /* we already read the 8 signature bytes */
    png_read_info(png_ptr, info_ptr);  /* read all PNG info up to image data */

    png_uint_32 width, height;
    int bit_depth, color_type;
    if (!png_get_IHDR(png_ptr, info_ptr, (png_uint_32*) &width, (png_uint_32*) &height, &bit_depth, &color_type,
      NULL, NULL, NULL)) return false;

    this->width = width; this->height = height;

    // now read the image
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return NULL;
    }

    static double display_exponent = 2.2;
    double gamma;

    /* expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
     * transparency chunks to full alpha channel; strip 16-bit-per-sample
     * images to 8 bits per sample; and convert grayscale to RGB[A] */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_expand(png_ptr);
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    /* unlike the example in the libpng documentation, we have *no* idea where
     * this file may have come from--so if it doesn't have a file gamma, don't
     * do any correction ("do no harm") */
    if (png_get_gAMA(png_ptr, info_ptr, &gamma))
        png_set_gamma(png_ptr, display_exponent, gamma);

    png_read_update_info(png_ptr, info_ptr);

    unsigned rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int channels = (int)png_get_channels(png_ptr, info_ptr);

    png_bytepp  row_pointers = NULL;

    if ((rawImg = (uint8_t *)realloc(rawImg,rowbytes*height)) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }
    if ((row_pointers = (png_bytepp)malloc(height*sizeof(png_bytep))) == NULL) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;
    }

    for (unsigned i = 0;  i < height;  ++i)
        row_pointers[i] = rawImg + i*rowbytes;

    png_read_image(png_ptr, row_pointers);

    free(row_pointers); png_read_end(png_ptr, NULL);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    uint8_t bg_red = 0;
    uint8_t bg_green = 0;
    uint8_t bg_blue = 0;

    if (channels == 4) {
        for (uint8_t * p = rawImg, *e = rawImg + channels*height*width
            ,*q = rawImg;p != e;) {
            uint8_t r = *p++;
            uint8_t g = *p++;
            uint8_t b = *p++;
            uint8_t a = *p++;

            switch (a) {
                case 255:
                    *q++ = r; *q++ = g; *q++ = b;
                    break;
                case 0:
                    *q++ = bg_red; 
                    *q++ = bg_green; 
                    *q++ = bg_blue;
                    break;
                default:
                    alpha_composite(*q++, r, a, bg_red);
                    alpha_composite(*q++, g, a, bg_green);
                    alpha_composite(*q++, b, a, bg_blue);
                    break;
            }
        }
    }

    return true;
}


/********************************************
 ******************  JPG   ******************
 ********************************************/
namespace {
    struct jpgManager {
        // this has to be first -- functions are addressed here
        struct jpeg_source_mgr pub;

        mrutils::BufferedReader * reader; int size;
        mrutils::ImageDecode * img;
    };
}

static int fill_input_buffer(j_decompress_ptr cinfo) {
    jpgManager &mgr = *(jpgManager*)cinfo->src;

    // copy mgr.size onto original
    ORIGCP(mgr.size);

    mgr.reader->read(mgr.size); // puts pos at end
    mgr.reader->read(0); // moves line to pos

    mgr.size = mgr.reader->readOnce();

    mgr.pub.next_input_byte = (JOCTET*)mgr.reader->line;
    mgr.pub.bytes_in_buffer = mgr.size;
    return 1;
}

static void skip_input_data(j_decompress_ptr cinfo, long skipSize) {
    jpgManager &mgr = *(jpgManager*)cinfo->src;

    // read whatever amount was actually read
    int read = mgr.pub.next_input_byte - (JOCTET*)mgr.reader->line;
    mgr.reader->read(read); // advances pos

    // copy this amount read
    ORIGCP(read);

    // now skip, but need a copy
    for (int ss = skipSize;ss > 0;) {
        int r = mgr.reader->read(ss);
        if (r <= 0) {
            fprintf(stderr,"mrutils::ImageDecoder error reading... got early EOF\n"); fflush(stderr);
            return;
        } 
        ORIGCP(r); ss -= r;
    }

    mgr.reader->read(0); // moves line to pos

    if (skipSize + read > mgr.size) {
        mgr.size = 0;
    } else {
        mgr.size -= read + skipSize;
    }

    mgr.pub.next_input_byte = (JOCTET*)mgr.reader->line;
    mgr.pub.bytes_in_buffer = mgr.size;
}

static void term_source(j_decompress_ptr cinfo) {
    jpgManager &mgr = *(jpgManager*)cinfo->src;
    
    // read whatever amount was actually read
    int read = mgr.pub.next_input_byte - (JOCTET*)mgr.reader->line;

    mgr.reader->read(read); // advance pos by amount read

    // copy this amount read
    ORIGCP(read);

    mgr.reader->read(0); // move line to pos
}

inline static void init_source (_UNUSED j_decompress_ptr cinfo) {
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  jmp_buf setjmp_buffer;	/* for return to caller */
};
typedef struct my_error_mgr * my_error_ptr;

void my_error_exit (j_common_ptr cinfo) {
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

bool mrutils::ImageDecode::decodeJPG(mrutils::BufferedReader& reader, bool read8) {
    struct jpeg_decompress_struct cinfo;
    struct my_error_mgr err_mgr;
    cinfo.err = jpeg_std_error (&err_mgr.pub);
    err_mgr.pub.error_exit = my_error_exit;

    jpeg_create_decompress (&cinfo); //jpeg_mem_src (&cinfo, buffer, p-buffer);

    jpgManager mgr;
    mgr.reader = &reader; mgr.size = 0; mgr.img = this;
    mgr.pub.init_source = init_source;
    mgr.pub.fill_input_buffer = fill_input_buffer;
    mgr.pub.skip_input_data = skip_input_data;
    mgr.pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
    mgr.pub.term_source = term_source;
    mgr.pub.bytes_in_buffer = (read8?8:0); 
    mgr.pub.next_input_byte = (JOCTET*)reader.line;

    cinfo.src = (struct jpeg_source_mgr *) &mgr;

    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(err_mgr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.
         */
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }


	jpeg_read_header (&cinfo, 1);
	cinfo.do_fancy_upsampling = 0;
	cinfo.do_block_smoothing = 0;
	jpeg_start_decompress (&cinfo);

    width = cinfo.output_width; height = cinfo.output_height;

    register JSAMPARRAY lineBuf
	    = cinfo.mem->alloc_sarray ((j_common_ptr) &cinfo, JPOOL_IMAGE, cinfo.output_components*width, 1);

	if (NULL == (rawImg = (uint8_t*)realloc (rawImg, 3 * width * height))) { 
        perror("memory allocation"); 
        jpeg_finish_decompress (&cinfo);
        jpeg_destroy_decompress (&cinfo);
        return false; 
    }

    int lineOffset = 3 * width;

    switch (cinfo.output_components) {
        case 1:
            for (unsigned y = 0; y < height; ++y) {
                jpeg_read_scanlines (&cinfo, lineBuf, 1);
                    
                for (int x = 0, lineBufIndex = 0; x < lineOffset; ++x) {
                    unsigned col = lineBuf[0][lineBufIndex];
                
                    rawImg[(lineOffset * y) + x] = col;
                    ++x;
                    rawImg[(lineOffset * y) + x] = col;
                    ++x;
                    rawImg[(lineOffset * y) + x] = col;
                    
                    ++lineBufIndex;
                }			
            }
            break;
        case 3:
            for (unsigned y = 0; y < height; ++y) {
                jpeg_read_scanlines (&cinfo, lineBuf, 1);
                memcpy(rawImg + lineOffset*y,lineBuf[0],lineOffset);
            }
            break;
        default:
            fprintf(stderr,"mrutils::ImageDecoder::only handle RGB jpegs.\n"); 
            jpeg_finish_decompress (&cinfo);
            jpeg_destroy_decompress (&cinfo);
            return false;
    }

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);

    return true;
}

#endif
