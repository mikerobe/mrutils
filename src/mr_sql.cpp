#include "mr_sql.h"

#define SYNC_READ(n) \
if (n != reader.read(n)) { \
    errId = -1; \
    ss.clear() << __FILE__ << ": " << __LINE__ \
        << " mrutils::Sql syncFromDump, content ended early.";\
    errStr = ss.str(); error = errStr.c_str();\
    return false; \
}
#define SYNC_LINE \
if (!reader.nextLine()) { \
    errId = -1; \
    ss.clear() << __FILE__ << ": " << __LINE__ \
        << " mrutils::Sql syncFromDump, content ended early.";\
    errStr = ss.str(); error = errStr.c_str();\
    return false; \
}

void mrutils::Sql::syncFromDumpAppendString(mrutils::BufferedReader& reader, mrutils::stringstream& ss) throw (mrutils::ExceptionNoSuchData) {
    uint64_t strlen = reader.get<uint64_t>();
    const uint64_t bufsz = (uint64_t) reader.bufSize;

    ss << '"';

    for (; strlen > 0ull; ) {
        const int len = strlen > bufsz ? reader.bufSize : (int)strlen;
        const int r = reader.read(len);
        if (r <= 0) throw mrutils::ExceptionNoSuchData("syncFromDumpAppendString ran out of data");
        strlen -= (uint64_t)r;
        mrutils::replaceCopyQuote(ss,reader.line,r);
    }

    // read final \0 character
    reader.get<char>();

    ss << '"';
}

int mrutils::Sql::syncFromDumpInsert(mrutils::BufferedReader& reader, int key, const std::string& priName, const std::string& insertStr, const std::vector<char>& colTypes, _UNUSED const char * table, fastdelegate::FastDelegate2<mrutils::BufferedReader&,int,bool> finishFunc) throw (mrutils::ExceptionNoSuchData) {
    mrutils::stringstream ss;
    ss << insertStr;
    ss << key;

    for (std::vector<char>::const_iterator it = colTypes.begin()+1; it != colTypes.end(); ++it) {
        ss << ',';
        switch (*it) {
            case 'd':
                ss << reader.get<int>();
                break;
            case 'f':
                ss << reader.get<double>();
                break;
            case 's': 
                syncFromDumpAppendString(reader,ss);
                break;
            default:
                errId = -1;
                ss.clear() << "syncFromDumpInsert unhandled type" << *it;
                errStr = ss.str(); error = errStr.c_str();
                throw mrutils::ExceptionNoSuchData(ss);
        }
    }

    int insertId = 0;

    ss << ")";
    if (!insert(ss.str().c_str(),&insertId)) {
        std::cerr << "SQL Error (" << error << ") query: " << ss;
    }

    if (finishFunc != NULL && !finishFunc(reader,key)) {
        // roll back insertion
        run(ss.clear() << "DELETE FROM " << table << " WHERE " << priName << "=" << key << " LIMIT 1");
        return -1;
    }

    return insertId;
}

void mrutils::Sql::syncFromDumpUpdate(mrutils::BufferedReader& reader, int key, const std::vector<std::string>& colNames, const std::vector<char>& colTypes, const char * table) throw (mrutils::ExceptionNoSuchData) {
    mrutils::stringstream ss;
    ss << "UPDATE " << table << " SET ";

    for (unsigned i = 1; i < colNames.size(); ++i) {
        if (i > 1) ss << ',';
        ss << '`' << colNames[i] << "`=";
        switch (colTypes[i]) {
            case 'd':
                ss << reader.get<int>();
                break;
            case 'f':
                ss << reader.get<double>();
                break;
            case 's':
                syncFromDumpAppendString(reader,ss);
                break;
            default:
                errId = -1;
                ss.clear() << "syncFromDumpInsert unhandled type" << colTypes[i];
                errStr = ss.str(); error = errStr.c_str();
                throw mrutils::ExceptionNoSuchData(ss);
        }
    }

    ss << " WHERE `" << colNames[0] << "`=" << key;
    if (!run(ss.str().c_str())) {
        mrutils::stringstream ss2; ss2 << "SQL Error. query:\n" << ss;
        throw mrutils::ExceptionNoSuchData(ss2);
    }
}

