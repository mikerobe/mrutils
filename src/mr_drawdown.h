#ifndef _MR_CPPLIB_DRAWDOWN_H
#define _MR_CPPLIB_DRAWDOWN_H

#include "mr_set.h"

/**
 * Drawdown
 * ========
 * Two types: one storing the time (or date, etc) of each drawdown start
 * & end and another that stores only the amounts.
 *
 * This simpler implementation only identifies the largest drawdown 
 */

namespace { class NoClass {}; }

namespace mrutils {

    template<class Value, class Time=NoClass>
    class Drawdown {
        public:
            struct obs_t { 
                obs_t() : time(),value() {} 

                Time time; Value value; 
            };

            struct drawdown_t { 
                drawdown_t() : start(), end(), drawdown() {}

                obs_t start, end; 
                Value drawdown;

                inline bool operator<(const drawdown_t& other) const 
                { return (other.drawdown < drawdown); }
            };

        public:
            Drawdown()
            : drawdown(drawdowns), current(drawdowns)
             ,first(true)
            {}

        public:

            inline void add(const Value& value, const Time& time) {
                if (first) { current->start.time = time; current->end.time = time; first = false; }
                if (current->start.value < value) {
                    if (current->end.value != current->start.value) 
                        current = drawdowns + (drawdown == drawdowns ? 1 : 0);
                    current->start.time = time; current->end.time = time;
                    current->start.value = value; current->end.value = value;
                } else if (value < current->end.value) {
                    current->end.time = time; current->end.value = value;
                    current->drawdown = current->start.value - value;
                    if (drawdown->drawdown < current->drawdown) drawdown = current;
                }
            }

        public:
            drawdown_t * drawdown;

        protected:
            drawdown_t * current;
            drawdown_t drawdowns[2];
            bool first;
    };

    template<class Value>
    class Drawdown<Value,NoClass> {
        public:
            struct drawdown_t { 
                drawdown_t() : start(), end(), drawdown() {}

                Value start, end, drawdown; 
            };
            
        public:
            Drawdown()
            : drawdown(drawdowns), current(drawdowns)
            {}

        public:
            inline void add(const Value& value) {
                if (current->start < value) {
                    current = drawdowns + (drawdown == drawdowns ? 1 : 0);
                    current->start = value; current->end = value;
                } else if (value < current->end) {
                    current->end = value; current->drawdown = current->start - value;
                    if (drawdown->drawdown < current->drawdown) drawdown = current;
                }
            }

        public:
            drawdown_t * drawdown;

        protected:
            drawdown_t * current;
            drawdown_t drawdowns[2];
    };


}


#endif
