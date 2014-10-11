#ifndef MR_STRUTILS_H
#define MR_STRUTILS_H

#include "mr_exports.h"

#include <iostream>
#include <vector>
#include <list>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <climits>
#include <string.h>
#include <algorithM>
#include <iterator>
#include "mr_oprintf.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define snprintf _snprintf
#endif

// compile-time strlen for string literals
template<size_t N>
inline size_t ctlen(char const (&)[N]) { return (N-1); }

namespace mrutils {
    // strcpy for string literals (length known at compile-time)
    template<size_t N>
    inline char * ctcpy(char * dest, const char (&src)[N]) {
        return ((char*)memcpy(dest,src,N) + (N-1));
    }
}

template <class T> 
inline T sMAX(T a, T b) {
	return (a > b? a: b);
}
template <class T> 
inline T sMIN(T a, T b) {
	return (a < b? a: b);
}

namespace mrutils {

    template <typename Container>
    void split(Container *dest, std::string const &str, char delim)
    {
        std::insert_iterator<Container> insert(*dest, dest->end());
        std::istringstream ss(str);
        for (std::string elem(32,'\0'); std::getline(ss, elem, delim);
                *insert = elem, ++insert)
        {}
    }


    namespace formatting {
        _API_ std::ostream& formatCommas(std::ostream& os);

       /**
        * Note: this is slow to instantiate. calls new() and deletes. 
        * creates a stringstream with a buffer and deletes.
        * retreives a locale. 
        * constructs an object. every time just to print.
        * 
        * A faster way to use this rather than building money("$",10)
        * every time is to reuse an existing money class on a single thread.
        */
        class Money {
            public:
                Money(const char * symbol, const double amt = 0);
                Money(const Money& other);

                inline friend Money& operator<<(Money& money, const double amt) 
                { money.amt = amt; return money; }

                friend std::ostream& operator<<(std::ostream& os, const Money& money) {
                    std::stringstream& ss = const_cast<Money&>(money).ss;
                    ss.str("");
                    ss.precision(money.amt > 1000 || money.amt < 1000 ? 0 : 2);
                    if (money.amt < 0) {
                        ss << std::fixed << -money.amt;
                        return os << "-" << money.symbol << ss.str();
                    } else {
                        ss << std::fixed << money.amt;
                        return os << money.symbol << ss.str();
                    }
                }

                const std::string symbol;
                double amt;
            private:
                std::stringstream ss;
        };
    }


    inline bool isNumber(const char * str, int len = -1) {
        while (len-- != 0 && *str) if (!isdigit(*str++)) return false;
        return true;
    }

    inline int atoin(const char * str, int len = -1) {
        if (len <= 0) return atoi(str);

        int result = 0; int neg;
        if (*str == '-') {
            ++str; neg = -1;
        } else neg = +1;
        while (len-- != 0 && isdigit(*str)) {
            result *= 10;
            result += *str++ - '0';
        }
        return (neg*result);
    }


   /**
    * Rather than 0,1,10,20,5,50 you get 0,1,5,10,20,50
    */
    static inline bool sortStringIntegral(const std::string& lhs, const std::string& rhs) {
        if (lhs.size() < rhs.size()) return true;
        if (lhs.size() > rhs.size()) return false;
        return (lhs < rhs);
    }

    /**
     * Takes a string containing backspace characters and replaces it
     * with actual overwrites. */
    inline char * compress(char * str) {
        char *s = str, *t = str;
        while (*s) {
            if (*s == '\010') {
                if (t != str) --t;
            } else *t++ = *s;
            ++s;
        }
        *t = '\0';
        return str;
    }

    inline void trim(std::string& str) {
        const char * sob = str.c_str();
        const char * p = sob;
        for (;*p != '\0' && isspace(*p);++p){}
        if (p != sob) str.erase(0,p - sob);

        p = sob + str.size();
        for (;p > sob && isspace(*(p-1));--p){}
        str.resize(p-sob);
    }

