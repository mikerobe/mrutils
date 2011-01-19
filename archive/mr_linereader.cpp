#include <iostream>
#include "mr_linereader.h"
#include "mr_strutils.h"
#include "mr_files.h"
#include "mr_sockets.h"

using namespace mrutils;

LineReader::~LineReader() {
    close();
}

bool LineReader::open() {
    if (opened) return false;

    if (strcmp(path.c_str(), "-") == 0) {
        isText = true;
        opened = true;
        line = lineBuffer;
        fd = 0;
        return true;
    }

    char extension[4]; extension[3] = '\0';
    const char * p = strchr(path.c_str(), ':');
    if (p != NULL && p-path.c_str() == 1) p = strchr(p+1, ':');

    if (p) {
        //isFF = true;
        //strncpy(extension,path.c_str() + (p - path.c_str()) - 3,3);
        // no longer using fast file
        return false;
    } else {
        strncpy(extension,path.c_str()+path.size()-3,3);
    }

    if (mrutils::stricmp(extension,"zip") == 0) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            fprintf(stderr,"zip not supported on windows.\n");
            return false;
        #else
            isZip = true;
            line = NULL;
            zipEnd = NULL;

            opened = true;
            if ((zipFile = unzOpen( path.c_str() )) == NULL) {
                std::cerr << "no such file path " << path << std::endl;
                return false;
            }

            int err = unzGoToFirstFile( zipFile );

            if (err != UNZ_OK) {
                std::cerr << "unz unable to go to first file, error " << err << std::endl;
                return false;
            }

            err = unzOpenCurrentFile( zipFile );
            if (err != UNZ_OK ) {
                std::cerr << "unz unable to open current file, error " << err << std::endl;
                return false;
            }

            return true;
        #endif
    } else if (mrutils::stricmp(extension,".gz") == 0) {
        #ifdef USE_VULCAN
            fprintf(stderr,"gzip not supported on windows.\n");
            return false;
        #else
            isGZip = true;
            line = NULL;
            zipEnd = NULL;
            opened = true;

            return gzipReader.open(path.c_str());
        #endif
    } else {
        isText = true;
        opened = true;
        if (isFF) {
            return true;
            //line = NULL;
            //zipEnd = NULL;

            //return fastFile.open(path.c_str());
        } else {
            line = lineBuffer;
            fd = MR_OPEN( path.c_str(), O_RDONLY );
            return (fd > 2);
        }
    }
}

void LineReader::suspend() {
    if (suspended) return;
    if (isText) {
        if (fd > 2) { // can't suspend stdin
            filePosition = MR_LSEEK(fd, 0, SEEK_CUR);
            MR_CLOSE(fd);
            opened = false;
            suspended = true;
        }
    } else if (isGZip) {
        #ifndef USE_VULCAN
            gzipReader.suspend();
            suspended = true;
        #endif
    }

}

bool LineReader::resume() {
    if (!suspended) return true;
    if (isText) {
        fd = MR_OPEN( path.c_str(), O_RDONLY );
        if (fd == -1) return false;
        MR_LSEEK(fd, filePosition, SEEK_CUR);
    } else if (isGZip) {
        #ifndef USE_VULCAN
            if (!gzipReader.resume()) return false;
        #endif
    }

    suspended = false;
    opened = true;
    return true;
}

bool LineReader::readBuf() {
    int chars = 0;

    if (isZip) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        #else
            chars = unzReadCurrentFile( zipFile, zipBuffer, LineReader::ZIP_BUFF_SIZE-1 );
        #endif
    } else if (isGZip) {
        #ifndef USE_VULCAN
            chars = gzipReader.read( zipBuffer, LineReader::ZIP_BUFF_SIZE-1 );
        #endif
    } else {
        sock->waitAccept();
        chars = sock->read(zipBuffer, LineReader::ZIP_BUFF_SIZE - 1);
        //chars = fullRead( fd, zipBuffer, LineReader::ZIP_BUFF_SIZE-1 );
        //chars = fastFile.read( zipBuffer, LineReader::ZIP_BUFF_SIZE-1 );
    }

    if (chars <= 0) return false;

    zipBuffer[chars] = '\0';
    return true;
}

bool LineReader::nextLine() {
    if (!opened && !open()) return false;
    if (suspended && !resume()) return false;

    if (!isText || isFF) {
        if (zipEnd != NULL) {
            zipEnd++;
            line = zipEnd;

            if (*line == '\0') {
                if ( !readBuf() ) {
                    return false;
                } else {
                    line = (char*)zipBuffer;
                    zipEnd = line;
                }
            }

            bool onBuffer = true;
            while ( (zipEnd = strchr( zipEnd, '\n' )) == NULL ) {
                line = mrutils::strcpy( 
                    (onBuffer?lineBuffer:line),  
                    (onBuffer?line:(char*)zipBuffer) );
                onBuffer = false;

                if (*(line-1) == '\r') *--line = '\0';

                if ( !readBuf() ) {
                    zipEnd = NULL;
                    line = lineBuffer;
                    return true;
                } else {
                    zipEnd = (char*)zipBuffer;
                }
            }

            if (zipEnd != zipBuffer && *(zipEnd-1) == '\r') *(zipEnd-1) = '\0';
            *zipEnd = '\0';

            if (!onBuffer) {
                mrutils::strcpy(line, (char*) zipBuffer);
                line = lineBuffer;
            }

            return true;
        } else { // first & last call
            line = (char*) zipBuffer;

            do {
                if ( !readBuf() ) {
                    if (line == zipBuffer) return false;
                    break;
                }
                zipEnd = strnl( (char*)zipBuffer );
                if (zipEnd != NULL) {
                    if (*zipEnd == '\r') *zipEnd++ = '\0';
                    *zipEnd = '\0';

                    if (line != zipBuffer) strcpy( line, (char*)zipBuffer );
                } else {
                    line = mrutils::strcpy( (line == zipBuffer?lineBuffer:line), (char*)zipBuffer );

                }

            } while (zipEnd == NULL);

            if (line != zipBuffer) line = lineBuffer;

            return true;
        }
    } else {
        if (fd == 0) {
            if (fgets(lineBuffer, lineBufferSize, stdin) == NULL) return false;
            mrutils::capNewLine(lineBuffer);
            return true;
        } else {
            while (mrutils::readLine(fd, lineBuffer, lineBufferSize) != NULL) {
                if (lineBuffer[0] == '\0') continue;
                return true;
            }
        }
        return false;
    }
}
