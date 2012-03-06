#include "mr_guiviewer.h"
#ifdef  _MR_CPPLIB_VIEWER_H_INCLUDE

#include <iostream>
#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

mrutils::GuiViewer::GuiViewer(int columns,int listRows,int y0, int x0, int rows, int cols)
   : colChooser(columns,y0,x0,listRows,cols)
    ,display(y0+listRows,x0,rows-listRows-1,cols)
    ,termLine(y0,x0,rows,cols)
    ,quit_(false)
   {
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

        termLine.assignFunction('m',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::toggleTarget));

        termLine.assignFunction('M',fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::toggleSearchTargeted));

        /** search functions */
        //@{

        mrutils::BufferedTerm::searchFunc search;
        search.searchFn = fastdelegate::MakeDelegate(&colChooser,
                &mrutils::ColChooser::liveSearch);
        search.endFn = fastdelegate::MakeDelegate(&colChooser,
                &mrutils::ColChooser::liveSearchEnd);

        search.useFn = true;
        termLine.assignSearch('#', search);
        search.useFn = false;

        termLine.assignSearch('/', search);

        search.invert = true;
        termLine.assignSearch('?', search);
        search.invert = false;

        // by line number
        search.endChar = '#';
        search.skipChar = false;
        search.checkFns = true;
        for (int i = 0; i < 10; ++i)
        {
            search.searchFn = fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::lineSearch);
            search.endFn = fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::lineSearchEnd);
            termLine.assignSearch('0' + i, search);
        }

        //@}

        termLine.assignFunction(',',
                fastdelegate::MakeDelegate(&colChooser,
                    &mrutils::ColChooser::clearSearch));

        termLine.assignFunction('q',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiViewer::quit));
        termLine.assignFunction(';',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiViewer::quit));

        termLine.assignFunction('o',fastdelegate::MakeDelegate(this,
                    &mrutils::GuiViewer::open));
   }

int mrutils::GuiViewer::get() {
    show();

    quit_ = false;

    for (;;) {
        if (termLine.nextLine()) return -1;
        if (quit_) return -1;
        if (colChooser.activeCol == colChooser.nCols - 1
            && colChooser.selected[colChooser.activeCol] >= 0) {
            return colChooser.selected[colChooser.activeCol];
        } else colChooser.nextCol();
    }
}

#endif
