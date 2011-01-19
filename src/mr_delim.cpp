#include "mr_delim.h"

void mrutils::DelimitedFileReader::setColumns(const int ncols, char const ** columns) {
    numColumns = ncols;
    lineSplits.resize(numColumns);
    if (fixedWidth) lineSplitsFixed.resize(numColumns);

    if (columns == NULL) {
        noLabels = true;
    } else {
        std::string colName;
        for (int i = 0; i < numColumns; ++i) {
            colName.assign(columns[i]);
            this->columns.insert(colName,&lineSplits[i]);
            this->colNames.push_back(colName);
        }
    }

    parsedColumns = true;
}

void mrutils::DelimitedFileReader::setColumns(const std::vector<std::string>& columns) {
    numColumns = columns.size();
    lineSplits.resize(numColumns);
    if (fixedWidth) lineSplitsFixed.resize(numColumns);

    for (int i = 0; i < numColumns; ++i) {
        this->columns.insert(columns[i],&lineSplits[i]);
        this->colNames.push_back( columns[i] );
    }

    parsedColumns = true;
}

bool mrutils::DelimitedFileReader::readHeader() {
    if (parsedHeader || lineNumber > 0) return true;

    while (lr.nextLineStripCR()) {
        ++lineNumber;
        if (*line == '\0' || (*line == '/' && *(line+1) == '/')) continue;

        char *name = line;
        char *value = strchr( name, delim );
        if ( value == NULL ) break;  

        char * p = value;
        while (p > line && (*--p == ' ' || *p == '"')){}
        if (*p != ':') break;// found <<field, value>>

        *p = '\0';
        while (name < p && (*name == ' ' || *name == '\t' || *name == '"')) ++name;

        header.insert(name,value+1);
    }

    // only OK if at least 1 line of content was read
    return (parsedHeader = (lineNumber > 0));
}

bool mrutils::DelimitedFileReader::readColumns() {
    if (!parsedHeader && !readHeader()) return false;
    parsedColumns = true;

    if (fixedWidth) return readColumnsFixed();

    if (noLabels) {
        splitLine = true;
        for (char * start = lr.line, *p = lr.line;;start = p+1) {
            ++numColumns; lineSplits.push_back(start);
            if (NULL == (p = strchr(start,delim))) break;
            *p = '\0';
        }
    } else {
        std::string colName;
        for(char * p = lr.line, *start = lr.line;;start = p + 1) {
            ++numColumns; 
            while (*start == ' ' || *start == '\t') ++start;
            if (NULL == (p = strchr(start,delim))) { colNames.push_back(start); break; }
            else colNames.push_back(colName.assign(start,p-start));
        }

        lineSplits.resize(numColumns,NULL);
        for (int i = 0; i < numColumns; ++i) 
            columns.insert(colNames[i],&lineSplits[i]);

        inContent = true;
        while (lr.nextLineStripCR()) {
            ++lineNumber;
            if  (*line == '\0' || (*line == '/' && *(line+1) == '/')) continue;
            lineSplits[0] = lr.line;
            inContent = false;
            break;
        }
    }

    return true;
}

bool mrutils::DelimitedFileReader::readColumnsFixed() {
    char *start = lr.line;
    char *p = start;

    lineSplits.push_back(lr.line);
    
    for (;;) {
        p = strchr( p, ' ' );
        if (p == NULL || *(p+1) == '\0') {}
        else if (*(++p) != ' ') continue;
        else *(p-1) = '\0';

        if (*start == '\0') break;
        ++numColumns;
        lineSplitsFixed.push_back(start-lr.line);
        lineSplits.push_back(start);

        if (!noLabels) colNames.push_back(start);

        if (p == NULL || *p == '\0') break;

        while (*(++p) == ' '){}
        start = p;
    }

    for (int i = 0; i < numColumns; ++i) 
        columns.insert(colNames[i],&lineSplits[i]);

    // advance if this was a labels row
    if (!noLabels) {
        inContent = true;
        while (lr.nextLineStripCR()) {
            ++lineNumber;
            if (*line == '\0' || (*line == '/' && *(line+1) == '/')) continue;
            lineSplits[0] = lr.line;
            inContent = false;
            break;
        }
    } else {
        splitLine = true;
    }

    return true;
}

void mrutils::DelimitedFileReader::parseLineFixed() {
    splitLine = numColumns;

    char * start, *end;
    for (int i = 1; i < numColumns; ++i) {
        end = lr.line + lineSplitsFixed[i];
        start = lineSplits[i-1];

        lineSplits[i] = end;

        for (end -= 2;end != start && *end == ' ';--end) {}
        end[1] = '\0';
    }

    // now the last column
    start = lineSplits[numColumns-1];
    if (lr.strlen > 0) {
        end = lr.line + lr.strlen - 1;
        for (;end != start && *end == ' ';--end) {}
        end[1] = '\0';
    }
}