    /** 
     * Like doing a strchr on '\n' and '\r' simultaneously.
     * */
    inline char * strnl(char * str) {
        for(;;) {
            if (*str == '\0') return NULL;
            if (*str == '\r' || *str == '\n') return str;
            ++str;
        }
    }
    inline const char * strnl(const char * str) {
        for(;;) {
            if (*str == '\0') return NULL;
            if (*str == '\r' || *str == '\n') return str;
            ++str;
        }
    }

    int stricmp ( const char * first, const char * second);
    inline bool containsI(const std::vector<std::string>& vec, const std::string& value) {
        for (std::vector<std::string>::const_iterator it = vec.begin(); it != vec.end(); ++it) {
            if (0==mrutils::stricmp(value.c_str(),(*it).c_str())) return true;
        }
        return false;
    }

    /**
      * Searches for a white space character (' ',\n,\r,\t)
      */
    inline const char * strws(const char * str) {
        for(;;) {
            if (*str == '\0') return NULL;
            if (*str == '\r' || *str == '\n' || *str == ' ' || *str == '\t') return str;
            ++str;
        }
    }

    /**
     * performs strchr on '\' and '/' simultaneously. 
     * */
    inline char * strdir(char * str) {
        for(;;) {
            if (*str == '\0') return NULL;
            if (*str == '/' || *str == '\\') return str;
            ++str;
        }
    }
    inline const char * strdir(const char * str) {
        for(;;) {
            if (*str == '\0') return NULL;
            if (*str == '/' || *str == '\\') return str;
            ++str;
        }
    }


    /**
     * fast implementation that writes an unsigned into the buffer
     * returns a pointer to the \0 at the end.
     * */
    _API_ char * uitoa(unsigned int n, char * dst);
    _API_ char * uitoa(unsigned int n, char * dst, int size);
    _API_ char * itoa(int n, char * dst);
    _API_ char * itoa(int n, char * dst, int size);

    /**
      * This is used instead to allow for (some) scientific notation
      * Allows (integer)e(positive integer)
      * This is also 4-5x faster than ::atoi
      */
    _API_ int atoi(const char * str);

    _API_ int atoiHex(const char * str);

    /** 
     * Modified version of strcpy; returns a pointer to the end of the
     * destination after the copy is finished, instead of the beginning.
     * */
    inline char * strcpy( char * target, const char * source) {
        const int size = strlen(source);
        memcpy(target,source,size+1);
        return (target+size);
    }

    inline char * strcpy( char * target, const std::string& source) {
        memcpy(target,source.c_str(),source.size()+1);
        return (target+source.size());
    }
    /**
     * if size is specified, then the output will be right-aligned
     * */
    inline char * strcpy( char * target, unsigned source) {
        return uitoa(source, target);
    }
    inline char * strcpy( char * target, unsigned source, int size) {
        return uitoa(source, target, size);
    }
    /**
     * if size is specified, then the output will be right-aligned
     * */
    inline char * strcpy( char * target, int source) {
        return itoa(source, target);
    }
    inline char * strcpy( char * target, int source, int size) {
        return itoa(source, target, size);
    }

    /** 
     * Modified version of strncpy; returns a pointer to the end of the
     * destination after the copy is finished, instead of the beginning.
     * */
    inline char * strncpy( char * target, const char * source, int n, char fill='\0') {
        for (;n > 0 && *source;--n) *target++ = *source++;
        if (fill != '\0') for (;n > 0;--n) *target++ = fill;
        if (n > 0) *target = '\0';
        return target;
    }

    /**
     * Case insensitive starts with 
     * */
    inline bool startsWithI(const char * str, const char * with) {
        while (*with) 
            if (!*str || tolower((unsigned char)*str++) != tolower((unsigned char)*with++))
                return false;
        return true;
    }

    /**
     * Case sensitive starts with 
     * */
    inline bool startsWith(const char * str, const char * with, bool skipWhitespace=false) {
        if (skipWhitespace) while(*str && (*str == ' ' || *str == '\t')) ++str;
        while (*with) 
            if (!*str || *str++ != *with++) return false;
        return true;
    }

    inline bool endsWithI(const char * str, const char * with) {
        const char * p = str + strlen(str); int len = strlen(with);
        if (p - str < len) return false;
        return startsWithI(p - len,with);
    }

