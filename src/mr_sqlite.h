#ifndef _MR_CPPLIB_SQLITE_H
#define _MR_CPPLIB_SQLITE_H

#include "mr_sql.h"
#include <sqlite3.h>

namespace mrutils {

class Sqlite : public Sql
{
public:
    Sqlite(const char * path) :
        path(path), db(NULL), statement(NULL)
    {}

    Sqlite(const Sqlite& other) :
        Sql(other), path(other.path), db(NULL), statement(NULL)
    {}

    ~Sqlite()
    {
        if (statement != NULL) sqlite3_finalize(statement);
        if (db != NULL) sqlite3_close(db);
    }

    inline Sqlite* clone() const { return new Sqlite(*this); }
    inline const char * autoIncrement() const { return "autoincrement"; }

public: // queries
    bool connect();

    inline bool run(const char * q) {
        return ((connected || connect()) && SQLITE_OK == sqlite3_exec(db,q,NULL,0,const_cast<char**>(&error)));
    }

    inline bool rr(const char * q) {
        if (statement != NULL) { sqlite3_finalize(statement); statement = NULL; }
        return ((connected || connect()) && SQLITE_OK == sqlite3_prepare_v2(db,q,-1,&statement,NULL) && NULL != statement);
    }

    inline bool nextLine() {
        if (statement == NULL) return false;
        return (SQLITE_ROW == sqlite3_step(statement));
    }

    inline bool insert(const char * q, int* lastInsertId) {
        if (!run(q)) return false;
        if (lastInsertId != NULL) 
            *lastInsertId = (int)sqlite3_last_insert_rowid(db);
        return true;
    }

    inline int getInt(int col) 
    { return sqlite3_column_int(statement,col); }
    inline unsigned getUnsigned(int col) 
    { return (unsigned)sqlite3_column_int(statement,col); }
    inline double getDouble(int col) 
    { return sqlite3_column_double(statement,col); }
    inline char getChar(int col) 
    { return *(char*)sqlite3_column_text(statement,col); }
    inline const char * getString(int col) 
    { return (char*)sqlite3_column_text(statement,col); }
    inline bool getBool(int col) 
    { return sqlite3_column_int(statement,col); }

    using Database::get;

public:
    bool dumpTableDesc(const char * table, std::vector<char> * colTypes, std::vector<std::string>* colNames, std::string * priName, const std::vector<std::string> * omitColumns = NULL);

public:
    std::string path;

private:
    sqlite3 *db;
    sqlite3_stmt *statement;
};

}

#endif
