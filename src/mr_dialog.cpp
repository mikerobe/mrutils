#include "mr_dialog.h"
#ifdef _MR_CPPLIB_DIALOG_H_INCLUDE

#include <iostream>
#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

#include "mr_strutils.h"
#include "mr_gui.h"

mrutils::Dialog::Dialog(const char * title, int rows, int cols) 
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : rows < 0 ? LINES + rows : 3*LINES/4))
    ,winCols(cols > 0?MIN_(cols,COLS ) : cols < 0 ? COLS  + cols : 3*COLS /4)
    ,win((void*)newwin(winRows,winCols,(LINES-winRows)/2,(COLS-winCols)/2))
    ,center_(false)
    ,line(0), building(false)
   {
       next() << center() << title;
       next();
   }

void mrutils::Dialog::init() {
    if (init_) return;

    box((WINDOW*)win, 0, 0);
    mvwaddch((WINDOW*)win, 2, 0, ACS_LTEE); 
    mvwhline((WINDOW*)win, 2, 1, ACS_HLINE, winCols - 2); 
    mvwaddch((WINDOW*)win, 2, winCols - 1, ACS_RTEE); 

    mrutils::mutexAcquire(gui::mutex);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);

    init_ = true;
}

void mrutils::Dialog::hide() {
    mrutils::mutexAcquire(gui::mutex);
    werase((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

mrutils::Dialog::~Dialog() {
    if (!init_) return;

    delwin((WINDOW*)win);
}

void mrutils::Dialog::buttons(const char * name, ...) { 
    static const int pad = 4;
    static const int startx = 10;

    int start = startx;

    std::stringstream ss;

    button = 0;
    ss << "[ " << name << " ]";
    buttons_.push_back(ss.str()); ss.str("");
    buttonStarts.push_back(start);
    start += 4 + strlen(name) + pad;

    va_list list; va_start(list,name);
    for (;;) {
        const char * s = va_arg(list, const char *);
        if (s == NULL) break;
        ss << "[ " << s << " ]";
        buttons_.push_back(ss.str()); ss.str("");
        buttonStarts.push_back(start);
        start += 4 + strlen(s) + pad;
    }
    va_end(list);

    if (start > winCols + pad) {
        int subtract = std::min(startx, startx - (startx - (start - winCols - pad)) / 2);
        for (unsigned i = 0; i < buttons_.size(); ++i) {
            buttonStarts[i] -= subtract;
        }
    }
}

void mrutils::Dialog::addBuiltLine() {
    if (++line > winRows - 4) return;

    if (center_) 
        wmove((WINDOW*)win, line, std::max(3,(winCols - (int)builder.str().size())/2));
    else wmove((WINDOW*)win,line,3);

    std::string str = builder.str();

    waddnstr((WINDOW*)win, str.c_str(), winCols-2);

    builder.str("");
    center_ = false;
}

void mrutils::Dialog::highlight(int button_) {
    unsigned button = (unsigned) button_;

    for (unsigned i = 0; i < buttons_.size(); ++i) {

        wmove((WINDOW*)win, winRows-2, buttonStarts[i]);

        wattrset((WINDOW*)win
            ,i == button ? gui::ATR_SELECTED : gui::ATR_BLANK );

        waddstr((WINDOW*)win, buttons_[i].c_str() );
    }

    mrutils::mutexAcquire(gui::mutex);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

int mrutils::Dialog::get(bool obtainMutex) {
    if (buttons_.empty()) return -1;

    if (!init_) init();

    if (building) addBuiltLine(); building = false;
    highlight( 0 );

    int c; 
    while ((c = mrutils::SignalHandler::getch(obtainMutex))) {
        switch (c) {
            case 10:
            case 13:
            case ' ':
                return button;

            case 'k':
            case 'h':
                highlight( button = 
                        (button -1 + buttonStarts.size()) % buttonStarts.size());
                break;

            case 'j':
            case 'l':
            case '\t':
                highlight(button = 
                    (button +1 + buttonStarts.size()) % buttonStarts.size());
                break;

            case 27: 
                c = mrutils::SignalHandler::getch();
                if (c != '[') return -1;
                c = mrutils::SignalHandler::getch();
                switch (c) {
                    case 'A': // up
                    case 'D': // left
                        highlight( button = 
                                (button -1 + buttonStarts.size()) % buttonStarts.size());
                        break;
                    case 'B': // down
                    case 'C': // right
                        highlight(button = 
                            (button +1 + buttonStarts.size()) % buttonStarts.size());
                        break;
                    default:
                        return -1;
                }
                break;
        }
    }
    
    // never reaches here. but get a gcc warning
    return -1;
}


#endif
