#include "mr_termutils.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)

    #define CHECKEMPTY \
        if (eob == line) {\
            writeStatus();\
        }

    #define BACKSPACE(n) \
        if (p > line) {\
            fwrite(utilBackspace,1,n,stdout); \
            fwrite(p,1,eob-p,stdout);\
            fwrite(utilSpace,1,n,stdout); \
            fwrite(utilBackspace,1,n + (eob-p),stdout);\
            char * s = p - n;\
            for (char * q = s; p < eob;) *q++ = *p++;\
            p = s; eob -= n;\
        }

    #define DEL(n)\
        if (p < eob) {\
            char * q = p + n;\
            fwrite(q,1,eob-q,stdout);\
            fwrite(utilSpace,1,n,stdout);\
            fwrite(utilBackspace,1,eob-p,stdout);\
            char * s = p;\
            for (;q < eob;) *p++ = *q++;\
            p = s; eob -= n;\
        }


    #define CLEAR_BACKWARD \
    {   int n = p - line;\
        BACKSPACE(n) }

    #define CLEAR_FORWARD \
        if (p == eob) {}\
        else {\
            fwrite(utilSpace,1,eob - p,stdout);\
            fwrite(utilBackspace,1,eob - p,stdout);\
            eob = p;\
        }

    #define CLEAR_LINE \
        fwrite(utilBackspace,1,p - line,stdout);\
        fwrite(utilSpace,1,eob - line,stdout);\
        fwrite(utilBackspace,1,eob - line,stdout);\
        eob = line; p = line;
        

    #define BACKWARD_WORD \
        if (p == line) n = 0;\
        else {\
            char * q = p; --q;\
            while (*q == ' ' && q > line) --q;\
            if (*q == '/') --q;\
            while (*q != ' ' && *q != '/' && q > line) --q;\
            if (q > line) ++q;\
            n = p - q;\
        }

    #define FORWARD_WORD \
        if (p == eob) n = 0;\
        else {\
            char * q = p; \
            while (*q == ' ' && q < eob) ++q;\
            while (*q != ' ' && q < eob) ++q;\
            n = q - p;\
        }\

    #define MOVE_LEFT(n) \
        if (p > line) {\
            moveCursor(-n);\
            p -= n;\
        }

    #define MOVE_RIGHT(n) \
        if (p < eob) {\
            moveCursor(n,p);\
            p += n;\
        }

    #define MOVE_HELPER(m)\
        std::string str = history.peekTail(historyFromTail + m);\
        historyFromTail += m;\
        int len = (eob == line?status.size():eob - line);\
        fwrite(utilBackspace,1,p - line,stdout);\
        fputs(str.c_str(),stdout);\
        if ((int)str.size() < len) {\
            len -= str.size();\
            fwrite(utilSpace,1,len,stdout);\
            fwrite(utilBackspace,1,len,stdout);\
        }\
        eob = mrutils::strcpy(line,str.c_str());\
        p = eob;

    #define MOVE_UP \
        if ((int)history.size() > historyFromTail + 1) {\
            MOVE_HELPER(1)\
        }

    #define MOVE_DOWN \
        if (historyFromTail == 0) { \
            CLEAR_LINE\
            CHECKEMPTY\
            historyFromTail = -1;\
        } else if (historyFromTail > 0) {\
            MOVE_HELPER(-1)\
        }

    bool mrutils::BufferedTerm::nextLine(const char * passChars) {
        if (!init_) init();

        mrutils::mutexAcquire(statusMutex);
            enteringText = true;
            fputs(prompt.c_str(),stdout); 
            writeStatus();
            fflush(stdout);
        mrutils::mutexRelease(statusMutex);

        for (;eob < EOB;) {
            if (mrutils::SignalHandler::get()) {

                mrutils::mutexAcquire(statusMutex);
                    CLEAR_LINE
                    CHECKEMPTY
                    fflush(stdout);
                mrutils::mutexRelease(statusMutex);

                mrutils::SignalHandler::directSetCalled(true);
            }

            int c = mrutils::SignalHandler::getch();
            if (c < 0) continue;
            mrutils::SignalHandler::directSetCalled(false);
            if (!parseChar(c)) break;
        }

        mrutils::mutexAcquire(statusMutex);
            *eob = '\0'; eob = line; p = line;
            enteringText = false;
        mrutils::mutexRelease(statusMutex);

        if (line[0]) {
            history.add(line);
            historyFromTail = -1;
        }

        return (line[0] != 0);
    }


    bool mrutils::BufferedTerm::parseChar(int c) {
        switch (c) {
            case 13:
            case 10:
                mrutils::mutexAcquire(statusMutex);
                    fputs("\r\n",stdout);fflush(stdout);
                    enteringText = false;
                mrutils::mutexRelease(statusMutex);
                return false;

            case 11: // C_K
                mrutils::mutexAcquire(statusMutex);
                    CLEAR_FORWARD
                    CHECKEMPTY
                mrutils::mutexRelease(statusMutex);
                break;

            case 27: // ESC
            case 21: // C_U
                mrutils::mutexAcquire(statusMutex);
                    CLEAR_LINE
                    CHECKEMPTY
                mrutils::mutexRelease(statusMutex);
                break;

            case 16: // C_P
                mrutils::mutexAcquire(statusMutex);
                    MOVE_UP
                mrutils::mutexRelease(statusMutex);
                break;

            case 14: // C_N
                mrutils::mutexAcquire(statusMutex);
                    MOVE_DOWN
                mrutils::mutexRelease(statusMutex);
                break;

            case 1: // C_A
                { int n = p-line; MOVE_LEFT(n) }
                break;

            case 5: // C_E
                { int n = eob-p; MOVE_RIGHT(n) }
                break;

            case 6: // C_F
                MOVE_RIGHT(1)
                break;

            case 2: // C_B
                MOVE_LEFT(1)
                break;

            case 23: // C_W
                mrutils::mutexAcquire(statusMutex);
                    {  int n; BACKWARD_WORD
                       BACKSPACE(n) }
                    CHECKEMPTY
                mrutils::mutexRelease(statusMutex);
                break;

            case 4: // C_D
                mrutils::mutexAcquire(statusMutex);
                    DEL(1)
                    CHECKEMPTY
                mrutils::mutexRelease(statusMutex);
                break;

            case 127: // C_BACKSPACE
            case 8: // backspace
                mrutils::mutexAcquire(statusMutex);
                    BACKSPACE(1)
                    CHECKEMPTY
                mrutils::mutexRelease(statusMutex);
                break;

            case 0: // function sequence
            case 224: // escape sequence
                c = mrutils::SignalHandler::getch();

                switch (c) {
                    case 48: // ctrl-alt b
                        { int n; BACKWARD_WORD
                          MOVE_LEFT(n) } 
                        break;

                    case 33: // ctrl-alt f
                        { int n; FORWARD_WORD
                          MOVE_RIGHT(n) } 
                        break;

                    case 14: // ctrl-alt backspace
                        {  int n; BACKWARD_WORD
                           BACKSPACE(n) }
                        break;

                    case 32: // ctrl-alt d
                        mrutils::mutexAcquire(statusMutex);
                            { int n; FORWARD_WORD
                              DEL(n) } 
                            CHECKEMPTY
                        mrutils::mutexRelease(statusMutex);
                        break;

                    case 71: // home
                    case 119: // ctrl-home
                    case 151: // alt-home
                        { int n = p-line; MOVE_LEFT(n) }
                        break;

                    case 72: // up arrow
                    case 141: // ctrl-up
                    case 152: // alt-up
                    case 73: // page up
                    case 132: // ctrl-page up
                    case 153: // alt-page up
                        mrutils::mutexAcquire(statusMutex);
                            MOVE_UP
                        mrutils::mutexRelease(statusMutex);
                        break;

                    case 80: // down arrow
                    case 145: // ctrl-down
                    case 154: // alt-down
                    case 81: // page down
                    case 118: // ctrl-page down
                    case 161: // alt-page down
                        mrutils::mutexAcquire(statusMutex);
                            MOVE_DOWN
                        mrutils::mutexRelease(statusMutex);
                        break;

                    case 75: // left arrow
                    case 115: // ctrl-left
                    case 155: // alt-left
                        MOVE_LEFT(1)
                        break;
                    
                    case 77: // right arrow
                    case 116: // ctrl-right
                    case 157: // alt-right
                        MOVE_RIGHT(1)
                        break;

                    case 79: // end
                    case 117: // ctrl-end
                    case 159: // alt-end
                        { int n = eob-p; MOVE_RIGHT(n) }
                        break;

                    case 82: // insert
                    case 146: // ctrl-insert
                    case 162: // alt-insert
                        break;

                    case 83: // del
                        mrutils::mutexAcquire(statusMutex);
                            DEL(1)
                            CHECKEMPTY
                        mrutils::mutexRelease(statusMutex);
                        break;
                    case 147: // ctrl-del
                    case 163: // alt-del
                        mrutils::mutexAcquire(statusMutex);
                            { int n; FORWARD_WORD
                              DEL(n) } 
                            CHECKEMPTY
                        mrutils::mutexRelease(statusMutex);
                        break;
                }

                break;
                
            default:
                mrutils::mutexAcquire(statusMutex);
                    if (eob == line) clearStatus();
                    if (p == eob) {
                        ++p; *eob++ = c;
                        fputc(c,stdout); 
                    } else {
                        char * s = eob+1;
                        for (char * q = eob; eob > p;) *q-- = *--eob;
                        eob = s; *p = c;
                        fwrite(p,1,eob-p,stdout);
                        fwrite(utilBackspace,1,eob-p-1,stdout);
                        ++p;
                    }
                mrutils::mutexRelease(statusMutex);
        }

        fflush(stdout);
        return true;
    }
