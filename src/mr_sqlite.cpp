#include "mr_sqlite.h"
#include <memory>

namespace {
    static const bool multithreadOk = (sqlite3_threadsafe() || SQLITE_OK == sqlite3_config(SQLITE_CONFIG_MULTITHREAD));
}

bool mrutils::Sqlite::connect() {
    if (statement != NULL) { sqlite3_finalize(statement); statement = NULL; }
    if (db != NULL) { sqlite3_close(db); db = NULL; }

    connected = false;
    if (!multithreadOk) return (std::cerr << "Sqlite couldn't initialize multithread support" << std::endl, false);
    if (0 != sqlite3_open(path.c_str(),&db)) return (std::cerr << "Sqlite can't open database at " << path << std::endl, false);
    return (connected = (db != NULL));
}

bool mrutils::Sqlite::dumpTableDesc(const char * table, std::vector<char> * colTypes, std::vector<std::string>* colNames, std::string * priName, const std::vector<std::string> * omitColumns) {
    mrutils::stringstream ss;
    
    int priCol = -1; 

    std::string desc;
    if (!get(ss.clear() << "select sql from sqlite_master where tbl_name=\"" << table << "\"",desc)) return (std::cerr << "unable to execute sqlite describe (" << error << ") : " << ss << std::endl,false);

    const char * p = desc.c_str();
    const char * const end = p + desc.size();
    if (NULL == (p = strchr(p,'('))) return false;
    ++p;

    std::string colName;

    int colNum = 0;
    for (const char * q = p;q != end;p = q+1) {
        if (NULL == (q = strchr(p,','))) q = end;

        for (;*p == ' ' || *p == '\t' || *p == '"' || *p == '`' || *p == '\'';++p) ;

        const char * r = p;
        for (;*r != ' ' && *r != '\t' && *r != '"' && *r != '`' && *r != '\'' && r != q;++r) ;
        if (r == q) continue;

        colName.assign(p,r-p);

        if (omitColumns != NULL && mrutils::containsI(*omitColumns, colName)) continue;

        colNames->push_back(colName);

        for (p = r+1;*p == ' ' || *p == '\t' || p == q;++p) ;
        for (r = p;*r != ' ' && *r != '\t' && r != '\0';++r) ;

        if (mrutils::startsWithI(p,"int")
            ||mrutils::startsWithI(p,"tinyint")
            ||mrutils::startsWithI(p,"smallint")
            ||mrutils::startsWithI(p,"bit")
            ) {

            if (mrutils::stristr(r,"primary key")) {
                if (priCol != -1) {
                    errId = -1;
                    error = "mrutils::Sql dump error; primary column must be a single integer type column.";
                    return false;
                }

                priCol = colNum;
                *priName = colNames->back();

                colTypes->push_back('p');
            } else {
                colTypes->push_back('d');
            }
        } else if (mrutils::startsWithI(p,"varchar")
            ||mrutils::startsWithI(p,"blob")
            ||mrutils::startsWithI(p,"mediumblob")
            ||mrutils::startsWithI(p,"char")
            ||mrutils::startsWithI(p,"enum")
            ) {
            colTypes->push_back('s');
        } else if (mrutils::startsWithI(p,"double")
            ||mrutils::startsWithI(p,"float")
            ) {
            colTypes->push_back('f');
        } else {
            errId = -1;
            errStr = "mrutils::Sql::dumpTable unhandled column type: "; errStr += p;
            error = errStr.c_str();
            return false;
        }

        ++colNum;
    }

    if (priCol == -1) {
        errId = -1;
        errStr = "mrutils::Sql dump error, no primary key found for table "; errStr += table;
        error = errStr.c_str();
        return false;
    }

    // make the primary key the first column
    if (priCol != 0) {
        (*colNames)[priCol] = (*colNames)[0];
        (*colTypes)[priCol] = (*colTypes)[0];
        (*colNames)[0] = *priName;
        (*colTypes)[0] = 'p';
    }

    return true;
}
