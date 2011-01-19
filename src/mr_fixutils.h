#ifndef _MR_CPPLIB_FIXUTILS_H
#define _MR_CPPLIB_FIXUTILS_H

#include "mr_exports.h"
#include <iostream>
#include <ctime>

namespace mrutils {

    std::ostream& operator << (std::ostream& os, const tm& t) {
        return os  << "TM type(" << 1900+t.tm_year << "/" << t.tm_mon << "/" << t.tm_mday 
                   << " " 
                   << t.tm_hour << ":" << t.tm_min << ":" << t.tm_sec << ")";
    }

    // expects 20010101-hh:mm:ss.mmm
    class QuickfixTime {
        public:
            QuickfixTime()
            : initialized(false)
             { 
                 time_t rawtime;
                 timeinfo = *localtime(&rawtime);
                 
             }
            inline unsigned toUnixTime(const char *timeStr_) {
                strcpy(timeStr,timeStr_);
                timeStr[8] = '\0';
                timeinfo.tm_mday = atoi(timeStr+6);
                timeStr[6] = '\0';
                timeinfo.tm_mon  = atoi(timeStr+4)-1;
                timeStr[4] = '\0';
                timeinfo.tm_year = atoi(timeStr) - 1900;
                timeStr[11] = timeStr[14] = '\0';
                timeinfo.tm_hour = atoi(timeStr+9);
                timeinfo.tm_min  = atoi(timeStr+12);
                timeStr[17] = '\0';
                timeinfo.tm_sec  = atoi(timeStr+15);
                return (unsigned) mktime(&timeinfo);

            }
            inline unsigned toMillis(const char * timeStr_) {
                strcpy(timeStr,timeStr_);
                timeStr[8] = '\0';
                timeinfo.tm_mday = atoi(timeStr+6);
                timeStr[6] = '\0';
                timeinfo.tm_mon  = atoi(timeStr+4)-1;
                timeStr[4] = '\0';
                timeinfo.tm_year = atoi(timeStr) - 1900;
                timeStr[11] = timeStr[14] = '\0';
                timeinfo.tm_hour = atoi(timeStr+9);
                timeinfo.tm_min  = atoi(timeStr+12);

                secondsOnly = (timeStr[17] == '\0');

                timeStr[17] = '\0';
                timeinfo.tm_sec  = atoi(timeStr+15);

                if (!initialized) (startTime = mktime(&timeinfo), initialized=true);

                unsigned thisTime = mktime(&timeinfo);
                if (startTime > thisTime) startTime = thisTime;

                return ((mktime(&timeinfo) - startTime) * 1000 + (secondsOnly?0:atoi(timeStr+18)));
            }
        private:
            unsigned startTime;
            struct tm timeinfo;
            char timeStr[22];
            bool initialized;
            bool secondsOnly;
            

    };


}

#endif
