#include "mr_time.h"


static MUTEX nextDayMapMutex = mrutils::mutexCreate();
static std::map<int,int> nextDayMap;

static MUTEX prevDayMapMutex = mrutils::mutexCreate();
static std::map<int,int> prevDayMap;

int mrutils::nextDay(int date) {
    int result;

    mrutils::mutexAcquire(nextDayMapMutex);
        std::map<int,int>::iterator it = nextDayMap.find(date);
        if (it != nextDayMap.end()) {
            result = it->second;
            mrutils::mutexRelease(nextDayMapMutex);
            return result;
        }

        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);
        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timeinfo->tm_hour = 25;
        timeinfo->tm_min  = 0;
        timeinfo->tm_sec  = 0;

        rawtime = timegm(timeinfo);
        char str[9];strftime(str,9,"%Y%m%d",gmtime(&rawtime));
        result = atoi(str);

        nextDayMap[date] = result;
    mrutils::mutexRelease(nextDayMapMutex);

    return result;
}

int mrutils::prevDay(int date) {
    int result;

    mrutils::mutexAcquire(prevDayMapMutex);
        std::map<int,int>::iterator it = prevDayMap.find(date);
        if (it != prevDayMap.end()) {
            result = it->second;
            mrutils::mutexRelease(prevDayMapMutex);
            return result;
        }

        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);
        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timeinfo->tm_hour = -1;
        timeinfo->tm_min  = 0;
        timeinfo->tm_sec  = 0;

        rawtime = timegm(timeinfo);
        char str[9];strftime(str,9,"%Y%m%d",gmtime(&rawtime));
        result = atoi(str);

        prevDayMap[date] = result;
    mrutils::mutexRelease(prevDayMapMutex);

    return result;
}


std::string mrutils::setLocalTZ(TZ::TZ tz) {
    std::string cTz;
    if (getenv("TZ") != NULL) cTz.assign(getenv("TZ"));

    switch (tz) {
        case mrutils::TZ::Japan:
            MR_SETENV("TZ","JST-9",1);
            break;
        case mrutils::TZ::San_Francisco:
            MR_SETENV("TZ","PST8PDT",1);
            break;
        case mrutils::TZ::New_York:
            MR_SETENV("TZ","EST5EDT",1);
            break;
        case mrutils::TZ::Hong_Kong:
            MR_SETENV("TZ","HKT-8",1);
            break;
        case mrutils::TZ::London:
            MR_SETENV("TZ","GMT0BST",1);
            break;
        case mrutils::TZ::Paris:
            MR_SETENV("TZ","CET-1CEST",1);
            break;
        case mrutils::TZ::Korea:
            MR_SETENV("TZ","KST-9",1);
            break;
        default:
            fprintf(stderr,"mrutils::setLocalTZ unhandled zone: %d\n",tz);
            break;
    }

    tzset();

    return cTz;
}

void mrutils::unsetLocalTZ(std::string& cTz) {
    if (!cTz.empty()) { MR_SETENV("TZ",cTz.c_str(),1); }
    else { MR_UNSETENV("TZ"); }
}


#define PARSEINT(i,p) \
    for (char c;(c = *p) >= '0' && c <= '9';++p) i = 10*i + (c - '0');

#define SKIPCHARS(p) \
    for (;;++p) {\
        const char c = *p;\
        if (c == '\0') return 0;\
        if (c >= '0' && c <= '9') break;\
    }

#define PARSE2INT(i,p) \
    { int tmp = *(p+1) - '0';\
    if (tmp >= 0 && tmp <= 9) {\
        i = 10 * (*p - '0') + tmp;\
        tmp = *(p+=2) - '0';\
        if (tmp >= 0 && tmp <= 9) return 0;\
    } else i = *p++ - '0'; }

#define PARSEYEAR(y,p) \
    PARSEINT(y,p);\
    if (y < 1900) {\
        if (y >= 200) return 0;\
        if (y > 40) y += 1900;\
        else y += 2000;\
    } else if (y > 3000) return 0;

namespace {
    struct month_t {
        int month; bool numerical;

