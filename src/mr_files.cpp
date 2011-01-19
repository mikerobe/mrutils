#include "mr_files.h"
#include "mr_strutils.h"
#include <fstream>
#include "mr_expcircbuf.h"
#include "mr_filecpr.h"
#include "mr_filermr.h"
#include "mr_memory.h"
#include "mr_threads.h"

#ifndef _MR_NUMERICAL_H
    #define MIN_(a,b) ((b)<(a)?(b):(a))
    #define MAX_(a,b) ((a)<(b)?(b):(a))
#endif

bool mrutils::rmR(const char * path, FILE * verboseOut) {
    mrutils::FileRemoveRecursive rmr(verboseOut);
    return rmr.rm(path);
}


bool mrutils::cpR(const char * src, const char * dest, bool replaceSymLinks, int replace, FILE * verboseOut) {
    mrutils::FileCopyRecursive cpr(replaceSymLinks, replace,verboseOut);
    return cpr.cp(src,dest);
}

void mrutils::dumpFile(std::ostream& os, const char * path, const char * prefix) {
    if (prefix[0] == 0) {
        std::ifstream in(path, std::ios::in);
        if (!in) return;
        os << in.rdbuf();
    } else {
        // max line length is 1024
        char buffer[1024];
        int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
        if (fd < 0) return ;
        for (char * p = mrutils::readLine(fd,buffer,1024,false); p > buffer; 
                    p = mrutils::readLine(fd,buffer,1024,false)) {
            os << prefix << buffer;
        }

        MR_CLOSE(fd);
    }
}

void mrutils::dumpFile(FILE * out, const char * path, const char * prefix) {
    if (prefix[0] == 0) {
        const size_t bufSz = 1024*1024;
        mrutils::scope_ptr_array<char> buffer(new char[bufSz]);
        int fd = fileno(out); if (fd < 0) return;
        int fs = MR_OPEN(path,O_BINARY|O_RDONLY); if (fs < 0) return;
        for (int r; 0 < (r = MR_READ(fs,buffer,bufSz));) 
            if (0==MR_WRITE(fd,buffer,r)) break;
    } else {
        char buffer[1024];

        // max line length is 1024
        int fd = MR_OPEN(path,O_RDONLY|O_BINARY);
        if (fd < 0) return ;
        for (char * p = mrutils::readLine(fd,buffer,1024,false); p > buffer; 
                    p = mrutils::readLine(fd,buffer,1024,false)) {
            fputs(prefix,out); fputs(buffer,out);
        }

        MR_CLOSE(fd);
    }
}


/**
 * This could be faster (could copy on the fly instead of upfront)
 * */
bool mrutils::mkdirP(const char * path_) {
    // make an editable copy
    mrutils::scope_ptr_array<char> path(new char[strlen(path_)+1]);
    strcpy(path, path_);
    bool windows = false;

    mrutils::formatPath(path, &windows);

    char * p = path;
    char dir = (windows?'\\':'/');
    while (*p && *p == dir) ++p;

    if (p - path > 1) {
        p = strchr(p, dir);
        if (p == NULL) return true;
        ++p;
    }

    while (*p && NULL != (p = strchr(p, dir))) {
        *p = '\0';

        if (!fileExists(path)) {
            if (!mkdir(path)) return false;
        }

        *p++ = dir;
    }

    if (p == NULL && !fileExists(path)) {
        if (!mkdir(path)) return false;
    }

    return true;
}

