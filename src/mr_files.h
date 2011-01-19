#ifndef _MR_CPP_LIB_FILES
#define _MR_CPP_LIB_FILES

#include "mr_exports.h"

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <vector>
#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include <ctype.h>

#include "mr_strutils.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #include "win_dirent.h"
    #include <io.h>
    #include <sys/utime.h>

    #define MR_ACCESS _access
    #define MR_STAT _stat
    #define MR_UTIME _utime
    #define MR_OPEN   _open
    #define MR_WRITE  _write
    #define MR_READ   _read
    #define MR_LSEEK  _lseek
    #define MR_CLOSE  _close
    #define MR_FLOCK(fd) mrutils::flock(fd,LOCK_EX)
    #define MR_FUNLOCK(fd) mrutils::flock(fd,LOCK_UN)
    #define MR_PIPE(fds) ::_pipe(fds,256,O_BINARY)

    #define MR_POPEN _popen
    #define MR_PCLOSE _pclose

    #define MR_PWD(buffer,size) _getcwd(buffer,size)

    #define	LOCK_SH		0x01		/* shared file lock */
    #define	LOCK_EX		0x02		/* exclusive file lock */
    #define	LOCK_NB		0x04		/* don't block when locking */
    #define	LOCK_UN		0x08		/* unlock file */

    #define MR_OPENW(path)\
        MR_OPEN(path,O_BINARY|O_CREAT | O_WRONLY | O_TRUNC, _S_IREAD | _S_IWRITE)
    #define MR_OPENWA(path)\
        MR_OPEN(path,O_BINARY|O_CREAT | O_WRONLY | O_APPEND, _S_IREAD | _S_IWRITE)

    #define O_EVTONLY 0

