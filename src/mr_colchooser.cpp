#include "mr_colchooser.h"
#ifdef _MR_CPPLIB_COL_CHOOSER_H_INCLUDE

#include <algorithm>
#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

using namespace mrutils;

char ColChooser::spaces[256] = "                                                                                                                                                                                                                                                               ";

ColChooser::ColChooser(int columns, int y0, int x0, int rows, int cols) 
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : LINES + rows))
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,win((void*)newwin(winRows,winCols,y0,x0))
    ,frozen(false)
    ,optionsFn(NULL), optionsData(NULL)
    ,selectionFn(NULL), selectionData(NULL)
    ,targetFn(NULL), targetData(NULL)
    ,searchFn(NULL), searchData(NULL)
    ,activeCol(-1), nCols(columns)
    ,mutex(mrutils::mutexCreate())
    ,atrSelected(gui::ATR_SELECTED)
    ,colSelected(gui::COL_SELECTED)
    ,building(false), startIndex(0)
    ,searchTargeted(false)
    {
        this->cols.resize(nCols, Column(*this));
        for (int i = 0 ; i < nCols; ++i) this->cols[i].colNumber = i;
        selected.resize(nCols, -1);
    }

ColChooser::~ColChooser() {
    if (!init_) return;

    mrutils::mutexDelete(mutex);

    delwin((WINDOW*)win);
}

