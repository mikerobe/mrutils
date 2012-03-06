#ifndef _MR_MYSQL_WRAPPER_H
#define _MR_MYSQL_WRAPPER_H

#include "mysql_sha1.h"
#include "mysql_crypt.h"
#include "mr_strutils.h"

namespace mrutils {
namespace mysql {

    enum messageType {
        MT_OK
       ,MT_ERR
       ,MT_EOF
       ,MT_RESULT
    };

    enum clientOption {
        CLIENT_LONG_PASSWORD     = 1              /* new more secure passwords */
       ,CLIENT_FOUND_ROWS        = 2              /* Found instead of affected rows */
       ,CLIENT_LONG_FLAG         = 4              /* Get all column flags */
       ,CLIENT_CONNECT_WITH_DB   = 8              /* One can specify db on connect */
       ,CLIENT_NO_SCHEMA         = 16             /* Don't allow database.table.column */
       ,CLIENT_COMPRESS          = 32             /* Can use compression protocol */
       ,CLIENT_ODBC              = 64             /* Odbc client */
       ,CLIENT_LOCAL_FILES       = 128            /* Can use LOAD DATA LOCAL */
       ,CLIENT_IGNORE_SPACE      = 256            /* Ignore spaces before '(' */
       ,CLIENT_PROTOCOL_41       = 512            /* New 4.1 protocol */
       ,CLIENT_INTERACTIVE       = 1024           /* This is an interactive client */
       ,CLIENT_SSL               = 2048           /* Switch to SSL after handshake */
       ,CLIENT_IGNORE_SIGPIPE    = 4096           /* IGNORE sigpipes */
       ,CLIENT_TRANSACTIONS      = 8192           /* Client knows about transactions */
       ,CLIENT_RESERVED          = 16384          /* Old flag for 4.1 protocol  */
       ,CLIENT_SECURE_CONNECTION = 32768          /* New 4.1 authentication */
    };

    enum commandType {
        COM_SLEEP            
       ,COM_QUIT            
       ,COM_INIT_DB         
       ,COM_QUERY           
       ,COM_FIELD_LIST      
       ,COM_CREATE_DB       
       ,COM_DROP_DB         
       ,COM_REFRESH         
       ,COM_SHUTDOWN        
       ,COM_STATISTICS      
       ,COM_PROCESS_INFO    
       ,COM_CONNECT         
       ,COM_PROCESS_KILL    
       ,COM_DEBUG           
       ,COM_PING            
       ,COM_TIME            
       ,COM_DELAYED_INSERT  
       ,COM_CHANGE_USER     
       ,COM_BINLOG_DUMP     
       ,COM_TABLE_DUMP      
       ,COM_CONNECT_OUT     
       ,COM_REGISTER_SLAVE  
       ,COM_STMT_PREPARE    
       ,COM_STMT_EXECUTE    
       ,COM_STMT_SEND_LONG_DATA
       ,COM_STMT_CLOSE      
       ,COM_STMT_RESET      
       ,COM_SET_OPTION      
       ,COM_STMT_FETCH      
    };

    const uint8_t commands[] = {
        0x00
       ,0x01
       ,0x02
       ,0x03
       ,0x04
       ,0x05
       ,0x06
       ,0x07
       ,0x08
       ,0x09
       ,0x0a
       ,0x0b
       ,0x0c
       ,0x0d
       ,0x0e
       ,0x0f
       ,0x10
       ,0x11
       ,0x12
       ,0x13
       ,0x14
       ,0x15
       ,0x16
       ,0x17
       ,0x18
       ,0x19
       ,0x1a
       ,0x1b
       ,0x1c
    };