    inline bool endsWith(const char * str, const char * with) {
        const char * p = str + strlen(str); int len = strlen(with);
        if (p - str < len) return false;
        return startsWith(p - len,with);
    }

    /**
     * Requires that T have a c_str() function 
     * */
    template <class T>
    inline typename std::list<T>::iterator 
    startsWith(std::list<T> v, const char * with) {
        const char * with_, *str;
        for (typename std::list<T>::iterator it = v.begin(); 
             it != v.end(); ++it) {
            str = it->c_str();

            with_ = with;
            while (*with_) 
                if (!*str || *str++ != *with_++) {with_ = NULL; break;}
            if (with_ == NULL) continue;
            return it;
        }

        return v.end();
    }

    /**
     * Requires that T have a c_str() function 
     * */
    template <class T>
    inline typename std::list<T>::iterator 
    startsWith(const char * str, std::list<T>& with) {
        const char * with_, *str_;
    
        for (typename std::list<T>::iterator it = with.begin(); 
             it != with.end(); ++it) {
            with_ = it->c_str();
            str_ = str;
            while (*with_) {
                if (!*str_ || *str_++ != *with_++) {with_ = NULL; break;}
            }
            if (with_ == NULL) continue;
            return it;
        }

        return with.end();
    }


    inline char * replace(char * str, char r, char w) {
        char * ret = str;
        while ((str = strchr(str, r)) != NULL) *str++ = w;
        return ret;
    }

    inline std::string& replace(std::string& str, char r, char w) {
        for (std::string::iterator it = str.begin()
            ;it != str.end()
            ;++it) if (*it == r) *it = w;
        return str;
    }

   /**
    * For strings: takes an existing const string & returns a temporary
    * with characters replaced
    */
    inline std::string replaceCopy(const std::string& str, char r, char w) {
        std::string tmp = str;
        for (std::string::iterator it = tmp.begin(), itE = tmp.end()
            ; it != itE; ++it) if (*it == r) *it = w;
        return tmp;
    }

    /**
     * Copies src to dest (up to size characters) replacing occurrences
     * of string r with string w. Returns a pointer to the last character
     * copied.
     * */
    inline char * replaceCopy(char * dest, const char * src, const char * r, const char * w, int size) {
        const char * end = dest + size;
        const char * to = src;
        const int len = strlen(r);

        while ((to = strstr(to,r)) != NULL && *src && dest != end) {
            dest = strncpy(dest, src, sMIN(end-dest, to - src));
            dest = strncpy(dest, w, end-dest);
            to += len; src = to;
        }

        // copy last part
        return strncpy(dest,src,end-dest);
    }

    /**
     * S is assumed to be a stream with operator<< and a write function.
     */
    template<class S, class T>
    inline S& replaceCopy(S& dest, const char * src, const char * r, const T& w) {
        const int len = strlen(r);
        for (const char * to = src
            ;*src && NULL != (to = strstr(to,r))
            ;to += len, src = to) 
            { dest.write(src,to - src); dest << w; }
         return dest << src;
    }

    inline char * replaceCopy(char * dest, const char * src, const char r, const char w) {
        for (;*src;) {
            if (*src == r) {
                *dest++ = w; ++src;
            } else *dest++ = *src++;
        } *dest = 0;
        return dest;
    }

    /**
     * Copies the src to dest, escaping the quotes in mysql-style (no
     * escape)
     * */
    inline char * replaceCopyQuote(char * dest, const char * src, int size) {
        const char * end = dest + size;
        const char * to = src;

        while ((to = strchr(to,'"')) != NULL && *src && dest != end) {
            dest = strncpy(dest, src, sMIN(end-dest, to - src));
            //if ( *(to-1) == '\\') {
            //    dest = strncpy(dest, "\\\\\"\"", end-dest);
            //} else {
                dest = strncpy(dest, "\"\"", end-dest);
            //}
            ++to; src = to;
        }

        // copy last part
        return strncpy(dest,src,end-dest);
    }

    class stringstream;

   /**
    * Same as above, but copies to a stringstream and assumes the src
    * has length srcLen, rather than null terminated
    */
    inline void replaceCopyQuote(mrutils::stringstream& ss, const char * str, int strLen);

