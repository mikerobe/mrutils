#ifndef _DELIM_H
#define _DELIM_H

#include "mr_exports.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <exception>
#include "mr_linereader.h"
#include "mr_strutils.h"

namespace mrutils {

class _API_ ExceptionNoSuchColumn : public std::exception {
    virtual const char* what() const throw() { return str.c_str(); }

    public:
        ExceptionNoSuchColumn(char const * const msg) : str(msg) {}
        ExceptionNoSuchColumn(std::string const & str) : str(str) {}
        ExceptionNoSuchColumn(std::stringstream const & ss) : str(ss.str()) {}
        virtual ~ExceptionNoSuchColumn() throw() {}

    private:
        std::string str;
};

class _API_ DelimitedFileReader {
    public:
        DelimitedFileReader(char const * const path, char const delimiter=',');
        DelimitedFileReader(std::string const & path, char const delimiter=',');
        ~DelimitedFileReader();

        inline void setFixedWidth(bool tf = true) { fixedWidth = tf; }

        void setColumns(int ncols, char const * columns[] = NULL);
        int ncols() { if (!parsedColumns) readColumns(); return numColumns; }
        const char * colName(int col) { return colNames[col].c_str(); }

        /**
         * call this if the file does not have column labels in the first line. 
         * Then the first line is read to establish the number of columns, 
         * but not treated as column names. 
         * */
        void setNoColLabels() { noCols = true; }

        bool nextLine();
        bool readColumns();

        /**
         * note that this function currently cripples the line reader --
         * you can't read any lines after calling this. */
        bool parseLine(char * line);

        /**
         * Strips the trailing return character at the end of the line.
         */
        inline void trimLine() { mrutils::capNewLine(lr.line); }

        inline void close() { lr.close(); }
        inline void suspend() { lr.suspend(); }
        inline bool isSuspended() { return lr.isSuspended(); }
        inline bool resume() { return lr.resume(); }

        inline const char * getPath() const { return lr.path.c_str(); }
        inline char * getLine() const { return lr.line; }
        inline char * reformLine() { 
            for (int i = 1; i < numColumns; ++i) {
                if (lineSplits[i] <= 0) continue;
                lr.line[lineSplits[i]-1] = delimiter;
            }
            return lr.line;
        }

        inline unsigned getLineNumber() const { return lineNumber; }

        inline void skipLines(unsigned int lines) {
            if (!openedFile && !openFile()) return;
            while (lines-- > 0) lr.nextLine();
        }

        /* ******************************************* *
         * Reading data from each column
         * ******************************************* */

        inline bool hasColumn(const std::string& name) { if (!parsedColumns) readColumns(); return (columns.find(name) != columns.end()); }

        inline char * getStringFast(int col) { return &lr.line[lineSplits[col]]; }
        inline double getDoubleFast(int col) { return atof(&lr.line[lineSplits[col]]); }
        inline unsigned getUnsignedFast(int col) { return strtoul(&lr.line[lineSplits[col]],NULL,0); }
        inline int getIntFast(int col) { return atoi(&lr.line[lineSplits[col]]); }
        inline bool getBoolFast(int col) { return (lr.line[lineSplits[col]] == 't' || lr.line[lineSplits[col]] == 'T'); }

        char * getString(const char * name) throw(ExceptionNoSuchColumn);
        inline double getDouble(const char * name) throw(ExceptionNoSuchColumn) { return atof(skipBlankChars(getString(name))); }
        inline unsigned getUnsigned(const char * name) throw(ExceptionNoSuchColumn) { return strtoul(skipBlankChars(getString(name)),NULL,0); }
        inline int getInteger(const char * name) throw(ExceptionNoSuchColumn) { return atoi(skipBlankChars(getString(name))); }
        inline int getInt(const char * name) throw(ExceptionNoSuchColumn) { return getInteger(name); }
        bool getBool(const char * name) throw(ExceptionNoSuchColumn);

        /* ******************************************* *
         * Header settings support
         * ******************************************* */

        /**
         * Support for header settings at the beginning of the file. 
         * Reads in settings for each line until reaching a line that 
         * is not of the right format, then stops. 
         *
         * Should be called before nextLine() or will not do anything.
         *
         * The format for settings is "Name:\delimiterValue\nNextName:\delimiterNextValue\n"
         *
         * Note that one line is skipped after the header section is finished.
         */
        bool ReadHeader();
        inline bool hasHeaderField(const std::string& name) { 
            if (!parsedHeader) ReadHeader();
            return header.find(name) != header.end(); 
        }

        inline bool getHeader(const char * name, const char *& str) {
            if (!parsedHeader && !ReadHeader()) return false;
            std::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else str = it->second.c_str(); return true;
        }
        bool getHeader(const char * name, unsigned& u) {
            if (!parsedHeader && !ReadHeader()) return false;
            std::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else u = strtoul(it->second.c_str(),NULL,10); return true;
        }
        bool getHeader(const char * name, int& i) {
            if (!parsedHeader && !ReadHeader()) return false;
            std::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else i = atoi(it->second.c_str()); return true;
        }
        bool getHeader(const char * name, double& d) {
            if (!parsedHeader && !ReadHeader()) return false;
            std::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else d = atof(it->second.c_str()); return true;
        }
        bool getHeader(const char * name, bool& b) {
            if (!parsedHeader && !ReadHeader()) return false;
            std::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else b = (strncmp(it->second.c_str(),"true",4) == 0 || strncmp(it->second.c_str(),"TRUE",4)==0); return true;
        }
        const std::string & getHeaderString(const char * name) throw (ExceptionNoSuchColumn);
        inline double getHeaderDouble(const char * name) throw(ExceptionNoSuchColumn) { return atof(skipBlankChars(getHeaderString(name))); }
        inline unsigned getHeaderUnsigned(const char * name) throw(ExceptionNoSuchColumn) { return strtoul(skipBlankChars(getHeaderString(name)),NULL,10); }
        inline int getHeaderInteger(const char * name) throw(ExceptionNoSuchColumn) { return atoi(skipBlankChars(getHeaderString(name))); }
        inline int getHeaderInt(const char * name) throw(ExceptionNoSuchColumn) { return getHeaderInteger(name); }
        bool getHeaderBool(const char * name) throw(ExceptionNoSuchColumn);

        bool openFile() {
            if (openedFile) return true;
            if (!lr.open()) {
                std::cerr << "unable to open file \"" << lr.path << "\"" << std::endl;
                return false;
            }
            openedFile = true;
            return true;
        }

        void parseLineFast() { doLineSplit(); }

        bool changePath(const char * newPath) { if (openedFile) return false; 
            return lr.changePath(newPath); }

    private:
        inline char const * skipBlankChars(const std::string& str) {
            return skipBlankChars(str.c_str());
        }

        inline char const * skipBlankChars(char const * str) {
            if (str == NULL) return NULL;
            while (*str != '\0' && (*str == ' ' || *str == '\t' || *str == '"')) str++;
            return str;
        }

        /**
         * Finds the delimiter points in the string. Also removes the terminating new line
         * */
        void doLineSplit();

    private:
        const char delimiter;
        LineReader lr;

        std::vector<std::string> colNames;
        std::map<std::string, int> columns;
        int numColumns;
        bool noCols;

        unsigned lineNumber;

        bool splitLine;
        int * lineSplits;

        bool fixedWidth;
        bool parsedHeader;
        bool parsedColumns;
        bool openedFile;

        // header data
        std::map<std::string, std::string> header;

};

}


#endif
