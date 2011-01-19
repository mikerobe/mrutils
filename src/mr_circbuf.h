#ifndef _MR_LIB_CIRCBUF_H
#define _MR_LIB_CIRCBUF_H

#include "mr_exports.h"
#include "mr_strutils.h"
#include <iostream>

#ifdef _ITERATOR_IMPLEMENTATION
#error "_ITERATOR_IMPLEMENTATION already defined"
#endif

#define _ITERATOR_IMPLEMENTATION                                                \
    inline self_t& operator=(const self_t& other) {                             \
        if (this != &other) {                                                   \
            elem = other.elem;                                                  \
            n = other.n;                                                        \
            cb = other.cb;                                                      \
        } return *this;                                                         \
    }                                                                           \
                                                                                \
    inline bool operator==(const self_t& other) const                           \
    { return (n == other.n); }                                                  \
                                                                                \
    inline bool operator!=(const self_t& other) const                           \
    { return (n != other.n); }                                                  \
                                                                                \
    inline self_t& operator++() {                                               \
        ++n; if (++elem == cb->end_) elem = cb->data;                           \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t& operator--() {                                               \
        --n; if (elem == cb->data) elem = cb->end_ - 1; else --elem;            \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t operator++(int) {                                             \
        self_t tmp(*this);                                                      \
        ++n; if (++elem == cb->end_) elem = cb->data;                           \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline self_t operator--(int) {                                             \
        self_t tmp(*this);                                                      \
        --n; if (elem == cb->data) elem = cb->end_ - 1; else --elem;            \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline bool operator<(const self_t& other) const {                          \
        return (n < other.n);                                                   \
    }                                                                           \
                                                                                \
    inline self_t& operator+=(const int diff) {                                 \
        n += diff;                                                              \
        if ((elem += diff) >= cb->end_)                                         \
            elem = cb->data + (elem - cb->end_);                                \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t& operator-=(const int diff) {                                 \
        n -= diff;                                                              \
        if ((elem -= diff) < cb->data)                                          \
            elem = cb->end_ + (elem - cb->data);                                \
    }                                                                           \
                                                                                \
    inline self_t operator+(const int diff) const {                             \
        self_t tmp(*this);                                                      \
        tmp.n += diff;                                                          \
        if ((tmp.elem += diff) >= cb->end_)                                     \
            tmp.elem = cb->data + (tmp.elem - cb->end_);                        \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline self_t operator-(const int diff) const {                             \
        self_t tmp(*this);                                                      \
        tmp.n -= diff;                                                          \
        if ((tmp.elem -= diff) < cb->data)                                      \
            tmp.elem = cb->end_ + (tmp.elem - cb->data);                        \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline int operator-(const self_t& other) const {                           \
        return (n - other.n);                                                   \
    }                                                                           \

#define _REV_ITERATOR_IMPLEMENTATION                                            \
    inline self_t& operator=(const self_t& other) {                             \
        if (this != &other) {                                                   \
            elem = other.elem;                                                  \
            n = other.n;                                                        \
            cb = other.cb;                                                      \
        } return *this;                                                         \
    }                                                                           \
                                                                                \
    inline bool operator==(const self_t& other) const                           \
    { return (n == other.n); }                                                  \
                                                                                \
    inline bool operator!=(const self_t& other) const                           \
    { return (n != other.n); }                                                  \
                                                                                \
    inline self_t& operator++() {                                               \
        --n; if (elem == cb->data) elem = cb->end_ - 1; else --elem;            \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t& operator--() {                                               \
        ++n; if (++elem == cb->end_) elem = cb->data;                           \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t operator++(int) {                                             \
        self_t tmp(*this);                                                      \
        --n; if (elem == cb->data) elem = cb->end_ - 1; else --elem;            \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline self_t operator--(int) {                                             \
        self_t tmp(*this);                                                      \
        ++n; if (++elem == cb->end_) elem = cb->data;                           \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline bool operator<(const self_t& other) const {                          \
        return (n > other.n);                                                   \
    }                                                                           \
                                                                                \
    inline self_t& operator+=(const int diff) {                                 \
        n -= diff;                                                              \
        if ((elem -= diff) < cb->data)                                          \
            elem = cb->end_ + (elem - cb->data);                                \
    }                                                                           \
                                                                                \
    inline self_t& operator-=(const int diff) {                                 \
        n += diff;                                                              \
        if ((elem += diff) >= cb->end_)                                         \
            elem = cb->data + (elem - cb->end_);                                \
        return *this;                                                           \
    }                                                                           \
                                                                                \
    inline self_t operator+(const int diff) const {                             \
        self_t tmp(*this);                                                      \
        tmp.n -= diff;                                                          \
        if ((tmp.elem -= diff) < cb->data)                                      \
            tmp.elem = cb->end_ + (tmp.elem - cb->data);                        \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline self_t operator-(const int diff) const {                             \
        self_t tmp(*this);                                                      \
        tmp.n += diff;                                                          \
        if ((tmp.elem += diff) >= cb->end_)                                     \
            tmp.elem = cb->data + (tmp.elem - cb->end_);                        \
        return tmp;                                                             \
    }                                                                           \
                                                                                \
    inline int operator-(const self_t& other) const {                           \
        return (other.n - n);                                                   \
    }                                                                           \

namespace mrutils {

   /**
    * CircBuf
    * =======
    */
    template<class T>
    class _API_ CircBuf {
        friend class iterator;

        public:
            CircBuf(const unsigned size)
            : alloc(size)
             ,count(0)
             ,head((T*)malloc(sizeof(T)*alloc))
             ,tail(head), data(head)
             ,end_(data + alloc)
            {}

            CircBuf(const CircBuf& other)
            : alloc(other.alloc)
             ,count(other.count)
             ,data((T*)malloc(sizeof(T)*alloc))
             ,end_(data + alloc)
            {
                if (other.count > 0) {
                    if (other.head < other.tail) {
                        head = data + (other.head - other.data); tail = head;
                        for (T * it = other.head
                            ;it != other.tail
                            ; ++it, ++tail) new(tail) T(*it);
                    } else {
                        T * itNew = data, *it = other.data;
                        for (;it != other.tail; ++it, ++itNew) new(itNew) T(*it);
                        tail = itNew; itNew += other.head - other.tail; head = itNew;
                        for (;it != other.end_; ++it, ++itNew) new(itNew) T(*it);
                    }
                } else head = tail = data;
            }


            virtual ~CircBuf() {
                for (iterator it = begin(), itE = end()
                    ;it != itE; ++it) it->~T();
                free(data);
            }

        public:
            class iterator : public std::iterator<std::random_access_iterator_tag, T> {
                friend class CircBuf<T>;
                typedef iterator self_t;

                protected:
                    iterator(const int n, T * elem, const CircBuf<T>* cb)
                    : elem(elem), n(n)
                     ,cb(cb)
                    {}

                public:
                    iterator(const self_t& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}

                    // for use by std::algorithm
                    iterator() {}
                public:

                    _ITERATOR_IMPLEMENTATION

                    inline T& operator*() const { return *elem; }
                    inline T* operator->() { return elem; }

                protected:
                    T * elem; int n; const CircBuf<T>* cb;
            };

            class reverse_iterator : public std::iterator<std::random_access_iterator_tag, T> {
                friend class CircBuf<T>;
                typedef reverse_iterator self_t;

                protected:
                    reverse_iterator(const int n, T * elem, const CircBuf<T>* cb)
                    : elem(elem), n(n)
                     ,cb(cb)
                    {}

                public:
                    reverse_iterator(const self_t& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}

                    // for use by std::algorithm
                    reverse_iterator() {}
                public:

                    _REV_ITERATOR_IMPLEMENTATION

                    inline T& operator*() const { return *elem; }
                    inline T* operator->() { return elem; }

                protected:
                    T * elem; int n; const CircBuf<T>* cb;
            };

            class const_iterator : public std::iterator<std::random_access_iterator_tag, T> {
                friend class CircBuf<T>;
                typedef const_iterator self_t;

                protected:
                    const_iterator(const int n, T * elem, const CircBuf<T>* cb)
                    : elem(elem), n(n)
                     ,cb(cb)
                    {}

                public:
                    const_iterator(const self_t& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}

                    const_iterator(const iterator& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}
                    
                    // for use by std::algorithm
                    const_iterator() {}

                public:
                    _ITERATOR_IMPLEMENTATION

                    inline self_t& operator=(const iterator& other) {
                        elem = other.elem; n = other.n; cb = other.cb;
                        return *this;
                    }

                    inline bool operator==(const iterator& other) const 
                    { return (n == other.n); }

                    inline bool operator!=(const iterator& other) const 
                    { return (n != other.n); }

                    inline const T& operator*() const { return *elem; }
                    inline const T* operator->() { return elem; }

                protected:
                    const T * elem; int n; const CircBuf<T>* cb;
            };

            class const_reverse_iterator : public std::iterator<std::random_access_iterator_tag, T> {
                friend class CircBuf<T>;
                typedef const_reverse_iterator self_t;

                protected:
                    const_reverse_iterator(const int n, T * elem, const CircBuf<T>* cb)
                    : elem(elem), n(n)
                     ,cb(cb)
                    {}

                public:
                    const_reverse_iterator(const self_t& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}

                    const_reverse_iterator(const reverse_iterator& other)
                    : elem(other.elem), n(other.n), cb(other.cb) {}
                    
                    // for use by std::algorithm
                    const_reverse_iterator() {}

                public:
                    _REV_ITERATOR_IMPLEMENTATION

                    inline self_t& operator=(const reverse_iterator& other) {
                        elem = other.elem; n = other.n; cb = other.cb;
                        return *this;
                    }

                    inline bool operator==(const reverse_iterator& other) const 
                    { return (n == other.n); }

                    inline bool operator!=(const reverse_iterator& other) const 
                    { return (n != other.n); }

                    inline const T& operator*() const { return *elem; }
                    inline const T* operator->() { return elem; }

                protected:
                    const T * elem; int n; const CircBuf<T>* cb;
            };

        public:
           /****************
            * Add/Remove
            ****************/

            inline void clear() { 
                // this whole block will disappear with noop destructor
                if (head < tail) {
                    for (T* it = head; it != tail; ++it) 
                        it->~T();
                } else {
                    for (T* it = data; it != tail; ++it)
                        it->~T();
                    for (T* it = head; it != end_; ++it)
                        it->~T();
                }

                count = 0; tail = head; 
            }

            inline friend CircBuf<T>& operator<<(CircBuf<T>& cb, const T& elem) 
            { return cb.add(elem); }

            inline CircBuf<T>& add(const T& elem) {
                if (count < alloc) {
                    new(tail) T(elem);
                    if (++tail == end_) tail = data;
                    ++count; 
                } else { 
                    head->~T(); new(tail) T(elem);
                    if (++tail == end_) tail = data;
                    head = tail;
                } return *this;
            }

            inline T& next() {
                T * elem = tail;

                if (count < alloc) {
                    *new(tail)T();
                    if (++tail == end_) tail = data;
                    ++count;
                } else {
                    head->~T(); *new(tail)T();
                    if (++tail == end_) tail = data;
                    head = tail;
                } return *elem;
            }

            inline iterator begin()
            { return iterator(0,head,this); }
            inline iterator end()
            { return iterator(count,tail,this); }

            inline reverse_iterator rbegin()
            { return reverse_iterator(count-1,(tail==data?end_:tail)-1,this); }
            inline reverse_iterator rend()
            { return reverse_iterator(-1,(head==data?end_:head)-1,this); }

            inline const_iterator begin() const 
            { return const_iterator(0,head,this); }
            inline const_iterator end() const 
            { return const_iterator(count,tail,this); }

            inline const_reverse_iterator rbegin() const
            { return const_reverse_iterator(count-1,tail,this); }
            inline const_reverse_iterator rend() const
            { return const_reverse_iterator(-1,(head==data?end_:head)-1,this); }

            inline void popHead() {
                --count; head->~T();
                if (++head == end_) head = data;
            }

            inline void popTail() {
                --count; (tail == data?tail = end_-1:--tail)->~T();
            }

            inline T& peekHead(const int offset = 0) const {
                T* elem = head + offset;
                if (elem >= end_) elem = data + (elem - end_);
                return *elem;
            }

            inline T& operator[](const int offset) const { 
                T* elem = head + offset;
                if (elem >= end_) elem = data + (elem - end_);
                return *elem;
            }

            inline T& peekTail(const int offset = 0) const {
                T* elem = tail - offset - 1;
                if (elem < data) elem = end_ + (elem - data);
                return *elem;
            }

            /**
             * Erases the first num elements
             */
            inline void popHead(const int num) {
                // this block disappears with noop destructor
                int i = 0;
                for (T* it = head; it != end_ && i < num; ++it, ++i) it->~T();
                for (T* it = data; i < num; ++it, ++i) it->~T();

                count -= num;
                if ((head += num) > end_)
                    head = data + (head - end_);
            }

            /**
             * Erases the last num elements
             */
            inline void popTail(const int num) {
                count -= num;
                if ((tail -= num) < data)
                    tail = end_ + tail - data;

                // all of the below will disappear if destructor is noop
                int i = 0;
                for (T* it = tail; it >= data && i > num; --it) it->~T();
                for (T* it = end_-1; i > num; --it) it->~T();
            }

            /**
             * Erases from head up to & not including iterator
             */
            inline void popHead(const iterator& iter) {
                // for loop optimized away if destructor is noop
                for (T* it = head; it != iter.elem;) 
                { it->~T(); if (++it == end_) it = data; }

                count -= iter.n; head = iter.elem;
            }

            /**
             * Erases from iterator (inclusive) to tail
             */
            inline void popTail(const iterator& iter) {
                // for loop optimized away if destructor is noop
                for (T* it = iter.elem; it != tail;) 
                { it->~T(); if (++it == end_) it = data; }

                count = iter.n;
                tail = iter.elem;
            }


        public:
           /***************
            * Utilities
            ***************/

            inline unsigned size() const { return count; }
            inline bool empty() const { return (count == 0); }

            inline friend std::ostream& operator<<(std::ostream& os, const CircBuf<T>& cb) 
            { return oprintf(os,"CircBuf (%u/%u)",cb.count,cb.alloc); }

            inline void print(std::ostream& os = std::cout, bool inOrder = true) const;

        public:
            const unsigned alloc;

            unsigned count;
            T * head, * tail;

            T * const data;

        protected:
            T * const end_;

        private:
            CircBuf& operator=(const CircBuf&); // disallowed
            bool operator==(const CircBuf&); // disallowed
    };

    template<class T>
    void CircBuf<T>::print(std::ostream& os, bool inOrder) const {
        os << *this << "\n";
        if (count == 0) return;

        static const int width = 10;

        if (!inOrder) {
            if (head < tail) {
                unsigned i = 0;

                for (const T * it = data; it != head; ++it) 
                    os << "    " << i++ << "\n"; 

                for (const T * it = head; it != tail; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }

                for (const T * it = tail; it != end_; ++it) 
                    os << "    " << i++ << "\n"; 
            } else {
                unsigned i = 0;

                for (const T * it = data; it != tail; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }

                for (const T * it = tail; it != head; ++it) 
                    os << "    " << i++ << "\n"; 

                for (const T * it = head; it != end_; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }
            }
        } else {
            if (head < tail) {
                unsigned i = 0;
                for (const T * it = head; it != tail; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }
                for (const T * it = tail; it != end_; ++it) 
                    os << "    " << i++ << "\n"; 
            } else {
                unsigned i = 0;
                for (const T * it = head; it != end_; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }
                for (const T * it = data; it != tail; ++it) {
                    os << "    "; os.width(width); os.setf(std::ios::left);
                    os << i++ << *it << "\n";
                }
            }
        }
    }
}

#undef _ITERATOR_IMPLEMENTATION

#endif