#else
    #define MR_ENDSEARCH \
        if (searchChar) { \
            searchChar = 0; \
            searchIt->second.searchFn(&searchChar); \
            if (!searchIt->second.endFn(false)) { \
                fputc('\n',stdout); fflush(stdout); \
                return false; \
            } \
        }
    
    #define MR_SEARCH \
        if (searchChar) { \
            *eob = '\0'; \
            searchIt->second.searchFn(line+searchIt->second.skipChar); \
        }
    #define MR_ATSTART \
        if (gotMutex) {\
            writeStatus();\
            gotMutex = false;\
            mrutils::mutexRelease(writeMutex);\
        } 
    
    void mrutils::BufferedTerm::setToFile(bool tf) {
        if (filePath.empty()) return;
        if (tf == this->pipeToFile) return;
    
        fflush(stdout);
    
        if (tf) {
            oldStdoutFD = dup(fileno(stdout));
    
            if (NULL == freopen(filePath.c_str(), (append?"a+":"w"), stdout)) {
                std::cerr << "Error opening " << filePath << std::endl;
                return;
            }
    
            oldStdout = fdopen( oldStdoutFD, "w" );
            append = true;
        } else {
            //close(fileno(stdout)); // don't close -- maintain continous stdout access
            dup2(oldStdoutFD, fileno(stdout));
        }
    
        this->pipeToFile = tf;
    }
    
    bool mrutils::BufferedTerm::nextLine(const char * passChars) {
        if (!init_) init();
    
        mrutils::mutexAcquire(writeMutex);
        fputs(prompt.c_str(),stdout); 
        writeStatus();
        mrutils::mutexRelease(writeMutex);
    
        p = line; eob = p;
    
        started  = true;
        gotMutex = false;
        checkFns = true;
    
        bool getChars = true;
        this->passChars = passChars;
    
        if (passChars) {
            const char * p = passChars;
            while (*p) {
                if (!parseChar(*p)) { getChars = false; break; }
                ++p;
            }
        }
    
        if (getChars) {
            int c; 
            while ((c = mrutils::SignalHandler::getch(obtainMutex))) {
                if (!parseChar(c)) break;
            }
        }

        started = false;
    
        *eob = '\0'; 
        if (line[0] == '\0') {
            if (gotMutex) mrutils::mutexRelease(writeMutex);
            return false;
        }
    
        history.add(line);
    
        if (gotMutex) mrutils::mutexRelease(writeMutex);
        return true;
    }
    
    void mrutils::BufferedTerm::appendText(const char * text) {
        p = mrutils::strcpy(p, text);
        fputs(text,stdout); fflush(stdout);
    }
    void mrutils::BufferedTerm::assignText(const char * text) {
        moveCursor(line-p);
        clearToEOL();
    
        p = mrutils::strcpy(line, text);
        eob = p;
    
        MR_ATSTART
    
        checkFns = true;
        searchChar = 0;
        fputs(text,stdout); 
        fflush(stdout);
    }
    
    bool mrutils::BufferedTerm::parseChar(int c) {
        if (checkFns && (
                (callIt = callFns.find(c)) != callFns.end()
                || (searchChar == 0 && (((searchIt = searchFns.find(c)) != searchFns.end())?(searchChar=searchIt->second.endChar):false))
                        )
           ) {
            if (callIt == callFns.end()) {
                checkFns = searchIt->second.checkFns;
                fputc(c,stdout); fflush(stdout);
                *p++ = c; ++eob;
                if (!searchIt->second.skipChar) {
                    *eob = '\0'; searchIt->second.searchFn(line);
                }
            } else {
                if (searchChar) {
                    checkFns = true;
                    searchChar = 0;
                    fputc(c,stdout); fputc('\n',stdout); fflush(stdout);
                    p = line; eob = p; *eob = '\0';
                    if (!searchIt->second.endFn(true)) return false;
                }
                if (!callIt->second()) {
                    fputc('\n',stdout); fflush(stdout);
                    return false;
                } return true;
            }
        } else {
            if (c == searchChar) {
                checkFns = true;
                searchChar = 0;
                fputc(c,stdout); fputc('\n',stdout); fflush(stdout);
                p = line; eob = p; *eob = '\0';
                return searchIt->second.endFn(true);
            } 

            switch (c) {
                case 13:
                case 10: // return
                    fputc('\n',stdout); fflush(stdout);
    
                    if (searchChar) {
                        checkFns = true;
                        searchChar = 0;
                        p = line; eob = p; *eob = '\0';
                        searchIt->second.endFn(true);
                        fputc('\n',stdout); fflush(stdout);
                        return false;
                    } else {
                        fromTail = 0;
                        return false;
                    }
                    
                case -115:  // <m-return>
                case 12:  // <c-l>
                case 9: // tab
    
                /** NOTE: 
                  * these functions will complete on the fly 
                  */
                    if ((callIt = callFns.find(c)) != callFns.end()) {
                        if (!callIt->second()) {
                            fputc('\n',stdout); fflush(stdout);
                            return false;
                        } return true;
                    }
                    break;
    
                case 27: // esc esc[x
                    c = mrutils::SignalHandler::getch();
                    if (c != '[') {
                        // escape pressed... 
                        
                        moveCursor(line-p);
                        clearToEOL();
    
                        MR_ATSTART
                        checkFns = true;
                        MR_ENDSEARCH
                        fflush(stdout);
    
                        p = line; eob = p; *eob = '\0';
    
                        /*
                        if (passChars != NULL)  {
                            eob = mrutils::strcpy(line,passChars);
                            fputs(passChars,stdout);
                            fflush(stdout);
                        }
                        */
    
                        return false;
                    } 
    
                    c = mrutils::SignalHandler::getch();
                    switch (c) {
                        case 'A': // up
                            return parseChar(C_PREV);
                        case 'B': // down
                            return parseChar(C_NEXT);
                        case 'C': // right
                            return parseChar(C_F);
                        case 'D': // left
                            return parseChar(C_B);
                        default:
                            break;
                    }
    
                case C_A:
                    if (searchChar && searchIt->second.skipChar) {
                        moveCursor(line-p + 1);
                        fflush(stdout);
                        p = line + 1;
                    } else {
                        moveCursor(line-p);
                        fflush(stdout);
                        p = line;
                    }
                    break;
                case C_E:
                    moveCursor(eob-p);
                    fflush(stdout);
                    p = eob;
                    break;
                case C_F:
                    if (p != eob) {
                        moveCursor(1);
                        fflush(stdout);
                        ++p;
                    }
                    break;
                case C_B:
                    if (p != line + (searchChar&&searchIt->second.skipChar?1:0)) {
                        moveCursor(-1);
                        fflush(stdout);
                        --p;
                    }
                    break;
                case C_K:
                    clearToEOL();
                    if (p == line) {
                        checkFns = true;
                        MR_ATSTART
                        MR_ENDSEARCH
                    } else MR_SEARCH
                    fflush(stdout);
                    eob = p;
                    break;
                case M_F:
                    {
                        if (p == eob) break;
                        char * q = p; 
                        while (*p == ' ' && p < eob) ++p;
                        while (*p != ' ' && p < eob) ++p;
                        moveCursor(p - q);
                        fflush(stdout);
                    } break;
                case M_B:
                    {
                        bool offset = searchChar&&searchIt->second.skipChar;
                        if (p == line+offset) break;
                        char * q = p; --p;
                        while (*p == ' ' && p > line+offset) --p;
                        while (*p != ' ' && p > line+offset) --p;
                        if (p > line+offset) ++p;
                        moveCursor(p - q);
                        fflush(stdout);
                    } break;
                
                case C_D:
                    {
                        if (p == eob) break;
                        char * q = p; --eob;
                        while (q < eob) { *q = *(q+1); ++q; }
                        clearToEOL(); *eob = '\0';
                        fputs(p,stdout);
                        moveCursor(p-eob);
                        if (p == line && eob == line) {
                            checkFns = true;
                            MR_ATSTART
                            MR_ENDSEARCH
                        } else MR_SEARCH
                        fflush(stdout);
                    } break;
    
                case M_D:
                    {
                        if (p == eob) break;
                        char * q = p; 
                        while (*q == ' ' && q < eob) ++q;
                        while (*q != ' ' && q < eob) ++q;
                        clearToEOL();
                        char * u = q; char * v = p;
                        while (u < eob) *v++ = *u++;
                        eob += (p - q); *eob = '\0';
                        fputs(p,stdout);
                        moveCursor(p-eob);
                        if (p == line && eob == line) {
                            checkFns = true;
                            MR_ATSTART
                            MR_ENDSEARCH
                        } else MR_SEARCH
                        fflush(stdout);
    
                    } break;
    
                case C_W:
                case C_DELWRD:
                    {
                        bool offset = searchChar&&searchIt->second.skipChar;
                        if (p == line+offset) break; 
    
                        char * q = p; --p;
                        while (*p == ' ' && p > line+offset) --p;
                        // the '/' is for paths
                        if (*p == '/') --p;
                        while (*p != ' ' && *p != '/' && p > line+offset) --p;
                        if (p > line+offset) ++p;
    
                        moveCursor(p-q);
                        clearToEOL();
    
                        char * u = q; char * v = p;
                        while (u < eob) *v++ = *u++;
                        eob += (p - q); *eob = '\0';
                        fputs(p,stdout);
                        moveCursor(p-eob);
                        if (p == line && eob == line) {
                            checkFns = true;
                            MR_ATSTART
                            MR_ENDSEARCH
                        } else MR_SEARCH
                        fflush(stdout);
                    } break;
                case C_PREV:
                    {
                        if (fromTail+1 > (int)history.size()) 
                            break;
    
                        MR_ENDSEARCH
    
                        moveCursor(line-p);
                        clearToEOL();
                        p = mrutils::strcpy(line, 
                                history.peekTail(fromTail++).c_str());
                        char * q = line;
                        while (q != p) fputc(*q++, stdout);
                        fflush(stdout);
                        eob = p;
                    } break;  
                case C_NEXT:
                    {
                        MR_ENDSEARCH
    
                        moveCursor(line-p);
                        clearToEOL();
                        fromTail -= 2;
                        if (fromTail < 0) {
                            fromTail = 0;
                            p = line; fflush(stdout);
                        } else {
                            p = mrutils::strcpy(line, 
                                    history.peekTail(fromTail++).c_str());
                            char * q = line;
                            while (q != p) fputc(*q++, stdout);
                            fflush(stdout);
                        }
                        eob = p;
                    } break;  
    
                case 127:
                case C_DEL:
                    if (p > line) {
                        char * q = --p; --eob;
                        while (q < eob) { *q = *(q+1); ++q; }
                        *eob = '\0';
    
                        moveCursor(-1);
                        clearToEOL();
                        fputs(p,stdout);
                        moveCursor(p - eob);
                        if (p == line && eob == line) {
                            checkFns = true;
                            MR_ATSTART
                            MR_ENDSEARCH
                        } else MR_SEARCH
                        fflush(stdout);
                    }
                    break;
                case C_CLLINE:
                    if (line != p) {
                        moveCursor(line-p);
                        clearToEOL();
                        MR_ATSTART
                        checkFns = true;
                        MR_ENDSEARCH
                        fflush(stdout);
                        p = line; eob = p; *eob = '\0';
                    }
                    break;
                default:
                    if (eob == EOB) break;
                    checkFns = false;
    
                    if (!gotMutex) {
                        mrutils::mutexAcquire(writeMutex);
                        if (smartTerm) clearToEOL();
                        gotMutex = true;
                    }
    
                    if (p == eob) {
                        //if (!status.empty()) clearToEOL(); // clear status
                        if (smartTerm) { fputc(c,stdout); fflush(stdout); }
                        *p++ = c; ++eob; 
                    } else {
                        clearToEOL();
                        char * q = p; ++eob; q = eob-1;
                        while (q > p) { *q = *(q-1); --q; }
                        *p = c; *eob = '\0'; 
                        fputs(p,stdout); 
                        moveCursor(++p - eob);
                        fflush(stdout);
                    }
    
                    MR_SEARCH
            }
        }
    
        return true;
    }
#endif
