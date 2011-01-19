#include "mr_filecpr.h"

char mrutils::FileCopyRecursive::fileStat(const char * path, bool isSrc, unsigned * modTime) {
    if (isSrc && reader != NULL) {
        // remote file
        writer->write("filestat ");
        writer->writeLine(path);
        writer->flush();

        if (!reader->read(1)) return 0;
        char type = *reader->line;
        
        if (type == 0) return 0;

        if (!reader->read(sizeof(int))) return 0;
        if (modTime != NULL) *modTime = ntohl(*(int*)reader->line);
        return type;
    } else {
        // local file
        struct MR_STAT s;
        if (MR_STAT(path,&s) != 0) return 0;
        if (modTime != NULL) *modTime = (unsigned)s.st_mtime;

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            return(s.st_mode & S_IFDIR?'d':'f');
        #else
            if (s.st_mode & S_IFDIR) return 'd';
            if (0 < ::readlink(path,buffer,bufSize)) return 'l';
            return 'f';
        #endif
    }
}

bool mrutils::FileCopyRecursive::listDir(const char * path, bool isSrc
    ,std::vector<std::string>& files
    ,std::vector<char>& types
    ,std::vector<unsigned>& modTimes
    ) {

    if (isSrc && reader != NULL) {
        // remote
        writer->write("filelist ");
        writer->writeLine(path);
        writer->flush();

        for (;;) {
            if (!reader->read(1)) return false;
            char type = *reader->line;
            if (!type) return true;
            if (!reader->read(sizeof(int))) return true;
            unsigned mtime = ntohl(*(unsigned*)reader->line);
            if (!reader->nextLine()) return true;
            files.push_back(reader->line);
            modTimes.push_back(mtime);
            types.push_back(type);
        }
    } else {
        DIR *dp; struct dirent *dirp;
        char * p = mrutils::strcpy(buffer,path); *p++ = '/';

        if((dp = opendir(path)) == NULL) return false;
        while ((dirp = readdir(dp)) != NULL) {
            switch(dirp->d_name[0]) {
                case 0:
                case '.':
                    continue;
            }

            struct MR_STAT s; 
            mrutils::strcpy(p,dirp->d_name);
            if (MR_STAT(buffer,&s) != 0) continue;

            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                types.push_back(s.st_mode & S_IFDIR?'d':'f');
            #else
                types.push_back(s.st_mode & S_IFDIR?'d':dirp->d_type==DT_LNK?'l':'f');
            #endif

            modTimes.push_back((unsigned)s.st_mtime);
            files.push_back(dirp->d_name);
        }
        closedir(dp);
    }

    return true;
}

bool mrutils::FileCopyRecursive::cp(const char * src, const char * dest, int mtime) {
    int fd = MR_OPENW(dest);
    if (fd < 0) return false;

    if (reader == NULL) {
        int fs = MR_OPEN(src,O_BINARY|O_RDONLY);
        if (fs < 0) { MR_CLOSE(fd); return false; }
        for (int r; 0 < (r = MR_READ(fs,buffer,bufSize));) {
            if (r!=MR_WRITE(fd,buffer,r)) break;
        }
        MR_CLOSE(fs);
    } else {
        // remote source
        writer->write("fileget ");
        writer->writeLine(src);

        // end request list
        writer->writeLine("");
        writer->flush();

        // read result -- one file
        if (!reader->read(sizeof(int))) { MR_CLOSE(fd); return false; }
        int size = ntohl(*(int*)reader->line);
        for (int r;size>0;size -=r) {
            r = reader->read(size);
            if (r == 0) { MR_CLOSE(fd); return false; }
            if (r!=MR_WRITE(fd,reader->line,r)) break;
        }
    }

    MR_CLOSE(fd);

    if (verboseOut) {
        fprintf(verboseOut,"copied %s to %s\n",src,dest);
        fflush(verboseOut);
    }

    // set modification time, access time to 
    struct utimbuf ut = {mtime, mtime};
    return (0==utime(dest,&ut));
}

bool mrutils::FileCopyRecursive::init(bool *sourceIsDir, unsigned * srcMod) {
    *sourceIsDir = false;

    char srcType = fileStat(srcPath,true,srcMod);

    if (!srcType) {
        std::cerr << "cpR: src " << srcPath << " does not exist." << std::endl;
        return false;
    }

    // see if the source is dir 
    *sourceIsDir = srcType == 'd';
    if (*sourceIsDir && *(endSrc-1) != '/') {
        *endSrc++ = '/'; *endSrc = '\0';
    }

    // check existence of dest file structure
    int nBack = 0;
    if (*(endDest-1) != '/') {
        if (*sourceIsDir) {
            // make dest a directory
            *endDest++ = '/'; *endDest = '\0';

            // if dest exists & is not a directory, return false
            if (fileExists(destPath)) {
                if (!isDir(destPath)) {
                    std::cerr << "cpR: can't copy to destination " << destPath
                              << ". exists & is not a directory." << std::endl;
                    return false;
                }
            }
        } else {
            char * p = mrutils::strchrRev(endDest-1, '/', destPath);
            if (p != NULL) {
                nBack = endDest - p;
                *p = '\0';
            }
        }
    } else {
        if (*sourceIsDir) {
            endDest = mrutils::strcpy(endDest, mrutils::fileName(srcPath, endSrc-srcPath-1));
            *endDest++ = '/'; *endDest = '\0';
        } 
    }

    if (!mkdirP(destPath)) return false;
    *(endDest-nBack) = '/';

    if (!*sourceIsDir) {
        if (*(endDest-1) == '/') 
            mrutils::strcpy(endDest, fileName(srcPath, endSrc-srcPath));
    }

    return true;
}

