#include "mr_guilist.h"
#ifdef _MR_CPPLIB_GUILIST_H_INCLUDE

#include <ncurses.h>
#include <algorithm>
#undef getch
//#undef LINES
//#undef COLS

using namespace mrutils;

GuiList::GuiList(int y0, int x0, int rows, int cols) 
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : LINES + rows))
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,win((void*)newwin(winRows,winCols,y0,x0))
    ,frozen(false)
    ,mutex(mrutils::mutexCreate())
    {
        // reserve enough to (generally) avoid a realloc
        this->rows.reserve(100);
    }

GuiList::~GuiList() {
    if (!init_) return;

    mrutils::mutexDelete(mutex);

    delwin((WINDOW*)win);
}

void GuiList::reset(bool lock) {
    if (lock) mrutils::mutexAcquire(mutex);
        for (unsigned i = 0; i < rows.size(); ++i) delete rows[i];
        rows.clear();
    if (lock) mrutils::mutexRelease(mutex);

    mrutils::mutexAcquire(gui::mutex);
    wclear((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

void GuiList::thaw() {
    frozen = false;

    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

GuiList::Row& GuiList::add(GuiListEntry& entry) {
    if (!init_) {
        Row* row = new Row(this,entry,rows.size());
        mrutils::mutexAcquire(mutex);
            rows.push_back(row);
        mrutils::mutexRelease(mutex);
        return *row;
    } else {
        Row* row = new Row(this,entry);

        mrutils::mutexAcquire(mutex);
            std::vector<Row*>::iterator pos = std::upper_bound(rows.begin(), rows.end(), row, Row::compare);

            if (pos == rows.end()) {
                int r = rows.size();
                row->row = r; rows.push_back(row);
                row->isB = r == 0 || 
                    (Row::compare(rows[r-1], row) ? 
                     !rows[r-1]->isB : rows[r-1]->isB);
                row->print();
            } else {
                int r = pos - rows.begin();
                rows.resize(rows.size()+1,NULL);
                row->row = r;

                bool invert;

                if (r > 0) {
                    if (Row::compare(rows[r-1], row)) {
                        row->isB = !rows[r-1]->isB;
                        invert = true;
                    } else {
                        row->isB = rows[r-1]->isB;
                        invert = false;
                    }
                } else {
                    row->isB = true;
                    invert = true;
                }
                row->print();

                for (int i = rows.size()-2; i >= r; --i) {
                    ++rows[i]->row;
                    if (invert) rows[i]->isB = !rows[i]->isB;
                    rows[i]->print();
                    rows[i+1] = rows[i];
                }

                rows[r] = row;
            }
        mrutils::mutexRelease(mutex);

        if (!frozen) {
            mrutils::mutexAcquire(gui::mutex);
            wrefresh((WINDOW*)win);
            mrutils::mutexRelease(gui::mutex);
        }
        return *row;
    }
}

void GuiList::debug() {
    wmove((WINDOW*)win,winRows-1,0); 
    wattrset((WINDOW*)win,gui::ATR_BLANK);
    for (unsigned i = 0; i < rows.size(); ++i) {
        waddstr((WINDOW*)win, rows[i]->str);
    }
}


void GuiList::show() {
    if (init_) return;

    mrutils::mutexAcquire(mutex);

        std::sort(rows.begin(), rows.end(), Row::compare);

        if (rows.size() > 0) rows[0]->print();
        for (unsigned row = 1; row < rows.size(); ++row) {
            rows[row]->isB = 
                (Row::compare(rows[row-1], rows[row]) ? 
                 !rows[row-1]->isB : rows[row-1]->isB
                );
            rows[row]->print();
        }

    mrutils::mutexRelease(mutex);

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }

    init_ = true;
}

void mrutils::GuiList::Row::drop() {
    mrutils::mutexAcquire(list.mutex);
        if (row+1 < list.rows.size()) {
            bool invert = row == 0?
                !list.rows[row+1]->isB : 
                compare(list.rows[row-1],list.rows[row+1])
                    == (list.rows[row-1]->isB == list.rows[row+1]->isB);
            for (unsigned i = row+1; i < list.rows.size(); ++i) {
                if (invert) list.rows[i]->isB = !list.rows[i]->isB;
                --list.rows[i]->row; list.rows[i]->print();
                list.rows[i-1] = list.rows[i];
            }
        }

        WINDOW* win = (WINDOW*)list.win;

        wmove(win,list.rows.size()-1,0);
        wclrtoeol(win);
        wchgat(win, list.winCols, 0, gui::COL_BLANK, NULL);

        list.rows.resize(list.rows.size()-1);
    mrutils::mutexRelease(list.mutex);

    if (!list.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh(win);
        mrutils::mutexRelease(gui::mutex);
    }

    delete this;
}

void mrutils::GuiList::Row::update(bool lock) {
    if (lock) mrutils::mutexAcquire(list.mutex);
        if (row > 0 && entry < list.rows[row-1]->entry) {
            // move up

            std::vector<Row*>::iterator pos 
                = std::upper_bound(list.rows.begin(), list.rows.begin() + row-1, this, Row::compare);
            int newRow = pos - list.rows.begin();

            bool invert = pos == list.rows.begin() || compare(list.rows[newRow-1],this);
            isB = (invert?(*pos)->isB:!(*pos)->isB);

            for (int i = row-1; i >= newRow; --i) {
                if (invert) list.rows[i]->isB = !list.rows[i]->isB;
                ++list.rows[i]->row; list.rows[i]->print();
                list.rows[i+1] = list.rows[i];

            }

            if (row+1 < list.rows.size() && 
                compare( list.rows[row], list.rows[row+1] ) == (list.rows[row]->isB == list.rows[row+1]->isB)) {
                for (++row; row < list.rows.size(); ++row) {
                    list.rows[row]->isB = !list.rows[row]->isB;
                    list.rows[row]->print();
                }
            }

            list.rows[ row = newRow ] = this; print();

        } else if (row+1 < list.rows.size() && compare(list.rows[row+1],this)) {
            // move down

            bool invert = (row == 0?!list.rows[row+1]->isB
                           : compare(list.rows[row-1],list.rows[row+1]) ==
                             (list.rows[row-1]->isB == list.rows[row+1]->isB));

            std::vector<Row*>::iterator pos 
                = std::upper_bound(list.rows.begin() + row+2, list.rows.end(), this, Row::compare);

            for (std::vector<Row*>::iterator it = list.rows.begin()+row+1; 
                 it != pos; ++it) {
                if (invert) (*it)->isB = !(*it)->isB;
                list.rows[ --(*it)->row ] = *it; (*it)->print();
            }

            row = pos - list.rows.begin() - 1;
            list.rows[row] = this;

            if (compare(list.rows[row-1],this)) {
                isB = !list.rows[row-1]->isB;
                invert = !invert;
            } else 
                isB = list.rows[row-1]->isB;

            print();

            if (invert) {
                for (; pos != list.rows.end(); ++pos) {
                    (*pos)->isB = !(*pos)->isB;
                    (*pos)->print();
                }
            }
        } else {
            if (row > 0 && ( compare(list.rows[row-1],this) == (list.rows[row-1]->isB==isB) )) {
                isB = !isB;
            } 

            if (row+1 < list.rows.size() 
                && ( compare(this,list.rows[row+1]) == (list.rows[row+1]->isB==isB) )) {
                print();
                for (unsigned i = row+1; i < list.rows.size(); ++i) {
                    list.rows[i]->isB = !list.rows[i]->isB;
                    list.rows[i]->print();
                }
            } else print();
        }

    if (lock) mrutils::mutexRelease(list.mutex);

    if (!list.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)list.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void mrutils::GuiList::Row::print() {
    if ((int)row >= list.winRows) return;

    wmove((WINDOW*)list.win,row,0); 

    if (isB) {
        wchgat((WINDOW*)list.win, list.winCols, 0, gui::COL_B, NULL);

        switch (highlight) {
            case  0:
                wattrset((WINDOW*)list.win, gui::ATR_B);
                break;
            case  1:
                wattrset((WINDOW*)list.win, gui::ATR_HB);
                break;
            case -1:
                wattrset((WINDOW*)list.win, gui::ATR_XB);
                break;
        }
    } else {
        wchgat((WINDOW*)list.win, list.winCols, 0, gui::COL_R, NULL);

        switch (highlight) {
            case  0:
                wattrset((WINDOW*)list.win, gui::ATR_R);
                break;
            case  1:
                wattrset((WINDOW*)list.win, gui::ATR_HR);
                break;
            case -1:
                wattrset((WINDOW*)list.win, gui::ATR_XR);
                break;
        }

    }
    waddnstr((WINDOW*)list.win, str, list.winCols);
}

#endif