        month_t(int month, bool numerical) 
        : month(month), numerical(numerical)
        { }

        month_t(const int m) 
        : month(m), numerical(false) {}
    };
    month_t parseMonth(char const *&p, bool skipAlpha = true);
}

int mrutils::parseGuessDate(const char * date) {
    if (date[0] >= '0' && date[0] <= '9') {
        char const * p = date;

        int i = 0; PARSEINT(i,p);

        if (i > 19000000) {
            // form is 20091025
            if (i < 30000000) return i;
            else return 0;

        } else if (i > 3000) {
            return 0;

        } else if (i > 1900) {
            // form is year month  day
            
            const int m = parseMonth(p).month;
            if (m == 0) return 0;

            SKIPCHARS(p);
            int d = 0; PARSE2INT(d,p);

            return (i * 10000 + m * 100 + d);
        } else {
            // form is day month year
            // form is month day year (if 10/25/2009)

            const char sep = *p;
            month_t m = parseMonth(p);
            if (m.month == 0) return 0;

            // read as american-style date unless first number > 12
            if (m.numerical && sep == '/' && i <= 12) {
                const int tmp = i; i = m.month; m.month = tmp;
            }

            SKIPCHARS(p); 
            int y = 0; PARSEYEAR(y,p);
            return (y * 10000 + m.month * 100 + i);
        }
    } else {
        char const * p = date;
        month_t m = parseMonth(p,false);
        if (m.month == 0) {
            for (;;++p) {
                const char c = *p;
                if (c == '\0') return 0;
                if (c == ' ' || c == '\t') break;
                if (c >= '0' && c <= '9') break;
            }
            for (char c;'\0' != (c = *p);++p) if (c != ' ' && c != '\t') break;
            return parseGuessDate(p);
        } else {
            // form is month day year
            SKIPCHARS(p);
            int i = 0; PARSE2INT(i,p);
            SKIPCHARS(p);
            int y = 0; PARSEYEAR(y,p);
            return (y * 10000 + m.month * 100 + i);
        }
    }
    return 0;
}

namespace {
    month_t parseMonth(char const *&p, bool skipAlpha) {
        for (;;++p) {
            char c = *p;
            if (c >= '0' && c <= '9') {
                month_t m(0,true);
                PARSE2INT(m.month,p);
                return m;
            } else {
                switch (::tolower(c)) {
                    case '\0': return 0;
                    case 'a':
                        c = ::tolower(*++p);
                        if (c == 'p') {
                            if ('r' == (c = ::tolower(*++p))) return 4;
                            else return 0;
                        } else if (c == 'u') {
                            if ('g' == (c = ::tolower(*++p))) return 8;
                            else return 0;
                        }
                        break;
                    case 'd':
                        if (::tolower(*++p) != 'e' || ::tolower(*++p) != 'c') return 0;
                        return 12;
                    case 'f':
                        if (::tolower(*++p) != 'e' || ::tolower(*++p) != 'b') return 0;
                        return 2;
                    case 'j':
                        c = ::tolower(*++p);
                        if (c == 'a') return ('n' == ::tolower(*++p)); 
                        else if (c == 'u') {
                            c = ::tolower(*++p);
                            if (c == 'n') return 6;
                            if (c == 'l') return 7;
                        }
                        return 0;
                    case 'm':
                        if (::tolower(*++p) != 'a') return 0;
                        c = ::tolower(*++p);
                        if (c == 'r') return 3;
                        if (c == 'y') return 5;
                        return 0;
                    case 'n':
                        if (::tolower(*++p) != 'o' || ::tolower(*++p) != 'v') return 0;
                        return 11;
                    case 'o':
                        if (::tolower(*++p) != 'c' || ::tolower(*++p) != 't') return 0;
                        return 10;
                    case 's':
                        if (::tolower(*++p) != 'e' || ::tolower(*++p) != 'p') return 0;
                        return 9;
                    case ' ': case '\t': continue;
                    default:
                        if (!skipAlpha) return 0;
                        else continue;
                }
            }
            return 0;
        }
    }
}
