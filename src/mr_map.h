#ifndef _MR_CPPLIB_MAP_H
#define _MR_CPPLIB_MAP_H

#include "mr_set.h"

namespace mrutils {

/**
  * Use this instead of std::map when the size 
  * of the set is capped at fewer than 
  * ~500 elements
  */
template <class K, class V, class Comp = std::less<K> > 
class map {
    public:
        struct pair {
            pair(const K& first)
            : first(first), second() {}

            pair(const K& first, const V& second)
            : first(first), second(second) {}

            K first; V second;
        };

    private:
        struct mapComp {
            inline bool operator()(const pair* a, const pair* b) const 
            { return comp(a->first,b->first); }

            inline bool operator()(const pair* a, const K& b) const 
            { return comp(a->first,b); }

            inline bool operator()(const K& a, const pair* b) const 
            { return comp(a,b->first); }

            Comp comp;
        };

    public:
        map(int initSize=64)
        : set(initSize)
         ,count(set.count),data(set.data),tail(set.tail)
        {}

        ~map() {
            for (pair** it = set.data; it != set.tail; ++it) 
                delete *it;
        }

        map(const map& other) 
        : set(other.set)
         ,count(set.count), data(set.data), tail(set.tail)
        {}

        inline map& operator=(const map& other) {
            if (this != &other) set = other.set;
            return *this;
        }

    public:
       /***************
        * Iterator
        ***************/

        typedef pair** iterator;

        inline iterator begin() const
        { return set.data; }

        inline iterator end() const
        { return set.tail; }

    public:
       /***************
        * Basic methods
        ***************/

        inline V& operator[](const K& first) { 
            iterator it = mrutils::lower_bound(set.data, set.count, first, set.comp); 
            if (it != set.tail && !set.comp.comp(first,(*it)->first)) return (*it)->second;
            pair* p = new pair(first);
            set.insert(it,p);
            return p->second;
        }

        inline iterator lower_bound(const K& first) const {
            return mrutils::lower_bound(set.data, set.count, first, set.comp); 
        }

        inline iterator find(const K& first) const { 
            iterator it = mrutils::lower_bound(set.data, set.count, first, set.comp); 
            if (it == set.tail || set.comp.comp(first,(*it)->first)) return set.tail; return it;
        }

        inline bool contains(const K& first) const { return set.contains(first); }

        inline iterator insert(const K& first, const V& second) {
            if (count == set.alloc) set.expand();
            iterator point = mrutils::lower_bound(data, count, first, set.comp);
            if (point != tail && !set.comp.comp(first,(*point)->first)) return ( (*point)->second = second, point );

            mrutils::arrayInsert(point,tail,new pair(first,second));
            ++count; ++tail;
            return point;
        }

        inline iterator erase(iterator it) 
        { return set.erase(it); }

        inline void clear() 
        { set.clear(); }

        inline bool empty() const
        { return (count == 0); }

        inline int size() const
        { return count; }

    public:
        private: mrutils::set<pair*, mapComp> set; public:

        int &count;
        iterator& data;
        iterator& tail;
};

}

#endif