    inline int readLengthCodedBinary(mrutils::BufferedReader& reader, uint64_t & result, bool useFirst = false, uint8_t len = 0) {
        int ret;

        if (!useFirst) {
            if (!reader.read(1)) return -1;
            len = (uint8_t)*reader.line;
            ret = 1;
        } else {
            ret = 0;
        }

        switch (len) {
            case 251: // means NULL
                result = 0;
                return ret;
                break;

            case 252:
            {
                if (!reader.read(2)) return false;
                uint16_t tmp = *(uint16_t*)reader.line;
                result = tmp;
                return (ret+2);
            } break;
                
            case 253:
            {
                if (!reader.read(3)) return false;
                char buffer[4]; buffer[3] = 0;
                memcpy(buffer,reader.line,3);
                uint32_t tmp = *(uint32_t*)buffer;
                result = tmp;
                return (ret+3);
            } break;

            case 254:
            {
                if (!reader.read(8)) return false;
                uint64_t tmp = *(uint64_t*)reader.line;
                result = tmp;
                return (ret+8);
            } break;

            default: // means single-byte value <= 250
            {
                result = len;
                return ret;
            } break;
        }

        return ret;
    }

    struct mysql_packet_headersend {
        char packetLength[3];
        uint8_t packetNumber;
    };

    struct mysql_packet_header {
        uint32_t packetLength; // actually only 3 bytes
        uint8_t packetNumber;

        inline bool read(mrutils::BufferedReader& reader) {
            // first 3 bytes are packet length
            char buffer[4]; buffer[3] = 0;
            if (!reader.read(3)) return false;
            memcpy(buffer,reader.line,3);
            packetLength = *(uint32_t*)buffer;

            // next byte is packet number
            if (!reader.read(1)) return false;
            packetNumber = *(uint8_t*)reader.line;

            return true;
        }
    };

    struct mysql_handshake {
        mysql_packet_header header;

        uint8_t protocolVersion;
        std::string serverVersion;
        uint32_t threadId;
        uint16_t capability;
        uint8_t language;
        uint16_t status;
        char scrambleBuf[SHA1_HASH_SIZE + 1];

        inline void print(FILE * fp = stdout) {
            fprintf(fp,"length: %d\n"
                    "number: %d\n"
                    "version: %d\n"
                    "server_version: %s\n"
                    "thread id: %d\n"
                    "language: %d\n"
                    "status: %d\n",
                    header.packetLength,
                    header.packetNumber,
                    protocolVersion,
                    serverVersion.c_str(),
                    threadId,
                    (int)language,
                    status);
        }

        inline void log(pantheios::pan_severity_t severity)
        {
            pantheios::logprintf(severity,"length: %d\n"
                    "number: %d\n"
                    "version: %d\n"
                    "server_version: %s\n"
                    "thread id: %d\n"
                    "language: %d\n"
                    "status: %d\n",
                    header.packetLength,
                    header.packetNumber,
                    protocolVersion,
                    serverVersion.c_str(),
                    threadId,
                    (int)language,
                    status);
        }

        inline bool read(mrutils::BufferedReader& reader) {
            header.read(reader);

            if (!reader.read(sizeof(uint8_t))) return false;
            protocolVersion = *(uint8_t*)reader.line;

            if (!reader.nextLine()) return false;
            serverVersion = reader.line;

            if (!reader.read(sizeof(uint32_t))) return false;
            threadId = *(uint32_t*)reader.line;

            // scramble buff
            if (!reader.read(8)) return false;
            memcpy(scrambleBuf,reader.line,8);

            // filler
            if (!reader.read(1)) return false;

            if (!reader.read(2)) return false;
            capability = *(uint16_t*)reader.line;

            if (!reader.read(1)) return false;
            language = *reader.line;

            if (!reader.read(2)) return false;
            status = *(uint16_t*)reader.line;

            // filler
            if (!reader.read(13)) return false;

            // scramble buff
            static const int leftover = SHA1_HASH_SIZE + 1 - 8;
            if (!reader.read(leftover)) return false;
            memcpy(scrambleBuf+8,reader.line,leftover);

            return true;
        }
    };

    struct mysql_login {
        mysql_packet_headersend header;

        uint32_t client_flags;
        uint32_t max_packet_size;
        uint8_t charset_number;
        const char * user;
        const char * scramblePass;
        const char * databaseName;

