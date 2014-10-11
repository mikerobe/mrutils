#ifndef _MR_CPPLIB_MYSQL_H
#define _MR_CPPLIB_MYSQL_H

#ifdef HAVE_PANTHEIOS
	#include <pantheios/pantheios.hpp>
	#include <pantheios/inserters.hpp>
#endif

#include "mr_exports.h"
#include "mr_threads.h"
#include "mr_sockets.h"
#include "mr_files.h"
#include "mr_bufferedreader.h"
#include "mr_bufferedwriter.h"
#include "mr_mysql_wrapper.h"
#include "mr_sql.h"

namespace mrutils {

class _API_ Mysql : public Sql {
public:
    inline Mysql(const char * server="localhost", uint16_t port = 3306
     ,const char * username = "simulate", const char * password = "simulate"
     ,const char * db = "securities",mrutils::Socket::type_t socketType = mrutils::Socket::SOCKET_STREAM)
    : rowNumber(-1), server(server), port(port)
     ,username(username), password(password), db(db)
     ,mutex(mrutils::mutexCreate())
     ,socket(NULL), socketType(socketType)
     ,col(0)
    {
        reader.setSocketWaitTime(60);
    }

    inline Mysql(const Mysql& other)
    : Sql(other), rowNumber(-1), server(other.server), port(other.port)
     ,username(other.username), password(other.password), db(other.db)
     ,mutex(mrutils::mutexCreate())
     ,socket(NULL), socketType(other.socketType)
     ,col(0)
    {
        reader.setSocketWaitTime(60);
    }

    inline ~Mysql() {
        if (connected) {
            query.type = mrutils::mysql::COM_QUIT;
            query.send(writer);
        }

        mrutils::mutexDelete(mutex);

        if (socket != NULL) delete socket;
    }

    inline Mysql * clone() const { return new Mysql(*this); }
    inline const char * autoIncrement() const { return "auto_increment"; }

public:
    void setServer(const char * server, mrutils::Socket::type_t socketType = mrutils::Socket::SOCKET_STREAM) {
        this->server = server;
        this->socketType = socketType;
        if (socket != NULL) delete socket;
        connected = false;
    }

   /**
    * This will write all mysql commands to a file. Pass false 
    */
    bool streamTo(const char * path, bool allCmds = true);

public:
   /*********************
    * Queries & results
    *********************/
    bool connect();

    bool run(const char * q);

    inline bool rr(const char * q) {
        if (!run_(q)) return false;

        rowNumber = 0;
        return true;
    }

    bool nextLine();

    inline void get(int count, ...) {
        va_list list; va_start(list,count);

        for (int col = 0; count-- != 0; ++col) {
            char type = (char) va_arg(list,int);
            if (type == 0) break;

            switch (type) {
                case 'd':
                    *static_cast<int*>(va_arg(list, int*)) = atoi(results[col].c_str());
                    break;
                case 'u':
                    *static_cast<unsigned*>(va_arg(list, unsigned*)) = strtoul(results[col].c_str(),NULL,0);
                    break;
                case 'f':
                    *static_cast<double*>(va_arg(list, double*)) = atof(results[col].c_str());
                    break;
                case 'c':
                    *static_cast<char*>(va_arg(list, char*)) = *results[col].c_str();
                    break;
                case 's':
                    static_cast<std::string*>(va_arg(list, std::string*))->assign(results[col].c_str());
                    break;
                case '*':
                    strcpy(static_cast<char*>(va_arg(list, char*)),results[col].c_str());
                    break;
                case 'b':
                    *static_cast<int*>(va_arg(list, int*)) = ::tolower(*results[col].c_str()) == 't';
                    break;
            }
        }
    }

    bool get(const char * q, int count, char type_, ...);

    inline bool insert(const char * q, int* lastInsertId) {
        // clear old data
        memset(&reply,0,sizeof(mrutils::mysql::mysql_reply));

        if (!run_(q) || !reply.read(reader)) return false;

        switch (reply.type) {
            case mrutils::mysql::MT_OK:
                if (reply.data.ok.affectedRows != 1) {
                    errId = 1;
                    error = "Error: got an OK packet with no affected rows on insert.";
                    return false;
                }

                *lastInsertId = (int)reply.data.ok.insertId;
                break;

            case mrutils::mysql::MT_ERR:
                errId = reply.data.err.errId;
                errStr = reply.data.err.message;
                error = errStr.c_str();
                return false;

            default:
                errId = -1;
                errStr = "Got unexpected message response from insert... ";
                errStr.append(q);
                error = errStr.c_str();
                return false;
        } return true;
    }

    inline int getInt(int col)
    { return atoi(results[col].c_str()); }
    inline unsigned getUnsigned(int col) 
    { return strtoul(results[col].c_str(),NULL,0); }
    inline double getDouble(int col) 
    { return atof(results[col].c_str()); }
    inline char getChar(int col) 
    { return *results[col].c_str(); }
    inline const char * getString(int col) 
    { return results[col].c_str(); }
    inline bool getBool(int col) 
    { return (::tolower(*results[col].c_str()) == 't'); }

    using Database::get;

public:
    inline void lock() { mrutils::mutexAcquire(mutex); }
    inline void unlock() { mrutils::mutexRelease(mutex); }
    bool dumpTableDesc(const char * table, std::vector<char> * colTypes, std::vector<std::string>* colNames, std::string * priName, const std::vector<std::string> * omitColumns = NULL);


public:
    std::vector<std::string> results;

    int rowNumber;
    std::string server; short port;
    std::string username, password, db;
    mrutils::mutex_t mutex;

private:
    inline bool testConnection() {
        query.type = mrutils::mysql::COM_PING;
        query.send(writer);

        if (!reply.read(reader))
        {
			#ifdef HAVE_PANTHEIOS
            pantheios::log(pantheios::notice,
                    "Mysql reconnecting to lost connection (dropped ping)...");
			#endif
            // then re-connect
            return connect();
        }

        return true;
    }

    bool run_(const char * q);
private:
    mrutils::Socket *socket;
    mrutils::Socket::type_t socketType;
    mrutils::BufferedReader reader;
    mrutils::BufferedWriter writer;

    mrutils::mysql::mysql_query query;
    mrutils::mysql::mysql_reply reply;
    int col;
};
}
#endif
