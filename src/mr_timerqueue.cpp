#include "mr_timerqueue.h"
#include <iostream>

void mrutils::TimerQueue::run(unsigned currentTime) {
    int startRunModified = runModified + 1;

    while (runModified != startRunModified) {
        runModified = startRunModified;

        for (EventMap::iterator it = events.begin(); 
                it != events.end() && it->first <= currentTime; ) {

            Function func = it->second.func;
            void * data = it->second.data;

            int leftInQ = --(*ids[it->second.id])[it->first];

            if (leftInQ == 0) {
                if (ids[it->second.id]->size() == 1) {
                    ids.erase(it->second.id);
                } else {
                    ids[it->second.id]->erase(it->first);
                }
            }

            events.erase(it++);

            (func)(data, leftInQ);
            if (runModified != startRunModified) break;
        }
    }

    runModified--;
}