        bool send(mrutils::BufferedWriter& writer)
        {
            uint8_t scrambleSize = SHA1_HASH_SIZE;
            size_t userLen = strlen(user) + 1;
            size_t databaseLen = strlen(databaseName);

            // compute total size
            uint32_t size = 32 
                + userLen
                + scrambleSize + 1
                + databaseLen;
            memcpy(&header.packetLength, &size, 3);
            header.packetNumber = 1;

            // 16 MB max packet size
            max_packet_size = 16777216u;
            charset_number = 8;

            client_flags = CLIENT_PROTOCOL_41
                         | CLIENT_SECURE_CONNECTION
                         | CLIENT_CONNECT_WITH_DB
                         | CLIENT_LOCAL_FILES
                         | CLIENT_IGNORE_SPACE
                         | CLIENT_LONG_PASSWORD
                         ;

            char buffer[23];
            memset(buffer,0,sizeof(buffer));

            try {
                writer.write((char*)&header,4);
                writer.write((char*)&client_flags,4);
                writer.write((char*)&max_packet_size,4);
                writer.write((char*)&charset_number,1);
                writer.write(buffer,23);
                writer.write(user,userLen);
                writer.write((char*)&scrambleSize,1);
                if (scrambleSize > 0)
                    writer.write(scramblePass,scrambleSize);
                writer.write(databaseName, databaseLen);
                writer.flush();
            } catch (mrutils::ExceptionNoSuchData const &e)
            {
                std::cerr << e.what() << std::endl;
                return false;
            }

            return true;
        }
    };

    struct mysql_reply {
        mysql_packet_header header;

        messageType type;

        /** data **/
        union data_t {
            struct ok_t {
                uint64_t affectedRows;
                uint64_t insertId;
                uint16_t status;
                uint16_t warningCount;
                char message[256];
            } ok;

            struct err_t {
                uint16_t errId;
                char sqlstate[5];
                char message[256];
            } err;

            struct result_t {
                uint8_t stage;
                uint64_t numCols;
            } result;
        } data;

        mysql_reply() {
            // clear all data
            memset(&data,0,sizeof(data));
        }

        inline void print(FILE * fp = stdout) {
            switch (type) {
                case MT_OK:
                    fprintf(fp,"packet %u affected %llu id %llu, OK: %s\n",(unsigned)header.packetNumber
                        ,data.ok.affectedRows, data.ok.insertId,data.ok.message);
                    break;
                case MT_ERR:
                    fprintf(fp,"packet %u, ERR %d: %s\n",(unsigned)header.packetNumber,data.err.errId,data.err.message);
                    break;
                case MT_EOF:
                    fprintf(fp,"packet %u, EOF\n",(unsigned)header.packetNumber);
                    break;
                case MT_RESULT:
                    fprintf(fp,"packet %u, RESULT\n",(unsigned)header.packetNumber);
                    break;
            }
        }

