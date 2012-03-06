#pragma once

#include "mr_exports.h"
#include "mr_bufferedreader.h"
#include "mr_bufferedwriter.h"
#include "mr_except.h"
#include <stdarg.h>

namespace mrutils {

class _API_ Database {
public:
    inline virtual ~Database() {}

protected:
    inline Database() : errId(0), error(""), connected(false) {}
    inline Database(const Database&) : errId(0), error(""), connected(false) {}

public: // queries & results
    struct Row
    {
        Row()
        {
            m_data.reserve(16);
        }

        inline void clear()
        { m_data.clear(); }

        template<typename T>
        inline T get(size_t const col) const
                throw (mrutils::ExceptionNoSuchData)
        {
            if (m_data.size() <= col)
            { throw mrutils::ExceptionNoSuchData("Row: no such row past m_data"); }
            return T(m_data[col]);
        }

        template<typename T>
        inline Row & operator <<(T const &rhs)
        {
            m_data.push_back(rhs);
            return *this;
        }

        template<int N>
        inline Row & operator <<(char const (&rhs)[N])
        {
            m_data.push_back(rhs);
            return *this;
        }

        std::vector<std::string> m_data;
    };

    typedef fastdelegate::FastDelegate2<mrutils::BufferedReader&,
            mrutils::Database::Row const&,bool> finishInFunc;
    typedef fastdelegate::FastDelegate2<mrutils::BufferedWriter&,
            mrutils::Database::Row const&> finishOutFunc;

    virtual bool connect() = 0;
    virtual bool run(const char * q) = 0;
    virtual bool rr(const char * q) = 0;
    virtual bool nextLine() = 0;
    virtual bool insert(const char * q, int* lastInsertId) = 0;

    inline void runExcept(const char * q)
            throw (mrutils::ExceptionNoSuchData)
    {
        if (!run(q)) throw mrutils::ExceptionNoSuchData(error);
    }

    inline void rrExcep(const char * q)
            throw (mrutils::ExceptionNoSuchData)
    {
        if (!rr(q)) throw mrutils::ExceptionNoSuchData(error);
    }

    inline void get(int count, char type1, ...)
    {
        va_list list; va_start(list,type1);
        getHelper(count,type1,list);
    }

    inline bool get(const char * q, int count, char type1, ...)
    {
        if (!rr(q) || !nextLine())
            return false;
        va_list list; va_start(list,type1);
        getHelper(count,type1,list);
        for (;nextLine();) ; // clear to end of results
        return true;
    }

    virtual int getInt(int col) = 0;
    virtual unsigned getUnsigned(int col) = 0;
    virtual double getDouble(int col) = 0;
    virtual char getChar(int col) = 0;
    virtual const char * getString(int col) = 0;
    virtual bool getBool(int col) = 0;

    inline bool get(const char * q, int& out) 
    { return get(q,1,'d',&out); }
    inline bool get(const char * q, unsigned& out) 
    { return get(q,1,'u',&out); }
    inline bool get(const char * q, double& out) 
    { return get(q,1,'f',&out); }
    inline bool get(const char * q, char& out) 
    { return get(q,1,'c',&out); }
    inline bool get(const char * q, std::string& out) 
    { return get(q,1,'s',&out); }
    inline bool get(const char * q, const char * out) 
    { return get(q,1,'*',out); }
    inline bool get(const char * q, bool& out)
    { return get(q,1,'b',&out); }

   /**
    * Inline returns a type (int, long, unsigned, etc.)
    * Throws an exception if it can't get enough data
    */
    template<class T> inline T get(const char * q) throw (mrutils::ExceptionNoSuchData);

public:
   /*********************
    * Syncing
    *********************/

   /**
    * Takes a table dump formatted as below from reader 
    * and synchronizes the entries in the local table to match. 
    * This is a slow operation -- every column of every matching 
    * primary key is updated with a separate UPDATE call.
    *
    * The fact that the dump is sorted ASC by primary key is
    * assumed. 
    */
    virtual bool syncFromDump(mrutils::BufferedReader& reader,
            const char * table,
            finishInFunc finish = NULL)
            = 0;

   /**
    * Uses dump formatted as in dumpTableChangeById. only deletion
    * and addition of rows
    */
    virtual bool syncFromDumpChangeById(mrutils::BufferedReader& reader,
            const char * table,
            std::vector<int> * deletedIds = NULL,
            std::vector<int> * addedIds = NULL)
            = 0;

   /**
    * Only additions are performed. Used with dumpTableLastId
    */
    virtual bool syncFromDumpLastId(mrutils::BufferedReader& reader,
            const char * table,
            std::vector<int> * addedIds = NULL,
            finishInFunc finish = NULL)
            = 0;

   /**
    * Returns the new insert id
    */
    virtual int syncFromDumpInsert(mrutils::BufferedReader& reader,
            int key,
            const std::string& priName,
            const std::string& insertStr,
            const std::vector<char>& colTypes,
            const char * table,
            finishInFunc finish = NULL)
            throw (mrutils::ExceptionNoSuchData)
            = 0;

    virtual void syncFromDumpUpdate(mrutils::BufferedReader& reader,
            int key,
            const std::vector<std::string>& colNames,
            const std::vector<char>& colTypes,
            const char * table)
            throw (mrutils::ExceptionNoSuchData)
            = 0;

