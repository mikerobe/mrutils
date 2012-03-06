#pragma once
#include <stdexcept>
#include "mr_strutils.h"

namespace mrutils {
class _API_ ExceptionNoSuchData : public std::exception {
public:
    virtual const char* what() const throw() { return str.c_str(); }
    ~ExceptionNoSuchData() throw() {}

public:
    ExceptionNoSuchData(char const * const msg = "", int err=0) throw() :
        str(msg),
        err(err)
    {}

    ExceptionNoSuchData(std::string const & str, int err=0) throw() :
        str(str),
        err(err)
    {}

    ExceptionNoSuchData(std::stringstream const & ss, int err=0) throw() :
        str(ss.str()),
        err(err)
    {}

    ExceptionNoSuchData(mrutils::stringstream const & ss, int err=0) throw() :
        str(const_cast<mrutils::stringstream&>(ss).str()),
        err(err)
    {}

    std::string const str;
    int err;
};


class _API_ ExceptionNoSuchColumn : public std::exception {
public:
    virtual const char* what() const throw() { return str.c_str(); }
    ~ExceptionNoSuchColumn() throw() {}

public:
    ExceptionNoSuchColumn(char const * const msg = "") throw() : str(msg) {}
    ExceptionNoSuchColumn(std::string const & str) throw() : str(str) {}
    ExceptionNoSuchColumn(std::stringstream const & ss) throw() : str(ss.str()) {}
    ExceptionNoSuchColumn(mrutils::stringstream const & ss) throw() : 
        str(const_cast<mrutils::stringstream&>(ss).str()) {}

private:
    std::string str;
};

class _API_ ExceptionNumerical : public std::exception {
public:
    virtual const char* what() const throw() { return str.c_str(); }
    ~ExceptionNumerical() throw() {}

public:
    ExceptionNumerical(char const * const msg = "") throw() : str(msg) {}
    ExceptionNumerical(std::string const & str) throw() : str(str) {}
    ExceptionNumerical(std::stringstream const & ss) throw() : str(ss.str()) {}
    ExceptionNumerical(mrutils::stringstream const & ss) throw() : 
        str(const_cast<mrutils::stringstream&>(ss).str()) {}

private:
    std::string str;
};

}
