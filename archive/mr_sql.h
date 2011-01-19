#ifndef _MR_SQL_H
#define _MR_SQL_H

#include "mr_exports.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #include <winsock.h>
#endif

#include <iostream>
#include <string>
#include "mysql.h"
#include <stdarg.h>
#include <stdio.h>
#include "mr_threads.h"

namespace mrutils {

class _API_ SqlConnect {
    public:
        static MYSQL mysql;
        static bool init_; 
        static bool connected_;
        static bool failed_;
        static MUTEX sqlMutex;

        static std::string server;
        static std::string username;
        static std::string password;

        inline static bool connect(MYSQL* m) {
            return (mysql_real_connect(m, 
                server.c_str(), username.c_str(), password.c_str(),
                "securities",0,0,0) != NULL);
        }

        inline static bool init() {
            if (!init_) {
                mrutils::mutexAcquire(sqlMutex);
                if (!init_) {
                    mysql_init(&SqlConnect::mysql);
                    init_ = true;
                }
                mrutils::mutexRelease(sqlMutex);
            }

            if (!connected_ && !failed_) {
                mrutils::mutexAcquire(sqlMutex);
                if (!connected_) {
                    connected_ = connect(&SqlConnect::mysql);
                    if (!connected_) {
                        fprintf(stderr, "MySQL Failed to connect: %s\n",
                                mysql_error(&SqlConnect::mysql));
                        failed_ = true;
                    }
                }
                mrutils::mutexRelease(sqlMutex);
            }

            return connected_;
        }

        inline static bool MR_SQL_CHECK() {
            if (!connected_ && !init()) return false;
            return true;
        }
        template <class T>
        inline static T MR_SQL_RETURN(T r) {
            mrutils::mutexRelease(sqlMutex);
            return r;
        }

        inline static bool MR_SQL_TRY_Q(MYSQL* c, const char * q) {
            // lock before executing query... 
            //whoever is using must release!
            mrutils::mutexAcquire(sqlMutex);
            if (mysql_query(c, q) != 0) {
                std::cerr << mysql_error(c) << std::endl;
                return false;
            } else return true;
        }

        inline static bool MR_SQL_NEXT(MYSQL_RES *res, MYSQL_ROW &row) {
            return (NULL != res && NULL != (row = mysql_fetch_row(res)));
        }

    public:

        /**
         * Multiple get: setup is q = query, 
         * followed by a list of args
         * type, pointer to element, type, ptr, type, ptr,...
         * type is one of (char) d, f, s, c
         * 
         * If a negative count is provided, then the list MUST
         * be terminated with NULL, 0 or '\0'
         * */
        inline static bool get(const char * q, int count, char type, ...) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            MYSQL_RES *res = mysql_use_result(&mysql);
            MYSQL_ROW row; bool got;
            if (got = MR_SQL_NEXT(res, row)) {
                va_list list;
                va_start(list,type);

                int col = 0; 
                while ( count-- != 0 && type ) {
                    switch (type) {
                        case 'd':
                            *static_cast<int*>(va_arg(list, int*)) = atoi(row[col++]);
                            break;
                        case 'f':
                            *static_cast<double*>(va_arg(list, double*)) = atof(row[col++]);
                            break;
                        case 's':
                            static_cast<std::string*>(va_arg(list, std::string*))->assign(row[col++]);
                            break;
                        case 'c':
                            *static_cast<char*>(va_arg(list, char*)) = *row[col++];
                            break;
                    }

                    type = (char) va_arg(list,int);
                }

                va_end(list);
            }
            mysql_free_result(res);
            return MR_SQL_RETURN(got);
        }


        inline static bool get(const char * q, int & out) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            MYSQL_RES *res = mysql_use_result(&mysql);
            MYSQL_ROW row; bool got;
            if (got = MR_SQL_NEXT(res, row)) out = atoi(row[0]);
            mysql_free_result(res);
            return MR_SQL_RETURN(got);
        }
        inline static bool get(const char * q, double & out) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            MYSQL_RES *res = mysql_use_result(&mysql);
            MYSQL_ROW row; bool got;
            if (got = MR_SQL_NEXT(res, row)) out = atof(row[0]);
            mysql_free_result(res);
            return MR_SQL_RETURN(got);
        }
        inline static bool get(const char * q, std::string & out) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            MYSQL_RES *res = mysql_use_result(&mysql);
            MYSQL_ROW row; bool got;
            if (got = MR_SQL_NEXT(res, row)) out.assign(row[0]);
            mysql_free_result(res);
            return MR_SQL_RETURN(got);
        }
        inline static bool get(const char * q, char * out, int size) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            MYSQL_RES *res = mysql_use_result(&mysql);
            MYSQL_ROW row; bool got;
            if (got = MR_SQL_NEXT(res, row)) ::strncpy(out,row[0],size);
            mysql_free_result(res);
            return MR_SQL_RETURN(got);
        }
        
        inline static bool run(const char * q) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            return MR_SQL_RETURN(true);
        }

        inline static bool insert(const char * q, int * lastInsertId) {
            if (!MR_SQL_CHECK() || !MR_SQL_TRY_Q(&mysql,q)) 
                return MR_SQL_RETURN(false);
            *lastInsertId = mysql_insert_id(&mysql);
            return MR_SQL_RETURN(true);
        }

        class _API_ ResultReader {
            public:
                ResultReader(const char * q) 
                { ok = start(q); }

                ~ResultReader() {
                    if (ok) mysql_free_result(res);
                    mysql_close(&mysql);
                }

                inline bool nextLine() {
                    return (ok && MR_SQL_NEXT(res, row));
                }

                inline const char * getString(int c) { return row[c]; }
                inline double getDouble(int c) { return atof(row[c]); }
                inline int getInt(int c) { return atoi(row[c]); }
            private:
                ResultReader(const ResultReader &r);
                ResultReader& operator = (const ResultReader &r);

                inline bool start(const char * q) {
                    mysql_init(&mysql);
                    if (!connect(&mysql)) return false;
                    if (mysql_query(&mysql, q) != 0) {
                        std::cerr << mysql_error(&mysql) << std::endl;
                        return false;
                    } 
                    res = mysql_use_result(&mysql);
                    return true;
                }

            private:
                bool ok;
                MYSQL_RES *res;
                MYSQL_ROW row; 
                MYSQL mysql;
        };
};
}


#endif
