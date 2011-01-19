#ifndef _DELIM_H
#define _DELIM_H

#include "mr_exports.h"
#include <iostream>
#include "mr_map.h"
#include <string>
#include <vector>
#include <sstream>
#include <exception>
#include "mr_bufferedreader.h"
#include "mr_strutils.h"
#include "mr_except.h"

namespace mrutils {

/**
  * Files of the form (first part is header)
  * header field:,value
  * header field 2:,value
  * field 1, field 2, field 3
  * value, value, value
  * value, value, value
  */
class _API_ DelimitedFileReader {
    public:
        DelimitedFileReader(char const * const path = "", char const delimiter=',')
        : br(new mrutils::BufferedReader()), lr(*br), path(lr.path)
         ,line(lr.line), lineNumber(0)
         ,delim(delimiter), numColumns(0), noLabels(false), fixedWidth(delimiter == '\0')
         ,splitLine(0), parsedHeader(false), parsedColumns(false), inContent(false)
         ,null_('\0')
        {
            if (*path) lr.open(path);
        }

        DelimitedFileReader(mrutils::BufferedReader & reader, char const delimiter=',')
        : br(NULL), lr(reader), path(lr.path)
         ,line(lr.line), lineNumber(0)
         ,delim(delimiter), numColumns(0), noLabels(false), fixedWidth(delimiter == '\0')
         ,splitLine(0), parsedHeader(false), parsedColumns(false), inContent(false)
         ,null_('\0')
         {
         }

        ~DelimitedFileReader() {
            if (br != NULL) delete br;
        }

    public:
       /******************
        * Settings
        ******************/

        /**
         * call this if the file does not have column labels in the first line. 
         * Then the first line is read to establish the number of columns, 
         * but not treated as column names. 
         * */
        inline void setNoColLabels() { noLabels = true; }

        void setColumns(const int ncols, char const * columns[] = NULL);
        void setColumns(const std::vector<std::string>& columns);

        inline void setFixedWidth(bool tf = true) { 
            if (tf) { fixedWidth = true; delim = '\0'; }
            else { fixedWidth = false; delim = ','; }
        }


    public:
       /******************
        * IO
        ******************/

        inline bool open(const char * const path, char const delimiter = ',') { 
            this->delim = delimiter;

            parsedHeader = parsedColumns = inContent = false;
            splitLine = lineNumber = 0;

            header.clear();
            lineSplits.clear();
            lineSplitsFixed.clear();
            columns.clear();
            colNames.clear();
            numColumns = 0;

            return lr.open(path); 
        }

        inline void close() { lr.close(); }

    public:
       /******************
        * Reading data
        ******************/

        bool readHeader();
        bool readColumns();

        inline bool nextLine() {
            if (!inContent) {
                if (!parsedHeader && !readHeader()) return false;
                if (!parsedColumns && !readColumns()) return false;
                inContent = true;
                return true;
            }

            splitLine = 0;
            while (lr.nextLineStripCR()) { 
                ++lineNumber;
                if (*line == '\0' || (*line == '/' && *(line+1) == '/')) continue;
                lineSplits[0] = lr.line;
                return true;
            }

            return false;
        }

        inline void parseLine() { parseLine(numColumns); }

        // call this for fast processing
        inline void parseLine(int toCol) { 
            if (splitLine >= ++toCol) return;
            if (fixedWidth) { parseLineFixed(); return; }

            char * start = lineSplits[splitLine];
            for (int i = splitLine+1; i <= toCol; ++i) {
                if (NULL == (start = strchr( start, delim ))) {
                    memset(&lineSplits[i],0,sizeof(char*)*(toCol-i));
                    break;
                }
                *start = '\0';
                lineSplits[i] = ++start;
            }

            splitLine = toCol;
        }

        inline bool hasColumn(const std::string& name) {
            if (!parsedColumns) readColumns();
            return columns.contains(name);
        }

        /** fast processing **/
        inline char * getString(int col) { 
            char * const tmp = lineSplits[col];
            if (tmp == NULL) return &null_;
            return tmp;
        }

        inline double getDouble(int col) { 
            char * const tmp = lineSplits[col];
            if (tmp == NULL) return 0;
            return atof(tmp);
        }

        inline unsigned getUnsigned(int col) { 
            char * const tmp = lineSplits[col];
            if (tmp == NULL) return 0;
            return strtoul(tmp,NULL,0); 
        }

        inline int getInt(int col) { 
            char * const tmp = lineSplits[col];
            if (tmp == NULL) return 0;
            return mrutils::atoi(tmp);
        }

        inline bool getBool(int col) { 
            char * const tmp = lineSplits[col];
            if (tmp == NULL) return 0;
            const char c = *tmp;
            return (c == '1' || c == 't' || c == 'T');
        }

        /** by column name **/
        inline int findColumn(const char * name) throw(ExceptionNoSuchColumn) {
            if (!parsedColumns && !readColumns()) throw ExceptionNoSuchColumn(std::string("No columns in file") + lr.path);
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) {
                throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            } return (it - columns.data);
        }

