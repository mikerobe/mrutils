#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUIFILESAVE_H
#endif

#ifndef _MR_CPPLIB_GUIFILESAVE_H
#define _MR_CPPLIB_GUIFILESAVE_H
#define _MR_CPPLIB_GUIFILESAVE_H_INCLUDE

#include "mr_guifilechooser.h"
#include "mr_guieditbox.h"

namespace mrutils {

class GuiFileSave {
    public:
        GuiFileSave(const char * startPath
            ,const char * home
            ,int y0 = 0, int x0 = 0, int rows = 0, int cols = 0, bool border = false);

        bool save(const char * defaultName = NULL, const char * startPath = NULL);
        
    public:
        mrutils::GuiFileChooser fc;
        mrutils::GuiEditBox eb;

        char path[1024];
};

}


#endif
