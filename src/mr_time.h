#ifndef _MR_CPPLIB_TIME_H
#define _MR_CPPLIB_TIME_H

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #define timegm _mkgmtime
    #define MR_PUTENV _putenv
#else
    #include <sys/time.h>
    #define MR_PUTENV putenv
#endif

#include "mr_exports.h"
#include <time.h>
#include <cstdlib>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <map>

#include "mr_threads.h"
#include "mr_strutils.h"
 
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif


#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
inline int MR_SETENV(const char * var, const char * value, bool unused) {
    // always overridden
    //if (!override && getenv(var) != NULL) return -1;
    std::string str = var;
    str += '='; str += value;
    return MR_PUTENV(str.c_str());
}
inline int MR_UNSETENV(const char * var) {
    std::string str = var;
    str.append("=\"\"");
    return MR_PUTENV(str.c_str());
}
#else
    #define MR_SETENV setenv
    #define MR_UNSETENV unsetenv
#endif


namespace mrutils {
    /**
     * returns the date as a number (i.e. 20090714).
     * assumes the input is of the form 07/14/2009
     * */
    int inline parseDate(const char * mmddYYYY) {
        return (
            (mmddYYYY[0]-'0')*1000u + (mmddYYYY[1]-'0')*100u
            + (mmddYYYY[3]-'0')*10u + (mmddYYYY[4]-'0')
            + (mmddYYYY[6]-'0')*10000000u
            + (mmddYYYY[7]-'0')*1000000u
            + (mmddYYYY[8]-'0')*100000u
            + (mmddYYYY[9]-'0')*10000u  );
    }

    /**
     * Tries to guess the date format to produce a reasonable number
     * Returns 0 on error
     */
    _API_ int parseGuessDate(char const * date);

    /**
     * returns the date as a number (i.e. 20090714).
     * assumes the input is of the form 7/8/9. 
     * Years >= 70 are assumed +1900. < 70, +2000.
     * note that the year can be 4 digits (e.g. 7/8/2009)
     * */
    int inline parseShortDate(const char * mmddYY) {
        const char * p = strchr(mmddYY, '/');
        if (p == NULL) return 0;

        int result = 0;
        if (p - mmddYY > 1) {
            result += (mmddYY[0] - '0') * 1000u
                   +  (mmddYY[1] - '0') * 100u;
        } else {
            result += (mmddYY[0] - '0') * 100u;
        }

        mmddYY = p + 1;
        p = strchr(mmddYY,'/');
        if (p - mmddYY > 1) {
            result += (mmddYY[0] - '0') * 10u
                   +  (mmddYY[1] - '0');
        } else {
            result += (mmddYY[0] - '0');
        }

        mmddYY = p + 1;
        p = strchr(mmddYY,'\0');
        if (p - mmddYY >= 4) {
            result += (mmddYY[0] - '0') * 10000000u
                   +  (mmddYY[1] - '0') * 1000000u
                   +  (mmddYY[2] - '0') * 100000u
                   +  (mmddYY[3] - '0') * 10000u;
        } else if (p - mmddYY > 1) {
            if (mmddYY[0] - '0' >= 7) result += 19000000u;
            else result += 20000000u;

            result += (mmddYY[0] - '0') * 100000u
                   +  (mmddYY[1] - '0') * 10000u;
        } else {
            result += 20000000u;

            result += (mmddYY[0] - '0') * 10000u;
        }

        return result;
    }

   /**
    * Returns the current date in yyyymmdd format
    * IN THE LOCAL TIME ZONE
    * */
    int inline getDate() {
        time_t now; time(&now); 
        struct tm * timeinfo = localtime(&now);
        char str[9];strftime(str,9,"%Y%m%d",timeinfo);
        return atoi(str);
    }

    namespace TZ {
        enum TZ {
            Hong_Kong
           ,New_York
           ,San_Francisco
           ,London
           ,Paris
           ,Korea
           ,Japan

           ,COUNT
        };