int mrutils::listDir(const char * dir, std::vector<std::string> &files, bool fullPath) {
    static const int MAX_PATHLEN = 256;

    if (DIR * dp = opendir(dir)) {
        char path[MAX_PATHLEN];
        char dirData[offsetof(struct dirent, d_name) + MAX_PATHLEN];

        char * endPath = strcpy( path, dir );
        *endPath++ = '/';

        for (struct dirent *dirp; 0 == readdir_r(dp, (struct dirent*)&dirData, &dirp) && dirp;) {
            if (dirp->d_name[0] == '.') continue;
            if (fullPath) {
                ::strcpy(endPath, dirp->d_name);
                files.push_back(path);
            } else {
                files.push_back(dirp->d_name);
            }
        }

        closedir(dp);

        return (int)files.size();
    } else {
        std::cerr << "Error(" << errno << ") opening " << dir << std::endl;
        return -1;
    }
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#else 
int mrutils::listDir(const char * dir, std::vector<std::string> &files, std::vector<struct dirent>& dTypes, bool fullPath, bool dirsOnly) {
    static const int MAX_PATHLEN = 256;

    if (DIR * dp = opendir(dir)) {
        char path[MAX_PATHLEN];
        char dirData[offsetof(struct dirent, d_name) + MAX_PATHLEN];

        char * endPath = strcpy( path, dir );
        *endPath++ = '/';

        for (struct dirent *dirp; 0 == readdir_r(dp, (struct dirent*)&dirData, &dirp) && dirp;) {
            if (dirp->d_name[0] == '.') continue;
            if (!dirsOnly || dirp->d_type == DT_DIR) {
                if (fullPath) {
                    ::strcpy(endPath, dirp->d_name);
                    files.push_back(path);
                    dTypes.push_back(*dirp);
                } else {
                    files.push_back(dirp->d_name);
                    dTypes.push_back(*dirp);
                }
            }
        }

        closedir(dp);

        return (int)files.size();
    } else {
        std::cerr << "Error(" << errno << ") opening " << dir << std::endl;
        return -1;
    }
}
#endif

bool mrutils::cp(const char * src, const char * dest, int replace, FILE * verboseOut, bool checkDir) {
    const size_t bufSz = 1024*1024; // 1MB
    mrutils::scope_ptr_array<char> buffer(new char[bufSz]);

    if (checkDir) {
        if (isDir(dest)) {
            strcpy(strcpy(strcpy(buffer,dest),"/"),fileName(src,strlen(src)));
            return cp(src, buffer,replace,verboseOut,checkDir);
        }
    }

    switch (replace) {
        case 0: {
            if (fileExists(dest)) return false;
        } break;

        case 1: {
            if (fileExists(dest)) {
                // only replace if dest is older
                if (fileCmpMTimes(src,dest) <= 0) return false;
            }
        } break;
    }


    int fd = MR_OPENW(dest);
    if (fd < 0) return false;
    int fs = MR_OPEN(src,O_BINARY|O_RDONLY);
    if (fs < 0) { MR_CLOSE(fd); return false; }
    for (int r; 0 < (r = MR_READ(fs,buffer,bufSz));) if (r!=MR_WRITE(fd,buffer,r)) break;
    MR_CLOSE(fs); MR_CLOSE(fd);

    if (verboseOut) { 
        fprintf(verboseOut,"copied %s to %s\n",src,dest);
        fflush(verboseOut);
    }

    // copy permissions & modification time
    struct MR_STAT s; MR_STAT(src, &s);
    struct utimbuf ut = {s.st_atime, s.st_mtime};
    return (0==chmod(dest,s.st_mode) && 0==utime(dest,&ut));
}


#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
#else 
int mrutils::breadthFind(const char * base, const char * str, FILE *fp) {
    static const int MAX_PATHLEN = 256;

    if (fp == NULL) return 0;

    int count = 0;

    ExpCircBuf<std::string> buf(8192); buf.add(base);

    char path[MAX_PATHLEN]; char * endPath;
    char dirData[offsetof(struct dirent, d_name) + MAX_PATHLEN];

    while (!buf.empty()) {
        endPath = strcpy(path,buf.head->c_str()); buf.popHead();
        *endPath++ = '/'; *endPath = '\0';

        if (DIR * dp = opendir(path)) {
            for (struct dirent *dirp; 0 == readdir_r(dp, (struct dirent*)&dirData, &dirp) && dirp;) {
                if (dirp->d_name[0] == '.') continue;

                if (stristr(dirp->d_name, str)) {
                    *endPath = '\0';
                    fprintf(fp,"\"%s%s\"\n",path,dirp->d_name);
                    fflush(fp);
                    ++count;
                }

                #if defined(__APPLE__)
                    if (dirp->d_type == DT_DIR) {
                        ::strcpy(endPath, dirp->d_name);
                #else 
                    struct MR_STAT buf1;
                    ::strcpy(endPath,dirp->d_name);
                    if (MR_STAT(path,&buf1) != 0) continue;
                    if (S_ISDIR(buf1.st_mode)) {
                #endif
                    buf.add(path);
                #if defined(__APPLE__)
                }
                #else
                } else *endPath = 0;
                #endif
            }

            closedir(dp);
        } else {
            std::cerr << "Error(" << errno << ") opening " << path << std::endl;
            perror("opendir");
            return errno;
        }
    }

    return count;
}

#endif

int mrutils::fullRead(int fd, char * buf, int len) {
    int cc;
    int total;

    total = 0;

    while (len > 0) {
        cc = MR_READ(fd, buf, len);

        if (cc < 0)
            return -1;

        if (cc == 0)
            break;

        buf += cc;
        total += cc;
        len -= cc;
    }

    return total;
}

char * mrutils::lastLine(const char * path, char * buf, int bufSize) {
    int fd = MR_OPEN(path, O_RDONLY|O_BINARY);

    if (fd == -1) {
        std::cerr << "Unable to open file" << std::endl;
        perror(path);
        return NULL;
    }

    char * p;

    off_t pos = MR_LSEEK(fd, (off_t) 0, SEEK_END);
    int bytes = MIN_(bufSize, (int)pos - 0);
    pos -= bytes;
    MR_LSEEK(fd, pos, SEEK_SET);
    bytes = fullRead(fd, buf, bytes);
    p = &buf[bytes-1];
    while (p > buf && (*p == '\n' || *p == '\r')) --p;
    *(p+1) = '\0';
    while (p > buf && *p != '\n') --p;

    MR_CLOSE(fd);

    if (*p == '\n')
        return ++p;
    return buf;
}

void mrutils::tail(int fd, int n) {
    static const off_t bufSize = 1024;
    char buf[bufSize]; 
    off_t pos = MR_LSEEK(fd, (off_t) 0, SEEK_END);

    for(;;) {
        int bytes = MIN_(bufSize, pos);
        if (bytes == 0) { MR_LSEEK(fd, (off_t) 0, SEEK_SET); return; }
        pos -= bytes;
        MR_LSEEK(fd, pos, SEEK_SET);
        bytes = fullRead(fd, buf, bytes);
        char * p = buf + bytes - 1;

        while (p > buf && n >= 0) 
            if (*p-- == '\n') --n;
        if (p > buf) {
            // found end
            pos += p +2 - buf;
            MR_LSEEK(fd, pos, SEEK_SET);
            return;
        }
    }
}


char* mrutils::readLine(int fd, char * buf, int len, bool replace) {
    int cc; int readSize = 1024; 
    char * p; char * start = buf;

    while (len > 0) {
        cc = MR_READ(fd, buf, (len < readSize?len:readSize));

        if (cc <= 0) {// error or end of file.
            MR_LSEEK(fd, start - buf, SEEK_CUR);
            return NULL;
        }

        p = strchr(buf,'\n');
        if (p != NULL) {
            MR_LSEEK(fd, (p - buf) - cc + 1, SEEK_CUR);

            if (p > buf && *(p-1) == '\r') {
                if (replace) *(p-1) = 0;
                else *++p = 0;
            } else {
                if (replace) *p = '\0';
                else *(++p) = '\0';
            }
            return p;
        } 

        buf += cc;
        len -= cc;
    }

    return NULL;
}

char* mrutils::readUntil(int fd, char * buf, int len, char c, bool replace) {
    int cc; int readSize = 1024; 
    char * p;

    while (len > 0) {
        cc = MR_READ(fd, buf, (len < readSize?len:readSize));

        if (cc <= 0) // error or end of file.
            return NULL;

        p = strchr(buf, c);
        if (p != NULL) {
            MR_LSEEK(fd, (p - buf) - cc + 1, SEEK_CUR);
            if (replace) *p = '\0';
            else *(++p) = '\0';
            return p;
        }

        buf += cc;
        len -= cc;
    }

    return NULL;
}

/**
 * Windows FLOCK
 * */
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    int mrutils::flock(int fd, int operation) {
        HANDLE h = (HANDLE) _get_osfhandle (fd);
        DWORD res;
        int non_blocking;

        if (h == INVALID_HANDLE_VALUE)
        {
            errno = EBADF;
            return -1;
        }

        non_blocking = operation & LOCK_NB;
        operation &= ~LOCK_NB;

        switch (operation)
        {
            case LOCK_SH:
                res = do_lock (h, non_blocking, 0);
                break;
            case LOCK_EX:
                res = do_lock (h, non_blocking, 1);
                break;
            case LOCK_UN:
                res = do_unlock (h);
                break;
            default:
                errno = EINVAL;
                return -1;
        }

        /* Map Windows errors into Unix errnos.  As usual MSDN fails to
         * document the permissible error codes.
         */
        if (!res)
        {
            DWORD err = GetLastError ();
            switch (err)
            {
                /* This means someone else is holding a lock. */
                case ERROR_LOCK_VIOLATION:
                    errno = EAGAIN;
                    break;

                    /* Out of memory. */
                case ERROR_NOT_ENOUGH_MEMORY:
                    errno = ENOMEM;
                    break;

                case ERROR_BAD_COMMAND:
                    errno = EINVAL;
                    break;

                    /* Unlikely to be other errors, but at least don't lose the
                     * error code.
                     */
                default:
                    errno = err;
            }

            return -1;
        }

        return 0;
    }
#endif


bool mrutils::FileAlterationMonitor::waitChange() {
    if (fd < 0) return false;

    if (pollMillis > 0) {
        for(;;) {
            int size = fileSize(path.c_str());
            if (size < lastSize) return false;
            if (size > lastSize) {
                lastSize = size;
                return true;
            }
            mrutils::sleep(pollMillis);
        }
    } else {
        #if defined(__APPLE__)
            int nevents = kevent(kq, NULL, 0, &kev, 1, NULL);

            if (nevents < 0) {
                // error
                if (opened_) MR_CLOSE(fd);
                fd = -1;
                return false;
            } else if (nevents == 0) return false;

            if (kev.fflags & NOTE_DELETE || kev.fflags & NOTE_ATTRIB) {
                // file deleted
                if (opened_) MR_CLOSE(fd);
                fd = -1;
                return false;
            }

            if (kev.fflags & NOTE_WRITE || kev.fflags & NOTE_EXTEND) {
                return true;
            }
        #elif defined(__linux__)
            for (;;) {
                int len = read(id,buf,bufsz);
                if (len <= 0) { perror("filealterationmonitor: buffer too small"); return false; }
                for (int i = 0; i < len; ) {
                    struct inotify_event * event = (struct inotify_event*)(buf+i);
                    
                    if (event->mask & IN_MODIFY) {
                        int size = fileSize(path.c_str());
                        if (size < lastSize) {
                            if (opened_) MR_CLOSE(fd);
                            fd = -1;
                            return false;
                        }

                        lastSize = size;
                        return true;
                    }

                    if (event->mask & IN_DELETE || event->mask & IN_ATTRIB) {
                        // assume deleted if attrib changes without a
                        // write
                        lastSize = 0;
                        if (opened_) MR_CLOSE(fd);
                        fd = -1;
                        return false;
                    }

                    i += sizeof(struct inotify_event) + event->len;
                }
            }
        #endif
    }

    return false;
}

void mrutils::FileAlterationMonitor::init() {
    if (fd < 0) return;

    #if defined(__APPLE__)
        if ((kq = kqueue()) == -1) {
            // error 
            if (opened_) MR_CLOSE(fd);
            fd = -1;
        }

        // NOTE_ATTRIB on a mac... means the file has been reopened 
        EV_SET(&kev, fd, EVFILT_VNODE, EV_ADD|EV_CLEAR, NOTE_WRITE | NOTE_DELETE | NOTE_EXTEND | NOTE_ATTRIB, 0, NULL);

        if (kevent(kq, &kev, 1, NULL, 0, NULL) == -1) {
            // error 
            if (opened_) MR_CLOSE(fd);
            fd = -1;
        }
    #elif defined(__linux__)
        if (0 > (id = inotify_init())) { 
            perror("inotify_init");
            if (opened_) MR_CLOSE(fd);
            fd = -1;
            return;
        }

        if (0 > (wd = inotify_add_watch(id,path.c_str(),IN_MODIFY|IN_DELETE|IN_ATTRIB))) {
            perror("inotify_add_watch");
            if (opened_) MR_CLOSE(fd);
            fd = -1;
            return;
        }
    #endif
}

mrutils::FileAlterationMonitor::FileAlterationMonitor(int fd, const char * path)
: fd(fd), opened_(false)
 ,path(path)
 ,pollMillis(0), lastSize(0)
#if defined(__APPLE__)
#elif defined(__linux__)
 ,id(-1), wd(-1)
#endif
{ init(); }

mrutils::FileAlterationMonitor::FileAlterationMonitor(const char * path)
: fd(MR_OPEN(path, O_EVTONLY | O_RDONLY | O_BINARY)), opened_(true)
 ,path(path)
 ,pollMillis(0), lastSize(0)
#if defined(__APPLE__)
#elif defined(__linux__)
 ,id(-1), wd(-1)
#endif
{ init(); }

mrutils::FileAlterationMonitor::~FileAlterationMonitor() {
    if (opened_ && fd > 0) MR_CLOSE(fd);
    #if defined(__APPLE__)
    #elif defined(__linux__)
    if (wd > 0) _UNUSED const int unused = inotify_rm_watch(id,wd);
    if (id > 0) close(id);
    #endif
}