bool mrutils::FileCopyRecursive::run() {
    std::vector<std::string> files;
    std::vector<char> types;
    std::vector<unsigned> modTimes;

    if (!listDir(srcPath,true,files,types,modTimes)) return false;

    if (reader != NULL) {
        bool hasFiles = false;

        std::vector<int> fileNumbers;
        fileNumbers.reserve(files.size());

        // then aggregate all of the plain files into a single request
        for (unsigned i = 0; i < files.size(); ++i) {
            if (types[i] == 'f') {
                mrutils::strcpy(endSrc, files[i].c_str());
                mrutils::strcpy(endDest, files[i].c_str());

                if (okToCopy(modTimes[i])) {
                    fileNumbers.push_back(i);

                    writer->write("fileget ");
                    writer->writeLine(srcPath);
                    hasFiles = true;
                }
            }
        }

        if (hasFiles) {
            writer->writeLine(""); writer->flush();

            // now read results in order
            for (std::vector<int>::iterator it = fileNumbers.begin()
                ;it != fileNumbers.end(); ++it) {
                int i = *it;

                if (types[i] == 'f') {
                    // destination file
                    mrutils::strcpy(endDest, files[i].c_str());
                    int fd = MR_OPENW(destPath);
                    if (fd < 0) {
                        fprintf(stderr,"Unable to open local file at %s\n",destPath);
                        return false;
                    }

                    // file size
                    if (!reader->read(sizeof(int))) { MR_CLOSE(fd); return false; }
                    int size = ntohl(*(int*)reader->line);
                    
                    // copy file contents
                    for (int r;size>0;size -=r) {
                        r = reader->read(size);
                        if (r == 0) { MR_CLOSE(fd); return false; }
                        if (r!=MR_WRITE(fd,reader->line,r)) break;
                    }

                    MR_CLOSE(fd);

                    if (verboseOut) {
                        mrutils::strcpy(endSrc, files[i].c_str());
                        fprintf(verboseOut,"copied %s to %s\n",srcPath,destPath);
                        fflush(verboseOut);
                    }

                    // set modification time, access time to 
                    struct utimbuf ut = {modTimes[i], modTimes[i]};
                    if (0!=utime(destPath,&ut)) {
                        fprintf(stderr,"Unable to set modification times for local file at %s\n"
                            ,destPath);
                        return false;
                    }
                }
            }
        }
    }

    for (unsigned i = 0; i < files.size(); ++i) {
        switch (types[i]) {
            case 'l':
            {
                mrutils::strcpy(endSrc, files[i].c_str());
                char * linkPath = buffer;
                int ret = readlink(srcPath, &linkPath, bufSize);
                if (ret < 0) {
                    *endSrc = '\0';
                    continue;
                }
                linkPath[ret] = '\0';
                *endSrc = '\0';

                mrutils::strcpy(endDest, files[i].c_str());

                if (replaceSymLinks) {
                    mrutils::strcpy(endSrc, linkPath);

                    // if source is a directory now, skip it
                    unsigned modTime;
                    char t = fileStat(srcPath,true,&modTime);
                    if (!t || t == 'd') {
                        continue;
                    } else {
                        // if the dest already exists, delete it if sym link
                        bool doCopy = true;
                        if (fileExists(destPath)) {
                            unsigned modTimeDest;
                            char t = fileStat(destPath,false,&modTimeDest);
                            if (t == 'l') {
                                doCopy = true;
                                if (!rm(destPath)) {
                                    std::cerr << "cpR: error removing " << destPath << std::endl;
                                    return false;
                                }
                            } else {
                                doCopy = (modTimeDest < modTime);
                            }
                        }
                        if (doCopy && !cp(srcPath,destPath,modTime)) {
                            std::cerr << "cpR: error copying " << srcPath << " to " << destPath << std::endl;
                            return false;
                        }
                    }
                } else {
                    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                    #else
                        mrutils::strcpy(endSrc, files[i].c_str());
                        if (0!=symlink(linkPath, destPath)) {
                            fprintf(stderr,"error creating link at %s to %s\n",linkPath, destPath);
                        }
                        *endDest = '\0';
                    #endif
                }

            } break;

            case 'd':
            {
                char * p = endSrc;
                char * q = endDest;

                endSrc = mrutils::strcpy(endSrc, files[i].c_str());
                *endSrc++ = '/'; *endSrc = '\0';
                endDest = mrutils::strcpy(endDest, files[i].c_str());
                *endDest++ = '/'; *endDest = '\0';

                // make sure destination exists
                if (!mkdir(destPath)) {
                    std::cerr << "cpR: unable to create " << destPath << std::endl;
                    return false;
                }

                if (!run()) {
                    return false;
                }

                endSrc = p; endDest = q;
                *endSrc = '/'; *endDest = '/';

            } break;

            default:
            {
                if (reader == NULL) {
                    mrutils::strcpy(endSrc, files[i].c_str());
                    mrutils::strcpy(endDest, files[i].c_str());

                    if (okToCopy(modTimes[i]) 
                        && !cp(srcPath,destPath,modTimes[i])) {
                        std::cerr << "cpR: error copying " << srcPath << " to " << destPath << std::endl;
                        return false;
                    }

                    *endDest = '\0'; *endSrc = '\0';
                }
            }
        }
    }

    return true;
}
