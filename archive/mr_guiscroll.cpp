#include "mr_guiscroll.h"
#ifdef _MR_CPPLIB_GUI_SCROLL_H_INCLUDE

#include <curses.h>
#undef getch
#include "mr_bufferedreader.h"
#include "mr_signal.h"
#include "mr_threads.h"
#include "mr_guifilechooser.h"

#define DRAW \
    prefresh((WINDOW*)pad,startRow,startCol,0,0,winRows-1,winCols-1)
#define SETEOF(row) \
    atEOF = true; mvwchgat((WINDOW*)pad,row,0,cols,GUI::ATR_ERROR, GUI::COL_ERROR, NULL)
#define EXPAND() \
    lines *= 2; wresize((WINDOW*)pad, lines+1, cols)
#define ADDLINE() \
    if (isStdin) ss << reader.line << '\n'; \
    if (maxWide < reader.strlen) maxWide = reader.strlen;\
    if (reader.strlen > cols) {\
        cols = winCols * CEIL(reader.strlen/(double)winCols);\
        wresize((WINDOW*)pad, lines+1, cols);\
    }\
    mvwaddstr((WINDOW*)pad, read, 0, reader.line); ++read

#define SCROLL_DOWN\
    if (!atEOF) {\
        if (startRow + winRows < read) {\
            ++startRow;\
            DRAW;\
        } else {\
            if (read >= lines-1) { EXPAND(); }\
            if (!reader.nextLine()) { SETEOF(read);\
            } else { ADDLINE(); }\
            ++startRow; DRAW;\
        }\
    } else if (startRow + winRows <= read) { \
        ++startRow;\
        DRAW;\
    }

mrutils::GuiScroll::GuiScroll(int y0, int x0, int rows, int cols, int header) 
   : createdScreen(initscr() != NULL), init_(false)
    ,winRows(rows > 0?MIN(rows,LINES) : LINES + rows)
    ,winCols(cols > 0?MIN(cols,COLS ) : COLS  + cols)
    ,frozen(false)
    ,mutex(mrutils::mutexCreate())
    ,pad(newpad(winRows,winCols))
    ,tty(fopen("/dev/tty","r"))
    ,header(header)
    {
        // hide cursor -- will emulate 
        curs_set(0);
        leaveok(stdscr,true);
        leaveok((WINDOW*)pad,true);
        idlok((WINDOW*)pad,true);
        mrutils::SignalHandler::setSingleCharStdin(fileno(tty));
    }

void mrutils::GuiScroll::close() {
    mrutils::GUI::forceClearScreen();

    if (createdScreen) {
        createdScreen = false;
        endwin();
    }
}

mrutils::GuiScroll::~GuiScroll() {
    if (!init_) return;

    mrutils::mutexDelete(mutex);

    delwin((WINDOW*)pad);
    if (createdScreen) endwin();

    fclose(tty);
}

void mrutils::GuiScroll::init() {
    init_ = true;
    mrutils::GUI::init();
    mousemask(BUTTON1_PRESSED, NULL);
    noecho(); cbreak();
}


