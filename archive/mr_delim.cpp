#include "mr_delim.h"

using namespace mrutils;

DelimitedFileReader::DelimitedFileReader(char const * const path, char const delimiter) :
    lr(path), lineNumber(0), delimiter(delimiter), lineSplits(NULL), parsedHeader(false), 
    parsedColumns(false), openedFile(false), numColumns(0), splitLine(false),
    fixedWidth(false), noCols(false)
{ }

DelimitedFileReader::DelimitedFileReader(std::string const & path, char const delimiter) :
    lr(path.c_str()), lineNumber(0), delimiter(delimiter), lineSplits(NULL), parsedHeader(false),
    parsedColumns(false), openedFile(false), numColumns(0), splitLine(false),
    fixedWidth(false), noCols(false)
{ }

DelimitedFileReader::~DelimitedFileReader() {
    if (lineSplits != NULL) delete[] lineSplits;
}

void DelimitedFileReader::setColumns(int ncols, char const ** columns) {
    numColumns = ncols;

    if (columns != NULL)
    for (int i = 0; i < numColumns; i++) {
        this->columns.insert( std::pair<std::string, int>(columns[i],i) );
        this->colNames.push_back( columns[i] );
    }

    lineSplits = new int[numColumns];
    lineSplits[0] = 0;

    parsedColumns = true;
}
bool DelimitedFileReader::readColumns() {
    if (!openedFile && !openFile()) return false;

    if (!parsedHeader && !lr.nextLine()) return false;

    while (
            (lr.line[0] == '/' && lr.line[1] == '/') ||
            (lr.line[0] == '\n' || lr.line[0] == '\0' || lr.line[0] == '\r') )
        if (!lr.nextLine()) return false;

    char *start = lr.line;
    char *pch = start;

    if (fixedWidth) {
        std::vector<int> fixedSplits;
        while (true) {
            pch = strchr( pch, ' ' );
            if (pch == NULL || *(pch+1) == '\0') 
                mrutils::capNewLine(start);
            else if (*(++pch) != ' ') continue;
            else *(pch-1) = '\0';

            if (*start == '\0') break;
            fixedSplits.push_back(start-lr.line);
            columns.insert( std::pair<std::string, int>(start,numColumns++) );
            colNames.push_back(start);
            if (pch == NULL || *pch == '\0') break;

            while (*(++pch) == ' ');
            start = pch;
        }
        lineSplits = new int[fixedSplits.size()];
        for (unsigned i =0; i < fixedSplits.size(); ++i) lineSplits[i] = fixedSplits[i];
    } else {
        std::vector<int> firstSplits;
        while (true) {
            pch = strchr( start, delimiter );

            if (pch != NULL) {
                *pch = '\0'; 
                firstSplits.push_back(pch-lr.line);
            }

            while (*start == ' ' || *start == '\t') *start++;
            columns.insert( std::pair<std::string, int>(start,numColumns++) );
            colNames.push_back(start);
            if (pch == NULL) break;
            start = pch + 1;
        }

        lineSplits = new int[numColumns];
        for (int i = 0; i < numColumns-1; ++i) lineSplits[i+1] = 1+firstSplits[i];
        lineSplits[0] = 0;
    }
    parsedColumns = true;
    return true;
}
void DelimitedFileReader::doLineSplit() {
    if (splitLine) return;

    if (fixedWidth) {
        char * end;
        for (int i = 1; i < numColumns; ++i) {
            end = &lr.line[lineSplits[i]];
            while (*(--end) == ' ' && end > lr.line);
            *(end+1) = '\0';
        }

        char * start = &lr.line[ lineSplits[numColumns-1] ];
        end = &start[strlen(start)-1];
        while (end >= start && (*end==' '||*end=='\r'||*end=='\n')) --end;
        *(end+1) = '\0';
    } else {
        char * start = lr.line;

        for (int i = 1; i < numColumns; ++i) {
            if (start == NULL || NULL == (start = strchr( start, delimiter ))) {
                lineSplits[i] = -1;
            } else {
                *start = '\0';
                lineSplits[i] = ++start - lr.line;
            }
        }
    }

    splitLine = true;
}