void ColChooser::clear() {
    mrutils::mutexAcquire(mutex);

    for (int i = 0; i < nCols; ++i) cols[i].clear(true);
    cols[0].colWidth = 0;
    activeCol = -1;
    wclear((WINDOW*)win);
    targeted.clear(); targets.clear();
    init_ = false;
    searchTargeted = false;

    mrutils::mutexRelease(mutex);

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::redraw(int id, bool clrSpaces) {
    if (colNumber > 0 && cc.selected[colNumber-1] < 0) return;

    if (matchNum >= 0 && id == match(matchNum)) {
        printEntry(matchNum-headNum, id, clrSpaces);
    } else {
        if (cGT(id,tailId) || cLT(id,headId)) return;
        int end = std::min(cc.winRows, (int)matches.size() - startNum - headNum);

        std::vector<int>::iterator it;
        if (direction > 0) it = std::lower_bound(matches.begin() + headNum, matches.begin() + end, id);
        else it = std::lower_bound(matches.end() - end, matches.end() - headNum, id);

        if (it == matches.end() || *it != id) return;
        int matchNum = (direction<0?matches.end()-1-it:it - matches.begin());
        printEntry(matchNum-headNum, id, clrSpaces);
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::rebuild() {
    if (direction < 0) {
        headId = entries.size()-1;
        tailId = std::max(0, headId - cc.winRows + 1);
    } else {
        tailId = std::min(cc.winRows,(int)entries.size())-1;
    }

    if (colNumber == cc.nCols - 1) {
        colWidth = cc.winCols - colStart;
    } else {
        colWidth = std::min(colWidth, 
            (cc.winCols - colStart - 
            (cc.nCols - colNumber - 1)) / (cc.nCols - colNumber)
            );
    }

    if (colNumber > 0) {
        mvwvline((WINDOW*)cc.win, 0, colStart-1
            ,ACS_VLINE, cc.winRows);
    }

    for (int row = 0; row <= direction*(tailId-headId); ++row) {
        wmove((WINDOW*)cc.win,row,colStart);
        wclrtoeol((WINDOW*)cc.win);
        printEntry(row, (direction<0?entries.size()-1 - row:row));
    }

    for (int i = direction*(tailId-headId)+1; i < cc.winRows; ++i) {
        wmove((WINDOW*)cc.win,i,colStart);
        wclrtoeol((WINDOW*)cc.win);
    }
}

void ColChooser::Column::printEntry(int row, int id, bool clrSpaces) {
    if (clrSpaces) {
        if (colNumber == cc.nCols-1) {
            wmove((WINDOW*)cc.win, row,colStart);
            wclrtoeol((WINDOW*)cc.win);
        } else {
            mvwaddnstr((WINDOW*)cc.win, row, colStart, cc.spaces, colWidth);
        }
    }

    if (showNumbers) {
        snprintf(cc.numberBuf, 6, "%3d: ", id + cc.startIndex);

        mvwaddstr((WINDOW*)cc.win, row, colStart, cc.numberBuf);
        mvwaddnstr((WINDOW*)cc.win
            ,row, colStart + 5, entries[id].c_str()
            ,colWidth - 5);
    } else {
        mvwaddnstr((WINDOW*)cc.win
            ,row, colStart, entries[id].c_str()
            ,colWidth);
    }

    if (matchNum >= 0 && match(matchNum) == id)
        select(row);
    else deselect(row);
}

void ColChooser::Column::deselect(int i) {
    int matchN = (i < 0 ? matchNum : headNum + i);
    if (direction < 0) matchN = matches.size() - 1 - matchN;

    mvwchgat((WINDOW*)cc.win, (i<0?matchNum-headNum:i), colStart, colWidth
        ,formatATR[matches[matchN]], formatCOL[matches[matchN]], NULL);
}
void ColChooser::Column::select(int i) {
    mvwchgat((WINDOW*)cc.win, (i<0?matchNum-headNum:i), colStart, colWidth, cc.atrSelected, cc.colSelected, NULL);
}

void ColChooser::Column::nextRow() {
    if (entries.empty()) return;

    if (matchNum < 0) {
        if (headId < 0) {
            return;
        } else {
            matchNum = headNum;
            cc.selected[colNumber] = headId;
            select();
            cc.buildLastColumn();
            cc.selectionChanged();
        }
    } else {
        if (match(matchNum) == tailId) {
            return nextPage();
        } else {
            deselect(); ++matchNum; select();
            cc.selected[colNumber] = match(matchNum);
            cc.buildLastColumn();
            cc.selectionChanged();
        }
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::prevRow() {
    if (entries.empty()) return;

    if (matchNum < 0) {
        if (headId < 0) {
            return;
        } else {
            matchNum = headNum;
            cc.selected[colNumber] = headId;
            select();
            cc.selectionChanged();
        }
    } else {
        if (match(matchNum) == headId) {
            return prevPage(true);
        } else {
            deselect(); --matchNum; select();
            cc.selected[colNumber] = match(matchNum);
            cc.buildLastColumn();
            cc.selectionChanged();
        }
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::nextPage() {
    if (entries.empty()) return;

    if (matchNum < 0) {
        if (headId < 0) {
            return;
        } else {
            matchNum = headNum;
            cc.selected[colNumber] = headId;
            select();
            cc.selectionChanged();
        }
    } else {
        int rows = matches.size() - startNum - headNum - cc.winRows;
        if (rows < 0) return highlightTail();

        if (rows > 0) {
            rows = MIN_(rows, cc.winRows);
        } else {
            rows = addMatches(cc.winRows);
            if (rows == 0) return highlightTail();
        }

        headNum += cc.winRows;
        matchNum = headNum;
        headId = match(headNum);
        tailId = match(headNum+rows-1);
        cc.selected[colNumber] = headId;
        cc.selectionChanged();

        for (int i = 0; i < rows; ++i) { 
            wmove((WINDOW*)cc.win,i,colStart); 
            wclrtoeol((WINDOW*)cc.win);
            printEntry(i, match(headNum+i));
        }
        for (int i = rows; i < cc.winRows; ++i) {
            wmove((WINDOW*)cc.win,i,colStart);
            wclrtoeol((WINDOW*)cc.win);
        }

        cc.buildLastColumn();
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::prevPage(bool highlightTail) {
    if (entries.empty()) return;

    if (matchNum < 0) {
        if (headId < 0) {
            return;
        } else {
            matchNum = headNum;
            cc.selected[colNumber] = headId;
            select();
            cc.selectionChanged();
        }
    } else if (headNum == 0) {
        return highlightHead();
    } else {
        headNum -= cc.winRows;
        headId = match(headNum);
        tailId = match(headNum+cc.winRows-1);

        if (highlightTail) {
            matchNum = headNum + cc.winRows-1;
            cc.selected[colNumber] = tailId;
        } else {
            matchNum = headNum;
            cc.selected[colNumber] = headId;
        }

        cc.selectionChanged();

        for (int i = 0; i < cc.winRows; ++i) { 
            wmove((WINDOW*)cc.win,i,colStart);
            wclrtoeol((WINDOW*)cc.win);
            printEntry(i, match(headNum+i));
        }

        select();

        cc.buildLastColumn();
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::highlightHead() {
    if (matchNum == headNum) return;
    deselect(); matchNum = headNum; select();
    cc.selected[colNumber] = match(matchNum);
    cc.buildLastColumn();
    cc.selectionChanged();

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::highlightTail() {
    int endNum = matches.size() - 1 - startNum; 

    if (matchNum == endNum) return;
    deselect(); matchNum = endNum; select();
    cc.selected[colNumber] = match(matchNum);
    cc.buildLastColumn();
    cc.selectionChanged();

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}



int ColChooser::Column::appendEntry(const char * str) {
    entries.push_back(str);
    formatATR.resize(entries.size(), gui::ATR_BLANK);
    formatCOL.resize(entries.size(), gui::COL_BLANK);

    int id = entries.size()-1;

    if (matchesSearch(str)) {
        int rows = matches.size() - startNum - headNum - cc.winRows;

        if (matches.size() == 0) {
            matches.push_back(id);
            headId = id; tailId = id;

            if (colNumber != cc.nCols - 1) {
                colWidth = std::min((int)strlen(str), 
                    (cc.winCols - colStart - 
                    (cc.nCols - colNumber - 1)) / (cc.nCols - colNumber)
                    );

                cc.cols[colNumber+1].colStart = colStart+colWidth+1;
                cc.cols[colNumber+1].colWidth = colStart+colWidth+1;

                mvwvline((WINDOW*)cc.win, 0, colStart+colWidth
                    ,ACS_VLINE, cc.winRows);
            } else {
                colWidth = cc.winCols - colStart;
            }

            printEntry(0, match(headNum));

            if (colNumber == cc.activeCol) {
                matchNum = 0;
                cc.selected[colNumber] = 0;
                select();
                cc.buildLastColumn();
                cc.selectionChanged();
            }

            if (!cc.frozen) {
                mrutils::mutexAcquire(gui::mutex);
                wrefresh((WINDOW*)cc.win);
                mrutils::mutexRelease(gui::mutex);
            }
        } else {
            if (direction > 0) {
                if (rows < 0) {
                    matches.push_back(id);
                    tailId = id;

                    printEntry(matches.size()-headNum-1, entries.size()-1);

                    if (!cc.frozen) {
                        mrutils::mutexAcquire(gui::mutex);
                        wrefresh((WINDOW*)cc.win);
                        mrutils::mutexRelease(gui::mutex);
                    }
                } else if (rows % cc.winRows != 0) {
                    matches.push_back(id);
                }
            } else {
                matches.push_back(id); 
                if (matchNum >= 0) ++matchNum;
                int tailNum;

                if (matchNum == headNum + cc.winRows) {
                    headNum = matchNum;

                    if (rows > 0) {
                        if (rows % cc.winRows == 0) {
                            tailNum = headNum + cc.winRows-1;
                            ++startNum;
                        } else {
                            tailNum = headNum+rows;
                        }
                    } else { // rows is 0
                        tailNum = headNum + addMatches(cc.winRows-1);
                    }

                } else {

                    if (rows >= 0) {
                        tailNum = headNum + cc.winRows-1;
                        if (rows % cc.winRows == 0) ++startNum;
                    } else {
                        tailNum = headNum+cc.winRows+rows;
                    }

                }

                headId = match(headNum);
                tailId = match(tailNum);

                for (int row = 0; row <= tailNum-headNum; ++row) {
                    printEntry(row, match(headNum+row), true);
                }

                for (int row = tailNum-headNum+1; row < cc.winRows; ++row) {
                    mvwaddnstr((WINDOW*)cc.win, row, colStart, spaces, colWidth);
                    mvwchgat((WINDOW*)cc.win, row, colStart, colWidth
                        ,gui::ATR_BLANK, gui::COL_BLANK, NULL);
                }
            }

            if (!cc.frozen) {
                mrutils::mutexAcquire(gui::mutex);
                wrefresh((WINDOW*)cc.win);
                mrutils::mutexRelease(gui::mutex);
            }
        }
    }

    return id;
}

void ColChooser::Column::search(const char * str, bool fn) {
    if (str != NULL && str[0] != 0) {
        if (fn) {
            searchFnTerms.resize(searchFnTerm+1);
            searchFnTerms[searchFnTerm] = str;
        } else {
            searchTerms.resize(searchTerm+1);
            searchTerms[searchTerm] = str;
        }
    }

    if (direction > 0) {
        matches.clear();
        tailId = -1;
    } else {
        matches.resize(entries.size());
        startNum = matches.size();
        tailId = matches.size();
    }

    headId = -1; headNum = 0; matchNum = -1;
    cc.selected[colNumber] = headId;

    int rows = addMatches(cc.winRows);

    if (rows > 0) {
        matchNum = 0; 
        headId = match(0);
        tailId = match(rows-1);

        cc.selected[colNumber] = headId;

        for (int i = 0; i < rows; ++i) {
            wmove((WINDOW*)cc.win, i, colStart);
            wclrtoeol((WINDOW*)cc.win);
            printEntry(i, match(i));
        }
    }

    cc.selectionChanged();

    for (int i = rows; i < cc.winRows; ++i) {
        wmove((WINDOW*)cc.win,i,colStart);
        wclrtoeol((WINDOW*)cc.win);
    }

    if (matchNum >= 0) cc.buildLastColumn();

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::Column::bottom() {
    bool redraw = false;

    for (;;) {
        int rows = matches.size() - startNum - headNum - cc.winRows;
        if (rows < 0) break;
        if (rows > 0) {
            if (rows > cc.winRows) {
                rows -= rows % cc.winRows;
                
                redraw = true;
                headNum += rows;
                headId = match(headNum);
                tailId = match(headNum+cc.winRows-1);
                continue;
            }
        } else {
            rows = addMatches(cc.winRows);
            if (rows == 0) break;
        }

        redraw = true;
        headNum += cc.winRows;
        headId = match(headNum);
        tailId = match(headNum+rows-1);
    }

    if (redraw) {
        int rows = std::min((int)matches.size() - startNum - headNum, cc.winRows);

        for (int i = 0; i < rows; ++i) { 
            wmove((WINDOW*)cc.win,i,colStart);
            wclrtoeol((WINDOW*)cc.win);
            printEntry(i,match(headNum+i));
        }

        for (int i = rows; i < cc.winRows; ++i) {
            wmove((WINDOW*)cc.win,i,colStart);
            wclrtoeol((WINDOW*)cc.win);
        }

        matchNum = headNum + rows-1;
        cc.selected[colNumber] = match(matchNum);
        cc.selectionChanged();

        select();
        cc.buildLastColumn();

        if (!cc.frozen) {
            mrutils::mutexAcquire(gui::mutex);
            wrefresh((WINDOW*)cc.win);
            mrutils::mutexRelease(gui::mutex);
        }
    } else {
        highlightTail();
    }
}

void ColChooser::Column::highlight(int id) {
    if (entries.empty()) return;
    if (headId < 0) return;

    int end = matches.size()-startNum;
    bool redraw = false;

    if (cGT(id,tailId)) {
        do {
            int rows = matches.size() - startNum - headNum - cc.winRows;
            if (rows < 0) { end = matches.size()-startNum; break; }
            if (rows > 0) rows = MIN_(rows, cc.winRows);
            else {
                rows = addMatches(cc.winRows);
                if (rows == 0) break;
            }

            headNum += cc.winRows;
            headId = match(headNum);
            tailId = match(headNum+rows-1);
            end = headNum + rows;
            redraw = true;
        } while (cGT(id,tailId));
    } else if (cLT(id,headId)) {
        do {
            if (headNum == 0) break;
            headNum -= cc.winRows;
            headId = match(headNum);
            tailId = match(headNum+cc.winRows-1);
            end = headNum + cc.winRows;
            redraw = true;
        } while (cLT(id,headId));
    }

    if (redraw) {
        for (int i = 0; i < end - headNum; ++i) { 
            wmove((WINDOW*)cc.win, i, colStart);
            wclrtoeol((WINDOW*)cc.win);
            printEntry(i,match(headNum+i));
        }

        for (int i = end-headNum; i < cc.winRows; ++i) {
            wmove((WINDOW*)cc.win,i,colStart);
            wclrtoeol((WINDOW*)cc.win);
        }
    } else {
        deselect();
    }

    std::vector<int>::iterator it;
    if (direction > 0) it = std::lower_bound(matches.begin() + headNum, matches.begin() + end, id);
    else it = std::lower_bound(matches.end() - end, matches.end() - headNum, id);

    if (it == matches.end() || *it != id) {
        if (!redraw) {
            if (matchNum == -1) return;

            if (colNumber < cc.nCols-1) {
                for (int i = 0; i < cc.winRows; ++i) {
                    wmove((WINDOW*)cc.win,i,colStart + colWidth);
                    wclrtoeol((WINDOW*)cc.win);
                }
            }
        }
        cc.selected[colNumber] = -1;
        matchNum = -1;
        cc.selectionChanged();
    } else {
        matchNum = (direction<0?matches.end()-1-it:it - matches.begin());
        cc.selected[colNumber] = match(matchNum);
        cc.selectionChanged();

        select();
        cc.buildLastColumn();
    }

    if (!cc.frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)cc.win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::show() {
    if (init_) return;
    init_ = true;

    if (building) {
        std::string entry = builder.str(); builder.str("");
        cols[activeCol+1].addEntry(entry); building = false;
    }

    activeCol = 0;

    if (cols[0].entries.size() > 0) {
        cols[activeCol].rebuild();
        cols[activeCol].nextRow();
    } else if (nCols == 1) {
        cols[0].colWidth = winCols;
    }

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }
}

void ColChooser::prevColDirect(bool lock) {
    if (lock) mrutils::mutexAcquire(mutex);
    if (activeCol == 0) {
        if (lock) mrutils::mutexRelease(mutex);
        return;
    }

    if (activeCol+1 < nCols) {
        for (int i = 0; i < winRows; ++i) {
            wmove((WINDOW*)win,i,cols[activeCol+1].colStart-1);
            wclrtoeol((WINDOW*)win);
        }
    }

    if (cols[activeCol].matchNum >= 0) {
        cols[activeCol].deselect();
        cols[activeCol].matchNum = -1;
        selected[activeCol] = -1;
        selectionChanged();
    }

    --activeCol;

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        wrefresh((WINDOW*)win);
        mrutils::mutexRelease(gui::mutex);
    }

    if (lock) mrutils::mutexRelease(mutex);
}
void ColChooser::nextColDirect(bool lock) {
    if (lock) mrutils::mutexAcquire(mutex);
    if (activeCol != nCols-1 && selected[activeCol] >= 0) {
        ++activeCol;
        cols[activeCol].nextRow();
    }
    if (lock) mrutils::mutexRelease(mutex);
}

namespace {
    template <class T>
    inline void moveHelp(T* start, T* end) {
        T value = *start;
        if (end > start) for(;start!=end; ++start) *start = *(start+1);
        else             for(;start!=end; --start) *start = *(start-1);
        *end = value;
    }
}

void ColChooser::move(int id, int to, int column, bool lock) {
    if (column < 0 || column > nCols-1 || column > activeCol+1) return;

    /**
     * to points to the element id is now less than. 
     * if to before id, use to & swap out. otherwise,
     * use -- to & swap out
     * */
    if (to > id) --to;
    if (to == id) return;

    if (lock) mrutils::mutexAcquire(mutex);
        Column &c = cols[column];

        int dir = (to>id?1:-1); int max = MAX_(to, id), min = MIN_(to, id);

        if (!c.matchesSearch(id)) {
            std::vector<int>::iterator it = std::lower_bound(
                c.matches.begin()+c.startNum, c.matches.end(), id);
            int * start = &c.matches[it - c.matches.begin()];
            int * end = (dir>0?&c.matches[c.matches.size()]:&c.matches[c.startNum-1]);
            if (dir>0) for (; start != end && *start <= to; ++start) --*start;
            else for (--start; start != end && *start >= to; --start) ++*start;
            if (c.headId <= max && c.headId >= min) c.headId -= dir;
            if (c.tailId <= max && c.tailId >= min) c.tailId -= dir;
            if (selected[column] <= max && selected[column] >= min) selected[column] -= dir;

            moveHelp(&c.entries[id],   &c.entries[to]);
            moveHelp(&c.formatATR[id], &c.formatATR[to]);
            moveHelp(&c.formatCOL[id], &c.formatCOL[to]);
        } else {
            if (c.matches.empty()) c.addMatches(winRows);

            while ( (c.direction > 0 && c.matches.back() < id) ||
                    (c.direction < 0 && c.matches[c.startNum] > id) ) {
                // add n matches
                int tailId_ = c.tailId; c.tailId = (c.direction>0?c.matches.back():c.matches[c.startNum]);
                c.addMatches(winRows); c.tailId = tailId_;
            }

            if (dir == c.direction) {
                // guaranteed to find id in matches. 
                // now add until at least "to" is found
                int rows = 0; 
                if (c.direction > 0) {
                    for (int i = c.matches.back() + 1; i <= to; ++i) {
                        if (c.matchesSearch(i)) {
                            c.matches.push_back(i); ++rows;
                        }
                    }
                } else {
                    for (int i = c.matches[c.startNum] - 1; i >= to; --i) {
                        if (c.matchesSearch(i)) {
                            c.matches[--c.startNum] = i; ++rows;
                        }
                    }
                }

                rows = winRows - (rows % winRows);
                if (rows < winRows) {
                    // add rows more matches to finish the page
                    int tailId_ = c.tailId; c.tailId = (c.direction>0?c.matches.back():c.matches[c.startNum]);
                    c.addMatches(rows); c.tailId = tailId_;
                }
            }

            std::vector<int>::iterator it = std::lower_bound(
                c.matches.begin()+c.startNum, c.matches.end(), id);
            int * start = &c.matches[it - c.matches.begin()];
            int * end = (dir>0?&c.matches[c.matches.size()]:&c.matches[c.startNum-1]);
            if (dir>0) {
                for (++start; start != end && *start < to; ++start) *(start-1) = *start-1;
                if (start == end) --start;
            } else {
                for (; start != end && *start > to; --start) *start = *(start-1)+1;
                if (start == end) ++start;
            }
            *start = to;

            if (selected[column] == id) {
                selected[column] = to;
                c.matchNum = (c.direction>0?start - &c.matches[0]:c.matches.size()-1-(start-&c.matches[0]));
            } else if (selected[column] <= max && selected[column] >= min) {
                selected[column] -= dir;
                c.matchNum -= dir;
            }

            moveHelp(&c.entries[id],   &c.entries[to]);
            moveHelp(&c.formatATR[id], &c.formatATR[to]);
            moveHelp(&c.formatCOL[id], &c.formatCOL[to]);

            if ( (c.cLT(min,c.headId) && c.cLT(max,c.headId)) || (c.cGT(min,c.tailId) && c.cGT(max,c.tailId)) ) {
                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                c.headId = c.match(c.headNum);
                c.tailId = c.match(  tailNum);

                // no visible change unless showing numbers
                if (c.showNumbers) {
                    for (int row = 0; row <= tailNum - c.headNum; ++row) {
                        c.printEntry(row, c.match(c.headNum+row), true);
                    }
                }
            } else {
                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                if (c.matchNum != -1 && (c.matchNum < c.headNum || c.matchNum > tailNum)) {
                    if (c.matchNum < c.headNum) {
                        int i = (c.headNum - c.matchNum)/winRows;
                        c.headNum -= winRows * i;
                        if (c.headNum > c.matchNum) c.headNum -= winRows;
                    } else {
                        int i = (c.matchNum - c.headNum)/winRows;
                        c.headNum += i * winRows;
                    }

                    tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;
                }

                c.headId = c.match(c.headNum);
                c.tailId = c.match(  tailNum);
                for (int row = 0; row <= tailNum - c.headNum; ++row) 
                    c.printEntry(row, c.match(c.headNum+row), true);
                for (int row = tailNum - c.headNum+1; row < winRows; ++row) {
                    mvwaddnstr((WINDOW*)win, row, c.colStart, spaces, c.colWidth);
                    mvwchgat((WINDOW*)win, row, c.colStart, c.colWidth
                        ,gui::ATR_BLANK, gui::COL_BLANK, NULL);
                }
            }

            if (!frozen) {
                mrutils::mutexAcquire(gui::mutex);
                wrefresh((WINDOW*)win);
                mrutils::mutexRelease(gui::mutex);
            }
        }

    if (lock) mrutils::mutexRelease(mutex);
}

void ColChooser::insert(int id, const char * str, int column,bool lock) {
    if (column < 0 || column > nCols-1 || column > activeCol+1) return;

    if (lock) mrutils::mutexAcquire(mutex);

        Column &c = cols[column];

        if (id >= (int)c.entries.size()) {
            c.appendEntry(str);
        } else {
            c.entries.insert(c.entries.begin() + id, str);
            c.formatATR.insert(c.formatATR.begin() + id, gui::ATR_BLANK);
            c.formatCOL.insert(c.formatCOL.begin() + id, gui::COL_BLANK);

            if (column == nCols -1) {
                if (targeted.size() < c.entries.size()-1) targeted.resize(c.entries.size()-1,false);
                targeted.insert(targeted.begin() + id, false);
                std::set<int> targets_ = targets; targets.clear();
                for (std::set<int>::iterator it = targets_.begin(); it != targets_.end(); ++it) {
                    if (*it >= id) targets.insert(*it+1);
                    else targets.insert(*it);
                }
            }

            std::vector<int>::iterator it = std::lower_bound(
                c.matches.begin() + c.startNum, c.matches.end(), id);
            int foundNum = (c.direction<0?c.matches.end()-1-it:it - c.matches.begin());

            if (!c.matchesSearch(str)) {
                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                for (; it != c.matches.end(); ++it) {
                    ++*it;

                    if (c.showNumbers) {
                        if (foundNum >= c.headNum && foundNum <= tailNum) {
                            c.printEntry(foundNum-c.headNum, c.match(foundNum), true);
                        }
                    }

                    foundNum += c.direction;
                }

                if (c.tailId > id) ++c.tailId;
                if (c.headId > id) ++c.headId;
                if (selected[column] > id) ++selected[column];

                if (c.showNumbers && !frozen) {
                    mrutils::mutexAcquire(gui::mutex);
                    wrefresh((WINDOW*)win);
                    mrutils::mutexRelease(gui::mutex);
                }
            } else {
                if ( (c.matches.size() - c.startNum) % winRows == 0 ) {
                    // add n-1 matches to end
                    int tailId_ = c.tailId; c.tailId = (c.direction>0?c.matches.back():c.matches[c.startNum]);
                    c.addMatches(winRows-1); c.tailId = tailId_;
                }

                if (c.direction < 0) {
                    if (c.matchNum > foundNum) ++c.matchNum;
                } else {
                    if (c.matchNum >= foundNum) ++c.matchNum;
                }

                c.matches.resize(c.matches.size()+1); if (c.direction<0) ++foundNum;
                int * end = &c.matches[0] + (c.direction>0?foundNum:c.matches.size()-1-foundNum);
                int * p = &c.matches[0] + c.matches.size()-1;
                while (p > end) { *p = *(p-1) + 1; --p; }
                *p = id;

                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                if (c.matchNum > tailNum) {
                    c.headNum += winRows;
                    tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;
                }

                for (int row = (c.showNumbers?0:std::max(0,foundNum - c.headNum))
                    ;row <= tailNum - c.headNum; ++row) {
                    c.printEntry(row, c.match(c.headNum+row), true);
                }
                    
                for (int row = tailNum - c.headNum+1; row < winRows; ++row) {
                    mvwaddnstr((WINDOW*)win, row, c.colStart, spaces, c.colWidth);
                    mvwchgat((WINDOW*)win, row, c.colStart, c.colWidth
                        ,gui::ATR_BLANK, gui::COL_BLANK, NULL);
                }

                c.headId = c.match(c.headNum);
                c.tailId = c.match(tailNum);
                if (selected[column] >= id) ++selected[column];

                if (!frozen) {
                    mrutils::mutexAcquire(gui::mutex);
                    wrefresh((WINDOW*)win);
                    mrutils::mutexRelease(gui::mutex);
                }
            }
        }

    if (lock) mrutils::mutexRelease(mutex);
}

void ColChooser::remove(int id, int column, bool lock) {
    if (column < 0 || column > nCols-1 || column > activeCol+1) return;

    if (lock)mrutils::mutexAcquire(mutex);
        Column &c = cols[column];

        if (id < 0 || id >= (int)c.entries.size()) {
            if (lock) mrutils::mutexRelease(mutex);
            return;
        }

        c.entries.erase(c.entries.begin() + id);
        c.formatATR.erase(c.formatATR.begin() + id);
        c.formatCOL.erase(c.formatCOL.begin() + id);

        if (column == nCols -1) {
            targeted.erase(targeted.begin() + id);
            targets.erase(id);
        }

        std::vector<int>::iterator it = std::lower_bound(
            c.matches.begin()+c.startNum, c.matches.end(), id);

        int foundNum = (c.direction<0?c.matches.end()-1-it:it - c.matches.begin());

        if (it == c.matches.end() || *it != id) {
            int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

            for (; it != c.matches.end(); ++it) {
                --*it;

                if (c.showNumbers) {
                    if (foundNum >= c.headNum && foundNum <= tailNum) {
                        c.printEntry(foundNum-c.headNum, c.match(foundNum), true);
                    }
                }

                foundNum += c.direction;
            }

            if (c.tailId > id) --c.tailId;
            if (c.headId > id) --c.headId;
            if (selected[column] > id) --selected[column];

            if (c.showNumbers && !frozen) {
                mrutils::mutexAcquire(gui::mutex);
                wrefresh((WINDOW*)win);
                mrutils::mutexRelease(gui::mutex);
            }
        } else {
            if ( (c.matches.size() - c.startNum) % winRows == 0 ) {
                // add 1 match
                int tailId_ = c.tailId; c.tailId = (c.direction>0?c.matches.back():c.matches[c.startNum]);
                c.addMatches(1); c.tailId = tailId_;
            }

            it = (c.direction>0?c.matches.begin()+foundNum:c.matches.end()-1-foundNum);

            if (selected[column] != id) {
                if (foundNum < c.matchNum) --c.matchNum;
                for (std::vector<int>::iterator i = it; i != c.matches.end(); ++i) --*i;
                if (selected[column] > id) --selected[column];
                c.matches.erase(it);

                if (foundNum < c.headNum) {
                    c.headNum -= winRows; foundNum = c.headNum;
                }

                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                if (c.matchNum > tailNum) {
                    c.headNum += winRows;
                    tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;
                }

                for (int row = (c.showNumbers?0:foundNum - c.headNum); row <= tailNum - c.headNum; ++row) {
                    c.printEntry(row, c.match(c.headNum+row), true);
                }

                for (int row = tailNum - c.headNum+1; row < winRows; ++row) {
                    mvwaddnstr((WINDOW*)win
                            ,row, c.colStart, spaces, c.colWidth);
                    mvwchgat((WINDOW*)win, row, c.colStart, c.colWidth
                        ,gui::ATR_BLANK, gui::COL_BLANK, NULL);
                }

                if (c.matches.empty()) { 
                    c.headId = -1; c.tailId = -1; 
                } else {
                    c.headId = c.match(c.headNum);
                    c.tailId = c.match(tailNum);
                }
            } else {
                for (;activeCol > column; --activeCol) {
                    cols[activeCol].matchNum = -1;
                    selected[activeCol] = -1;
                }

                for (int i =0; i < foundNum - c.headNum; ++i) {
                    wmove((WINDOW*)win,i,c.colStart+c.colWidth);
                    wclrtoeol((WINDOW*)win);
                }

                for (int i =foundNum-c.headNum; i < winRows; ++i) {
                    wmove((WINDOW*)win,i,c.colStart);
                    wclrtoeol((WINDOW*)win);
                }

                for (std::vector<int>::iterator i = it; i != c.matches.end(); ++i) --*i;
                c.matches.erase(it);

                int tailNum = std::min((int)c.matches.size() - c.startNum, c.headNum + winRows)-1;

                if (c.headNum > tailNum) {
                    if (c.headNum == 0) {
                        // no content left
                        selected[activeCol] = -1;
                        c.matchNum = -1;
                        c.headId = -1; c.tailId = -1;
                        if (c.searchTerms.empty() && activeCol > 0) --activeCol;
                    } else {
                        bool frozen_ = frozen;
                        frozen = true;
                            c.prevPage();
                            c.deselect();
                            c.matchNum = tailNum;
                            selected[activeCol] = c.match(c.matchNum);
                            c.select();
                        frozen = frozen_;
                    }
                } else {
                    if (foundNum > tailNum) c.matchNum = foundNum - 1;
                    else c.matchNum = foundNum;

                    selected[activeCol] = c.match(c.matchNum);

                    c.printEntry(c.matchNum-c.headNum, c.match(c.matchNum));

                    for (int row = (c.showNumbers?0:c.matchNum - c.headNum+1); row <= tailNum - c.headNum; ++row) {
                        c.printEntry(row, c.match(c.headNum+row));
                    }

                    c.headId = c.match(c.headNum);
                    c.tailId = c.match(tailNum);

                    buildLastColumn();
                }

                selectionChanged();
            }

            if (!frozen) {
                mrutils::mutexAcquire(gui::mutex);
                wrefresh((WINDOW*)win);
                mrutils::mutexRelease(gui::mutex);
            }
        }

    if (lock) mrutils::mutexRelease(mutex);
}

void ColChooser::clearTargets() {
    mrutils::mutexAcquire(mutex);
        for (std::set<int>::iterator it = targets.begin(); 
             it != targets.end(); ++it) {
            targeted[*it] = false;

            cols[nCols-1].formatATR[*it] = gui::ATR_BLANK;
            cols[nCols-1].formatCOL[*it] = gui::COL_BLANK;
            cols[nCols-1].redraw(*it);
        }
        targets.clear();

    mrutils::mutexRelease(mutex);
}

void ColChooser::setTarget(int id, bool tf) {
    mrutils::mutexAcquire(mutex);

    if (id >= (int)targeted.size()) targeted.resize(id+1,false);

    if (tf != targeted[id]) { 
        if (!tf) { 
            targeted[id] = false; targets.erase(id);

            if (activeCol >= nCols-2) {
                if (id < (int)cols[nCols-1].entries.size()) {
                    cols[nCols-1].formatATR[id] = gui::ATR_BLANK;
                    cols[nCols-1].formatCOL[id] = gui::COL_BLANK;
                    cols[nCols-1].redraw(id);
                }
            }
        } else {
            targeted[id] = true;
            targets.insert(id);

            if (activeCol >= nCols-2) {
                if (id < (int)cols[nCols-1].entries.size()) {
                    cols[nCols-1].formatATR[id] = gui::ATR_TARGETED;
                    cols[nCols-1].formatCOL[id] = gui::COL_TARGETED;
                    cols[nCols-1].redraw(id);
                }
            }
        }
    }

    mrutils::mutexRelease(mutex);
}

void ColChooser::thaw() {
    show();
    frozen = false;

    mrutils::mutexAcquire(gui::mutex);
    touchwin((WINDOW*)win);
    wrefresh((WINDOW*)win);
    mrutils::mutexRelease(gui::mutex);
}

#endif
