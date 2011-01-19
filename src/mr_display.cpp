#include "mr_display.h"
#ifdef  _MR_CPPLIB_DISPLAY_H_INCLUDE

#include <iostream>
#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

mrutils::Display::Display(int y0, int x0, int rows, int cols)
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : LINES + rows))
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,win((void*)newwin(winRows,winCols,y0,x0))
    ,frozen(false), line(0)
    {
    }

mrutils::Display::~Display() {
    if (!init_) return;

    delwin((WINDOW*)win);
}

void mrutils::Display::show() {
    if (init_) return;

    update();

    init_ = true;
}

void mrutils::Display::thaw() {
    frozen = false;

    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

void mrutils::Display::clear() { 
    line = 0; 
    builder.str(""); mainText.str("");

    wclear((WINDOW*)win);

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }
}
void mrutils::Display::update() { 
    if (!builder.str().empty()) build(builder);

    if (!mainText.str().empty()) {
        wmove((WINDOW*)win,line++,0);
        build(mainText,false);
    }

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }
}

mrutils::Display& mrutils::Display::header() {
    if (!builder.str().empty()) build(builder);
    wmove((WINDOW*)win,line++,0);
    return *this;
}

void mrutils::Display::build(mrutils::stringstream& builder, bool header) {
    const char * start = builder.str().c_str();
    const char * end = start + strlen(start);
    const char * ptr;
    int chars;

    if (!header) {
        while (*start == '\n') ++start;
        // wrap text by words 
        while (end - start > winCols && line < winRows) {
            // skip 1 space
            if (*start == ' ' || *start == '\n') ++start;
            ptr = start;
            chars = winCols;

            for(ptr = strws(start);;
                ptr = strws(++ptr)) {

                if (ptr == NULL || (ptr-start)>=winCols) break;

                chars = ptr-start;

                if (*ptr == '\n') break;
            }

            waddnstr((WINDOW*)win, start, chars);
            wmove((WINDOW*)win,line++,0);
            start += chars;
        }
    }

    while (*start == '\n') ++start;
    waddnstr((WINDOW*)win, start, winCols);
    if (header) {
        mvwchgat((WINDOW*)win, line-1, 0, winCols, gui::ATR_INPUT | A_BOLD, gui::COL_INPUT, NULL);
    }

    builder.str("");
}

#endif