        bool read(mrutils::BufferedReader& reader) {
            if (!header.read(reader)) return false;

            // get type
            if (!reader.read(1)) return false;

            switch ((uint8_t)*reader.line) {
                case 0x00:
                {
                    type = MT_OK;

                    unsigned length = 1;
                    int ret;

                    if (0>(ret=readLengthCodedBinary(reader,data.ok.affectedRows))) return false;
                    length += ret;

                    if (0>(ret=readLengthCodedBinary(reader,data.ok.insertId))) return false;
                    length += ret;

                    if (header.packetLength <= length) return true;
                    if (!reader.read(2)) return false;
                    data.ok.status = *(uint16_t*)reader.line;
                    length += 2;

                    if (header.packetLength <= length) return true;
                    if (!reader.read(2)) return false;
                    data.ok.warningCount = *(uint16_t*)reader.line;
                    length += 2;

                    if (header.packetLength > length) {
                        // read up to 255 chars, then skip
                        int chars = std::min(255u,header.packetLength - length);
                        if (!reader.read(chars)) return false;
                        mrutils::strncpy(data.ok.message,reader.line,chars);
                        length += chars;
                        if (!reader.skip(header.packetLength - length)) return false;
                    }
                } break;

                case 0xff:
                { 
                    type = MT_ERR;

                    if (!reader.read(2)) return false;
                    data.err.errId = *(uint16_t*)reader.line;

                    if (header.packetLength > 3) {
                        if (!reader.read(1)) return false;
                        if (*reader.line == '#') {
                            // sql state
                            if (!reader.read(5)) return false;
                            memcpy(data.err.sqlstate,reader.line,5);

                            // (optional) message
                            if (header.packetLength > 9) {
                                if (!reader.read(header.packetLength - 9)) return false;
                                mrutils::strncpy(data.err.message,reader.line
                                    ,std::min(255u,header.packetLength - 9));
                            } else data.err.message[0] = 0;
                        } else {
                            memset(data.err.sqlstate,0,5);
                            data.err.message[0] = *reader.line;

                            // read rest of message
                            if (!reader.read(header.packetLength - 4)) return false;
                            mrutils::strncpy(data.err.message+1,reader.line
                                ,std::min(254u,header.packetLength - 4));
                        }
                    } else data.err.message[0] = 0;
                } break;

                case 0xfe:
                {
                    type = MT_EOF;
                    if (!reader.skip(header.packetLength - 1)) return false;
                } break;

                default:
                {
                    type = MT_RESULT;

                    unsigned length = 1;
                    int ret;

                    switch (data.result.stage) {
                        case 0: { // column count packet
                            if (0>(ret=readLengthCodedBinary(reader,data.result.numCols,true,*reader.line))) return false;
                            data.result.stage = 1;

                            // skip any leftover (should be 0)
                            if (!reader.skip(header.packetLength - length)) return false;
                        } break;

                        case 1: { // column description packet
                            // just ignore. user will know
                            if (!reader.skip(header.packetLength - length)) return false;
                        } break;

                        case 2: { // row data packets
                            // will parse with parseRow
                        } break;
                    }
                } break;
            }

            return true;
        }
    };

    struct mysql_query {
        mysql_packet_headersend header;
        commandType type;
        const char * query;

        mysql_query(commandType type = COM_QUERY)
        : type(type) 
        {}

        inline bool send(mrutils::BufferedWriter& writer)
        {
            try {
                switch(type) {
                    case COM_QUERY:
                        {
                            // compute total size
                            uint32_t size = 1 + strlen(query);
                            memcpy(&header.packetLength, &size, 3);

                            // always reset
                            header.packetNumber = 0;

                            try {
                                writer.write((char*)&header,4);
                                writer.write((char*)&commands[type],1);
                                writer.write(query,size-1);
                                writer.flush();
                            } catch (mrutils::ExceptionNoSuchData const &)
                            {
                                return false;
                            }
                        } break;

                    default:
                        {
                            uint32_t size = 1;
                            memcpy(&header.packetLength, &size, 3);
                            header.packetNumber = 0;

                            writer.write((char*)&header,4);
                            writer.write((char*)&commands[type],1);
                            writer.flush();
                        } break;
                }
            } catch (mrutils::ExceptionNoSuchData const &)
            {
                return false;
            }

            return true;
        }
    };

    inline bool parseRow(mrutils::BufferedReader& reader, mysql_reply & reply, std::vector<std::string>& results) {
        if (reply.type != MT_RESULT || reply.data.result.stage != 2) {
            fprintf(stderr,"Mysql: error, call to parseRow and not in stage 2 result packet.\n");
            return false;
        }

        uint32_t length = 1; int ret;
        uint64_t strLen = 0;
        std::string result;

        for (int col = 0; reply.header.packetLength > length; ++col) {
            if (col == 0) {
                if (0>(ret=readLengthCodedBinary(reader,strLen,true,*reader.line))) return false;
            } else {
                if (0>(ret=readLengthCodedBinary(reader,strLen))) return false;
            }

            length += (uint32_t)ret + (uint32_t)strLen;

            if (strLen == 0) {
                results.push_back("");
            } else {
                // packet won't be larger than 16MB
                int len = (int)strLen;

                // this could be long, so do multiple calls
                result.clear();
                result.reserve(len);

                for (; len > 0; ) {
                    int r = reader.read(len);
                    if (r == 0) return false;
                    len -= r;
                    result.append(reader.line,r);
                }

                results.push_back(result);
            }
        }

        return true;
    }

}} 


#endif
