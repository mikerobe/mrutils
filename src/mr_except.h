#ifndef _MR_CPPLIB_EXCEPT_H
#define _MR_CPPLIB_EXCEPT_H

#include <stdexcept>

namespace mrutils {
    class _API_ ExceptionNoSuchData : public std::exception {
        public:
            virtual const char* what() const throw() { return str.c_str(); }
            ~ExceptionNoSuchData() throw() {}

        public:
            ExceptionNoSuchData(char const * const msg) throw() : str(msg) {}
            ExceptionNoSuchData(std::string const & str) throw() : str(str) {}
            ExceptionNoSuchData(std::stringstream const & ss) throw() : str(ss.str()) {}

        private:
            std::string str;
    };


    class _API_ ExceptionNoSuchColumn : public std::exception {
        public:
            virtual const char* what() const throw() { return str.c_str(); }
            ~ExceptionNoSuchColumn() throw() {}

        public:
            ExceptionNoSuchColumn(char const * const msg) throw() : str(msg) {}
            ExceptionNoSuchColumn(std::string const & str) throw() : str(str) {}
            ExceptionNoSuchColumn(std::stringstream const & ss) throw() : str(ss.str()) {}

        private:
            std::string str;
    };

    class _API_ ExceptionNumerical : public std::exception {
        public:
            virtual const char* what() const throw() { return str.c_str(); }
            ~ExceptionNumerical() throw() {}

        public:
            ExceptionNumerical(char const * const msg) throw() : str(msg) {}
            ExceptionNumerical(std::string const & str) throw() : str(str) {}
            ExceptionNumerical(std::stringstream const & ss) throw() : str(ss.str()) {}

        private:
            std::string str;
    };

}

#endif
