#ifndef _MR_CPPLIB_FIXPARSER_H
#define _MR_CPPLIB_FIXPARSER_H

#include <iostream>
#include "mr_fixfields.h"
#include "mr_exports.h"

namespace mrutils {

    class FixParser {
        public:
            FixParser()
                : msgNumber(0)
            { memset(lastMsg, 0, MRFIX_MAXFIELD); }

            inline void parse(char * msg) {
                ++msgNumber;
                startNum = msg; 

                for (;;) {
                    switch (*msg) {
                        case '\0':
                        case '\001': 
                            fixField = atoi(startNum);
                            if (fixField >= MRFIX_MAXFIELD) return;
                            lastMsg[fixField] = msgNumber;

                            switch (fixField) {
                                case FixField::MDEntryType:
                                case FixField::MDEntryPx:
                                case FixField::MDEntrySize:
                                    if (lastMsg[FixField::NoMDEntries] != msgNumber) {
                                        lastMsg[fixField] = 0;
                                    } else {
                                        *((char**)(fix[fixField])) = startVal;
                                        fix[fixField] += 4;
                                    }
                                    break;
                                case FixField::NoMDEntries:
                                    fix[FixField::MDEntryType] = (char*)fixMDType;
                                    fix[FixField::MDEntryPx]   = (char*)fixMDPx;
                                    fix[FixField::MDEntrySize] = (char*)fixMDSize;
                                    fix[fixField] = startVal;
                                    break;
                                default:
                                    fix[fixField] = startVal;
                                    break;
                            }

                            if (*msg == '\0') return;
                            *msg = '\0';
                            startNum = ++msg;
                            break;
                        case '=':
                            *msg = '\0';
                            startVal = ++msg;
                            break;
                        default:
                            ++msg;
                    }
                }
            }

            inline bool has(mrutils::FixField::Field field) {
                if (lastMsg[field] != msgNumber) return false;

                switch (field) {
                    case FixField::MDEntryType:
                        return ((char**)fix[field] - fixMDType > 0);
                    case FixField::MDEntryPx:
                        return ((char**)fix[field] - fixMDPx   > 0);
                    case FixField::MDEntrySize:
                        return ((char**)fix[field] - fixMDSize > 0);
                    default:
                        return true;
                }
            }

            /**
             * Returns pointer to the relevant part of the original 
             * fix message. If the message didn't have this field, then 
             * returns NULL. */
            inline char * get(mrutils::FixField::Field field) {
                if (lastMsg[field] != msgNumber) return NULL;
                switch (field) {
                    case FixField::MDEntryType:
                    case FixField::MDEntryPx:
                    case FixField::MDEntrySize:
                        fix[field] -= 4;
                       return *((char**)(fix[field]));
                    default:
                        return fix[field];
                }
            }
        private:
            unsigned msgNumber;
            char * fix[MRFIX_MAXFIELD];
            unsigned lastMsg[MRFIX_MAXFIELD];
        private:
            char * fixMDType[MRFIX_MAXDUPS];
            char * fixMDPx[MRFIX_MAXDUPS];
            char * fixMDSize[MRFIX_MAXDUPS];
        private:
            char *startNum, *startVal;
            int fixField;
    };

}

#endif
