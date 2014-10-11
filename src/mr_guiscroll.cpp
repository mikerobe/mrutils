#include "mr_guiscroll.h"
#ifdef _MR_CPPLIB_GUI_SCROLL_H_INCLUDE

#include <ncurses.h>
#undef getch
//#undef LINES
//#undef COLS

#include "mr_bufferedreader.h"
#include "mr_signal.h"
#include "mr_threads.h"
#include "mr_guifilesave.h"

#define MAXROWS 10000

#define DRAW \
    prefresh((WINDOW*)pad,startRow,startCol,header,0,winRows-1,winCols-1)
#define REDRAW \
    if (header > 0) {\
        prefresh((WINDOW*)pad,0,startCol,0,0,header,winCols-1);\
    } DRAW
#define SETEOF(row) \
    atEOF = true; mvwchgat((WINDOW*)pad,row,0,cols,gui::ATR_ERROR, gui::COL_ERROR, NULL)
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
        if (startRow + rows < read) {\
            ++startRow;\
            DRAW;\
        } else {\
            if (read >= lines-1) { EXPAND(); }\
            if (read > MAXROWS || !reader.nextLine()) { SETEOF(read);\
            } else { ADDLINE(); }\
            ++startRow; DRAW;\
        }\
    } else if (startRow + rows <= read) { \
        ++startRow;\
        DRAW;\
    }

mrutils::GuiScroll::GuiScroll(_UNUSED int y0, _UNUSED int x0, int rows, int cols, int header) 
   : init_(false)
    ,winRows((gui::init(),rows > 0?MIN_(rows,LINES) : LINES + rows))
    ,winCols(cols > 0?MIN_(cols,COLS ) : COLS  + cols)
    ,frozen(false)
    ,mutex(mrutils::mutexCreate())
    ,pad(newpad(winRows,winCols))
    ,header(header)
    {
        // hide cursor -- will emulate 
        curs_set(0);
        leaveok(stdscr,true);
        leaveok((WINDOW*)pad,true);
        idlok((WINDOW*)pad,true);
    }

void mrutils::GuiScroll::close() {
    mousemask(0, NULL);
    //mrutils::gui::forceClearScreen();
}

mrutils::GuiScroll::~GuiScroll() {
    if (!init_) return;

    mrutils::mutexDelete(mutex);

    delwin((WINDOW*)pad);
}

void mrutils::GuiScroll::init() {
    init_ = true;
    //noecho(); cbreak();
}


