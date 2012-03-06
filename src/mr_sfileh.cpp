#include "mr_sfileh.h"
#include "mr_strutils.h"
#include "mr_files.h"

bool mrutils::SocketFileHandler::handleMessage(mrutils::BufferedReader & reader, FILE * verboseOut) {
    static const char zero = 0;

    if (!mrutils::startsWithI(reader.line,"file")) return false;
    reader.line += 4;

    writer.setFD(reader.socket->s_);

    if (0==mrutils::strnicmp(reader.line,"list ",5)) {
        const char * dir = reader.line + 5;
        DIR *dp;
        struct dirent *dirp;

        char * p = mrutils::strcpy(buffer,dir);
        *p++ = '/'; *p = 0;

        if((dp = opendir(dir)) == NULL) {
            reader.socket->write(zero);
            return true;
        }

        while ((dirp = readdir(dp)) != NULL) {
            if (dirp->d_name[0] == '.') continue;

            // full path
            mrutils::strcpy(p,dirp->d_name);

            // get the type
            struct MR_STAT s;
            if (MR_STAT(buffer,&s) != 0) continue;

            #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
                writer << (s.st_mode & S_IFDIR?'d':'f');
            #else
                writer << (s.st_mode & S_IFDIR?'d':dirp->d_type==DT_LNK?'l':'f');
            #endif

            // add the modification time
            writer << ((unsigned)s.st_mtime);

            // and the short path
            writer << dirp->d_name;
        }

        writer << '\0';
        writer.flush();
    } else if (0==mrutils::strnicmp(reader.line,"stat ",5)) {
        struct MR_STAT s;

        if (MR_STAT(reader.line+5,&s) != 0) {
            reader.socket->write(zero);
            return true;
        } 

        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            writer.write(s.st_mode & S_IFDIR?'d':'f');
        #else
            if (s.st_mode & S_IFDIR) writer << 'd';
            else if (0 < ::readlink(reader.line+5,buffer,sizeof(buffer)))
                writer << 'l';
            else writer << 'f';
        #endif

        writer << (int)s.st_mtime;
        writer.flush();
    } else if (0==mrutils::strnicmp(reader.line,"get ",4)) {
        reader.line -= 4;

        // have to read all the data first, then write files
        std::vector<std::string> paths; 

        // aggregated file request -- stop at empty line
        do {
            paths.push_back(reader.line+8);
        } while (reader.nextLine() && *reader.line);

        // now write files
        for (std::vector<std::string>::iterator it = paths.begin()
            ;it != paths.end(); ++it) {
            const char * path = it->c_str();

            if (verboseOut != NULL) { fprintf(verboseOut,"writing file at %s...", path); fflush(verboseOut); }
            int size = (mrutils::fileExists(path)?mrutils::fileSize(path):0);
            writer << size;
            if (size > 0) writer.putFile(path);
            if (verboseOut != NULL) { fprintf(verboseOut,"done\n"); fflush(verboseOut); }
        }

        writer.flush();
    } else if (0==mrutils::strnicmp(reader.line,"readlink ",9)) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            reader.socket->write(zero);
        #else
            int ret = readlink(reader.line+9, buffer, sizeof(buffer));
            if (ret < 0) reader.socket->write(zero);
            buffer[ret] = 0;
            reader.socket->writeLine(buffer);
        #endif
    } else return false;

    return true;
}
