#ifndef _MR_CPPLIB_FIXED_INTERVAL_H
#define _MR_CPPLIB_FIXED_INTERVAL_H

namespace mrutils {

    template<class Time, class Data>
    class FixedInterval {
        public:
            FixedInterval(const Time& interval)
            : interval(interval)
             ,time(), data()
             ,init_(false)
            { }

        public:
           /**
            * While this is true, print the time and use data in this
            * structure. Then update the data.
            */
            inline bool nextLine(const Time& now) {
                if (!init_) {
                    time = now; init_ = true;
                    return false;
                } else if (time + interval < now) {
                    time += interval;
                    return true;
                } else return false;
            }

            inline void clear() { init_ = false; }

        public:
            const Time interval;
            Time time; Data data;

        private:
            bool init_;

    };

}

#endif
