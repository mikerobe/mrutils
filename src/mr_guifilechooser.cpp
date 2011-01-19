#include "mr_guifilechooser.h"
#ifdef _MR_CPPLIB_GUIFILECHOOSER_H_INCLUDE

#define CHOOSE_DIR_STR  "(choose dir)"

using namespace mrutils;

GuiFileChooser::GuiFileChooser(const char * startPath, const char * home, int y0, int x0, int rows, int cols, bool border) 
   : select(1,y0,x0,rows,cols,border,true)
    ,edit("Path"), frozen(false)
    ,chooseDir(false), home(home), p(path)
   {
        select.termLine.setHideCursor();
        select.termLine.setBold();
        edit.termLine.setBold();

        select.termLine.assignFunction('c',fastdelegate::MakeDelegate(this,
                &mrutils::GuiFileChooser::changePath));
        select.termLine.assignFunction('C',fastdelegate::MakeDelegate(this,
                &mrutils::GuiFileChooser::mkdir));
        select.termLine.assignFunction('U',fastdelegate::MakeDelegate(this,
                &mrutils::GuiFileChooser::updir));
        select.termLine.assignFunction('.',fastdelegate::MakeDelegate(this,
                &mrutils::GuiFileChooser::updir));


        p = mrutils::strcpy(this->path, startPath);
        if (p == this->path || *(p-1) != '/') {
            *p++ = '/'; *p = 0;
        }

        if (!this->home.empty()) {
            if (this->home[this->home.size()-1] == '/')
                this->home = this->home.substr(0,this->home.size()-1);
        }

        edit.termLine.assignFunction('\t',fastdelegate::MakeDelegate(this,
                &mrutils::GuiFileChooser::completePath));
   }

bool GuiFileChooser::completePath() {
    mrutils::TermLine &t = edit.termLine;

    std::string listThis, matchThis; 
    char * p = t.eob;
    char buffer[1024];

    char * q = mrutils::strchrRev(p-1, '/', t.line);
    if (q == NULL || q == p-1) return false; ++q;

    // replace ~ with home in list dir
    listThis.assign(t.line,q - t.line);
    mrutils::replaceCopy(buffer,listThis.c_str(),"~",home.c_str(),sizeof(buffer));
    listThis.assign(buffer);

    matchThis.assign(q,t.eob - q);

    std::vector<std::string> dirFiles;
    std::vector<struct dirent> dirFileTypes;

    mrutils::listDir(listThis.c_str(), dirFiles, dirFileTypes, false);
    bool found = false;

    for (unsigned i =0; i < dirFiles.size(); ++i) {
        if (dirFileTypes[i].d_type != DT_DIR) continue;
        if (mrutils::startsWithI(dirFiles[i].c_str(), matchThis.c_str())) {
            if (found) return true; // more than 1 match
            found = true;
            listThis += dirFiles[i];
            listThis += "/";
        }
    }

    if (!found) return true;
    t.assignText(listThis.c_str());

    return true;
}

bool GuiFileChooser::updir() {
    if (p-1 > path) {
        char * q = mrutils::strchrRev(p-2, '/', path);
        if (q != NULL) {
            p = q + 1; *p = 0;
            select.freeze();
            populateChooser();
            select.thaw();
        }
    }
    return true;
}

bool GuiFileChooser::mkdir() {
    select.freeze();

    char * dir = edit.editText("");
    edit.freeze();

    if (dir[0] == '\0') { select.thaw(); return true; }

    char * oldP = p;
    p = mrutils::strcpy(p,dir);
    if (*(p-1) != '/') { *p++ = '/'; *p = 0; }

    if (!mrutils::mkdir(path)) {
        select.termLine.setStatus("unable to create dir");
        *oldP = 0; p = oldP;
        select.thaw();
        return true;
    }

    populateChooser();
    select.thaw();

    return true;
}

bool GuiFileChooser::changePath() {
    char oldPath[1024]; 

    for (;;) {
        select.freeze();

        char * newPath = edit.editText(path);
        if (0==mrutils::stricmp(path,newPath)) {
            edit.freeze();
            select.thaw();
            return true;
        }

        if (p == path) oldPath[0] = '\0';
        else memcpy(oldPath,path,p-path+1);

        p = mrutils::replaceCopy(path,newPath,"~",home.c_str(),sizeof(path));
        edit.freeze();

        if (p == this->path || *(p-1) != '/') { *p++ = '/'; *p = 0; }

        if (!mrutils::fileExists(path) && !mrutils::mkdirP(path)) {
            select.thaw();
            select.termLine.setStatus("unable to create dir");
            continue;
        }

        populateChooser();
        select.thaw();
        return true;
    }

    // unreachable
    return true;
}

void GuiFileChooser::populateChooser() {
    select.colChooser.clear(); dirFiles.clear(); dirFileTypes.clear();
    mrutils::listDir(path, dirFiles, dirFileTypes, false, chooseDir);
    select.termLine.setStatus(path);

    if (chooseDir) {
        select.colChooser.next() << CHOOSE_DIR_STR;
    }

    for (unsigned i = 0; i < dirFiles.size(); ++i) {
        select.colChooser.next() << dirFiles[i];

        if ( dirFileTypes[i].d_type == DT_DIR ) {
            select.colChooser.setColor(i + chooseDir
                ,mrutils::gui::ATR_DIR
                ,mrutils::gui::COL_DIR
                ,0, false);
        }
    }
}

void GuiFileChooser::get() {
    if (frozen) thaw();

    mrutils::TermLine &t = select.termLine;
    mrutils::ColChooser &c = select.colChooser;

    populateChooser();

    files.clear();

    for(;;) {
        int chosen = select.get();

        if (chosen < 0) {
            if (t.line[0] == 0) {
                select.freeze();
                return;
            }
            if (t.line[0] != '>') continue;

            /*
            char * name = t.line + 3;
            switch (t.line[1]) {
                case 'e': // edit
                    break;
            }
            */
        } else {
            if (chooseDir && chosen == 0) {
                files.push_back(path);
            } else {
                if (chooseDir) --chosen;
                if (dirFileTypes[chosen].d_type == DT_DIR) {
                    p = mrutils::strcpy(p,dirFiles[chosen].c_str());
                    *p++ = '/'; *p = 0;
                    populateChooser();
                    continue;
                } else {
                    mrutils::strcpy(p, dirFiles[chosen].c_str());
                    files.push_back(path);

                    for (std::set<int>::iterator it = c.targets.begin()
                        ,itE = c.targets.end(); it != itE; ++it) {
                        if ( *it == chosen ) continue;

                        mrutils::strcpy(p, dirFiles[*it].c_str());
                        files.push_back(path);
                    }

                    *p = 0;
                }
            }

            break;
        }
    }


}

#endif
