#include "mr_colchooser.h"
#ifdef _MR_CPPLIB_COL_CHOOSER_H_INCLUDE

#include <iostream>
#include <curses.h>
#undef getch

#include "mr_threads.h" // for sleep
#include "mr_strutils.h"
#include "fastdelegate.h"

#define KEY_ESC 27
#define KEY_DELETE 8

mrutils::ColChooser::ColChooser(int columns, mrutils::ColChooser::optionsFunc optionsFn, void* optionData, int startIndex, int lines) 
    : createdWin(false), enableMouse(true)
     ,init_(false), done_(false), building(-1), startIndex(startIndex)
     ,active(-1), colStart(0), enteringSearch(false), enteringText(false)
     ,name(""), optionsFn(optionsFn), optionData(optionData)
     ,selectNumber(0), hideNumbers(false)
     ,acceptReturn(true), visible(true)
     ,changeFn(NULL),changeData(NULL)
     ,openFn(NULL),openData(NULL)
     ,onTargetFn(NULL)
     {
         updateMutex = mrutils::mutexCreate();

         createdWin = (initscr() != NULL);
         this->maxLines = LINES;
         this->lines = (lines==0?LINES:MIN(LINES,lines)); cols = COLS;
         chooserWin = (void*)newwin(LINES,cols,0,0);
         data.resize(columns, Column(*this));
         for (int i = 0 ; i < columns; ++i) data[i].colNumber = i;
         depth.resize(columns, -1);

         inputP = data[0].search;
         lastCommand[0] = '\0';
}


void mrutils::ColChooser::redraw() {
    werase((WINDOW*)chooserWin);
    wrefresh((WINDOW*)chooserWin);
    refresh();

    colStart = 0;
    for (int i = 0; i < active; ++i) {
        data[i].highlight(depth[i],true,false,false);
        colStart += data[i].colWidth + 1;
    }
    if (active >= 0) data[active].highlight(depth[active]);
}


void mrutils::ColChooser::clear() {
    building = -1; 
    choiceBuilder.str("");

    colStart = 0;

    targeted.clear();
    targets.clear();

    for (int i = 0; i < data.size(); ++i) {
        data[i].clear();
    }

    active = -1;
    selectNumber = 0;
}

