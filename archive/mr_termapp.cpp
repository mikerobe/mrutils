#include "mr_termapp.h"
#ifdef _MR_CPPLIB_TERMAPP_H_INCLUDE

#include "mr_threads.h"
#include "mr_gui.h"
#include <curses.h>


#define KEY_ESC 27
#define KEY_DELETE 8

const int mrutils::Chooser::COL_BLANK    = 1;
const int mrutils::Chooser::COL_SELECTED = 2;
const int mrutils::Chooser::COL_INPUT    = 3;
const int mrutils::Chooser::COL_TARGETED = 4;

const int mrutils::Chooser::ATR_BLANK    = A_NORMAL | COLOR_PAIR(COL_BLANK   );
const int mrutils::Chooser::ATR_SELECTED = A_BLINK  | COLOR_PAIR(COL_SELECTED);
const int mrutils::Chooser::ATR_INPUT    = A_NORMAL | COLOR_PAIR(COL_INPUT   );
const int mrutils::Chooser::ATR_TARGETED = A_BLINK  | COLOR_PAIR(COL_TARGETED);

int mrutils::Chooser::mouse() {
    MEVENT mouseEvent;
    if (getmouse(&mouseEvent) == OK) {
        if (mouseEvent.y < numVisible() && mouseEvent.y >= 0) {
            if (mouseEvent.bstate & BUTTON3_CLICKED) {
                target(searchMatches[headMatches+mouseEvent.y]);
                select(searchMatches[headMatches+mouseEvent.y]);
                return -1;
            } else if (mouseEvent.bstate & BUTTON1_CLICKED) {
                highlight(searchMatches[headMatches+mouseEvent.y]);
                return selected();
            }
        }
    }
    return -1;
}

bool mrutils::Chooser::init() {
    createdWin = (initscr() != NULL);
    lines = LINES; cols = COLS;
    chooserWin = (void*)newwin(lines,cols,0,0);
    refresh();

    if (createdWin) {
        use_default_colors();
        start_color();
    }

    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS, NULL);

    init_pair(COL_BLANK   ,   -1,     -1);
    init_pair(COL_SELECTED,    0,      4);
    init_pair(COL_INPUT   ,    0,      8);
    init_pair(COL_TARGETED,    0,     11);

    return true;
}
void mrutils::Chooser::done(bool destroy) {
    attrset(ATR_BLANK);

    if (done_) return; 
    clear();
    if (!init_) return;

    if (destroy) {
        delwin((WINDOW*)chooserWin);
        if (createdWin) endwin();
        done_ = true;
    }
}

void mrutils::Chooser::clear() {
    choices.clear();

    selectNumber = 0;

    head = tail = -1;
    searchMatches.clear();
    enteringSearch = applySearch = false;
    headMatches = selMatches = 0;

    building = -1;
    choiceBuilder.str("");
}

void mrutils::Chooser::target(int hid) {
    if (targeted[hid]) {
        targeted[hid] = false;
        targets.erase(hid);
    } else {
        targeted[hid] = true;
        targets.insert(hid);
    }
}

void mrutils::Chooser::highlight(int hid, bool refreshInput, bool selectFirst) {
    int start, end, d, id; bool addMatches = false;

    if (hid > tail) {
        start = tail + 1;
        end  = choices.size();

        headMatches += tail == -1 ? 0 : MIN(lines-1,(unsigned)searchMatches.size()-headMatches);

        if (headMatches+1 >= searchMatches.size()) {
            addMatches = true;
            head = tail = -1;
        } else {
            head = searchMatches[headMatches];
            tail = searchMatches[MIN((unsigned)searchMatches.size()-1,headMatches + lines-2)];
        }
    } else {
        if (hid < head) {
            headMatches = MAX(0u, headMatches - lines+1);
            head = searchMatches[headMatches];
            tail = searchMatches[MIN((unsigned)searchMatches.size()-1,headMatches + lines-2)];
        }

        start = headMatches;
        end = searchMatches.size();
    }

    d = headMatches;
    selMatches = -1;

    wattrset((WINDOW*)chooserWin,ATR_BLANK);

    for (int i = start; i < end && d-headMatches < lines-1; ++i) {
        if (addMatches) {
            if (applySearch && !mrutils::stristr(choices[i].c_str(), userInput)) {
                continue;
            }

            if (d == headMatches) head = i; tail = i;
            searchMatches.push_back(i);
        }

        id = searchMatches[d];
        wmove((WINDOW*)chooserWin,d++ - headMatches,0); wclrtoeol((WINDOW*)chooserWin);

        if (id == hid) {
            selMatches = d-1;

            wattrset((WINDOW*)chooserWin,ATR_SELECTED);
            waddnstr((WINDOW*)chooserWin,choices[id].c_str(), -1);
            wattrset((WINDOW*)chooserWin,ATR_BLANK);

            wchgat((WINDOW*)chooserWin,-1, 0, COL_SELECTED, NULL);
        } else {
            if (targeted[id]) 
                wattrset((WINDOW*)chooserWin,ATR_TARGETED);
            waddnstr((WINDOW*)chooserWin,choices[id].c_str(), -1);
            wattrset((WINDOW*)chooserWin,ATR_BLANK);
            if (targeted[id]) 
                wchgat((WINDOW*)chooserWin,-1, 0, COL_TARGETED, NULL);
            else 
                wchgat((WINDOW*)chooserWin,-1, 0, COL_BLANK, NULL);
        }
    }

    if (selMatches == -1 && selectFirst) {
        selMatches = headMatches;
        wmove((WINDOW*)chooserWin,0,0);
        wchgat((WINDOW*)chooserWin, -1, A_BLINK, COL_SELECTED, NULL);
    }

    for (int i = d - headMatches; i < lines-1; ++i) { 
        wmove((WINDOW*)chooserWin,i,0); wclrtoeol((WINDOW*)chooserWin);
    }

    if (refreshInput) { printPrompt(name.c_str()); }
    else {
        wrefresh((WINDOW*)chooserWin);
        move(lines-1,0);
    }
}