bool mrutils::Sql::syncFromDump(mrutils::BufferedReader& reader, char const * table, fastdelegate::FastDelegate2<mrutils::BufferedReader&,int,bool> finishFunc) {
    mrutils::stringstream ss;

    try {
        std::string priName;
        std::vector<char> colTypes; std::vector<std::string> colNames;

        if (!dumpTableReadCols(reader,&colTypes,&colNames,&priName)) return false;

        std::string insertStr;
        ss.clear() << "INSERT INTO " << table << " (`" << colNames[0] << '`';
        for (unsigned i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << ") VALUES (";
        insertStr = ss.str();

        // get localIds list, sorted
        std::vector<int> localIds;
        rr(ss.clear() << "SELECT `" << priName << "` FROM " << table << " ORDER BY `" << priName << "` ASC");
        while (nextLine()) localIds.push_back(getInt(0));

        for (std::vector<int>::iterator it = localIds.begin();;) {
            unsigned row = reader.get<unsigned>();
            if (row == 0u) {
                // delete all remaining locals
                for (;it != localIds.end();++it) {
                    run(ss.clear() << "DELETE FROM " << table << " WHERE `"
                        << priName << "`=" << *it);
                }
                return true;
            }

            int key = reader.get<int>();

            for (;;) {
                for (;;) {
                    if (it == localIds.end()) {
                        // insert remaining keys
                        for (;;) {
                            if (0 > syncFromDumpInsert(reader,key,priName,insertStr,colTypes,table,finishFunc)) return false;
                            row = reader.get<unsigned>();
                            if (row == 0u) return true;
                            key = reader.get<int>();
                        }
                        return true;
                    }
                    if (*it < key) {
                        run(ss.clear() << "DELETE FROM " << table << " WHERE `"
                            << priName << "`=" << *it);
                        ++it;
                    } else break;
                }

                for (;;) {
                    if (key < *it) {
                        // insert this key
                        if (0 > syncFromDumpInsert(reader,key,priName,insertStr,colTypes,table,finishFunc)) return false;

                        row = reader.get<unsigned>();
                        if (row == 0u) {
                            // delete all remaining locals
                            for (;it != localIds.end();++it) {
                                run(ss.clear() << "DELETE FROM " << table << " WHERE `"
                                    << priName << "`=" << *it);
                            }
                            return true;
                        }
                        key = reader.get<int>();
                    } else break;
                }

                if (key == *it) {
                    // update local row
                    syncFromDumpUpdate(reader,key,colNames,colTypes,table);
                    ++it;
                    break;
                }
            }
        }

        // unreachable
        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1;
        ss.clear() << __FILE__ << ": " << __LINE__ 
            << " mrutils::Sql syncFromDump on table " << table << ", missing data in dump"
            << e.what()
            << " error: " << error;
        errStr = ss.str(); error = errStr.c_str();
        return false;
    }
}

bool mrutils::Sql::syncFromDumpLastId(mrutils::BufferedReader& reader, const char * table, std::vector<int> * addedIds, fastdelegate::FastDelegate2<mrutils::BufferedReader&,int,bool> finishFunc) {
    mrutils::stringstream ss;

    try {
        error = "";

        std::vector<char> colTypes; std::vector<std::string> colNames; std::string priName;
        if (!dumpTableReadCols(reader,&colTypes,&colNames,&priName)) return false;

        std::string insertStr;
        ss.clear() << "INSERT INTO " << table << " (`" << colNames[0] << '`';
        for (size_t i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << ") VALUES (";
        insertStr = ss.str();

        for (unsigned row; 0u != (row = reader.get<unsigned>()); ) {
            const int id = reader.get<int>();
            if (addedIds != NULL) addedIds->push_back(id);
            if (0 > syncFromDumpInsert(reader,id,priName,insertStr,colTypes,table,finishFunc)) return false;
        }

        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1;
        ss.clear() << __FILE__ << ": " << __LINE__ 
            << " mrutils::Sql syncFromDumpLastId, missing data in dump "
            << e.what()
            << " error: " << error;
        errStr = ss.str(); error = errStr.c_str();
        return false;
    }
}

bool mrutils::Sql::syncFromDumpChangeById(mrutils::BufferedReader& reader, const char * table, std::vector<int> * deletedIds, std::vector<int> * addedIds) {
    mrutils::stringstream ss;

    try {
        std::string priName = reader.getLine();

        // first, delete all the ids sitting in reader
        for (;;) {
            const int id = reader.get<int>(); if (id == -1) break;
            if (deletedIds != NULL) deletedIds->push_back(id);
            if (!run(ss.clear() << "DELETE FROM " << table << " WHERE `" << priName << "`=" << id)) 
                return false;
        }

        std::vector<char> colTypes; 
        std::vector<std::string> colNames;

        if (!dumpTableReadCols(reader,&colTypes,&colNames,&priName)) return false;

        std::string insertStr;
        ss.clear() << "INSERT INTO " << table << " (`" << colNames[0] << '`';
        for (size_t i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << ") VALUES (";
        insertStr = ss.str();

        for (unsigned row; 0u != (row = reader.get<unsigned>()); ) {
            const int id = reader.get<int>();
            if (addedIds != NULL) addedIds->push_back(id);
            if (0 > syncFromDumpInsert(reader,id,priName,insertStr,colTypes,table)) return false;
        }

        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1;
        ss.clear() << __FILE__ << ": " << __LINE__ 
            << " mrutils::Sql syncFromDumpChangeById, missing data in dump"
            << e.what()
            << " error: " << error;
        errStr = ss.str(); error = errStr.c_str();
        return false;
    }
}

bool mrutils::Sql::dumpTable(mrutils::BufferedWriter& out, const char * table, const std::vector<std::string> * omitColumns, const char * whereClause, fastdelegate::FastDelegate2<mrutils::BufferedWriter&,int> finishFunc) {
    try {
        mrutils::stringstream ss;

        std::vector<char> colTypes;
        std::vector<std::string> colNames;
        std::string priName;
        if (!dumpTableDesc(table,&colTypes,&colNames,&priName,omitColumns)) return false;

        // output column row in dump 
        for (unsigned i = 0; i < colNames.size(); ++i) out << colTypes[i] << colNames[i];
        out << '\0';

        // build SELECT query
        ss.clear() << "SELECT `" << colNames[0] << '`';
        for (unsigned i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << " FROM " << table;
        if (whereClause != NULL) ss << " WHERE " << whereClause;
        ss << " ORDER BY `" << priName << "` ASC";

        // send result
        dumpTableSend(out,colTypes,ss.c_str(), finishFunc);

        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1;
        errStr = e.what();
        error = errStr.c_str();
        return false;
    }
}

bool mrutils::Sql::dumpTableChangeById(mrutils::BufferedReader& reader, mrutils::BufferedWriter& out, const char * table, const std::vector<std::string> * omitColumns) {
    try {
        mrutils::stringstream ss;

        std::vector<int> sentIds;
        for (;;) {
            const int id = reader.get<int>(); if (id == -1) break;
            sentIds.push_back(id);
        }

        std::vector<char> colTypes;
        std::vector<std::string> colNames;
        std::string priName;
        if (!dumpTableDesc(table,&colTypes,&colNames,&priName,omitColumns)) return false;

        std::vector<int> localIds;
        rr(ss.clear() << "SELECT `" << priName << "` FROM " << table << " ORDER BY `" << priName << "` ASC");
        while (nextLine()) localIds.push_back(getInt(0));

        // build SELECT query
        ss.clear() << "SELECT `" << colNames[0] << '`';
        for (unsigned i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << " FROM " << table << " WHERE `" <<  priName << '`';

        // send delete IDs & build inclusion list in ss
        out << priName;
        const bool newRows = dumpTableChangeByIdHelper(sentIds,localIds,priName,out,ss);
        out << -1;

        // output column row in dump 
        for (unsigned i = 0; i < colNames.size(); ++i) {
            out << colTypes[i] << colNames[i];
        } out << "";

        if (!newRows) {
            out << 0u;
        } else {
            // send result
            ss << " ORDER BY `" << priName << "` ASC";
            dumpTableSend(out,colTypes,ss.c_str());
        }

        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1;
        errStr = e.what();
        error = errStr.c_str();
        return false;
    }
}

// TODO speed there's an unnecessary comparison in this algo...
bool mrutils::Sql::dumpTableFromLastId(mrutils::BufferedReader& reader, mrutils::BufferedWriter& out, const char * table, const std::vector<std::string> * omitColumns, const char * whereClause, fastdelegate::FastDelegate2<mrutils::BufferedWriter&,int> finishFunc) {
    try {
        mrutils::stringstream ss;

        const int lastId = reader.get<int>();

        std::vector<char> colTypes; std::vector<std::string> colNames; std::string priName;
        if (!dumpTableDesc(table,&colTypes,&colNames,&priName,omitColumns)) return false;

        // output column row in dump 
        for (unsigned i = 0; i < colNames.size(); ++i) out << colTypes[i] << colNames[i];
        out << '\0';

        // build SELECT query
        ss.clear() << "SELECT `" << colNames[0] << '`';
        for (unsigned i = 1; i < colNames.size(); ++i) ss << ",`" << colNames[i] << '`';
        ss << " FROM " << table << " WHERE " << priName << " > " << lastId;
        if (whereClause != NULL) ss << " AND " << whereClause;
        ss << " ORDER BY `" << priName << "` ASC";

        // send result
        dumpTableSend(out,colTypes,ss.c_str(),finishFunc);

        return true;
    } catch (const mrutils::ExceptionNoSuchData& e) {
        errId = -1; errStr = e.what(); error = errStr.c_str();
        return false;
    }
}

bool mrutils::Sql::dumpTableChangeByIdHelper(const std::vector<int>& sentIds, const std::vector<int>& localIds, const std::string& priName, mrutils::BufferedWriter& out, mrutils::stringstream& ss) {
    unsigned inCount = 0;

    for (std::vector<int>::const_iterator itSent = sentIds.begin(), itLocal = localIds.begin()
        ;;++itSent, ++itLocal) {

        if (itLocal != localIds.end()) {
            for (;;) {
                if (itSent == sentIds.end()) {
                    // add remaining local to list
                    if (inCount > 0) ss << ") OR `" << priName << '`';
                    ss << " >= " << *itLocal;
                    return true;
                }
                if (*itSent < *itLocal) {
                    // delete this id
                    out << *itSent;
                    ++itSent;
                } else break;
            }
        }

        for (;;) {
            if (itLocal == localIds.end()) {
                // delete all remaining in sent
                for (;itSent != sentIds.end();++itSent) out << *itSent;
                return (inCount != 0);
            }
            if (*itLocal < *itSent) {
                // add itLocal to in list
                if (inCount++ != 0) ss << ',';
                else ss << " IN (";
                ss << *itLocal;
                ++itLocal;
            } else break;
        }
    }

    // unreachable
    return true;
}

void mrutils::Sql::dumpTableSend(mrutils::BufferedWriter& out, const std::vector<char>& colTypes, const char * query
    ,fastdelegate::FastDelegate2<mrutils::BufferedWriter&,int> finishFunc) throw (mrutils::ExceptionNoSuchData) {
    rr(query);

    // fetch ALL of the results into memory, then transfer over the
    // network otherwise, this will lock out writes to the database for the
    // duration of the transfer
    out.setNoAutoFlush();

    for (unsigned lineNumber = 1; nextLine(); ++lineNumber) {
        out << lineNumber;

        int id = 0;

        for (int i = 0; i < (int)colTypes.size(); ++i) {
            switch (colTypes[i]) {
                case 'd': 
                    out << getInt(i);
                    break;
                case 'p':
                    out << (id = getInt(i));
                    break;
                case 'f':
                    out << getDouble(i);
                    break;
                case 's':
                default: {
                    const int len = strlen(getString(i));
                    out << (uint64_t)len;
                    out.write(getString(i),len+1);
                    break;
                }
            }
        }

        if (finishFunc != NULL) finishFunc(out,id);
    }

    out << 0u;

    out.setNoAutoFlush(false);
}
