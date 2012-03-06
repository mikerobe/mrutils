#ifndef _MR_CPPLIB_THREADS_H
#define _MR_CPPLIB_THREADS_H

#include "mr_exports.h"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    #if !defined(_MT)
        #error _beginthreadex requires a multithreaded C run-time library.
    #endif

    #include <process.h>
    #include "critsectex.h"

    #define ATOMIC_INT volatile int
    #define ATOMIC_INCREMENT(a) InterlockedIncrement(&a)
    #define ATOMIC_DECREMENT(a) InterlockedDecrement(&a)
    #define ATOMIC_AND(a,b) _InterlockedAnd(&a,b)
    #define ATOMIC_OR(a,b) _InterlockedOr(&a,b)
    #define ATOMIC_GET_AND_SET(a,b) _InterlockedExchange(&a,b)
#else
    #include <pthread.h>
    #include <unistd.h>

    #define ATOMIC_INT volatile int
    #define ATOMIC_INCREMENT(a) __sync_add_and_fetch(&a,1)
    #define ATOMIC_DECREMENT(a) __sync_sub_and_fetch(&a,1)
    #define ATOMIC_GET_AND_INCREMENT(a) __sync_fetch_and_add(&a,1)
    #define ATOMIC_GET_AND_DECREMENT(a) __sync_fetch_and_sub(&a,1)
    #define ATOMIC_AND(a,b) __sync_fetch_and_and(&a,b)
    #define ATOMIC_OR(a,b) __sync_fetch_and_or(&a,b)
    #define ATOMIC_GET_AND_SET(a,b) __sync_lock_test_and_set(&a,b)
#endif

#include "fastdelegate.h"

namespace mrutils {
    typedef fastdelegate::FastDelegate1<void*> threadFunction;

#   if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
    typedef HANDLE thread_t;
    typedef mrutils::p::WindowsFastMutex* mutex_t;
#   else
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;
#   endif

	namespace p {
	struct _threadData { 
		threadFunction func; void * data;

#       if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
        HANDLE thread;

        _threadData(threadFunction func, void * data)
        : func(func), data(data)
        { }

        ~_threadData() {
            CloseHandle(thread);
        }

        #else
        pthread_attr_t attr;
        pthread_t thread;

        _threadData(threadFunction func, void * data) 
        : func(func), data(data)
        {
            pthread_attr_init(&attr);
        }

        ~_threadData() {
            pthread_attr_destroy(&attr);
        }
#       endif
	};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
	static inline unsigned __stdcall _threadRun(void* threadData) {
		(((_threadData*)threadData)->func)(((_threadData*)threadData)->data);
		delete (_threadData*)threadData;
		return 0;
	}

    class WindowsFastMutex {
        public:
            WindowsFastMutex() 
            : state(false)
             , m(0x80000400)
            { 
            }

        public:
            CritSectEx m;
            bool state;
    };

#else
	static inline void* _threadRun(void *threadData) {
		(((_threadData*)threadData)->func)(((_threadData*)threadData)->data);
		delete (_threadData*)threadData;
		pthread_exit(NULL);
        return NULL;
	}
#endif
	}

    static inline mutex_t mutexCreate() {
        #ifdef WIN32
            return new p::WindowsFastMutex();

            //mutex_t m = new CRITICAL_SECTION();
            //InitializeCriticalSectionAndSpinCount(m, 0x80000400);
            //return m;

            //return CreateMutex(NULL,false,NULL);
        #else
            mutex_t m = PTHREAD_MUTEX_INITIALIZER;
            return m;
        #endif
    }

    static inline void mutexDelete(_UNUSED mutex_t& m) {
        #ifdef WIN32
            delete m;
            //DeleteCriticalSection(m);
            //delete m;
        #else
            //pthread_mutex_destroy( &m );
        #endif
    }

    static inline bool mutexTryAcquire(mutex_t& m) {
        #ifdef WIN32
            return m->m.Lock(m->state, 0);
            //return TryEnterCriticalSection(m);
            //return (WaitForSingleObject(m, 1) != WAIT_TIMEOUT);
        #else
            return (pthread_mutex_trylock( &m ) == 0);
        #endif
    }

