#include "mr_gzipreader.h"
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _CRT_SECURE_NO_DEPRECATE
    #define _CRT_SECURE_NO_WARNINGS
    #define ZLIB_WINAPI

    #define read _read
#endif
#include "zlib.h"

mrutils::GZipReader::GZipReader()
: reader_(new mrutils::BufferedReader("",32768))
 ,reader(*reader_)
 ,parsedHeader(false)
 ,strm(new z_stream)
{}

mrutils::GZipReader::GZipReader(mrutils::BufferedReader& reader)
: reader_(NULL)
 ,reader(reader)
 ,parsedHeader(false)
 ,strm(new z_stream)
{}

mrutils::GZipReader::~GZipReader() { 
    if (parsedHeader) inflateEnd((z_stream*)strm);
    delete (z_stream*)strm; 
    if (reader_ != NULL) delete reader_; 
}

void mrutils::GZipReader::reset() {
    if (parsedHeader) inflateEnd((z_stream*)strm);
    parsedHeader = false;
}

int mrutils::GZipReader::read(char * buffer, unsigned int size) {
    z_stream & strm = *(z_stream*)this->strm;

    if (!parsedHeader && !readHeader()) return 0;

    if (0 == strm.avail_in) {
        reader.skip(reader.unreadOnBuffer());
        strm.avail_in = reader.read(reader.bufSize);
        if (strm.avail_in <= 0) return 0;
        strm.next_in = (unsigned char *) reader.line;
    }

    strm.next_out = (unsigned char*) buffer;
    strm.avail_out = size;

    switch (inflate(&strm, Z_NO_FLUSH)) {
        case Z_STREAM_END: break;
        case Z_OK: break;
        case Z_STREAM_ERROR: 
            std::cerr << "GZipReader: internal stream error" << std::endl;
            return 0;
        case Z_MEM_ERROR: 
            std::cerr << "GZipReader: out of memory" << std::endl;
            return 0;
        case Z_DATA_ERROR: 
            std::cerr << "GZipReader: invalid compressed data in " << reader.path << std::endl;
            return 0;
        default:
            perror("GZipReader: inflate error ");
            return 0;
    }

    return (size - strm.avail_out);
}

bool mrutils::GZipReader::readHeader() {
    z_stream & strm = *(z_stream*)this->strm;

    try {
        int flags;
        unsigned n;

        if (reader.get<uint8_t>() != 31 || reader.get<uint8_t>() != 139) return (std::cerr << reader.path <<  " not a gzip file" << std::endl, false);
        if (reader.get<uint8_t>() != 8) return (std::cerr << "unknown compression method in" << reader.path << std::endl, false);

        flags = reader.get<uint8_t>();

        // unhandled flag
        if (flags & 0xe0) return (std::cerr << "unknown header flags set in" << reader.path << std::endl, false);

        if (!reader.skip(6)) return false;
        if (flags & 0x4) {
            n = reader.get<uint8_t>();
            n += (unsigned)(reader.get<uint8_t>()) << 8;
            if (!reader.skip(n)) return false;
        }
        if (flags & 0x8) while (reader.get<uint8_t>() != 0) ;
        if (flags & 0x10) while (reader.get<uint8_t>() != 0) ;
        if (flags & 0x2) if (!reader.skip(2)) return false;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        return false;
    }

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (inflateInit2(&strm, -15) != Z_OK) {
        std::cerr << "GZipReader:: out of memory" << std::endl;
        return false;
    }

    reader.read(strm.avail_in = reader.unreadOnBuffer());
    strm.next_in = (unsigned char *)reader.line;

    return (parsedHeader = true);
}

void mrutils::GZipReader::readerWillSwapBuffer(char * newBuffer) {
    z_stream & strm = *(z_stream*)this->strm;
    strm.next_in = (unsigned char * )(newBuffer + (strm.next_in - (unsigned char *)reader.buffer));
}
