#include "mr_mysql.h"
#ifdef HAVE_PANTHEIOS
	#include <pantheios/pantheios.hpp>
#endif
//#define MR_SQL_DEBUG_2

bool mrutils::Mysql::run_(const char * q) {
    if (connected) {
        if (rowNumber >= 0) { while (nextLine()){} }
    } else if (!connect()) {
        errId = -1; error = "Can't connect to server";
        return false;
    }

	#ifdef HAVE_PANTHEIOS
    pantheios::logprintf(pantheios::debug,
        "SQL %d: rowNumber %d query.send(%s)\n",socket->s_,rowNumber,q);
	#endif

    query.query = q;
    query.type  = mrutils::mysql::COM_QUERY;

    if (!query.send(writer) && !(connect() && query.send(writer))) {
        errId = -1; error = "Got disconnected sending query";
        connected = false;
        return false;
    }

    rowNumber = -1;
    return true;
}

bool mrutils::Mysql::connect() {
    if (socket != NULL)
    {
		#ifdef HAVE_PANTHEIOS
        pantheios::logprintf(pantheios::debug,
                "Mysql %d is reconnecting...\n", socket->s_);
		#endif
        delete socket, socket = NULL;
    } else
    {
		#ifdef HAVE_PANTHEIOS
        pantheios::logprintf(pantheios::debug,
                "New mysql is connecting...\n");
		#endif

    }

    socket = new mrutils::Socket(server.c_str(),port,socketType);

    if (!socket->initClient(10)) {
		#ifdef HAVE_PANTHEIOS
        pantheios::logprintf(pantheios::error,
                "MySql unable to connect to server at %s:%d\n",
                server.c_str(),(int)port);
		#endif
        return false;
    }

    reader.use(*socket);
    writer.setFD(socket->s_);

    // receive handshake
    mrutils::mysql::mysql_handshake handshake;
    if (!handshake.read(reader)) return false;

    // now build login
    mrutils::mysql::mysql_login login;

    // calculate hashes
    char scrambledReply[SHA1_HASH_SIZE]; // no null at end
    mrutils::mysql::scramble(scrambledReply,handshake.scrambleBuf, password.c_str());

    login.user = username.c_str();
    login.scramblePass = scrambledReply;
    login.databaseName = db.c_str();

    // send login
    if (!login.send(writer))
        return false;

    if (!reply.read(reader)) return false;
    switch (reply.type) {
        case mrutils::mysql::MT_ERR:
            errId = reply.data.err.errId;
            errStr = reply.data.err.message;
            error = errStr.c_str();
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::error,
                    "MySql: login refused at %s:%d, username %s. %d:%s\n",
                    server.c_str(),(int)port,username.c_str(),errId,
                    reply.data.err.message);
			#endif
            return false;
        case mrutils::mysql::MT_OK:
            break;
        default:
            errId = -1;
            error = "Got unexpected response to login";
            return false;
    }

    rowNumber = -1;
    return (connected = true);
}

bool mrutils::Mysql::nextLine() {
    results.clear();

    if (rowNumber < 0) return false;

    if (rowNumber == 0) {
        // clear old data
        memset(&reply,0,sizeof(mrutils::mysql::mysql_reply));

        // skip col description header
        for (;;) {
            if (!reply.read(reader)) {
                errId = -1; error = "Got disconnected reading results";
                rowNumber = -1;
                return false;
            }

            #ifdef MR_SQL_DEBUG_2
				#ifdef HAVE_PANTHEIOS
                pantheios::logprintf(pantheios::debug,
                        "SQL %d result header: ", socket->s_);
                reply.log(pantheios::debug);
				#endif
            #endif

            switch (reply.type) {
                case mrutils::mysql::MT_OK:
                    // end of set -- no results
                    //rowNumber = -1;
                    //return false;
                    //skip OK messages
                    break;

                case mrutils::mysql::MT_ERR:
                    errId = reply.data.err.errId;
                    errStr = reply.data.err.message;
                    error = errStr.c_str();
                    rowNumber = -1;
                    return false;

                case mrutils::mysql::MT_RESULT:
                    // col description
                    continue;

                case mrutils::mysql::MT_EOF:
                    // done with header
                    reply.data.result.stage = 2;
                    break;
            }
            break;
        }
    }

    for (;;) {
        if (!reply.read(reader)) {
            errId = -1; error = "Got disconnected reading results";
            rowNumber = -1;
            return false;
        }

        #ifdef MR_SQL_DEBUG_2
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::debug,
                    "SQL %d result row %d: ", socket->s_, rowNumber);
            reply.log(pantheios::debug);
			#endif
        #endif

        switch (reply.type) {
            case mrutils::mysql::MT_ERR:
                errId = reply.data.err.errId;
                errStr = reply.data.err.message;
                error = errStr.c_str();
                rowNumber = -1;
                return false;
            case mrutils::mysql::MT_OK:
                break; // loop again -- shouldn't get OK here. only OK at beginning if no results
            case mrutils::mysql::MT_EOF:
                rowNumber = -1;
                return false;
            default:
                if (!mrutils::mysql::parseRow(reader,reply,results)) {
                    errId = -1; error = "failed on parseRow";
                    rowNumber = -1;
                    return false;
                }

                ++rowNumber;
                if (results.empty()) continue;
                else return true;
        }
    }

    rowNumber = -1;
    return false;
}