    virtual void syncFromDumpAppendString(mrutils::BufferedReader& reader,
            mrutils::stringstream& ss,
            mrutils::Database::Row *row = NULL)
            throw (mrutils::ExceptionNoSuchData)
            = 0;

   /**
    * Dumps the given table row by row.
    * Requires that the table has 1 unique primary key 
    * with integer type 
    *
    * Format as follows: 
    * char identifying column then \0-terminated string for
    * each column name then a \0 to terminate column names
    * then unsigned (starting with 1) for each 
    * row then a 0 unsigned to terminate
    *
    * types: d for int, f for float, s string, 
    * p is primary key (assumed int)
    *
    * strings: s, followed by length as uint64_t followed
    * by content null-terminated. length does NOT include 
    * null character
    *
    * the primary key is always the first column output
    *
    * the primary key can't be omitted
    */
    virtual bool dumpTable(mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL,
            const char * whereClause = NULL,
            finishOutFunc finish = NULL)
            = 0;

   /**
    * Similar to dumpTable except that a list of ids already present
    * is passed via the reader (terminated by -1); the function
    * first sends the primary key name then 
    * sends a list of ids to remove terminated by -1 then a table
    * dump similar to dumpTable for only the missing rows. 
    * The list of passed ids is assumed to be sorted.
    */
    virtual bool dumpTableChangeById(mrutils::BufferedReader& reader,
            mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL)
            = 0;

   /**
    * Assumes the reader's next entry is an id (integer) with the
    * latest known id. Dumps only rows after that id.
    * Calls finishfunc after each row has been output
    */
    virtual bool dumpTableFromLastId(mrutils::BufferedReader& reader,
            mrutils::BufferedWriter& out,
            const char * table,
            const std::vector<std::string> * omitColumns = NULL,
            const char * whereClause = NULL,
            finishOutFunc finish = NULL,
            int limit=0)
            = 0;

    static bool dumpTableReadCols(mrutils::BufferedReader& reader,
            std::vector<char> * colTypes,
            std::vector<std::string>* colNames,
            std::string * priName)
            throw (mrutils::ExceptionNoSuchData);

    virtual bool dumpTableDesc(const char * table,
            std::vector<char> * colTypes,
            std::vector<std::string>* colNames,
            std::string * priName,
            const std::vector<std::string> * omitColumns = NULL)
            = 0;

public:
    int errId;
    const char * error;
    bool connected;

protected:
    inline void getHelper(int count, char type1, va_list list)
    {
        for (int col = 0; count != 0; ++col,--count)
        {
            char type = col == 0 ? type1 : (char) va_arg(list,int);
            if (type == 0)
                break;

            switch (type)
            {
                case 'd':
                    *static_cast<int*>(va_arg(list, int*)) = getInt(col);
                    break;
                case 'u':
                    *static_cast<unsigned*>(va_arg(list, unsigned*)) = getUnsigned(col);
                    break;
                case 'f':
                    *static_cast<double*>(va_arg(list, double*)) = getDouble(col);
                    break;
                case 'c':
                    *static_cast<char*>(va_arg(list, char*)) = *getString(col);
                    break;
                case 's':
                    static_cast<std::string*>(va_arg(list, std::string*))->assign(getString(col));
                    break;
                case '*':
                    strcpy(static_cast<char*>(va_arg(list, char*)),getString(col));
                    break;
                case 'b':
                    *static_cast<int*>(va_arg(list, int*)) = getBool(col);
                    break;
            }
        }
    }
};

template<> inline int mrutils::Database::get<int>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    int ret;
    if (!get(q,1,'d',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}

template<> inline unsigned mrutils::Database::get<unsigned>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    unsigned ret;
    if (!get(q,1,'u',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}

template<> inline double mrutils::Database::get<double>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    double ret;
    if (!get(q,1,'f',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}

template<> inline char mrutils::Database::get<char>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    char ret;
    if (!get(q,1,'c',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}

template<> inline std::string mrutils::Database::get<std::string>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    std::string ret;
    if (!get(q,1,'s',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}

template<> inline bool mrutils::Database::get<bool>(const char * q) throw (mrutils::ExceptionNoSuchData) {
    bool ret;
    if (!get(q,1,'b',&ret)) throw mrutils::ExceptionNoSuchData(q);
    return ret;
}
template<>
inline int mrutils::Database::Row::get<int>(size_t const col) const
        throw (mrutils::ExceptionNoSuchData)
{
    if (m_data.size() <= col)
    { throw mrutils::ExceptionNoSuchData("Row: no such row past m_data"); }
    return atoi(m_data[col].c_str());
}
template<>
inline double mrutils::Database::Row::get<double>(size_t const col) const
        throw (mrutils::ExceptionNoSuchData)
{
    if (m_data.size() <= col)
    { throw mrutils::ExceptionNoSuchData("Row: no such row past m_data"); }
    return atof(m_data[col].c_str());
}

template<>
inline mrutils::Database::Row & mrutils::Database::Row::operator << <int>(int const &rhs)
{
    char buffer[12];
    mrutils::itoa(rhs, buffer);
    m_data.push_back(buffer);
    return *this;
}

template<>
inline mrutils::Database::Row & mrutils::Database::Row::operator << <double>(double const &rhs)
{
    char buffer[25];
    sprintf(buffer,"%.16e",rhs);
    m_data.push_back(buffer);
    return *this;
}
} // mrutils namespace