int mrutils::ColChooser::get() {
    if (!init_ && !(init_ = init())) {
        return -1;
    }

    if (building >= 0) build();
    if (active == -1) {
        active = 0;
        if (!data[0].choices.empty())
            data[0].highlight(0);
    }

    attrset(ATR_INPUT);

    int c;
    for (;;) {
        c = getch();

        if (enteringSearch) {
            switch (c) {
                case '/':
                case '\n':
                    enteringSearch = false;
                    printPrompt(name.c_str());
                    break;
                case '?':
                    enteringSearch = false;
                    data[active].applySearch = false;
                    data[active].resetSearch();
                    data[active].highlight( 0 );
                    break;
                case KEY_ESC:
                    enteringSearch = data[active].applySearch = false;
                    data[active].resetSearch();
                    data[active].highlight(0);
                    break;
                case KEY_BACKSPACE:
                case KEY_DELETE:
                    if (inputP == data[active].search) {
                        enteringSearch = data[active].applySearch = false;
                        data[active].resetSearch();
                        data[active].highlight(0);
                    } else {
                        *(--inputP) = '\0';
                        data[active].resetSearch();
                        data[active].highlight(0);
                        printPrompt(data[active].search,"/");
                    }
                    break;
                default:
                    *inputP++ = c; *inputP = '\0';
                    data[active].resetSearch();
                    data[active].highlight(0);
                    printPrompt(data[active].search,"/");
            }
        } else {
            if (c == 'q' || c == ';') break;
            switch(c) {
                case KEY_ESC:
                case '?':
                    if (data[active].applySearch) {
                        data[active].applySearch = false;
                        data[active].resetSearch();
                        data[active].highlight( 0 );
                    }
                    break;

                case 'o':
                    if (active == data.size()-1 && openFn != NULL) {
                        (openFn)(*this,openData);
                        redraw();
                        break;
                    } 
                case '\n': 
                {
                    if (!acceptReturn) break;

                    selectNumber = 0;
                    if (active == data.size()-1) {
                        return depth[active];
                    } else {
                        nextColumn();
                    }
                    break;
                }
                case KEY_MOUSE:
                    selectNumber = 0;

                    if ((c = mouse()) >= 0) {
                        if (!acceptReturn) break;
                        return c;
                    }
                    break;


                case 'L':
                    redraw();
                    break;

                case 'l':
                    selectNumber = 0;
                    nextColumn();
                    break;
                case 'h':
                    selectNumber = 0;
                    prevColumn();
                    break;
                case 'j':
                    selectNumber = 0;
                    data[active].highlightNext();
                    break;
                case 'k':
                    selectNumber = 0;
                    data[active].highlightPrev();
                    break;
                case '.': // repeat last command
                    if (lastCommand[0] == '\0') break;
                    strcpy(data[active].search, lastCommand);
                    printPrompt(data[active].search, "sent command ");
                    return -1;

                case 'x':
                case 'm':
                    if (active == data.size()-1) {
                        toggleTargetInternal(depth[active]);
                    }
                    break;

                case '>':
                    selectNumber = 0;
                    printPrompt(">");
                    enteringText = true;

                    wattrset((WINDOW*)chooserWin,ATR_INPUT);
                    getnstr(data[active].search,sizeof(data[active].search));
                    mrutils::compress(data[active].search);
                    strcpy(lastCommand, data[active].search);
                    if (data[active].search[0] == '\0') {
                        resetPrompt();
                        enteringText = false;
                        break;
                    }
                    enteringText = false;
                    return -1;

                case 'p':
                case 'u':
                    selectNumber = 0;
                    data[active].pageUp();
                    break;

                case 'd':
                case 'n':
                case ' ':
                    selectNumber = 0;
                    data[active].pageDn();
                    break;

                case '/':
                    inputP = data[active].search;
                    data[active].applySearch = true;
                    enteringSearch = true;
                    printPrompt("search for...");
                    break;

                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '0': 
                {
                    if (data[active].choices.empty()) break;

                    if (selectNumber == 0) inputP = data[active].search;
                    int tmp = 10 * selectNumber + (c - '0');

                    if (tmp >= data[active].choices.size() + startIndex) {
                        printPrompt(data[active].search);
                        break;
                    } else {
                        *inputP++ = c; *inputP = '\0';
                        selectNumber = tmp;
                        if (selectNumber >= data[active].head+startIndex 
                            && selectNumber <= data[active].tail+startIndex) 
                            data[active].highlight(selectNumber - startIndex, false, false);
                        printPrompt(data[active].search);
                    }
                    break;
                }

                case KEY_BACKSPACE:
                case KEY_DELETE:
                    if (data[active].choices.empty()) break;

                    if (selectNumber == 0) {
                        resetPrompt();
                        break;
                    }

                    selectNumber -= *(--inputP) - '0'; 
                    selectNumber /= 10; *inputP = '\0';

                    if (data[active].search[0] == '\0') {
                        resetPrompt();
                    } else {
                        if (selectNumber >= data[active].head 
                            && selectNumber <= data[active].tail) 
                            data[active].highlight(selectNumber, false, false);
                        printPrompt(data[active].search);
                    }
                    break;
                default:
                    resetPrompt();
            }
        }
    }

    data[active].search[0] = '\0';
    done();
    return -1;

}

bool mrutils::ColChooser::init() {
    mrutils::mutexAcquire(updateMutex);
    refresh();

    if (createdWin) {
        use_default_colors();
        start_color();
    }

    keypad(stdscr, TRUE);
    if (enableMouse) mousemask(ALL_MOUSE_EVENTS, NULL);

    init_pair(COL_BLANK   ,   -1,     -1);
    init_pair(COL_SELECTED,    0,     12);
    init_pair(COL_INPUT   ,    0,      8);
    init_pair(COL_TARGETED,    0,     11);

    if (data.size() > 1) {
        int columns = data.size();
        int width = (cols - data[0].colWidth) / (columns - 1);
        data[columns-1].colWidth = cols - data[0].colWidth;
        for (int i = 1; i < columns-1; ++i) {
            data[i].colWidth = width;
            data[columns-1].colWidth -= width;
            mvwvline((WINDOW*)chooserWin, 0, 
                data[i-1].colWidth, ACS_VLINE, lines-1);
        }

        mvwvline((WINDOW*)chooserWin, 0, 
            cols - data[columns-1].colWidth, ACS_VLINE, lines-1);
    }

    wmove((WINDOW*)chooserWin,0,0);

    mrutils::mutexRelease(updateMutex);
    return true;

}

void mrutils::ColChooser::done(bool destroy) {
    attrset(ATR_BLANK);

    if (done_) return; 
    //clear();
    if (!init_) return;

    if (destroy) {
        delwin((WINDOW*)chooserWin);
        if (createdWin) endwin();
        done_ = true;
    }
}