        static const char * humanNames[] = {
            "Hong Kong"
           ,"New York"
           ,"San Francisco"
           ,"London"
           ,"Paris"
           ,"Korea"
           ,"Japan"
        };

        static const char * names[] = {
            "HKT-8"
           ,"EST5EDT"
           ,"PST8PDT"
           ,"GMT0BST"
           ,"CET-1CEST"
           ,"KST-9"
           ,"JST-9"
        };

        inline bool findTZ(const char * name, TZ & tz) {
            for (int i = 0; i < COUNT; ++i) {
                if (0==stricmp(names[i],name)) {
                    tz = (TZ)i; return true;
                } else if (0==stricmp(humanNames[i],name)) {
                    tz = (TZ)i; return true;
                }
            }
            return false;
        }

    }

    _API_ std::string setLocalTZ(TZ::TZ tz);
    _API_ void unsetLocalTZ(std::string& oldTZ);

    inline int getDateInTZ(TZ::TZ tz) {
        std::string cTz = setLocalTZ(tz);
        int result = getDate();
        unsetLocalTZ(cTz);
        return result;
    }

    /**
     * Takes a date such as 20091031 and returns the next day
     * (20091101). This is cached in a static map for efficiency
     * of repeated calls.
     * */
    _API_ int nextDay(int date);
    _API_ int prevDay(int date);

    int inline getDate(time_t now) {
        char str[9];strftime(str,9,"%Y%m%d",gmtime(&now));
        return atoi(str);
    }

    static _UNUSED const char dayOfWeekChar[8] = "SMTWHFA";
    inline int getDayOfWeek(const int date) {
        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);

        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timegm(timeinfo);

