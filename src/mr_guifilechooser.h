#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define _MR_CPPLIB_GUIFILECHOOSER_H
#endif

#ifndef _MR_CPPLIB_GUIFILECHOOSER_H
#define _MR_CPPLIB_GUIFILECHOOSER_H
#define _MR_CPPLIB_GUIFILECHOOSER_H_INCLUDE

#include "mr_files.h"
#include "mr_guiselect.h"
#include "mr_guieditbox.h"

namespace mrutils {

class GuiFileChooser {
    public:
        GuiFileChooser(const char * startPath
            ,const char * home
            ,int y0 = 0, int x0 = 0, int rows = 0, int cols = 0, bool border = false);
        
    public:
        void setChooseDir(bool tf = true)
        { chooseDir = tf; }

    public:
       /***************
        * call functions
        ***************/

        bool mkdir();
        bool updir();
        bool changePath();
        bool completePath();

        inline void freeze() {
            select.freeze();
            frozen = true;
        }

        inline void thaw() {
            select.thaw();
            frozen = false;
        }

        inline void setPath(const char * path) { strcpy(this->path,path); }

    public:
        void get();
        std::vector<std::string> files;

    private:
        void populateChooser();

    private:
        public: mrutils::GuiSelect select; private:
        mrutils::GuiEditBox edit;
        bool frozen; bool chooseDir;
        std::string home;
        char path[1024];
        char * p;

        std::vector<std::string> dirFiles;
        std::vector<struct dirent> dirFileTypes;

};

}

#endif