void mrutils::ColChooser::Column::highlight(int hid, bool refreshInput,
    bool selectFirst, bool refreshWindow) {

    mrutils::mutexAcquire(cc.updateMutex);

    int start, end, d, id; bool addMatches = false;

    if (hid > tail) {
        start = tail + 1;
        end  = choices.size();

        headMatches += tail == -1 ? 0 : MIN(cc.lines-1,(unsigned)searchMatches.size()-headMatches);

        if (headMatches+1 >= searchMatches.size()) {
            addMatches = true;
            head = tail = -1;
        } else {
            head = searchMatches[headMatches];
            tail = searchMatches[MIN((unsigned)searchMatches.size()-1,headMatches + cc.lines-2)];
        }
    } else {
        if (hid < head) {
            headMatches = MAX(0u, headMatches - cc.lines+1);
            head = searchMatches[headMatches];
            tail = searchMatches[MIN((unsigned)searchMatches.size()-1,headMatches + cc.lines-2)];
        }

        start = headMatches;
        end = searchMatches.size();
    }

    d = headMatches;
    selMatches = -1;

    wattrset((WINDOW*)chooserWin,ATR_BLANK);

    for (int i = start; i < end && d-headMatches < cc.lines-1; ++i) {
        if (addMatches) {
            if (applySearch && !mrutils::stristr(choices[i].c_str(), search)) {
                continue;
            }

            if (d == headMatches) head = i; tail = i;
            searchMatches.push_back(i);
        }

        id = searchMatches[d];
        wmove((WINDOW*)chooserWin,d++ - headMatches,cc.colStart); wclrtoeol((WINDOW*)chooserWin);

        if (id == hid) {
            selMatches = d-1;

            wattrset((WINDOW*)chooserWin,ATR_SELECTED);
            waddnstr((WINDOW*)chooserWin,choices[id].c_str(), colWidth);
            wattrset((WINDOW*)chooserWin,ATR_BLANK);

            int left = colWidth - choices[id].size();
            if (left > 0)
                wchgat((WINDOW*)chooserWin,left, 0, COL_SELECTED, NULL);
        } else {
            bool targeted = colNumber+1 == cc.data.size() && cc.targeted[id];
            if (targeted)
                wattrset((WINDOW*)chooserWin,ATR_TARGETED);
            waddnstr((WINDOW*)chooserWin,choices[id].c_str(), colWidth);
            wattrset((WINDOW*)chooserWin,ATR_BLANK);

            int left = colWidth - choices[id].size();
            if (left > 0) {
                if (targeted)
                    wchgat((WINDOW*)chooserWin,left, 0, COL_TARGETED, NULL);
                else 
                    wchgat((WINDOW*)chooserWin,left, 0, COL_BLANK, NULL);
            }
        }
    }

    if (selMatches == -1 && selectFirst) {
        selMatches = headMatches;
        wmove((WINDOW*)chooserWin,0,cc.colStart);
        wchgat((WINDOW*)chooserWin, colWidth, A_BLINK, COL_SELECTED, NULL);
    }

    for (int i = d - headMatches; i < cc.lines-1; ++i) { 
        wmove((WINDOW*)chooserWin,i,cc.colStart); wclrtoeol((WINDOW*)chooserWin);
    }

    // set the depth selection
    if ( selMatches < 0 ||
         selMatches >= searchMatches.size() )
        cc.depth[colNumber] = -1;
    else 
        cc.depth[colNumber] = searchMatches[selMatches] + cc.startIndex;

    // build next column
    if (cc.data.size() > colNumber + 1 
        && hid >= 0 && cc.depth[colNumber] >= 0) {


        mvwvline((WINDOW*)chooserWin, 0, 
                cc.colStart + colWidth, ACS_VLINE, cc.lines-1);

        std::vector<std::string>& choices = cc.data[colNumber+1].choices;

        if (refreshWindow) {
            cc.targeted.clear();
            cc.targets.clear();

            choices.clear();
            (cc.optionsFn)(cc, cc.optionData);
            if (cc.building >= 0) cc.build();
        }

        int colWidth = cc.data[colNumber+1].colWidth;
        int rows = MIN((unsigned)choices.size(), cc.lines-1);

        for (int i = 0; i < rows; ++i) {
            bool targeted = colNumber+2 == cc.data.size() && cc.targeted[i];
            if (targeted)
                wattrset((WINDOW*)chooserWin,ATR_TARGETED);
            wmove((WINDOW*)chooserWin,i,cc.colStart+this->colWidth+1); 
            waddnstr((WINDOW*)chooserWin,choices[i].c_str(), colWidth);
            if (targeted) {
                wchgat((WINDOW*)chooserWin,-1, 0, COL_TARGETED, NULL);
                wattrset((WINDOW*)chooserWin,ATR_BLANK);
            }
        }
    }

    if (refreshInput) { cc.printPrompt(cc.name.c_str()); }
    else {
        if (refreshWindow) wrefresh((WINDOW*)chooserWin);
        move(cc.maxLines-1,0);
    }

    if (colNumber == cc.data.size()-1 && cc.changeFn != NULL) {
        (cc.changeFn)(cc, cc.changeData);
    }

    mrutils::mutexRelease(cc.updateMutex);
}