    /**
     * Inefficient replacing of " with "" in a std::string
     */
    inline std::string replaceQuote(std::string str) {
        for (size_t pos = str.find("\""); pos != std::string::npos; pos = str.find("\"",pos+2)) {
            str.insert(pos,"\"");
        } return str;
    }

    /**
     * Inefficient replacing of " with \" and \ with \\.
     */
    inline std::string escapeQuote(std::string str) {
        for (size_t pos = str.find_first_of("\"\\"); pos != std::string::npos; pos = str.find_first_of("\"\\",pos+2)) {
            if (str[pos] == '\\') str.insert(pos,"\\\\");
            else str.insert(pos,"\"");
        } return str;
    }

	std::string escapeHTML(std::string const &str);

    inline char * replace(char * str, char r, const char * w, char * buf) {
        char * s = strchr(str, r); if (s == NULL) return str;

        char * sb = buf, * sbN; 
        strcpy(sb, s+1); 

        for (;;) {
            s = strcpy(s, w);

            sbN = strchr(sb, r);
            if (sbN == NULL) {
                strcpy(s, sb);
                break;
            } else {
                s = strncpy(s, sb, sbN - sb);
                sb = sbN + 1;
            }
        }

        return str;
    }


    /**
     * Takes a path of the form /usr///local/.././b/c/../d.txt 
     * and returns /usr/b/d.txt
     * Also handles windows paths
     * Returns a pointer to the END of the path.
     * */
    _API_ char * formatPath(char * path, bool * isWindows = NULL);

    std::string formURL(const char * baseURL, const char * relative);

    inline static bool isURLCharacter(char c) {
        if (c < 0) return false;
        //[0-9a-zA-Z]$-_.+!*'(),:/?&%=;
        static char ok[128] = "0000000000000000000000000000000001001111111111111111111111110101011111111111111111111111111000010111111111111111111111111110000";
        return (ok[(uint8_t)c] == '1');
    }

    template <class T>
    inline void printMatrix(std::ostream& os, const T * data, int rows, int cols) {
        for (int r = 0 ; r < rows; r++) {
            os << "|  ";
            for (int c = 0; c < cols; c++) {
                os.width(15);
                os << data[r*cols+c];
            }
            os << "  |" << std::endl;
        }
    }
    
    template <class T>
    inline void printMatrix(std::ostream& os, const std::vector<T> &data, int rows, int cols) {
        for (int r = 0 ; r < rows; r++) {
            os << "|  ";
            for (int c = 0; c < cols; c++) {
                os.width(10);
                os << data[r*cols+c];
            }
            os << "  |" << std::endl;
        }
    }
    
    template <class T>
    inline void printColMatrix(std::ostream& os, const T * data, int maxRows, int rows, int cols) {
        for (int r = 0 ; r < rows; r++) {
            os << "|  ";
            for (int c = 0; c < cols; c++) {
                os.width(10);
                os << data[r + c*maxRows];
                os << "  ";
            }
            os << "|" << std::endl;
        }
    }

    template <class T>
    inline void printVector(std::ostream& os, const T * data, int rows) {
        os << "[  ";
        for (int r = 0 ; r < rows; r++) {
            os.width(10);
            os << data[r];
        }
        os << "  ]";
    }

    template <class T>
    inline void printVector(std::ostream& os, const std::vector<T>& data) {
        os << "[  ";
        for (unsigned r = 0 ; r < data.size(); r++) {
            os.width(10);
            os << data[r];
        }
        os << "  ]";
    }

    class _API_ stringstream : public std::ostringstream {
        public:
            stringstream()
            : lastPos(0)
            { }

            stringstream(const stringstream& other)
            : std::basic_ios<char, std::char_traits<char> >(), std::basic_ostringstream<char, std::char_traits<char>, std::allocator<char> >()
             ,lastPos(other.lastPos), string(other.string) 
            { 
                stringstream& other_ = const_cast<stringstream&>(other);
                std::ostringstream::write(other_.c_str(),other_.str().size()); 
            }

