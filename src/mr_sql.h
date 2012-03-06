#pragma once
#include "mr_db.h"

namespace mrutils {

class Sql : public Database {
public:
    inline virtual ~Sql() {}

protected:
    inline Sql() {}
    inline Sql(const Sql& o) : Database(o) {}

public: // convenience
    virtual Sql* clone() const = 0;
    virtual const char * autoIncrement() const = 0;


public: // receiving
    bool syncFromDump(mrutils::BufferedReader& reader,
            const char * table,
            finishInFunc finish = NULL);

    bool syncFromDumpChangeById(mrutils::BufferedReader& reader,
            const char * table,
            std::vector<int> * deletedIds = NULL,
            std::vector<int> * addedIds = NULL);

    bool syncFromDumpLastId(mrutils::BufferedReader& reader,
            const char * table,
            std::vector<int> * addedIds = NULL,
            finishInFunc finish = NULL);

    int syncFromDumpInsert(mrutils::BufferedReader& reader,
            int key,
            const std::string& priName,
            const std::string& insertStr,
            const std::vector<char>& colTypes,
            const char * table,
            finishInFunc finish = NULL)
            throw (mrutils::ExceptionNoSuchData);

    void syncFromDumpUpdate(mrutils::BufferedReader& reader,
            int key,
            const std::vector<std::string>& colNames,
            const std::vector<char>& colTypes,
            const char * table)
            throw (mrutils::ExceptionNoSuchData);

    void syncFromDumpAppendString(mrutils::BufferedReader& reader,
            mrutils::stringstream& ss,
            mrutils::Database::Row *row = NULL)
            throw (mrutils::ExceptionNoSuchData);

public: // sending
    bool dumpTable(mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL,
            const char * whereClause = NULL,
            finishOutFunc finish = NULL);

    bool dumpTableChangeById(mrutils::BufferedReader& reader,
            mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL);

    bool dumpTableFromLastId(mrutils::BufferedReader& reader,
            mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL,
            const char * whereClause = NULL,
            finishOutFunc finish = NULL,
            int limit=0);

    bool dumpTableDesc(const char * table,
            std::vector<char> * colTypes,
            std::vector<std::string>* colNames,
            std::string * priName,
            const std::vector<std::string> * omitColumns = NULL)
            = 0;

protected:
    bool dumpTableChangeByIdHelper(const std::vector<int>& sentIds,
            const std::vector<int>& localIds,
            const std::string& priName,
            mrutils::BufferedWriter& out,
            mrutils::stringstream& ss);

    void dumpTableSend(mrutils::BufferedWriter& out,
            const std::vector<char>& colTypes,
            const char * query,
            finishOutFunc finish = NULL)
            throw (mrutils::ExceptionNoSuchData);

protected:
    std::string errStr;
};

}