        inline char * getString(const char * name) throw(ExceptionNoSuchColumn) { 
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            parseLine(it - columns.data);
            char * const tmp = *(*it)->second;
            if (tmp == NULL) return &null_;
            return tmp;
        }

        inline double getDouble(const char * name) throw(ExceptionNoSuchColumn) { 
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            parseLine(it - columns.data);
            char * const tmp = *(*it)->second;
            if (tmp == NULL) return 0;
            return atof(tmp);
        }

        inline unsigned getUnsigned(const char * name) throw(ExceptionNoSuchColumn) {
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            parseLine(it - columns.data);
            char * const tmp = *(*it)->second;
            if (tmp == NULL) return 0;
            return strtoul(tmp,NULL,0);
        }

        inline int getInt(const char * name) throw(ExceptionNoSuchColumn) { 
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            parseLine(it - columns.data);
            char * const tmp = *(*it)->second;
            if (tmp == NULL) return 0;
            return mrutils::atoi(tmp);
        }

        inline bool getBool(const char * name) throw(ExceptionNoSuchColumn) { 
            mrutils::map<std::string, char **>::iterator const it = columns.find(name);
            if (it == columns.end()) throw ExceptionNoSuchColumn(mrutils::stringstream().clear()
                    << "No such column '" << name << "' in file " << lr.path);
            parseLine(it - columns.data);
            char * const tmp = *(*it)->second;
            if (tmp == NULL) return false;
            const char c = *tmp;
            return (c == '1' || c == 't' || c == 'T');
        }

    public:
       /******************
        * Reading header
        ******************/

        inline bool hasHeaderField(const std::string& name) { 
            if (!parsedHeader) readHeader();
            return header.contains(name);
        }

        inline bool getHeader(const char * name, const char *& str) {
            if (!parsedHeader && !readHeader()) return false;
            mrutils::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else str = (*it)->second.c_str(); return true;
        }
        bool getHeader(const char * name, unsigned& u) {
            if (!parsedHeader && !readHeader()) return false;
            mrutils::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else u = strtoul((*it)->second.c_str(),NULL,10); return true;
        }
        bool getHeader(const char * name, int& i) {
            if (!parsedHeader && !readHeader()) return false;
            mrutils::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else i = mrutils::atoi((*it)->second.c_str()); return true;
        }
        bool getHeader(const char * name, double& d) {
            if (!parsedHeader && !readHeader()) return false;
            mrutils::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else d = atof((*it)->second.c_str()); return true;
        }
        bool getHeader(const char * name, bool& b) {
            if (!parsedHeader && !readHeader()) return false;
            mrutils::map<std::string,std::string>::iterator it = header.find(name);
            if (it == header.end()) return false;
            else b = (0==mrutils::strnicmp((*it)->second.c_str(),"true",4)); return true;
        }

    public:
       /******************
        * Suspending 
        ******************/

        inline bool suspend() { return lr.suspend(); }
        inline bool suspended() { return lr.suspended(); }
        inline bool resume() { return lr.resume(); }

    public:
       /******************
        * General data
        ******************/

        inline int ncols() { if (!parsedColumns) readColumns(); return numColumns; }

        // moved here to get the constructor
        private: mrutils::BufferedReader * br; public:

        mrutils::BufferedReader& lr;

        std::string& path;
        char *& line;
        unsigned lineNumber;
        std::vector<std::string> colNames;

    public:
       /******************
        * Utilities
        ******************/

        inline bool skipLines(unsigned int lines) {
            if (!noLabels && !parsedColumns) readColumns();

            for (;lines > 0;) {
                if (!lr.nextLineStripCR()) return false;
                if (lr.line[0] == '\0') continue;
                if (*lr.line == '/' && *(lr.line+1) == '/') continue;
                --lines;
            }

            return true;
        }

        inline char * reformLine() { 
            for (int i = 1; i < numColumns; ++i) {
                if (lineSplits[i] == NULL) continue;
                *(lineSplits[i]-1) = delim;
            }
            return lr.line;
        }

        inline void reparseLine() {
            for (int i = 1; i < numColumns; ++i) {
                if (lineSplits[i] == NULL) continue;
                *(lineSplits[i]-1) = '\0';
            }
        }

        inline void printCols(FILE * file = stdout) {
            if (!parsedColumns && !readColumns()) return;

            fputs(colNames[0].c_str(),file);
            for (int i = 1; i < numColumns; ++i) {
                putc(delim,file); fputs(colNames[i].c_str(),file);
            }

            putc('\n',file);
        }

    private:
        void parseLineFixed();
        bool readColumnsFixed();

    private:
        char delim;
        int numColumns;
        mrutils::map<std::string,char**> columns;
        std::vector<char *> lineSplits;
        std::vector<int> lineSplitsFixed;

        // descriptors
        bool noLabels, fixedWidth;

        // status
        int splitLine;
        bool parsedHeader, parsedColumns, inContent;


        // header data
        mrutils::map<std::string, std::string> header;

        char null_;
};

}


#endif