bool mrutils::GuiScroll::readFrom(const char * path, int startLine, const char * const saveDir, const char * const homeDir) {
    if (!reader.open(path)) return false;
    if (!init_) init();
    wclear((WINDOW*)pad);

    int read = 0; int maxWide = 0;
    int lines = winRows; int cols = winCols;
    const int rows = winRows - header;

    bool atEOF = false;

    const bool isStdin = 0==strcmp(path,"-");
    const int fastMult = 3;

    mrutils::stringstream ss;

    // read the header
    while (read < header) {
        if (!reader.nextLine()) { SETEOF(read); break; } 
        if (read >= lines) { EXPAND(); }
        ADDLINE();
    }

    // now skip lines to the startline
    for (int i = 0; i < startLine - header; ++i) {
        if (!reader.nextLine()) { SETEOF(read); break; } 
    }

    // now read the rest of the window, starting at offset
    while (read < winRows) {
        if (!reader.nextLine()) { SETEOF(read); break; } 
        if (read >= lines) { EXPAND(); }
        ADDLINE();

        // refresh after each line
        prefresh((WINDOW*)pad,0,0,0,0,winRows-1,winCols-1);
    }

    mrutils::GuiFileSave fs(saveDir, homeDir);

    int drag = 0;
    int dragStart = 0;

    if (!frozen) {
        mrutils::mutexAcquire(gui::mutex);
        mousemask(BUTTON1_PRESSED, NULL);

        // draw the initial window
        prefresh((WINDOW*)pad,0,0,0,0,winRows-1,winCols-1);

        int startRow = header, startCol = 0;

        namespace sh = mrutils::SignalHandler;

        for (int c;'q' != (c = sh::getch());) {
            if (c == ';') break;
            switch (c) {
                case 27:
                    if (91 == sh::getch()) { // escape sequence
                        if (77 == sh::getch()) { // mouse
                            c = sh::getch();
							sh::getch(); // x + 33
                            const int y = sh::getch() - 33; // y + 33
                            switch (c) {
                                case 34: // secondary down
                                    mvwchgat((WINDOW*)pad,startRow-header + y,0,cols,0,0, NULL);
                                    drag = 2; dragStart = startRow-header + y;
                                    DRAW;
                                    break;
                                case 32: // button down
                                    mvwchgat((WINDOW*)pad,startRow-header + y,0,cols,gui::ATR_SELECTED, gui::COL_SELECTED, NULL);
                                    drag = 1; dragStart = startRow-header + y;
                                    DRAW;
                                    break;
                                case 35: // button up
                                {
                                    const int dragEnd = startRow-header + y;
                                    const int start = dragStart < dragEnd ?dragStart :dragEnd;
                                    const int end = start == dragEnd ? dragStart : dragEnd;
                                    switch (drag) {
                                        case 0:
                                            break;
                                        case 1: // primary drag
                                            for (int row = start; row <= end; ++row) 
                                                mvwchgat((WINDOW*)pad,row,0,cols,gui::ATR_SELECTED, gui::COL_SELECTED, NULL);
                                            DRAW;
                                            break;
                                        case 2: // secondary drag
                                            for (int row = start; row <= end; ++row) 
                                                mvwchgat((WINDOW*)pad,row,0,cols,0,0, NULL);
                                            DRAW;
                                            break;
                                    } drag = 0;
                                } break;
                                case 97: // scroll down
                                    SCROLL_DOWN
                                    break;
                                case 96: // scroll up
                                    if (startRow > header) {
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

                // TODO: this wont work for reasonably large files
                case 's':
                    if (isStdin) {
                        if (!atEOF) {
                            for (;;) {
                                if (read >= lines-1) { EXPAND(); }
                                if (read > MAXROWS || !reader.nextLine()) { SETEOF(read); break; }
                                ADDLINE();
                            }
                        }

                        mousemask(0, NULL);
                        mrutils::mutexRelease(gui::mutex);
                            if (fs.save()) {
                                int fd = MR_OPENW(fs.path);
                                if (fd > 0) {
                                    MR_WRITE(fd,ss.str().c_str(),ss.str().size());
                                    MR_CLOSE(fd);
                                }
                            }
                        mrutils::mutexAcquire(gui::mutex);
                        mousemask(BUTTON1_PRESSED, NULL);
                        REDRAW;
                    } else {
                        mousemask(0, NULL);
                        mrutils::mutexRelease(gui::mutex);
                            if (fs.save(mrutils::fileName(path,strlen(path)))) {
                                mrutils::cp(path,fs.path);
                            }
                        mrutils::mutexAcquire(gui::mutex);
                        mousemask(BUTTON1_PRESSED, NULL);
                        REDRAW;
                    }
                    break;

                case 1: // C-A
                case 'g':
                    startRow = header; 
                    DRAW;
                    break;

                /* TODO : this doesn't work for reasonably large files */
                case 5: // C-E
                case 'G':
                    // read the full file 
                    if (!atEOF) {
                        for (;;) {
                            if (read >= lines-1) { EXPAND(); }
                            if (read > MAXROWS || !reader.nextLine()) { SETEOF(read); break; }
                            ADDLINE();
                        }
                    }

                    startRow = read - rows + 1; // to show EOF
                    DRAW;
                    break;

                case 'l':
                    if (startCol + winCols < maxWide) {
                        ++startCol; REDRAW;
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
                                REDRAW;
                            }
                        } else {
                            if (startCol < max) {
                                startCol = max;
                                REDRAW;
                            }
                        }
                    }
                    break;

                case '}':
                case 'L':
                    if (maxWide > winCols) {
                        startCol = maxWide - winCols;
                        REDRAW;
                    }
                    break;

                case 'H':
                case '{':
                    if (startCol > 0) {
                        startCol = 0; REDRAW;
                    }
                    break;
                case 'h':
                    if (startCol > 0) {
                        --startCol; REDRAW;
                    }
                    break;

                case -24:
                case '[':
                    if (startCol > 0) {
                        startCol -= winCols/2;
                        if (startCol < 0) startCol = 0;
                        REDRAW;
                    }
                    break;

                case 10:
                case 13:
                case 'J': // scroll dn slower
                    SCROLL_DOWN
                    break;

                case 'j':
                    if (!atEOF) {
                        if (startRow + rows + fastMult > read) {
                            for (int r = startRow + rows + fastMult - read; r > 0; --r) {
                                if (read >= lines-1) { EXPAND(); }
                                if (read > MAXROWS || !reader.nextLine()) { SETEOF(read); break;
                                } else { ADDLINE(); }
                            }
                        }
                        startRow += fastMult;
                        DRAW;
                    } else if (startRow + rows <= read) { // show EOF line
                        startRow += fastMult;
                        DRAW;
                    }
                    break;

                case 'n':
                case ' ':
                case 'd':
                    if (!atEOF) {
                        if (startRow + 2*rows > read) {
                            for (int r = startRow + 2*rows - read; r > 0; --r) {
                                if (read >= lines-1) { EXPAND(); }
                                if (read > MAXROWS || !reader.nextLine()) { SETEOF(read); break;
                                } else { ADDLINE(); }
                            }
                        }
                        startRow += rows;
                        DRAW;
                    } else if (startRow + rows <= read) { // show EOF line
                        startRow += rows;
                        DRAW;
                    }
                    break;

                case 'k': // scroll up faster
                    if (startRow > header) {
                        startRow -= fastMult;
                        if (startRow < header) startRow = header;
                        DRAW;
                    }
                    break;

                case 'K': // scroll up slower
                    if (startRow > header) {
                        --startRow;
                        DRAW;
                    }
                    break;
                case 'p':
                case 'u':
                    if (startRow > header) {
                        startRow -= winRows;
                        if (startRow < header) startRow = header;
                        DRAW;
                    }
                    break;
            }
        }

        mrutils::mutexRelease(gui::mutex);
    }

    return true;
}


#endif
