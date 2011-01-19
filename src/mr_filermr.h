#ifndef _MR_CPPLIB_FILE_REMOVE_RECURSIVE_H
#define _MR_CPPLIB_FILE_REMOVE_RECURSIVE_H

#include "mr_exports.h"
#include "mr_strutils.h"
#include "mr_files.h"

namespace mrutils {

/**
 * File/directory must be local
 */
class _API_ FileRemoveRecursive {
    public:
        FileRemoveRecursive(FILE * verboseOut = NULL)
        : verboseOut(verboseOut)
        {}

    public:
        inline bool rm(const char * path) {
            if (!isDir(path)) {
                return mrutils::rm(path);
            }

            endPath = mrutils::strcpy(this->path,path);
            switch (*(endPath-1)) {
                case '/':
                case '\\':
                    break;
                default:
                    *endPath++ = '/';
                    *endPath = 0;
            }

            return run();
        }

    private:
        bool run();

    private:
        char path[1024];
        char * endPath;
        FILE * verboseOut;
};

}

#endif