void mrutils::ColChooser::printPrompt(const char * prompt, const char * prefix) {
    int n = 0;

    wmove((WINDOW*)chooserWin,maxLines-1,0);
    wattrset((WINDOW*)chooserWin,ATR_INPUT);
    wclrtoeol((WINDOW*)chooserWin);

    if (prefix != NULL) {
        waddnstr((WINDOW*)chooserWin,prefix,-1);
        n += strlen(prefix);
    }

    waddnstr((WINDOW*)chooserWin,prompt,-1);
    n += strlen(prompt);

    if (data[active].applySearch && !enteringSearch && prompt[0] != '>') {
        waddnstr((WINDOW*)chooserWin," searching /",-1);
        n += 12;
        waddnstr((WINDOW*)chooserWin,data[active].search,-1);
        n += strlen(data[active].search);
        waddnstr((WINDOW*)chooserWin,"/",-1);
        ++n;
    }

    wattrset((WINDOW*)chooserWin,ATR_BLANK);
    wchgat((WINDOW*)chooserWin, -1, A_NORMAL, COL_INPUT, NULL);
    wrefresh((WINDOW*)chooserWin);

    move(maxLines-1,n);
}

void mrutils::ColChooser::nextColumn(bool highlight) {
    if (active == data.size()-1) return;
    if (data[active+1].choices.empty()) return;
    colStart += data[active].colWidth + 1;
    if (highlight) data[++active].highlight(0);
}
void mrutils::ColChooser::prevColumn(int colsBack) {
    if (active == 0) return;
    if (colsBack > active) colsBack = active;

    data[active].search[0] = '\0';
    data[active].applySearch = false;

    for (int i = 0; i < colsBack; ++i) {
        colStart -= data[active-1-i].colWidth + 1;
        data[active-i].resetSearch();
    }

    active -= colsBack;
    data[active].highlight(depth[active]);
}

void mrutils::ColChooser::clearTargets() {
    targets.clear();
    for (int i = 0; i < targeted.size(); ++i) {
        targeted[i] = false;
    }
}

void mrutils::ColChooser::setTarget(int hid, bool tf) {
    if (hid+1 > targeted.size()) 
        targeted.resize(hid+1,false);

    if (tf) {
        targeted[hid] = true;
        targets.insert(hid);
    } else {
        targeted[hid] = false;
        targets.erase(hid);
    }
}
void mrutils::ColChooser::toggleTarget(int hid) {
    if (hid+1 > targeted.size()) 
        targeted.resize(hid+1,false);

    if (targeted[hid]) {
        targeted[hid] = false;
        targets.erase(hid);
    } else {
        targeted[hid] = true;
        targets.insert(hid);
    }
}

int mrutils::ColChooser::mouse() {
    MEVENT mouseEvent;
    if (getmouse(&mouseEvent) == OK) {
        int col = 0; int x = 0;
        for (int i = 0; i < data.size(); ++i, ++col) {
            if (i > active+1) return -1;
            x += data[i].colWidth;
            if (mouseEvent.x < x) break;
        }

        if (col > active) nextColumn();
        else if (col < active) prevColumn(active - col);

        return data[col].mouse((void*)&mouseEvent);
    }
    return -1;
}

int mrutils::ColChooser::Column::mouse(void* mouseEvent_) {
    MEVENT &mouseEvent = *((MEVENT*)mouseEvent_);

    if (mouseEvent.y < numVisible() && mouseEvent.y >= 0) {
        if (mouseEvent.bstate & BUTTON3_CLICKED
                && cc.active == cc.data.size()-1) {
            cc.toggleTargetInternal(searchMatches[headMatches+mouseEvent.y]);
            highlight(searchMatches[headMatches+mouseEvent.y]);
            return -1;
        } else if (mouseEvent.bstate & BUTTON1_CLICKED) {
            highlight(searchMatches[headMatches+mouseEvent.y]);
            if (cc.active == cc.data.size()-1) return cc.selected();
            else return -1;
        }
    }

    return -1;
}

#endif