bool DelimitedFileReader::parseLine(char * line) {
    if (!openedFile && !openFile()) return false;
    if (!parsedHeader) ReadHeader();
    if (!parsedColumns && !readColumns()) return false;
    splitLine = false;

    lr.line = line;

    return true;
}

bool DelimitedFileReader::nextLine() {
    if (!openedFile && !openFile()) return false;
    if (!parsedColumns && !parsedHeader) ReadHeader();
    if (!parsedColumns && !readColumns()) return false;
    splitLine = false;

    if (noCols) { noCols = false; splitLine = true; return true; }

    while (lr.nextLine()) {
        lineNumber++;

        if (lr.line[0] == '/' && lr.line[1] == '/') continue;
        if (lr.line[0] == '\n' || lr.line[0] == '\0') continue;
        else return true;
    }

    return false;
}

/* ******************************************* *
 * Reading data from each column
 * ******************************************* */

char * DelimitedFileReader::getString(const char * name) throw(ExceptionNoSuchColumn) {
    doLineSplit();
    std::map<std::string, int>::iterator it = columns.find(name);
    if (it == columns.end()) {
        std::stringstream ss;
        ss << "No such column '" << name << "' in file " << lr.path;
        throw ExceptionNoSuchColumn(ss);
    }

    if (lineSplits[it->second] < 0) return NULL;
    return &lr.line[lineSplits[it->second]];
}

bool DelimitedFileReader::getBool(const char * name) throw(ExceptionNoSuchColumn) { 
    const char * str = skipBlankChars(getString(name));
    if (str == NULL) return false;
    return ( (str[0] == 't' || str[0] == 'T') &&
            (str[1] == 'r' || str[1] == 'R') &&
            (str[2] == 'u' || str[2] == 'U') &&
            (str[3] == 'e' || str[3] == 'E') );
}

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
bool DelimitedFileReader::ReadHeader() {
    if (parsedHeader) return true;
    if (lineNumber > 0) return false;
    if (!openFile()) return false;

    if (!lr.nextLine()) return false;
    lineNumber++;

    do {
        if (lr.line[0] == '/' && lr.line[1] == '/') continue;
        if (lr.line[0] == '\n' || lr.line[0] == '\0') continue;

        char *name = lr.line;
        char *value = strchr( name, delimiter );
        if ( value == NULL ) break;  
        if (*(value - 1) != ':') break;

        *(value-1) = '\0';
        while (name < value && (*name == ' ' || *name == '\t' || *name == '"')) name++;

        mrutils::capNewLine(value);

        header.insert( std::pair<std::string,std::string>(name,++value) );
    } while (lr.nextLine() && lineNumber++);

    parsedHeader = true;

    return !header.empty();
}

const std::string & DelimitedFileReader::getHeaderString(const char * name) throw (ExceptionNoSuchColumn) {
    if (!parsedHeader && !ReadHeader()) {
        std::stringstream ss;
        ss << "Unable to read header of file " << lr.path;
        throw ExceptionNoSuchColumn(ss);
    }

    std::map<std::string,std::string>::iterator it = header.find(name);
    if (it == header.end()) {
        std::stringstream ss;
        ss << "No such header '" << name << "' in file " << lr.path;
        throw ExceptionNoSuchColumn(ss);
    }

    return (it->second);
}

bool DelimitedFileReader::getHeaderBool(const char *  name) throw(ExceptionNoSuchColumn) {
    const char * str = skipBlankChars(getHeaderString(name));
    if (str == NULL) return false;
    return ( (str[0] == 't' || str[0] == 'T') &&
            (str[1] == 'r' || str[1] == 'R') &&
            (str[2] == 'u' || str[2] == 'U') &&
            (str[3] == 'e' || str[3] == 'E') );
}

