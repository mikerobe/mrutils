#include "mr_guieditbox.h"
#ifdef _MR_CPPLIB_GUIEDITBOX_H_INCLUDE

#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

#include "mr_strutils.h"
#include "mr_gui.h"
#include "mr_numerical.h"

mrutils::GuiEditBox::GuiEditBox(const char * title, int cols) 
   : init_(false)
    ,winRows((gui::init(),7))
    ,winCols(cols > 0?MIN_(cols,COLS ) : cols < 0 ? COLS  + cols : 3*COLS /4)
    ,win((void*)newwin(winRows,winCols,(LINES-winRows)/2,(COLS-winCols)/2))
    ,frozen(false)
    ,title(title)
    ,termLine( (LINES-winRows)/2 + 4, (COLS-winCols)/2 + 2, 1, winCols - 4 )
   {
       termLine.setPrompt("");
   }

mrutils::GuiEditBox::~GuiEditBox() {
    if (!init_) return;
    delwin((WINDOW*)win);
}

void mrutils::GuiEditBox::init() {
    if (init_) return;

    redraw();

    mrutils::mutexAcquire(gui::mutex);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);

    init_ = true;
}

void mrutils::GuiEditBox::redraw() {
    box((WINDOW*)win, 0, 0);
    mvwaddch((WINDOW*)win, 2, 0, ACS_LTEE); 
    mvwhline((WINDOW*)win, 2, 1, ACS_HLINE, winCols - 2); 
    mvwaddch((WINDOW*)win, 2, winCols - 1, ACS_RTEE); 

    wmove((WINDOW*)win, 1, MAX_(3,(int)(winCols - title.size())/2));
    waddnstr((WINDOW*)win, title.c_str(), winCols-2);
}

void mrutils::GuiEditBox::thaw() {
    if (!init_) init();
    frozen = false;

    redraw();

    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);

    termLine.thaw();
}

char * mrutils::GuiEditBox::editText(const char * text) {
    if (!init_) init();
    if (frozen) thaw();

    if (termLine.nextLine(text)) return termLine.line;
    else return NULL;
}

#endif