void mrutils::Chooser::printPrompt(const char * prompt, const char * prefix) {
    int n = 0;

    wmove((WINDOW*)chooserWin,lines-1,0);
    wattrset((WINDOW*)chooserWin,ATR_INPUT);
    wclrtoeol((WINDOW*)chooserWin);

    if (prefix != NULL) {
        waddnstr((WINDOW*)chooserWin,prefix,-1);
        n += strlen(prefix);
    }

    waddnstr((WINDOW*)chooserWin,prompt,-1);
    n += strlen(prompt);

    if (applySearch && !enteringSearch && prompt[0] != '>') {
        waddnstr((WINDOW*)chooserWin," searching /",-1);
        n += 12;
        waddnstr((WINDOW*)chooserWin,userInput,-1);
        n += strlen(userInput);
        waddnstr((WINDOW*)chooserWin,"/",-1);
        ++n;
    }

    wattrset((WINDOW*)chooserWin,ATR_BLANK);
    wchgat((WINDOW*)chooserWin, -1, A_NORMAL, COL_INPUT, NULL);
    wrefresh((WINDOW*)chooserWin);

    move(lines-1,n);
}

int mrutils::Chooser::get(bool clear) {
    if (!init_ && !(init_ = init())) {
        userInput[0] = '\0';
        return -1;
    }

    if (building >= 0) build();
    if (selMatches == 0) highlight(0);

    attrset(ATR_INPUT);

    int c;
    while ((c = getch()) != 'q') {
        if (enteringSearch) {
            switch (c) {
                case '/':
                case '\n':
                    enteringSearch = false;
                    printPrompt(name.c_str());
                    break;
                case KEY_ESC:
                    enteringSearch = applySearch = false;
                    resetSearch();
                    break;
                case KEY_BACKSPACE:
                case KEY_DELETE:
                    if (inputP == userInput) {
                        enteringSearch = applySearch = false;
                        resetSearch();
                    } else {
                        *(--inputP) = '\0';
                        resetSearch();
                        printPrompt(userInput,"/");
                    }
                    break;
                default:
                    *inputP++ = c; *inputP = '\0';
                    resetSearch();
                    printPrompt(userInput,"/");
            }
        } else 
            switch(c) {
                case KEY_ESC:
                    if (applySearch) {
                        applySearch = false;
                        resetSearch();
                    }
                    break;
                case KEY_MOUSE:
                    selectNumber = 0;
                    if (choices.empty()) break;

                    if ((c = mouse()) >= 0) {
                        if (clear) done();
                        return c;
                    }
                    break;

                case '\n': 
                {
                    selectNumber = 0;
                    if (choices.empty()) break;

                    int s = selected();

                    if (clear) done();
                    return s;
                }

                case 'x':
                    target(selected());
                    break;

                case 'j':
                    selectNumber = 0;
                    highlightNext();
                    break;
                case 'k':
                    selectNumber = 0;
                    highlightPrev();
                    break;
                case '.': // repeat last command
                    if (lastCommand[0] == '\0') break;
                    strcpy(userInput, lastCommand);
                    printPrompt(userInput, "sent command ");
                    return -1;

                case '>':
                    selectNumber = 0;
                    printPrompt(">");

                    wattrset((WINDOW*)chooserWin,ATR_INPUT);
                    getnstr(userInput,sizeof(userInput));
                    mrutils::compress(userInput);
                    strcpy(lastCommand, userInput);
                    if (userInput[0] == '\0') {
                        resetPrompt();
                        break;
                    }
                    return -1;

                case 'p':
                case 'u':
                    selectNumber = 0;
                    pageUp();
                    break;

                case 'd':
                case 'n':
                case ' ':
                    selectNumber = 0;
                    pageDn();
                    break;

                case '/':
                    inputP = userInput;
                    applySearch = enteringSearch = true;
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
                    if (choices.empty()) break;

                    if (selectNumber == 0) inputP = userInput;
                    int tmp = 10 * selectNumber + (c - '0');

                    if (tmp >= choices.size() + startIndex) {
                        printPrompt(userInput);
                        break;
                    } else {
                        *inputP++ = c; *inputP = '\0';
                        selectNumber = tmp;
                        if (selectNumber >= head+startIndex && selectNumber <= tail+startIndex) 
                            highlight(selectNumber - startIndex, false, false);
                        printPrompt(userInput);
                    }
                    break;
                }

                case KEY_BACKSPACE:
                case KEY_DELETE:
                    if (choices.empty()) break;

                    if (selectNumber == 0) {
                        resetPrompt();
                        break;
                    }

                    selectNumber -= *(--inputP) - '0'; 
                    selectNumber /= 10; *inputP = '\0';

                    if (userInput[0] == '\0') {
                        resetPrompt();
                    } else {
                        if (selectNumber >= head && selectNumber <= tail) 
                            highlight(selectNumber, false, false);
                        printPrompt(userInput);
                    }
                    break;
                default:
                    resetPrompt();
            }
    }

    userInput[0] = '\0';
    done();
    return -1;
}

#endif