bool mrutils::Mysql::get(const char * q, int count, char type_, ...) {
    if (!run_(q)) return false;
    
    // clear any old data
    memset(&reply,0,sizeof(reply));
    results.clear();

    bool setResult = false;

    va_list list;
    va_start(list,type_);

    // read results
    for (int hit = 0;hit < 2;) {
        if (!reply.read(reader)) {
            errId = -1; error = "Got disconnected reading results";
            return false;
        }

        #ifdef MR_SQL_DEBUG_2
			#ifdef HAVE_PANTHEIOS
            pantheios::logprintf(pantheios::debug,
                    "SQL %d response to get: ", socket->s_);
            reply.log(pantheios::debug);
			#endif
        #endif

        switch (reply.type) {
            case mrutils::mysql::MT_OK:
                //hit = 2;
                break;

            case mrutils::mysql::MT_EOF:
                ++hit; reply.data.result.stage = 2;
                break;

            case mrutils::mysql::MT_ERR:
                errId = reply.data.err.errId;
                errStr = "Got error packet, message: ";
                errStr.append(reply.data.err.message);
                error = errStr.c_str();
                return false;

            case mrutils::mysql::MT_RESULT:
                if (reply.data.result.stage != 2) continue;
                if (!setResult) {
                    if (!mrutils::mysql::parseRow(reader,reply,results)) {
                        errId = -1;
                        error = "Got disconnected while reading result row";
                        return false;
                    }
                    if (results.size() == 0) continue;
                    setResult = true;

                    for (int col = 0; count-- != 0; ++col) {
                        char type = (col==0?type_:(char) va_arg(list,int));
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
                            case '*':
                                strcpy(static_cast<char*>(va_arg(list, char*)),results[col].c_str());
                                break;
                            case 's':
                                static_cast<std::string*>(va_arg(list, std::string*))->assign(results[col].c_str());
                                break;
                            case 'b':
                                *static_cast<int*>(va_arg(list, int*)) = ::tolower(*results[col].c_str()) == 't';
                                break;
                            default:
                                // unknown type
                                va_arg(list, void*);
                                break;
                        }
                    }
                } else {
                    // skip rows past the first result
                    if (!reader.skip(reply.header.packetLength - 1)) {
                        errId = -1;
                        error = "Got disconnected while skipping results past the first.";
                        return false;
                    }
                }
                break;
        }
    }

    //va_end(list);
    return setResult;
}

bool mrutils::Mysql::run(const char * q) {
    if (!run_(q)) return false;

    // read result packet
    if (!reply.read(reader)) {
        errId = -1; error = "Got disconnected reading result";
        connected = false;
        return false;
    }

    switch (reply.type) {
        case mrutils::mysql::MT_OK: return true;
        case mrutils::mysql::MT_ERR:
            errId = reply.data.err.errId;
            error = reply.data.err.message;
            return false;
        default: break;
    }

    errId = -1; error = "Got unexpected response to query";
    return false;
}

bool mrutils::Mysql::dumpTableDesc(const char * table,
        std::vector<char> * colTypes,
        std::vector<std::string>* colNames,
        std::string * priName,
        const std::vector<std::string> * omitColumns)
{
	#ifdef HAVE_PANTHEIOS
    pantheios::log(pantheios::debug,
            __PRETTY_FUNCTION__," table=",table);
	#endif

    mrutils::stringstream ss;

    int priCol = -1;

    rr(ss.clear() << "DESC " << table);
    for (int colNum = 0; nextLine();)
    {
        char const * type = getString(1);
        if (omitColumns != NULL &&
                mrutils::containsI(*omitColumns, getString(0)))
            continue;

        if (mrutils::startsWithI(type,"int")
            ||mrutils::startsWithI(type,"tinyint")
            ||mrutils::startsWithI(type,"smallint")
            ||mrutils::startsWithI(type,"bit")
            )
        {

            if (mrutils::stristr(getString(3),"pri"))
            {
                if (priCol != -1)
                {
                    errId = -1;
                    error = "mrutils::Sql dump error; primary column must be a single integer type column.";
                    return false;
                }

                priCol = colNum;
                *priName = getString(0);

                colTypes->push_back('p');
            } else
            {
                colTypes->push_back('d');
            }
        } else if (mrutils::startsWithI(type,"varchar")
            ||mrutils::startsWithI(type,"blob")
            ||mrutils::startsWithI(type,"mediumblob")
            ||mrutils::startsWithI(type,"char")
            ||mrutils::startsWithI(type,"enum")
            )
        {
            colTypes->push_back('s');
        } else if (mrutils::startsWithI(type,"double")
            ||mrutils::startsWithI(type,"float")
            )
        {
            colTypes->push_back('f');
        } else
        {
            errId = -1;
            errStr = "mrutils::Sql::dumpTable unhandled column type: "; errStr += type;
            error = errStr.c_str();
            return false;
        }

        colNames->push_back(getString(0));
        ++colNum;
    }

    if (priCol == -1)
    {
        errId = -1;
        errStr = "mrutils::Sql dump error, no primary key found for table ";
        errStr += table;
        error = errStr.c_str();
        return false;
    }

    // make the primary key the first column
    (*colNames)[priCol] = (*colNames)[0];
    (*colTypes)[priCol] = (*colTypes)[0];
    (*colNames)[0] = *priName;
    (*colTypes)[0] = 'p';

    return true;
}
