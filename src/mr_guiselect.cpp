#include "mr_guiselect.h"
#ifdef _MR_CPPLIB_GUI_SELECT_H_INCLUDE

#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS
#undef hline

using namespace mrutils;

GuiSelect::GuiSelect(int columns, int y0, int x0, int rows, int cols, bool border, bool termOnTop) 
   : termLine(y0 + border,x0 + border,rows - 2*border,cols -2*border, termOnTop)
    ,colChooser(columns,y0+border+termOnTop,x0+border,rows-1-2*border,cols-2*border)
    ,winRows(rows > 0?MIN_(rows,LINES) : LINES + rows)
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,win((void*)newwin(winRows,winCols,y0,x0))
    ,quit_(false)
    {
        if (border) {
            box((WINDOW*)win, 0, 0);
        }

        colChooser.setShowNumbers(true);

        termLine.setPrompt("");

        termLine.assignFunction('j',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::nextRow));
        termLine.assignFunction('k',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::prevRow));

        termLine.assignFunction(' ',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::nextPage));
        termLine.assignFunction('d',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::nextPage));
        termLine.assignFunction('u',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::prevPage));

        termLine.assignFunction('n',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::nextPage));
        termLine.assignFunction('p',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::prevPage));

        termLine.assignFunction('g',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::top));
        termLine.assignFunction('G',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::bottom));

        termLine.assignFunction('h',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::prevCol));
        termLine.assignFunction('l',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::nextCol));

        termLine.assignFunction('M',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::toggleSearchTargeted));
        termLine.assignFunction('m',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::toggleTarget));

        termLine.assignSearch('#',fastdelegate::MakeDelegate(&colChooser,&mrutils::ColChooser::liveSearchFn)
            ,fastdelegate::MakeDelegate(&colChooser,&mrutils::ColChooser::liveSearchFnEnd));

        termLine.assignSearch('/',fastdelegate::MakeDelegate(&colChooser,&mrutils::ColChooser::liveSearch)
            ,fastdelegate::MakeDelegate(&colChooser,&mrutils::ColChooser::liveSearchEnd));

        termLine.assignFunction('?',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::clearSearch));
        termLine.assignFunction(',',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::clearSearch));

        termLine.assignFunction('q',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiSelect::quit));
        termLine.assignFunction(';',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiSelect::quit));
        termLine.assignFunction('o',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiSelect::open));

        for (int i = 0; i < 10; ++i) {
            termLine.assignSearch('0' + i
                    ,fastdelegate::MakeDelegate(&colChooser, &mrutils::ColChooser::lineSearch)
                    ,fastdelegate::MakeDelegate(this, &mrutils::GuiSelect::lineSearchEnd)
                    ,'#',false,true);
        }
    }

GuiSelect::~GuiSelect() {
    delwin((WINDOW*)win);
}

void GuiSelect::thaw() {
    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    touchwin((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);

    termLine.thaw();
    colChooser.thaw();
}

int GuiSelect::get(const char * passChars) {
    thaw(); quit_ = false;

    for (;;) {
        if (termLine.nextLine(passChars)) return -1;
        if (quit_) { termLine.line[0] = 0; return -1; }
        if (colChooser.activeCol < colChooser.nCols-1) {
            colChooser.nextCol();
            continue;
        }
        if (colChooser.selected[colChooser.nCols-1] < 0) continue;
        return colChooser.selected[0];
    }
}


#endif
