#ifndef _MR_CPPLIB_FIXREADER_H
#define _MR_CPPLIB_FIXREADER_H

#include <iostream>
#include "mr_fixfields.h"
#include "mr_fixparser.h"
#include "mr_bufferedreader.h"
#include "mr_exports.h"

namespace mrutils {
class FixReader {
    public:
        FixReader(const char * file)
        : parsed(false), lineNumber(0)
        { 
            lr.open(file);
        }

        inline bool nextLine() { parsed = false; ++lineNumber; return lr.nextLine(); }
        inline unsigned getLineNumber() const { return lineNumber; }
        inline char * getLine() { return lr.line; }

        inline bool has(mrutils::FixField::Field field) { 
            if (!parsed) (parser.parse(lr.line), parsed = true);
            return parser.has(field);
        }

        /**
         * Returns pointer to the relevant part of the original 
         * fix message. If the message didn't have this field, then 
         * returns NULL. */
        inline char * get(mrutils::FixField::Field field) {
            if (!parsed) (parser.parse(lr.line), parsed = true);
            return parser.get(field);
        }
    private:
        bool parsed;
        unsigned lineNumber;
        mrutils::FixParser parser;
        mrutils::BufferedReader lr;

};
}


#endif
