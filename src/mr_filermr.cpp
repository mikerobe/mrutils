#include "mr_filermr.h"

bool mrutils::FileRemoveRecursive::run() {
    static const int MAX_PATHLEN = 256;

    DIR *dp; struct dirent * dirp = NULL;
    char dirData[offsetof(struct dirent, d_name) + MAX_PATHLEN];
    if((dp = opendir(path)) == NULL) return false;

    for (char * p = endPath;0 == readdir_r(dp,(struct dirent*)&dirData,&dirp) && dirp;) {
        switch(dirp->d_name[0]) {
            case 0:
                continue;
            case '.':
                switch(dirp->d_name[1]) {
                    case 0:
                    case '.':
                        continue;
                }
        }

        endPath = mrutils::strcpy(p,dirp->d_name);

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        if (mrutils::isDir(path)) {
        #else
        if (dirp->d_type == DT_DIR) {
        #endif
            *endPath++ = '/'; *endPath = 0;
            if (!run()) return false;
        } else {
            if (!mrutils::rm(path)) {
                if (verboseOut) {
                    fprintf(verboseOut,"rm %s: %s\n",path,strerror(errno));
                    fflush(verboseOut);
                } 
                return false;
            } else if (verboseOut) {
                fprintf(verboseOut,"rm %s\n",path);
                fflush(verboseOut);
            }
        }

        endPath = p;
    }

    closedir(dp);

    // now remove the dir
    *endPath = 0;
    if (verboseOut) {
        if (mrutils::rmdir(path)) {
            fprintf(verboseOut,"rmdir %s\n",path);
            fflush(verboseOut);
            return true;
        } else { 
            fprintf(verboseOut,"rm %s: %s\n",path,strerror(errno));
            fflush(verboseOut);
            return false;
        }
    } else return mrutils::rmdir(path);
}