    static inline bool mutexWaitAcquire(mutex_t& m, int secs) {
        #ifdef WIN32
            return m->m.Lock(m->state, secs*1000);
            //return TryEnterCriticalSection(m);
            //return (WaitForSingleObject(m, 1) != WAIT_TIMEOUT);
        #elif defined(__APPLE__) || defined(__CYGWIN__)
            // no pthread_mutex_timedlock
            unsigned millis = secs * 1000;
            do {
                if (mrutils::mutexTryAcquire(m)) return true;
                usleep(100000);
                millis -= 100;
            } while (millis > 0);

            return false;
        #else
            struct timespec deltatime;
            deltatime.tv_sec = secs; deltatime.tv_nsec = 0;
            return (pthread_mutex_timedlock( &m, &deltatime ) == 0);
        #endif
    }

    static inline void mutexAcquire(mutex_t& m) {
        #ifdef WIN32
            m->m.Lock(m->state, INFINITE);
            //EnterCriticalSection(m);
            //WaitForSingleObject(m, INFINITE);
        #else
            pthread_mutex_lock( &m );
        #endif
    }
    static inline void mutexRelease(mutex_t& m) {
        #ifdef WIN32
            m->m.Unlock(m->state);
           //LeaveCriticalSection(m);
           //ReleaseMutex(m);
        #else
            pthread_mutex_unlock( &m );
        #endif
    }

    class mutexScopeAcquire {
        public:
            inline mutexScopeAcquire(mutex_t& m)
            : m(m) 
            { mrutils::mutexAcquire(m); }

            inline ~mutexScopeAcquire() 
            { mrutils::mutexRelease(m); }
        private:
            mutex_t & m;
    };

    static inline void sleep(unsigned millis) {
        #if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            Sleep(millis);
        #else
            usleep(millis * 1000L);
        #endif
    }

    static inline thread_t threadRun(threadFunction func, void*data = NULL, bool detached = true) {
		p::_threadData *d = new p::_threadData(func,data);

		#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            #error WINDOWS TODO handle detached thread option
            d->thread = (HANDLE)_beginthreadex(NULL, 0, p::_threadRun, d, 0, NULL);
            return d->thread;
        #else
            pthread_attr_setdetachstate(&d->attr,detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
			if (pthread_create(&d->thread, &d->attr, p::_threadRun, d) != 0) return NULL;
            return d->thread;
        #endif
    }

    static inline int threadJoin(thread_t threadId) { 
		#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__TOS_WIN__)
            #error "TODO windows threading"
        #else
            return (pthread_join(threadId,NULL));
        #endif
    }


    /**
     * Setup is 2 threads, one needs to signal the other.
     * It is assumed that whoever creates the signal is the thread
     * that will be doing the signalling.
     */
    class Signal {
        public:
            Signal()
            : push(true), pop(true)
            {
                mutex[0] = mrutils::mutexCreate();
                mutex[1] = mrutils::mutexCreate();
                mrutils::mutexAcquire(mutex[0]);
            }

            ~Signal()
			{
                mrutils::mutexDelete(mutex[0]);
                mrutils::mutexDelete(mutex[1]);
            }

        public:

            bool wait(int secs)
			{
                if (mrutils::mutexWaitAcquire(mutex[!pop], secs))
				{
                    pop = !pop;
                    mrutils::mutexRelease(mutex[pop]);
                    return true;
                }
				else
				{
					return false;
				}
            }

            void wait()
			{
                mrutils::mutexAcquire(mutex[!pop]);
                pop = !pop;
                mrutils::mutexRelease(mutex[pop]);
            }

            void send()
			{
                if (mrutils::mutexTryAcquire(mutex[push]))
				{
                    if (pop == push)
						mrutils::mutexRelease(mutex[push = !push]);
                    else
						mrutils::mutexRelease(mutex[push]);
                }
            }

        private:
            mutex_t mutex[2];
            bool push, pop;
    };

}


#endif
