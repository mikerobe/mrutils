#include "mr_guifilesave.h"
#ifdef _MR_CPPLIB_GUIFILESAVE_H_INCLUDE
#include "mr_strutils.h"

using namespace mrutils;

GuiFileSave::GuiFileSave(const char * startPath, const char * home, int y0, int x0, int rows, int cols, bool border) 
: fc(startPath,home,y0,x0,rows,cols,border)
 ,eb("file name")
{
    fc.setChooseDir();
}

bool GuiFileSave::save(const char * defaultName, const char * startPath) {
    if (startPath != NULL) fc.setPath(startPath);

    fc.get(); fc.freeze();
    if (!fc.files.empty()) {
        const char * saveName = eb.editText(defaultName == NULL ? "" : defaultName);
        eb.freeze();
        if (saveName != NULL && saveName[0] != 0) {
            char * p = mrutils::strcpy(mrutils::strcpy(path,fc.files[0]),saveName);

            // append an extension
            if (!strchr(saveName,'.')) {
                const char * ext = defaultName;
                if (ext != NULL && NULL != (ext = mrutils::strchrRev(ext+strlen(ext),'.',ext))) {
                    mrutils::strcpy(p,ext);
                } else {
                    mrutils::ctcpy(p,".txt");
                }
            }
            return true;
        }
    }

    return false;
}

#endif
