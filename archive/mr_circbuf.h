#ifndef _MR_LIB_CIRCBUF_H
#define _MR_LIB_CIRCBUF_H

#include "mr_exports.h"
#include <iostream>

namespace mrutils {

template <class T>
class ExpandingCircBuf;

template<class T>
class CircBuf {
    public:
        CircBuf(int size) 
        : alloc(new T[size]), allocated(size)
         ,elem(alloc), size_(size)
         ,count(0), tail(0), seqNum(0)
        {}

        ~CircBuf() {
            delete[] alloc;
        }

    public:

       /******************
        * Convenience fns
        ******************/
        
        inline void clear() { count = 0; }
        inline int size() const { return count; }
        inline int capacity() const { return size_; } 
        inline bool empty() const { return (count == 0); }
        inline bool full() const { return (count == size_); }

    public:

       /****************
        * Main functions
        ****************/

        void resize(int size) {
            int head = (tail+size_-count) % size_;

            if (size < size_) {
                if (head < tail) {
                    if (size > count) {
                        elem += head;
                    } else {
                        elem += tail-size;
                        tail = 0;
                        count = size;
                    }
                } else {
                    if (tail > size) {
                        elem += tail-size;
                        tail = 0;
                        count = size;
                    } else if (count > 0) {
                        int r = size - tail;
                        while (size_ - head > r) ++head;
                        r = size_ - head;
                        count = tail + r;
                        if (r > tail) {
                            r = size_ - size; int from = tail;
                            T* to = elem + tail + r;
                            while (from > 0) *--to = elem[--from];
                            elem += r;
                        } else {
                            T* to = elem+size-(size_-head);
                            while (head < size_) *to++ = elem[head++];
                        }
                    }
                }
            } else {
                if (head < tail) {
                    if (allocated < size) {
                        T* tmp = new T[size];
                        T* to = tmp;
                        T* from = &elem[head];
                        T* end = &elem[tail];
                        while (from < end) *to++ = *from++;
                        delete[] alloc;
                        tail = to - tmp;
                        alloc = tmp; elem = alloc;
                        allocated = size;
                        size_ = size;
                    } else {
                        int d = size - size_;
                        int left = elem - alloc;
                        if (left > d) left = d;
                        tail += left;
                        elem -= left;
                    }
                } else {
                    if (allocated < size) {
                        T* tmp = new T[size];
                        T* to = tmp;
                        for (int i = 0; i < tail; ++i)
                            *to++ = elem[i];
                        tail = to - tmp; to = tmp + head + size - size_;
                        for (int i = head; i < size_; ++i)
                            *to++ = elem[i];
                        delete[] alloc;
                        alloc = tmp; elem = alloc;
                        allocated = size;
                    } else {
                        int left, right;
                        int d = size - size_;
                        if (size_-head>tail) {
                            // left expansion preferred
                            left = elem - alloc;
                            if (left > d) left = d;
                            else if (left < d) {
                                right = alloc + allocated - elem - size_;
                                if (right >= d) left = 0;
                            }

                            right = d - left;
                        } else {
                            // right preferred
                            right = alloc + allocated - elem - size_;
                            if (right > d) right = d;
                            else if (right < d) {
                                left = elem - alloc;
                                if (left >= d) right = 0;
                            }

                            left = d - right;
                        }

                        if (tail == 0 && (left == 0 || right == 0)) {
                            if (right > 0) tail = count;
                            else {
                                tail = 0;
                                elem -= left;
                            }
                        } else {
                            if (left > 0) {
                                // move from 0 to tail left 
                                T* to = elem - left;
                                int from = 0;
                                while (from < tail) 
                                    *to++ = elem[from++];
                            }

                            if (right > 0) {
                                int from = size_;
                                T* to = elem + size_ + right;
                                while (from > head)
                                    *--to = elem[--from];
                            }

                            elem -= left;
                        }
                    }
                }
            }

            size_ = size;
        }

        inline void add(const T& entry) {
            elem[tail] = entry;
            if (++count > size_) count = size_;
            if (++tail == size_) tail = 0;
        }

        /**
          * Instead of copying an entry provided
          * in a function, this returns the entry
          * in the data structure that can then be 
          * modified
          */
        inline T& next() {
            T* t = &elem[tail];
            if (++count > size_) count = size_;
            if (++tail == size_) tail = 0;
            return *t;
        }

        inline const T& remove() { 
            const T& r = getFromHead(0); 
            --count;
            return r; 
        }
        inline void remove(int num) { count -= num; }

        /**
         * Returns offset elements back from the last 
         * element added.
         * */
        inline T& getFromTail(int offset) {
            if (offset > 0) offset *= -1;
            return elem[(size_+tail-1+offset) % size_];
        }

        inline T& getFromHead(int offset) {
            return elem[(tail+size_-count+offset) % size_];
        }

    public:

       /**************
        * Utilities
        **************/

        inline friend std::ostream& operator << (std::ostream& os, CircBuf& b) {
            os << "Circular Buffer (" << b.size() << ", capacity " << b.capacity() << ") ";
            for (int i = 0; i < b.size(); i++) os << b.getFromHead(i) << " ";
            return os;
        }

        // this works best if the elements are char's
        void printRaw() {
            if (count == 0) {
                fprintf(stdout,"[%*s]\n",size_,"");
                return;
            }

            std::cout << "[";

            int head = (tail+size_-count) % size_;
            int i = 0;

            if (head < tail) {
                for (; i < head; ++i) 
                    std::cout << '_';
                for (; i < tail; ++i)
                    std::cout << elem[i];
                for (; i < size_; ++i)
                    std::cout << ' ';
            } else {
                for (; i < tail; ++i) 
                    std::cout << elem[i];
                for (; i < head; ++i)
                    std::cout << '_';
                for (; i < size_; ++i) 
                    std::cout << elem[i];
            }

            std::cout << "]" << count << std::endl;;
        }

    private:
        T* alloc;
        int allocated;

        T *elem;
        int size_;

        int count;
        int tail;

        // for expandingcircbuf
        unsigned seqNum;



    friend class ExpandingCircBuf<T>;
};
}

#endif