            inline stringstream& operator=(const stringstream& other) {
                std::ostringstream::str("");
                lastPos = other.lastPos; string = other.string;
                stringstream& other_ = const_cast<stringstream&>(other);
                std::ostringstream::write(other_.c_str(),other_.str().size()); 
                return *this;
            }

        public:
           /********************
            * Basic
            ********************/

            inline stringstream& clear() { std::ostringstream::str(""); lastPos = 0; string.clear(); return *this; }
            inline bool empty() { return (tellp() == 0); }
            inline stringstream& str(const char * s) 
            { std::ostringstream::str(s); if (*s == 0) string.clear(); lastPos = 0; return *this; }

        public:
           /********************
            * Writing
            ********************/

            inline stringstream& write(const char* s , std::streamsize n) 
            { std::ostringstream::write(s,n); return *this; }

            inline stringstream& put(const char c)
            { std::ostringstream::put(c); return *this; }

            template <class T>
            friend stringstream& operator<<(stringstream&ss, const T& elem);


        public:
           /**********************
            * Getting content
            **********************/

            inline operator char const *() { return c_str(); }
            inline operator std::string() { return str(); }

            inline const char * c_str() {
                if (lastPos != tellp()) { string = std::ostringstream::str(); lastPos = tellp(); }
                return string.c_str();
            }

            inline std::string& str() {
                if (lastPos != tellp()) { string = std::ostringstream::str(); lastPos = tellp(); }
                return string;
            }

        public:
           /******************
            * Utilities
            ******************/

            template<class S>
            inline friend S& operator<<(S& os, mrutils::stringstream& ss) {
                return os << ss.str();
            }
        private:
            std::streampos lastPos;
            std::string string;
    };

    template<class T>
    inline stringstream& operator<<(stringstream& ss, const T& elem) 
    { ((std::ostringstream&)ss) << elem; return ss; }



    // TODO: this could be more efficient 
    // using the itoa_quick, etc. functions
    class _API_ ScopeStringMaker {
        public:
            ScopeStringMaker() :null_(0) { }
            ~ScopeStringMaker() { 
                for (std::vector<std::string*>::iterator it = strings.begin();
                     it != strings.end(); ++it) {
                    delete *it;
                }
            }

            template <class T>
            const char * make(const T& num) {
                ss.str(""); ss << num; 
                std::string * ptr = new std::string(ss.str());
                strings.push_back(ptr);
                return ptr->c_str();
            }

            const char * make(char* s) {
                if (s == NULL) return &null_;
                strings.push_back(new std::string(s));
                return strings.back()->c_str();
            }
            const char * make(const char* s) {
                if (s == NULL) return &null_;
                strings.push_back(new std::string(s));
                return strings.back()->c_str();
            }

            inline void clear() { strings.clear(); }

        private:
            const char null_;
            std::vector<std::string*> strings;
            mrutils::stringstream ss;
    };

   /**
    * This is used to simplify the construction of delimited text files.
    * Construct with something like "%d","this","%f","that",NULL
    * and it will build the header string ("this,that\n") and the
    * formatting string ("%d,%f\n")
    */
    struct _API_ delimStr {
        delimStr(const int cols, const char * const format, const char * const name, ...) {
            size_t lenF = 1, lenH = 1; 
            va_list list;  char * f, * h;

            const char * format_ = format, * name_ = name;
            va_start(list,name);
            for(int i = 0; i != cols; ++i) {
                lenH += strlen(name_) + 1;
                lenF += strlen(format_) + 1;

                format_ = va_arg(list,const char *);
                if (format_ == NULL) break;
                else name_ = va_arg(list,const char *);
            }

            this->format = (char *)malloc(lenF);
            header = (char *)malloc(lenH);

            f = mrutils::strcpy(this->format,format);
            h = mrutils::strcpy(header,name);

            va_start(list,name);
            for(int i = 1; i != cols; ++i) {
                format_ = va_arg(list,const char *);
                if (format_ == NULL) break;
                else name_ = va_arg(list,const char *);
                *f++ = ','; *h++ = ','; 
                f = mrutils::strcpy(f,format_);
                h = mrutils::strcpy(h,name_);
            }

            memcpy(f,"\n",2); memcpy(h,"\n",2); 
        }

