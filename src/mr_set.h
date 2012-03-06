#ifndef _MR_CPPLIB_SET_H
#define _MR_CPPLIB_SET_H

#include <iostream>
#include "mr_numerical.h"
#include "mr_prog.h"

namespace mrutils {

    template<class T>
    static inline bool ptrCmp(const T* const lhs, const T* const rhs) {
        return (*lhs < *rhs);
    }

    template<class T>
    struct ptrCmp_t {
        bool operator()(const T*const lhs, const T* const rhs) const {
            return (*lhs < *rhs);
        }
    };

    /**
      * Use this instead of std::set when the size 
      * of the set is capped at fewer than 
      * ~500 elements
      *
      * NOTE: this implementation uses malloc
      * and does not ensure the destructor calls 
      */
    template <class T, class Comp = std::less<T> > 
    class set {
        template <class K, class V, class Comp2>
        friend class map;

        public:
            set(int initSize = 64, Comp comp = Comp())
            : count(0)
             ,data((T*)malloc(sizeof(T)*initSize))
             ,tail(data)
             ,comp(comp)
             ,alloc(initSize)
            {
                // construct every element for speed
                // of other operations
                // I assume operator= is faster than 
                // (destructor then constructor)
                new(data) T[initSize];
            }

            ~set() { 
                // explicit destructor for each element
                while (alloc) data[--alloc].~T();
                free(data); 
            }

            set(const set& other) 
            : count(other.count)
             ,data((T*)malloc(sizeof(T)*other.alloc))
             ,tail(data+other.count), comp(other.comp)
			 ,alloc(other.alloc)
            {
                memcpy(data,other.data,sizeof(T)*other.count);
            }

            set& operator=(const set& other) {
                if (this != &other) {
                    if (other.alloc > alloc) {
                        data = (T*)realloc(data,sizeof(T)*other.alloc);
                        alloc = other.alloc;
                    }
                    count = other.count;
                    tail = data + count;
                    memcpy(data,other.data,sizeof(T)*other.count);
                    comp = other.comp;
                }

                return *this;
            }

            /**
             * Generally this is used when the default constructor has
             * to be called first (for example with an array of sets)
             */
            void resize(const int size) {
                int oldAlloc = alloc; alloc = size;
                data = (T*)realloc(data,alloc*sizeof(T));
                tail = data + count;

                // explicit construction
                new(data+oldAlloc) T[oldAlloc];
            }

        public:
           /***************
            * Iterator
            ***************/

            typedef T* iterator;
            typedef const T* const_iterator;

            iterator begin() const
            { return data; }

            iterator end() const
            { return tail; }

        public:
           /***************
            * Basic methods
            ***************/

            T& operator[](int pos) 
            { return data[pos]; }

            /**
              * Returns an iterator to the element
              */
            iterator insert(const T& elem) {
                if (count == alloc) expand();
                T* point = mrutils::lower_bound(data, count, elem, comp);
                if (point != tail && !comp(elem,*point)) *point = elem;
                else {
                    mrutils::arrayInsert(point,tail,elem);
                    ++count; ++tail; 
                } return point;
            }

            bool erase(const T& elem) {
                T* point = mrutils::lower_bound(data, count, elem, comp);
                if (point == tail || comp(elem,*point)) return false;
                mrutils::arrayErase(point, tail); 

                --count; --tail; return true;
            }

            /**
              * erase from start up to but not including end
              */
            void erase(iterator start, iterator end) {
                const int c = end - start; 
                if (c == 0) return;
                count -= c; tail -= c;
                for (T* p = data, * pE = p + count;p != pE;) *p++ = *end++;
            }

            iterator insert(iterator position, const T& elem) {
                int pos = position - data;
                if (count == alloc) expand();
                mrutils::arrayInsert(data + pos,tail,elem);
                ++count; ++tail;
                return (data + pos);
            }

            iterator erase(iterator it) {
                mrutils::arrayErase(it, tail); 
                --count; --tail; return it;
            }

            template <class U>
            bool contains(const U& elem) const {
                iterator it = mrutils::lower_bound(data, count, elem, comp); 
                return (it != tail && !comp(elem,*it));
            }

            template <class U>
            iterator find(const U& elem) const { 
                iterator it = mrutils::lower_bound(data, count, elem, comp); 
                if (it == tail || comp(elem,*it)) return tail; return it;
            }

            /**
              * This is used to move an element. 
              * The element originally located at the iterator
              * is being updated & may be moved. 
              * The passed iterator is moved to the new location
              *
              * Elem must NOT be a reference to an entry in the array
              * -- make a copy or call update(point)
              */
            void update(iterator& from, const T& elem) {
                iterator to = (
                    from+1 == tail
                    ?mrutils::lower_bound(data, count-1, elem, comp)
                    :from == data
                     ?mrutils::lower_bound(data+1, count-1, elem, comp)
                     :comp(*(from-1),elem)
                      ?mrutils::lower_bound(from+1, tail-from-1, elem, comp)
                      :mrutils::lower_bound(data, from-data, elem, comp)
                    ); 

                if (to > from) --to;
                if (to == from) return;
                if (to > from) for(;from!=to; ++from) *from = *(from+1);
                else           for(;from!=to; --from) *from = *(from-1);
                *to = elem;
            }
            void update(iterator& point) {
                T elem = *point;
                update(point,elem);
            }

            void clear() 
            { count = 0; tail = data; }

            bool empty() const
            { return (count == 0); }

            int size() const
            { return count; }

        private:
            void expand() {
                fputs("warning: had to expand() an mrutils::set\n",stderr);
                fflush(stderr);
                int oldAlloc = alloc; alloc *= 2; 
                data = (T*)realloc(data,alloc*sizeof(T));
                tail = data + count;

                // explicit construction
                new(data+oldAlloc) T[oldAlloc];
            }

        public:
            int count;
            T* data; 
            T* tail;
            Comp comp;

        private:
            int alloc;
            
    };

}

#endif
