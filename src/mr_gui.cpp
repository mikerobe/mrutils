#include "mr_gui.h"
#ifdef _MR_CPPLIB_GUI_H_INCLUDE

#include <string>
#include <cstdlib>
#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

#include "mr_termutils.h"

std::string mrutils::gui::termTitle = "xterm";
mrutils::mutex_t mrutils::gui::mutex = mrutils::mutexCreate();

//int LINES = 24;
//int COLS = 80;

namespace {
    static bool init_ = false;
    static mrutils::mutex_t guiInitMutex = mrutils::mutexCreate();
    static FILE * tty = fopen("/dev/tty","r");

    void closeFunc() { endwin(); }
}

namespace mrutils {
    namespace gui {
        int _ACS_VLINE = ACS_VLINE;
        int _ACS_HLINE = ACS_HLINE;

        const int COL_BLANK    = 1;
        const int ATR_BLANK    = A_NORMAL | COLOR_PAIR(COL_BLANK   );

        const int COL_SELECTED = 2;
        const int ATR_SELECTED = A_BOLD   | COLOR_PAIR(COL_SELECTED);

        const int COL_INPUT    = 3;
        const int ATR_INPUT    = A_NORMAL | COLOR_PAIR(COL_INPUT   );

        const int COL_TARGETED = 4;
        const int ATR_TARGETED = A_BOLD   | COLOR_PAIR(COL_TARGETED);

        const int COL_CURSOR   = 5;
        const int ATR_CURSOR   = A_NORMAL | COLOR_PAIR(COL_CURSOR  );

        const int COL_BLACK    = 6;
        const int ATR_BLACK    = A_NORMAL | COLOR_PAIR(COL_BLACK   );

        const int COL_WHITE    = 7;
        const int ATR_WHITE    = A_NORMAL | COLOR_PAIR(COL_WHITE   );

        const int COL_ERROR    = 8;
        const int ATR_ERROR    = A_BOLD   | COLOR_PAIR(COL_ERROR   );

        const int COL_NEW      = 9;
        const int ATR_NEW      = A_BOLD   | COLOR_PAIR(COL_NEW     );

        const int COL_DIR      = 16;
        const int ATR_DIR      = A_BOLD   | COLOR_PAIR(COL_DIR     );

        /**
         * these are all for bloomberg emulation
         */
        const int COL_R        = 10;
        const int COL_HR       = 11;
        const int COL_XR       = 12;
        const int COL_B        = 13;
        const int COL_HB       = 14;
        const int COL_XB       = 15;

        const int ATR_R        = A_NORMAL | COLOR_PAIR(COL_R       );
        const int ATR_HR       = A_NORMAL | COLOR_PAIR(COL_HR      );
        const int ATR_XR       = A_NORMAL | COLOR_PAIR(COL_XR      );
        const int ATR_B        = A_NORMAL | COLOR_PAIR(COL_B       );
        const int ATR_HB       = A_NORMAL | COLOR_PAIR(COL_HB      );
        const int ATR_XB       = A_NORMAL | COLOR_PAIR(COL_XB      );

        void init() {
            if (!init_) {
                mrutils::mutexAcquire(guiInitMutex);
                if (!init_) {
                    mrutils::SignalHandler::priv::destroy.func = &closeFunc;
                    mrutils::SignalHandler::setSingleCharStdin(fileno(tty));

                    // have to manually set enviro vars for ncurses
                    int slines=0, scols=0;
                    mrutils::tty_getsize(&slines,&scols);
                    putenv(const_cast<char*>((const char*)(mrutils::stringstream().clear() << "LINES=" << slines)));
                    putenv(const_cast<char*>((const char*)(mrutils::stringstream().clear() << "COLUMNS=" << scols)));

                    initscr(); 

                    use_default_colors();
                    start_color();

                    init_pair(COL_BLACK    ,    0,     15);
                    init_pair(COL_WHITE    ,   15,      0);
                    init_pair(COL_BLANK    ,   -1,     -1);
                    init_pair(COL_B        ,    3,      4);
                    init_pair(COL_HB       ,    8,      4);
                    init_pair(COL_XB       ,    0,      4);
                    init_pair(COL_R        ,    3,      1);
                    init_pair(COL_HR       ,    8,      1);
                    init_pair(COL_XR       ,    0,      1);
                    init_pair(COL_INPUT    ,    0,      7);
                    init_pair(COL_CURSOR   ,   15,      4);
                    init_pair(COL_SELECTED ,    0,     12);
                    //init_pair(COL_SELECTED ,    0,      6);
                    //init_pair(COL_TARGETED ,    0,     11); // orange
                    init_pair(COL_TARGETED ,    0,      8);
                    init_pair(COL_ERROR    ,   15,      1);
                    init_pair(COL_NEW      ,    0,      2);
                    init_pair(COL_DIR      ,    6,     -1);
                }
                init_ = true;
                mrutils::mutexRelease(guiInitMutex);
            }
        }

        void restoreTerm() { mrutils::SignalHandler::restoreTerm(); }

        void setTermTitle(const char * title) {
            mrutils::mutexAcquire(gui::mutex);
                fprintf(stdout,"\033]0;%s\007",title);
                //std::string str = "printf \\\\033]0\\;\\%s\\\\007 \""; 
                //str.append(title); str.append("\"");
                //_UNUSED const int ignore = system(str.c_str());
                termTitle = title;
            mrutils::mutexRelease(gui::mutex);
        }

        void mrefresh(void * win) {
            mrutils::mutexAcquire(gui::mutex);
            if (win == NULL) refresh();
            else wrefresh((WINDOW*)win);
            mrutils::mutexRelease(gui::mutex);
        }

        void forceClearScreen(bool cursor) {
            if (cursor) { printf("\e[?25l");   }

            /*
            mrutils::mutexAcquire(mutex);
            write(1,"\e[H\e[J",6);
            mrutils::mutexRelease(mutex);
            */

            WINDOW * win = newwin(LINES,COLS,0,0);
            mrutils::mutexAcquire(mutex);
            wclear((WINDOW*)win);
            wrefresh((WINDOW*)win);
            mrutils::mutexRelease(mutex);
            delwin((WINDOW*)win);
            setTermTitle(termTitle.c_str());
        }

        void * _newwin(int rows, int cols, int y, int x) {
            init();
            return (void*)newwin(rows,cols,y,x);
        }

        void _refresh() { refresh(); }
        void _wrefresh(void* win) { wrefresh((WINDOW*)win); }
        void _wmove(void * win, int y, int x) { wmove((WINDOW*)win,y,x); }
        void _mvwaddnstr(void * win, int y, int x, const char * str, size_t len)
        { mvwaddnstr((WINDOW*)win,y,x,str,len); }
        void _werase(void * win) { werase((WINDOW*)win); }
        void _mvwvline(void * win, int y, int x, int ch, int n)
        { mvwvline((WINDOW*)win,y,x,ch,n); }
        void _mvwhline(void * win, int y, int x, int ch, int n)
        { mvwhline((WINDOW*)win,y,x,ch,n); }
    }
}


#endif