        ~delimStr() 
        { free(header); free(format); }

        char * header;
        char * format;
    };


    class _API_ ColumnBuilder {
        public:
            ColumnBuilder()
            : null_(0) 
             ,printImmediately(NULL)
             ,store(true)
             ,ncols(0), r(0), c(0)
             ,justDidPrint(false)
            { }

            inline void setPrintImmediately(FILE * fp = stdout, bool andStore = false) { 
                this->printImmediately = fp; 
                this->store = andStore;
            }
            void print(FILE* file = stdout); 
            void print(std::ostream&os);
            inline void clear() { 
                ncols = 0; r = 0; c = 0; strings.clear(); 
                summary.clear(); colWidths.clear();
            }

            inline bool empty() const { return (r == 0); }

            inline void col(const char * str) {
                if (store) summary.push_back(strings.make(str));
                colWidths.resize(ncols+1, 0);
                colWidths[c] = sMAX(colWidths[c], (int)strlen(str));

                if (printImmediately)
                    fprintf(printImmediately, "%s%*s", str, padding, "");

                ++ncols; ++c;
            }

           /**
            * of the form cols(-1,"a","b","c",NULL)
            * or cols(2,"one","two")
            */
            inline void cols(int num, ...) {
                va_list list; va_start(list,num);
                for(const char * str
                    ;num != 0 && NULL != (str = va_arg(list, const char*))
                    ;--num, col(str)){}
            }

            template <class T>
            inline bool add(const T& elem) {
                if (c == ncols) { 
                    r++; c = 0; 

                    if (printImmediately) 
                        fputs("\n", printImmediately);
                    if (store)
                        summary.resize(ncols * (r+1),&null_);
                }

                if (printImmediately) {
                    ss << elem;
                    fprintf(printImmediately, "%s%*s", ss.str().c_str(), colWidths[c]+padding - (int)ss.str().size(), "");
                    ss.str("");
                }

                if (store) {
                    summary[r*ncols + c] = strings.make(elem);
                    colWidths[c] = sMAX(colWidths[c], (int)strlen(summary[r*ncols+c]));
                }

                ++c;
                return true;
            }

            inline bool add(mrutils::stringstream& ss) { return add(ss.str()); }

            template <class T>
            inline bool fro(const T& elem) {
                r++; c = 0;

                if (printImmediately) {
                    if (justDidPrint) justDidPrint = false;
                    else fputs("\n", printImmediately);
                    ss << elem;
                    fprintf(printImmediately, "%s%*s", ss.str().c_str(), colWidths[c]+padding - (int) ss.str().size(), "");
                    ss.str("");
                } 

                if (store) {
                    summary.resize(ncols * (r+1),&null_);
                    summary[r*ncols + c] = strings.make(elem);
                    colWidths[c] = sMAX(colWidths[c], (int)strlen(summary[r*ncols+c]));
                }

                ++c;
                return true;
            }

            void dumpCSV(FILE* outFile) {
                for (int i = 0; i <= r; ++i) {
                    for (int j = 0; j < ncols; ++j) {
                        if (j > 0) fputc(',',outFile);
                        fputs(summary[i*ncols+j],outFile);
                    }
                    fputc('\n',outFile);
                }
            }

        public:
            std::vector<int> colWidths;
            static const int padding = 3;

            mrutils::stringstream ss;

        private:
            const char null_;
            FILE * printImmediately;
            bool store;
            std::vector<const char*> summary;
            int ncols; int r, c; bool justDidPrint;
            ScopeStringMaker strings;
    };

