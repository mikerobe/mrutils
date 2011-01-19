#ifndef _MR_UTILS_MAXRATE_H
#define _MR_UTILS_MAXRATE_H

#include "mr_expcircbuf.h"
#include <algorithm>

namespace mrutils {

    /**
     * MaxRate
     * =======
     * Computes the maximum number of events over a floating interval.
     * Events are specified with a templatized parameter. 
     */
    template<class T>
    class _API_ MaxRate {
        public:
            MaxRate(const T& interval, const unsigned maxEvents = 1024)
            : maxRate(0)
             ,interval(interval)
             ,events(maxEvents)
            {}

        public:
            inline friend MaxRate& operator<<(const MaxRate& mr, const T& now) 
            { mr.add(now); }

            inline void add(const T& now) {
                if (*events.head + interval < now) 
                    events.popHead(std::lower_bound(events.begin(),events.end(),now-interval));
                events.add(now);
                if (events.count > maxRate) maxRate = events.count;
            }

            unsigned maxRate;
            const T interval;

        private:
            mrutils::ExpCircBuf<T> events;
    };

}

#endif