bool mrutils::GuiScroll::readFrom(const char * path) {
    if (!reader.open(path)) return false;
    if (!init_) init();

    int read = 0; int maxWide = 0;
    int lines = winRows; int cols = winCols;

    bool atEOF = false;

    const bool isStdin = 0==strcmp(path,"-");

    mrutils::stringstream ss;

    for (int row = 0
        ;row < winRows 
        ;++row) {

        if (!reader.nextLine()) { SETEOF(row); break; } 
        if (row >= lines) { EXPAND(); }
        ADDLINE();
    }

    mrutils::GuiFileChooser fc("/Users/firmion/Downloads/"
        ,"/Users/firmion/");

    mrutils::GuiEditBox eb("file name");

    if (!frozen) {
        mrutils::mutexAcquire(GUI::mutex);

        int startRow = 0, startCol = 0;
        prefresh((WINDOW*)pad,startRow,startCol,0,0,winRows-1,winCols-1);

        using namespace mrutils::SignalHandler;

        for (int c;'q' != (c = getch());) {
            if (c == ';') break;
            switch (c) {
                case 27:
                    if (91 == getch()) { // escape sequence
                        if (77 == getch()) { // mouse
                            c = getch();
                            getch(); // x + 33
                            getch(); // y + 33
                            switch (c) {
                                case 97: // scroll down
                                    SCROLL_DOWN
                                    break;
                                case 96: // scroll up
                                    if (startRow > 0) {
                                        --startRow;
                                        DRAW;
                                    }
                                    break;
                                default:
                                    // x & y
                                    break;
                            }
                        }
                    } 
                    break;
                case 's':
                    if (isStdin) {
                        if (!atEOF) {
                            for (;;) {
                                if (read >= lines-1) { EXPAND(); }
                                if (!reader.nextLine()) { SETEOF(read); break; }
                                ADDLINE();
                            }
                        }

                        mousemask(0, NULL);
                        mrutils::mutexRelease(GUI::mutex);
                            fc.setChooseDir();
                            fc.get(); fc.freeze();
                            if (!fc.files.empty()) {
                                const char * name = eb.editText("");
                                eb.freeze();
                                if (name != NULL && name[0] != 0) {
                                    mrutils::stringstream savePath;
                                    savePath << fc.files[0] << name;
                                    int fd = MR_OPENW(savePath.str().c_str());
                                    if (fd > 0) {
                                        MR_WRITE(fd,ss.str().c_str(),ss.str().size());
                                        MR_CLOSE(fd);
                                    }
                                }
                            }
                        mrutils::mutexAcquire(GUI::mutex);
                        mousemask(BUTTON1_PRESSED, NULL);
                        DRAW;
                    } else {
                        mousemask(0, NULL);
                        mrutils::mutexRelease(GUI::mutex);
                            fc.setChooseDir();
                            fc.get(); fc.freeze();
                            if (!fc.files.empty()) {
                                const char * name = eb.editText(mrutils::fileName(path,strlen(path)));
                                eb.freeze();
                                if (name != NULL && name[0] != 0) {
                                    mrutils::stringstream savePath;
                                    savePath << fc.files[0] << "/" << name;
                                    mrutils::cp(path,savePath.str().c_str());
                                }
                            }
                        mrutils::mutexAcquire(GUI::mutex);
                        mousemask(BUTTON1_PRESSED, NULL);
                        DRAW;
                    }
                    break;

                case 'g':
                    startRow = 0; 
                    DRAW;
                    break;
                case 'G':
                    // read the full file 
                    if (!atEOF) {
                        for (;;) {
                            if (read >= lines-1) { EXPAND(); }
                            if (!reader.nextLine()) { SETEOF(read); break; }
                            ADDLINE();
                        }
                    }

                    startRow = read - winRows + 1; // to show EOF
                    DRAW;
                    break;

                case 'l':
                    if (startCol + winCols < maxWide) {
                        ++startCol; DRAW;
                    }
                    break;

                case -20:
                case ']':
                    if (maxWide > winCols) {
                        const int proposed = startCol + winCols/2;
                        const int max = maxWide - winCols;
                        if (proposed < max) {
                            if (startCol < proposed) {
                                startCol = proposed; 
                                DRAW;
                            }
                        } else {
                            if (startCol < max) {
                                startCol = max;
                                DRAW;
                            }
                        }
                    }
                    break;

                case '}':
                case 'L':
                    if (maxWide > winCols) {
                        startCol = maxWide - winCols;
                        DRAW;
                    }
                    break;

                case 'H':
                case '{':
                    if (startCol > 0) {
                        startCol = 0; DRAW;
                    }
                    break;
                case 'h':
                    if (startCol > 0) {
                        --startCol; DRAW;
                    }
                    break;

                case -24:
                case '[':
                    if (startCol > 0) {
                        startCol -= winCols/2;
                        if (startCol < 0) startCol = 0;
                        DRAW;
                    }
                    break;

                case 10:
                case 13:
                case 'j':
                    SCROLL_DOWN
                    break;
                case 'n':
                case ' ':
                case 'd':
                    if (!atEOF) {
                        if (startRow + 2*winRows > read) {
                            for (int r = startRow + 2*winRows - read; r > 0; --r) {
                                if (read >= lines-1) { EXPAND(); }
                                if (!reader.nextLine()) { SETEOF(read); break;
                                } else { ADDLINE(); }
                            }
                        }
                        startRow += winRows;
                        DRAW;
                    } else if (startRow + winRows <= read) { // show EOF line
                        startRow += winRows;
                        DRAW;
                    }
                    break;

                case 'k':
                    if (startRow > 0) {
                        --startRow;
                        DRAW;
                    }
                    break;
                case 'p':
                case 'u':
                    if (startRow > 0) {
                        startRow -= winRows;
                        if (startRow < 0) startRow = 0;
                        DRAW;
                    }
                    break;
            }
        }

        mrutils::mutexRelease(GUI::mutex);
    }

    return true;
}


#endif