    inline void printCharMatrix(std::ostream& os, const char ** data, int rows, int cols, int * colWidths) {
        for (int r = 0 ; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (data[r*cols+c] == NULL) os << std::string().insert(0,colWidths[c] + ColumnBuilder::padding,' ').c_str();
                else os << data[r*cols+c] << std::string().insert(0,colWidths[c]+ColumnBuilder::padding-strlen(data[r*cols+c]),' ').c_str();
            }
            os << std::endl;
        }
    }

    inline void printCharMatrix(FILE* fp, const char ** data, int rows, int cols, int * colWidths) {
        for (int r = 0 ; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (data[r*cols+c] == NULL) 
                    fprintf(fp,"%*s",colWidths[c] + ColumnBuilder::padding,"\0");
                else 
                    fprintf(fp,"%-*s",colWidths[c]+ColumnBuilder::padding,data[r*cols+c]);
            }
            fputc('\n',fp);
        }
    }

    inline void printCharMatrix(std::ostream& os, const char ** data, int rows, int cols) {
        //int colWidths[cols];
		int *colWidths = new int[cols];

        for (int c = 0; c < cols; c++) {
            colWidths[c] = 0; 
            for (int r = 0; r < rows; r++) 
                if (data[r*cols+c] != NULL)
                    colWidths[c] = sMAX(colWidths[c],(int)strlen(data[r*cols+c]));
        }
        printCharMatrix(os,data,rows,cols,colWidths);
		delete[] colWidths;
    }

    inline void printCharMatrix(FILE* fp, const char ** data, int rows, int cols) {
        //int colWidths[cols];
		int *colWidths = new int[cols];

        for (int c = 0; c < cols; c++) {
            colWidths[c] = 0; 
            for (int r = 0; r < rows; r++) 
                if (data[r*cols+c] != NULL)
                    colWidths[c] = sMAX(colWidths[c],(int)strlen(data[r*cols+c]));
        }
        printCharMatrix(fp,data,rows,cols,colWidths);
		delete[] colWidths;
    }


    /**
     * Replaces instances of \n with \0 in the string. 
     * This is windows-compatible (searches for \r first)
     * */
    inline char * capNewLine(char * str) {
        char *t = strchr( str, '\r' );
        if (t == NULL) {
            t = strchr( str, '\n' );
            if (t != NULL) *t = '\0';
        } else {
            *t = '\0';
            *(t+1) = '\0';
        }
        return str;
    }

    /**
     * Removes characters outside the standard ascii table range
     * Does not adjust the length, returns pointer to the BEGINNING
     */
    static inline char * stripNonAscii(char * const str_, char * const end_ = NULL) {
        unsigned char * const end = (unsigned char * const)end_;
        for (unsigned char * str = (unsigned char * const)str_;str != end;++str) {
            if (*str == 0) return str_;
            if (*str > 0x7F) *str = ' ';
        } return str_;
    }

    /**
     * Copies, replacing UTF-8 and ISO8859-1 chars outside the 0-127
     * range with similar-looking ASCII characters.
     * Returns pointer to the END of the destination buffer.
     * Replace quote will replace double quotes with two double quotes
     */
    _API_ char* copyToAscii(char * to, const int destSz, const char * from, const bool stripCR = false);
    _API_ char* copyMacToAscii(char * to, const int destSz, const char * from, const bool stripCR = false);
    _API_ char* copyToWindowsLatin1(char * to, const int destSz, const char * from, const bool stripCR = false);
    _API_ char* copyLatin1ToTerminal(char * to, const int destSz, const char * from);

   /**
    * Removes all <xxx> blocks from the string
    */
    inline std::string stripTags(const std::string& str) {
        const char * p = str.c_str();
        const char * q = strchr(p,'<');
        if (q == NULL) return str;
        std::string ret; ret.reserve(str.size());
        for (;;) {
            ret.append(p,q-p);
            p = strchr(q+1,'>');
            if (p == NULL) break;
            ++p; q = strchr(p,'<');
            if (q == NULL) { ret.append(p); break; }
        } return ret;
    }

    /**
      * returns a pointer to the terminating null char
      */
    inline char * toupper(char * str) {
        for (;*str;++str) *str = ::toupper(*str);
        return str;
    }
    inline char * tolower(char * str) {
        for (;*str;++str) *str = ::tolower(*str);
        return str;
    }

    inline std::string & toupper(std::string & str) {
		for (std::string::iterator it = str.begin(); 
			it != str.end(); ++it) *it = ::toupper(*it); 
        return str;
    }

	inline std::string & tolower(std::string & str) {
		for (std::string::iterator it = str.begin(); 
			it != str.end(); ++it) *it = ::tolower(*it); 
        return str;
    }

    /**
     * Does a case insensitive comparison of first & second. 
     * */
    inline int stricmp ( const char * first, const char * second) {
        int c1, c2;
        do {
            c1 = ::tolower( (unsigned char) *first++ );
            c2 = ::tolower( (unsigned char) *second++ );
        } while (c1 == c2 && c1 != 0);
        return (c1 - c2);
    }

    inline char * strichr( char * str, char c ) {
        int lc = ::tolower( (unsigned char) c ), 
            uc = ::toupper( (unsigned char) c );
        while (*str != lc && *str != uc) 
            if (*str == '\0') return NULL;
            else ++str;
        return str;
    }
    inline const char * strichr( const char * str, char c ) {
        int lc = ::tolower( (unsigned char) c ), 
            uc = ::toupper( (unsigned char) c );
        while (*str != lc && *str != uc) 
            if (*str == '\0') return NULL;
            else ++str;
        return str;
    }

    inline int strnicmp ( const char * first, const char * second, int n ) {
        if (n == 0) return 0;

        int c1 = 0, c2 = 0;
        while (n-- > 0 &&
			(c1 = ::tolower( (unsigned char) *first++ )) == (c2 = ::tolower((unsigned char) *second++ ))){}
        return (c1 - c2);
    }

    inline const char * strchrRev(const char * str, char c, const char * beginning) {
        while (*str != c)
            if (str == beginning) return NULL;
            else --str;
        return str;
    }

    inline const char * strichrRev(const char * str, char c, const char * beginning) {
        c = ::tolower(c);
        while (::tolower(*str) != c)
            if (str == beginning) return NULL;
            else --str;
        return str;
    }

    inline char * strichrRev(char * str, char c, const char * beginning) {
        c = ::tolower(c);
        while (::tolower(*str) != c)
            if (str == beginning) return NULL;
            else --str;
        return str;
    }

    inline char * strchrRev(char * str, char c, const char * beginning) {
        while (*str != c)
            if (str == beginning) return NULL;
            else --str;
        return str;
    }

    template<typename T>
    inline T * strstrRev(T *h, const char *n, const char * beginning) {
        if (*n == '\0') return h;

        size_t len = strlen(n);
        for (; (h = strchrRev(h, *n, beginning)) != NULL; --h)
            if (strncmp(h, n, len) == 0)
                return h;
        return NULL;
    }

    inline char * stristrRev(char *h, const char *n, const char * beginning) {
        if (*n == '\0') return h;

        size_t len = strlen(n);
        for (; (h = strichrRev(h, *n, beginning)) != NULL; --h)
            if (strnicmp(h, n, len) == 0)
                return h;
        return NULL;
    }

    /**
     * Case insensitive strstr
     * Note: if it's necessary to re-implement this, use this algo: 
     * http://en.wikipedia.org/wiki/Boyer-Moore_string_search_algorithm
     * */
    static inline const char * stristr ( const char * h, const char * n) {
        return strcasestr(h,n);
    }
    static inline char * stristr ( char * h, const char * n) {
        return strcasestr(h,n);
    }
}

/***********************************************************/

namespace mrutils {
    inline void replaceCopyQuote(mrutils::stringstream& ss, const char * str, int strLen) {
        for (const char * p = str, *end = str + strLen, *q = str; ;++q) {
            if (q == end) { ss.write(p,q-p); break; }
            if (*q == '"') { ss.write(p,q-p); ss << '"'; p = q; }
        }
    }

    template<>
    inline stringstream& operator<< <double>(stringstream& ss, const double& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <float>(stringstream& ss, const float& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <unsigned>(stringstream& ss, const unsigned& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <int>(stringstream& ss, const int& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <long long>(stringstream& ss, const long long& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <short>(stringstream& ss, const short& elem) 
    { ss.operator<<(elem); return ss; }

    template<>
    inline stringstream& operator<< <unsigned long long>(stringstream& ss, const unsigned long long& elem) 
    { ss.operator<<(elem); return ss; }
}

#endif