        char dayOfWeek[4]; strftime(dayOfWeek,3,"%a",timeinfo);
        switch (*dayOfWeek) {
            case 'S':
                switch (*(dayOfWeek+1)) {
                    case 'u':
                    case 'U':
                        return 0;
                    default:
                        return 6;
                }
            case 'M': return 1;
            case 'T':
                switch (*(dayOfWeek+1)) {
                    case 'u':
                    case 'U':
                        return 2;
                    default:
                        return 4;
                }
            case 'W': return 3;
            case 'F': return 5;
            default: return 0;
        }
    }

    /**
     * Takes a date of the form yyyymmdd and a time 
     * in hours (e.g., 22 or 24.5) and returns the 
     * unix timestamp (seconds from 1970)
     * */
    inline unsigned getTimeOnDate(int date, double hours) {
        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);

        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timeinfo->tm_hour = static_cast<int>(hours);
        hours -= timeinfo->tm_hour;
        timeinfo->tm_min  = static_cast<int>(60 * hours);
        timeinfo->tm_sec  = static_cast<int>( 60*(60*hours - timeinfo->tm_min) );
        return (unsigned)timegm(timeinfo);
    }

    /**
     * Takes a date of the form yyyymmdd and a time 
     * in hours (e.g., 22 or 24.5) and returns the 
     * unix timestamp (seconds from 1970)
     * */
    inline unsigned getTimeOnDateLocal(int date, double hours) {
        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);

        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timeinfo->tm_hour = static_cast<int>(hours);
        hours -= timeinfo->tm_hour;
        timeinfo->tm_min  = static_cast<int>(60 * hours);
        timeinfo->tm_sec  = static_cast<int>( 60*(60*hours - timeinfo->tm_min) );
        return (unsigned)mktime(timeinfo);
    }

    inline unsigned getTimeOnDateTZ(int date, double hours, TZ::TZ tz) {
        std::string ctz = setLocalTZ(tz);

        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);

        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;
        timeinfo->tm_hour = static_cast<int>(hours);
        hours -= timeinfo->tm_hour;
        timeinfo->tm_min  = static_cast<int>(60 * hours);
        timeinfo->tm_sec  = static_cast<int>( 60*(60*hours - timeinfo->tm_min) );

        unsigned result = (unsigned) mktime(timeinfo);

        unsetLocalTZ(ctz);

        return (unsigned)result;
    }

    inline unsigned getTimeOnDate(int date, int hour, int minute = 0, int second = 0) {
        time_t rawtime; time(&rawtime);
        struct tm *timeinfo = gmtime(&rawtime);

        timeinfo->tm_year = date / 10000 - 1900;
        timeinfo->tm_mon  = (date % 10000) /100 - 1;
        timeinfo->tm_mday = date % 100;

        timeinfo->tm_hour = hour;
        timeinfo->tm_min  = minute;
        timeinfo->tm_sec  = second;
        return (unsigned) timegm(timeinfo);
    }

    /**
     * Returns the number of hours since midnight for the given time 
     * in the GMT timezone.
     * */
    inline double getHours(time_t now) {
        struct tm* ptr = gmtime((time_t*)&now);
        return (ptr->tm_hour + ptr->tm_min/60.);
    }
    
    /**
     * Returns the number of seconds since midnight for the given time 
     * in the GMT timezone.
     * */
    inline unsigned getSeconds(time_t now) {
        struct tm* ptr = gmtime((time_t*)&now);
        return (ptr->tm_hour*3600 + ptr->tm_min*60 + ptr->tm_sec);
    }

    inline unsigned getSecondsLocal(time_t now) {
        struct tm* ptr = localtime((time_t*)&now);
        return (ptr->tm_hour*3600 + ptr->tm_min*60 + ptr->tm_sec);
    }

    inline unsigned getSecondsLocal() {
        time_t now; time(&now);
        return getSecondsLocal(now);
    }

    inline double getHoursLocal(time_t now) {
        struct tm* ptr = localtime((time_t*)&now);
        return (ptr->tm_hour + ptr->tm_min/60.);
    }

    inline double getHoursLocal() {
        time_t now; time(&now);
        return getHoursLocal(now);
    }

    unsigned inline getTime(time_t now, double hours) {
        time_t n = (time_t)now;
        struct tm* ptr = gmtime(&n);

        ptr->tm_hour = static_cast<int>(hours);
        ptr->tm_min = static_cast<int>((hours - static_cast<int>(hours)) * 60);
        ptr->tm_sec = 0;
        return static_cast<unsigned>(timegm(ptr));
    }
    time_t inline getTime(time_t now, const char * hhMM) {
        int hour = 10 * (hhMM[0] - '0') + (hhMM[1] - '0');
        int min = (strlen(hhMM)==5?10*(hhMM[3]-'0')+(hhMM[4]-'0')
                   : 10*(hhMM[2]-'0')+(hhMM[1]-'0'));
        time_t n = (time_t)now;
        struct tm* ptr = gmtime(&n);
        ptr->tm_hour = hour;
        ptr->tm_min = min;
        ptr->tm_sec = 0;
        return timegm(ptr);
    }

    /**
     * This is a slow way of doing this. It's designed for 
     * convenience in printing with <<. 
     * */
    inline std::string getRealTimeFromSeconds(unsigned secondsFromMidnight) {
        const unsigned hours = secondsFromMidnight / 3600u;
        secondsFromMidnight -= hours * 3600u;
        const unsigned minutes = secondsFromMidnight / 60u;
        secondsFromMidnight -= minutes * 60u;

        std::string str(8,0); str.reserve(9);
        sprintf(&str[0], "%02d:%02d:%02d", hours, minutes, secondsFromMidnight);
        return str;
    }

    /**
     * This is a slow way of doing this. It's designed for 
     * convenience in printing with <<. 
     * */
    inline std::string getRealTimeFromMillis(const unsigned millisFromMidnight) {
        std::string str(12,0); str.reserve(13);
        sprintf(&str[0], "%02u:%02u:%02u.%03u"
            ,millisFromMidnight / 3600000u
            ,millisFromMidnight / 60000u % 60u
            ,millisFromMidnight / 1000u % 60u
            ,millisFromMidnight % 1000u);
        return str;
    }

    /**
     * str must be at least 13 characters long.  returns pointer to the
     * end of the string
     * */
    inline char * getRealTimeFromMillis(const unsigned millisFromMidnight, char * str) {
        return (str + sprintf(str, "%02u:%02u:%02u.%03u"
            ,millisFromMidnight / 3600000u
            ,millisFromMidnight / 60000u % 60u
            ,millisFromMidnight / 1000u % 60u
            ,millisFromMidnight % 1000u));
    }

    /**
     * Use on mrutils::stringstream, std::stringstream and 
     * on ostreams.
     * */
    template <class T> 
    inline void printTimeSecs(T& os, unsigned time) {
        int days = 0; int hours = 0; int min = 0;
        if (time > 86400) {
            days = time / 86400;
            time -= days * 86400;
            os << days << " d ";
        }
        if (time > 3600) {
            hours = time / 3600;
            time -= hours * 3600;
            os << hours << " h ";
        }
        if (time > 60) {
            min = time / 60;
            time -= min * 60;
            os << min << " m ";
        }
        if (time > 0) os << time << " s";
    }

    /**
     * Use on mrutils::stringstream, std::stringstream and 
     * on ostreams.
     * */
    template <class T> 
    inline void printTime(T& os, unsigned time) {
        int days = 0; int hours = 0; int min = 0; int sec = 0; 
        if (time > 86400000) {
            days = time / 86400000;
            time -= days * 86400000;
            os << days << " d ";
        }
        if (time > 3600000) {
            hours = time / 3600000;
            time -= hours * 3600000;
            os << hours << " h ";
        }
        if (time > 60000) {
            min = time / 60000;
            time -= min * 60000;
            os << min << " m ";
        }
        if (time > 1000) {
            sec = time / 1000;
            time -= sec * 1000;
            os << sec << " s ";
        }
        if (time > 0) os << time << " ms";
    }

    inline std::string getMillisStr(unsigned time) {
        std::stringstream ss;
        printTime(ss,time);
        return ss.str();
    }

    inline char * getMillisStr(unsigned time, char * buf) {
        int days = 0; int hours = 0; int min = 0; int sec = 0; 
        if (time > 86400000) {
            days = time / 86400000;
            time -= days * 86400000;
            buf = mrutils::strcpy(buf,days);
            buf = mrutils::strcpy(buf," d ");
        }
        if (time > 3600000) {
            hours = time / 3600000;
            time -= hours * 3600000;
            buf = mrutils::strcpy(buf,hours);
            buf = mrutils::strcpy(buf," h ");
        }
        if (time > 60000) {
            min = time / 60000;
            time -= min * 60000;
            buf = mrutils::strcpy(buf,min);
            buf = mrutils::strcpy(buf," m ");
        }
        if (time > 1000) {
            sec = time / 1000;
            time -= sec * 1000;
            buf = mrutils::strcpy(buf,sec);
            buf = mrutils::strcpy(buf," s ");
        }
        if (time > 0) {
            buf = mrutils::strcpy(buf,time);
            buf = mrutils::strcpy(buf," ms ");
        }

        return buf;
    }

    inline std::string getSecsStr(unsigned time) {
        std::stringstream ss;
        printTimeSecs(ss,time);
        return ss.str();
    }


    /**
     * Time must be in the format hh:mm:ss.mmm
     * */
    inline unsigned getMillisFromRealTime(const char * time) {
        unsigned milliTime = 0;

        for (;!isdigit(*time);++time) if (*time == 0) return milliTime;

        // hour
        milliTime = 3600000u * atoi(time);
        for (;*time != ':';++time) if (*time == 0) return milliTime;

        // minute
        milliTime += 60000u * ( (*(time+1)-'0')*10 + (*(time+2)-'0') );
        if (*(time += 3) != ':') return milliTime;

        // second
        milliTime += 1000u * ( (*(time+1)-'0')*10 + (*(time+2)-'0') );
        if (*(time += 3) != '.') return milliTime;

        // millisecond
        milliTime += 100u * (*++time-'0'); if (*++time == 0) return milliTime;
        milliTime += 10u  * (*time-'0');   if (*++time == 0) return milliTime;
        milliTime +=        (*time-'0'); return milliTime;
    }

    inline void printRealTimeFromMillis(std::ostream &os, unsigned millisFromMidnight) {
        const unsigned hours = millisFromMidnight / 3600000u;
        millisFromMidnight -= hours * 3600000u;
        const unsigned minutes = millisFromMidnight / 60000u;
        millisFromMidnight -= minutes * 60000u;
        const unsigned secs = millisFromMidnight / 1000u;
        millisFromMidnight -= secs * 1000u;

        os << std::setw(2) << std::setfill('0') << hours << ':'
           << std::setw(2) << std::setfill('0') << minutes << ':'
           << std::setw(2) << std::setfill('0') << secs << '.'
           << std::setw(3) << std::setfill('0') << millisFromMidnight;
    }

    inline void printRealTimeFromEpoch(std::ostream& os, time_t now) {
        time_t n = (time_t)now;
        struct tm* ptr = gmtime(&n);
        char buf[18];
        strftime(buf, 18, "%Y%m%d %H:%M:%S", ptr);
        os << buf;
    }

    inline std::string getRealTimeFromEpoch(time_t now) {
        std::stringstream ss;
        printRealTimeFromEpoch(ss, now);
        return ss.str();
    }

    inline std::string getLocalTimeFromEpoch(time_t now) {
        time_t n = (time_t)now;
        struct tm* ptr = localtime(&n);
        char buf[18];
        strftime(buf, 18, "%Y%m%d %H:%M:%S", ptr);
        return buf;
    }

    inline double durationMillis(struct timeval& start, struct timeval& stop) {
        return (1000*(stop.tv_sec - start.tv_sec) + stop.tv_usec/1000. - start.tv_usec/1000.);
    }


    /**
      * Converts (case insensitive) OCT into 10
      */
    inline int stringMonthToNumber(const char * m) {
        switch (*m) {
            case 'A':
            case 'a':
                switch (*(m+1)) {
                    case 'P':
                    case 'p': 
                        return 4;
                    default:
                        return 8;
                } 
            case 'D':
            case 'd':
                return 12;
            case 'F':
            case 'f':
                return 2;
            case 'J':
            case 'j':
                switch (*(m+1)) {
                    case 'a':
                    case 'A':
                        return 1;
                    default:
                        switch (*(m+2)) {
                            case 'l':
                            case 'L':
                                return 7;
                            default:
                                return 6;
                        }
                }
            case 'M':
            case 'm':
                switch (*(m+2)) {
                    case 'r':
                    case 'R':
                        return 3;
                    default:
                        return 5;
                }
            case 'N':
            case 'n':
                return 11;
            case 'O':
            case 'o':
                return 10;
            default:
                return 9;
        }
    }

    /**
      * 10 -> OCT
      */
    static _UNUSED const char * monthToStr[] = {
        ""
       ,"JAN"
       ,"FEB"
       ,"MAR"
       ,"APR"
       ,"MAY"
       ,"JUN"
       ,"JUL"
       ,"AUG"
       ,"SEP"
       ,"OCT"
       ,"NOV"
       ,"DEC"
    }; 
}

/************************
 * CPU cycle counting (getticks)
 ************************/
#include "cycle.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    inline int gettimeofday(struct timeval *tv, void * IGNORED) {
        FILETIME ft;
        unsigned __int64 tmpres = 0;

        if (NULL != tv) {
            GetSystemTimeAsFileTime(&ft);

            tmpres |= ft.dwHighDateTime;
            tmpres <<= 32;
            tmpres |= ft.dwLowDateTime;

            /*converting file time to unix epoch*/
            tmpres /= 10;  /*convert into microseconds*/
            tmpres -= DELTA_EPOCH_IN_MICROSECS;

            tv->tv_sec = (long)(tmpres / 1000000UL);
            tv->tv_usec = (long)(tmpres % 1000000UL);
        }

        return 0;
    }
#endif

#endif
