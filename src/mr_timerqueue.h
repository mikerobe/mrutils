#ifndef _TIMERQUEUE_H
#define _TIMERQUEUE_H
#include "mr_exports.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #include "stdafx.h"
#endif
#include "fastdelegate.h"
#include <map>
#include <climits>
#include <iostream>

namespace mrutils {

class _API_ TimerQueue {
    struct Data;
    typedef fastdelegate::FastDelegate2<void*,int> Function;
    typedef std::multimap<unsigned, Data> EventMap;
    typedef std::map<int, std::map<unsigned,int>* > IdMap;

    public:
        TimerQueue() : runModified(0) {}
        ~TimerQueue() {
            for (IdMap::iterator it = ids.begin(); it != ids.end(); ++it) {
                delete it->second;
            }
        }

        inline void add(unsigned time, Function func, void* data, int identifier = 0) {
            runModified++;
            events.insert(std::pair<unsigned,TimerQueue::Data>(
                        time, TimerQueue::Data(func,data,identifier) ));

            std::map<unsigned,int> * idCounts;

            IdMap::iterator it = ids.find( identifier );
            if (it == ids.end()) {
                idCounts = new std::map<unsigned,int>();
                ids[identifier] = idCounts;
            } else idCounts = it->second;

            ++(*idCounts)[time];
        }

        void run(unsigned currentTime);

        bool empty() { return events.empty(); }

        inline void clear() { events.clear(); }

        inline unsigned nextTime() {
            if (events.empty()) return UINT_MAX;
            else return (events.begin()->first);
        }
    private:
        struct Data {
            Function func; void* data; int id;
            Data (Function func, void*data, int id) 
            : func(func), data(data), id(id) {}
        };

        EventMap events;
        IdMap ids;
        volatile int runModified;
};

}

#endif
