#include "mr_db.h"

bool mrutils::Database::dumpTableReadCols(mrutils::BufferedReader& reader, std::vector<char> * colTypes, std::vector<std::string>* colNames, std::string * priName) throw (mrutils::ExceptionNoSuchData) {
    int priCol = -1;

    for (int i = 0;;++i) {
        const char type = reader.get<char>();
        if (type == '\0') break;

        const char * name = reader.getLine();
        if (type == 'p') {
            priName->assign(name);
            colTypes->push_back('d');
            colNames->push_back(*priName);
            priCol = i;
        } else { 
            colTypes->push_back(type);
            colNames->push_back(name);
        }
    }

    // check that primary key was found
    if (priCol != 0) {
        std::cerr << __FILE__ << ": " << __LINE__ 
            << " mrutils::Database dumpTableReadCols, dump didn't have key." << std::endl;
        return false;
    }

    return true;
}