#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>

    // this is for the file event monitor
    #if defined(__APPLE__)
        #include <sys/event.h>
    #elif defined(__linux__)
        #include <sys/inotify.h>
        #include <sys/file.h>
    #endif

    #ifndef O_EVTONLY
        #define O_EVTONLY 0
    #endif

    #include <termios.h>
    #include <utime.h>

    #define MR_ACCESS ::access
    #define MR_STAT stat
    #define MR_UTIME utime
    #define MR_OPEN   ::open
    #define MR_WRITE  ::write
    #define MR_READ   ::read
    #define MR_LSEEK  ::lseek
    #define MR_CLOSE  ::close
    #define MR_PIPE(fds) ::pipe(fds)

    #define MR_POPEN popen
    #define MR_PCLOSE pclose

    #ifdef LOCK_EX
        #define MR_FLOCK(fd)  ::flock(fd,LOCK_EX)
        #define MR_FUNLOCK(fd) ::flock(fd,LOCK_UN)
    #else
        #define MR_FLOCK(fd) mrutils::file_lock(fd)
        #define MR_FUNLOCK(fd) mrutils::file_unlock(fd)
    #endif

    #define MR_PWD(buffer,size) getcwd(buffer,size)

    #ifndef O_BINARY
        #define O_BINARY 0
    #endif

    #define MR_OPENW(path)\
        MR_OPEN(path,O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
    #define MR_OPENWA(path)\
        MR_OPEN(path,O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif

namespace mrutils {

    inline bool fileExists(const char * path, int mode = 0) {
        return (MR_ACCESS(path, mode) == 0);
        //struct MR_STAT buf;
        //return (MR_STAT(path, &buf) == 0);
    }

    inline int fileSize(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
            if (0==GetFileAttributesEx(path, GetFileExInfoStandard, (void*)&fileInfo)) 
                return 0;
            return fileInfo.nFileSizeLow;
        #else
            struct MR_STAT buf;
            if (MR_STAT(path,&buf) != 0) return 0;
            return buf.st_size;
        #endif
    }

    /**
      * Comparison of modification times of 2 files.
      * Returns -1 if path1 is older than path2
      * Returns +1 if path2 is older than path1
      * 0 if identical
      */
    inline int fileCmpMTimes(const char * path1, const char * path2) {
        struct MR_STAT buf1;
        struct MR_STAT buf2;

        if (MR_STAT(path1,&buf1) != 0) return -1;
        if (MR_STAT(path2,&buf2) != 0) return +1;

        if (buf1.st_mtime > buf2.st_mtime) return +1;
        if (buf1.st_mtime < buf2.st_mtime) return -1;
        return 0;
    }

    inline bool isDir(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            // windows can't stat "asdf/"; must be "asdf" even if a directory
            size_t size = strlen(path); 
            if (size == 0) return true; // suppose \0 means root...
            char end = path[size-1];
            if (end == '/' || end == '\\') return true;
        #endif

        struct MR_STAT buf1;
        if (MR_STAT(path,&buf1) != 0) return false;
        return ((buf1.st_mode & S_IFDIR) != 0);
    }

    inline bool touch(const char * path) {
        int fd = MR_OPENW(path);
        if (fd < 0) return false;
        MR_CLOSE(fd); return true;
    }

    /**
      * Recursivley remove the given path & contents
      */
    _API_ bool rmR(const char * path, FILE * verboseOut = NULL);

    inline bool mv(const char * src, const char * dest) {
        return 0==rename(src,dest);
    }

    inline bool rm(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            return 0!=DeleteFile(path);
        #else
            return 0==remove(path);
        #endif
    }

    inline bool rmdir(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            return (0!=RemoveDirectory(path));
        #else
            return 0==remove(path);
        #endif
    }

    inline char * fileName(char * fullPath, int size) {
        char * a = mrutils::strchrRev(&(fullPath[size-1]), '/', fullPath);
        char * b = mrutils::strchrRev(&(fullPath[size-1]), '\\', fullPath);
        if (b == NULL) {
            if (a == NULL) return fullPath; 
            else return (a+1);
        }
        if (a == NULL) return (b+1);
        if (a > b) return (a+1); 
        else return (b+1);
    }

    inline const char * fileName(const char * fullPath, int size) {
        const char * a = mrutils::strchrRev(&(fullPath[size-1]), '/', fullPath);
        const char * b = mrutils::strchrRev(&(fullPath[size-1]), '\\', fullPath);
        if (b == NULL) {
            if (a == NULL) return fullPath; 
            else return (a+1);
        }
        if (a == NULL) return (b+1);
        if (a > b) return (a+1); 
        else return (b+1);
    }

    /**
     * Returns parent directory as a string without terminating slash 
     */
    inline std::string parentDir(const char * fullPath, int size) {
        const char * p = fullPath + size - 1;
        for (;p > fullPath && *p != '/' && *p != '\\';--p){}
        std::string ret; ret.assign(fullPath, p - fullPath);
        return ret;
    }

    inline bool fileIsAbsPath(const char * path) {
        if (*path == '/' || *path == '\\') return true;
        if (*path == 0) return false;
        if (*(path+1) == ':') return true;
        return false;
    }

    /**
      * Could be adjusted to include ftp://, afp:// etc
      */
    inline bool fileIsNetworkPath(const char * path) {
        if (*path == '\\' && *(path+1) == '\\') return true;
        return false;
    }

    /**
     * Performs recursive copying of files from one dir to another. 
     * If the windows flag is set, all symbolic links are replaced
     * with the file pointed to unless that file is a directory. 
     * */
    _API_ bool cpR(const char * src, const char * dest, bool replaceSymLinks = false, int replace = 1, FILE * verboseOut = NULL);

    /**
      * Replace: 0 if no replace, 1 if always replace if more recent, and 2 if always replace
      */
    _API_ bool cp(const char * src, const char * dest, int replace = 1, FILE * verboseOut=NULL, bool checkDir=true);

    _API_ void dumpFile(std::ostream& os, const char * path, const char * prefix = "");
    _API_ void dumpFile(FILE * out, const char * path, const char * prefix = "");

    /**
     * Creates specified dir -- if the parent directory
     * of the target doesn't exist, will fail (to create the necessary
     * parent directories, call mkdirP) 
     * */
    inline bool mkdir(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            if (fileExists(path)) return true;
            //return CreateDirectory(path,NULL);
            return 0==_mkdir(path) && 0==_chmod(path,_S_IREAD|_S_IWRITE);
        #else 
            if (fileExists(path)) return true;
            return 0==::mkdir(path, 0755) && 0==chmod(path,00755);
        #endif
    }

    /**
      * TODO: sets mode to 755... no real control over groups/etc
      */
    inline bool fileMakeExec(const char * path) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            return (0==_chmod(path,_S_IREAD|_S_IWRITE));
        #else
            return (0==chmod(path,00755));
        #endif
    }

    _API_ bool mkdirP(const char * path);

    unsigned inline fileGetModDate(const char * file) {
        struct stat attrib; stat(file, &attrib);
        char str[9];strftime(str,9,"%Y%m%d",gmtime(&attrib.st_mtime));
        return strtoul(str,NULL,10);
    }

    /**
     * Retuns a pointer to the start of the file name
     * Does not have support for windows file paths
     * */
    inline char * fileGetName(char * path) {
        char * ret = path, *tmp = path;
        while ( (tmp = strchr(tmp, '/')) != NULL )
            (++tmp, ret = tmp);
        return ret;
    }
    inline const char * fileGetName(const char * path) {
        const char * ret = path, *tmp = path;
        while ( (tmp = strchr(tmp, '/')) != NULL )
            (++tmp, ret = tmp);
        return ret;
    }

    _API_ int listDir(const char * dir, std::vector<std::string> &files, bool fullPath = false);

    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #else 
    _API_ int listDir(const char * dir, std::vector<std::string> &files, std::vector<struct dirent>& dTypes, bool fullPath = false, bool dirsOnly = false);
    #endif

    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #else 
    /** 
     * Searches the file system for file names containing str
     * breadth-first and returns each found file one by one. 
     * Once done, returns the number of matching files. 
     * */
    int breadthFind(const char * base, const char * str, FILE *fp);
    #endif

    /**
     * Prints the last line of the file to the buffer.
     * Returns a pointer to the start of the line or NULL.
     * */
    _API_ char * lastLine(const char * path, char * buf, int bufSize);

    /**
     * Adjusts the read position to the last n lines of the file.
     * */
    _API_ void tail(int fd, int n = 10);

    /**
     * Read all of the supplied buffer from a file.
     * This does multiple reads as necessary.
     * Returns the amount read, or -1 on an error.
     * A short read is returned on an end of file.
     */
    _API_ int fullRead(int fd, char *buf, int len);
    
    /**
     * Like fullRead, but reads until the character c is 
     * reached (or end of buffer). replaces the occurrence
     * of c with '\0'. returns pointer to end if found
     * or NULL if not
     */
    _API_ char* readUntil(int fd, char *buf, int len, char c, bool replace = true);
    _API_ char* readLine(int fd, char *buf, int len, bool replace = true);

    inline int readChar(int fd = 0) {
        char buf;
        if (MR_READ(fd, &buf, 1) <= 0) return '\0';
        else return buf;
    }

    /**
     * Windows FLOCK
     * */
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        inline BOOL file_size(HANDLE h, DWORD * lower, DWORD * upper) {
                *lower = GetFileSize (h, upper);
                return 1;
            }

        /* LOCKFILE_FAIL_IMMEDIATELY is undefined on some Windows systems. */
        #ifndef LOCKFILE_FAIL_IMMEDIATELY
        # define LOCKFILE_FAIL_IMMEDIATELY 1
        #endif

        /* Acquire a lock. */
        inline BOOL do_lock(HANDLE h, int non_blocking, int exclusive) {
                BOOL res;
                DWORD size_lower, size_upper;
                OVERLAPPED ovlp;
                int flags = 0;

                /* We're going to lock the whole file, so get the file size. */
                res = file_size (h, &size_lower, &size_upper);
                if (!res)
                    return 0;

                /* Start offset is 0, and also zero the remaining members of this struct. */
                memset (&ovlp, 0, sizeof ovlp);

                if (non_blocking)
                    flags |= LOCKFILE_FAIL_IMMEDIATELY;
                if (exclusive)
                    flags |= LOCKFILE_EXCLUSIVE_LOCK;

                return LockFileEx (h, flags, 0, size_lower, size_upper, &ovlp);
            }

        /* Unlock reader or exclusive lock. */
        inline BOOL do_unlock(HANDLE h) {
                int res;
                DWORD size_lower, size_upper;

                res = file_size (h, &size_lower, &size_upper);
                if (!res)
                    return 0;

                return UnlockFile (h, 0, 0, size_lower, size_upper);
            }

        /* Now our BSD-like flock operation. */
        int flock(int fd, int operation);
    #else
        #ifndef LOCK_EX
            inline int file_lock(int fd) {
                struct flock ret;
                ret.l_type = F_WRLCK;
                ret.l_start = 0;
                ret.l_whence = SEEK_SET;
                ret.l_len = 0;
                ret.l_pid = getpid();
                if (-1 == ::fcntl(fd, F_SETLKW, &ret)) return -1;
                return 0;
            }
            inline int file_unlock(int fd) {
                struct flock ret;
                ret.l_type = F_WRLCK;
                ret.l_start = 0;
                ret.l_whence = SEEK_SET;
                ret.l_len = 0;
                ret.l_pid = getpid();
                if (-1 == ::fcntl(fd, F_UNLCK, &ret)) return -1;
                return 0;
            }
        #endif
    #endif

    class _API_ FileAlterationMonitor {
        public:
            FileAlterationMonitor(int fd, const char * path);
            FileAlterationMonitor(const char * path);

            ~FileAlterationMonitor();

        public:
            inline void setUsePoll(unsigned millis = 100) {
                this->pollMillis = millis;
                lastSize = fileSize(path.c_str());
            }

            bool waitChange();

        private:
            void init();

        private:
            int fd; bool opened_;
            std::string path;
            unsigned pollMillis;
            int lastSize;

            #if defined(__APPLE__)
               /**********************
                * Mac/BSD (kevent)
                **********************/
                struct kevent kev;
                int kq;
            #elif defined(__linux__)
               /**********************
                * Linux (inotify)
                **********************/
                int id, wd;
                static const size_t bufsz = 1024;
                char buf[bufsz];
            #endif

    };

    class _API_ TailBinary {
        public:
            TailBinary(int fd, const char * path, char * buffer, int size)
            : line(buffer), fm(fd,path), fd(fd), buffer(buffer), size(size)
             ,p(buffer), eob(buffer)
            { }

        public:
            inline void setUsePoll(unsigned millis = 100) {
                fm.setUsePoll(millis);
            }

            inline bool read(int amt, bool wait = true) {
                if (eob - p >= amt) {
                    line = p; p += amt;
                    return true;
                } else {
                    MR_LSEEK(fd, p-eob, SEEK_CUR);

                    eob = buffer; line = buffer; p = buffer;
                    do {
                        int r = mrutils::fullRead(fd, buffer, size);
                        if (r > 0) {
                            eob += r;
                            if (eob < p + amt) return false;
                            else {
                                p += amt;
                                return true;
                            }
                        }
                    } while (wait && fm.waitChange());

                    // file closed
                    return false;
                }
            }

            char * line;

        private:
            mrutils::FileAlterationMonitor fm;
            int fd;
            char * buffer; int size; char * p, * eob;
    };

    class _API_ Tail {
        public:
            Tail(int fd, const char * path, char * buffer, int size)
            : line(buffer), fm(fd,path), fd(fd), buffer(buffer), size(size)
             ,p(NULL)
            { }

        public:
            inline void setUsePoll(unsigned millis = 100) { fm.setUsePoll(millis); }

            inline bool nextLine() {
                if (fd < 0) return false;

                if (p != NULL) {
                    line = p; p = strchr(p, '\n');
                    if (p != NULL) { *p++ = '\0'; return true; }
                    else {
                        MR_LSEEK(fd, -1 * (int)strlen(line), SEEK_CUR);
                    }

                }

                line = buffer;
                do {
                    int r = mrutils::fullRead(fd, buffer, size-1);
                    if (r > 0) {
                        buffer[r] = '\0';
                        p = strchr(buffer,'\n');

                        if (p != NULL) { 
                            *p++ = '\0'; return true; 
                        } else if (r == size-1) {
                            std::cerr << "Tail: buffer too small for line." << std::endl;
                            p = NULL;
                            return true;
                        } else {
                            MR_LSEEK(fd, -1 * (int)strlen(line), SEEK_CUR);
                        }
                    }
                } while (fm.waitChange());

                // file closed
                return false;
            }

            char * line;

        private:
            mrutils::FileAlterationMonitor fm;
            int fd;
            char * buffer; int size; char * p;
    };
}

#endif
