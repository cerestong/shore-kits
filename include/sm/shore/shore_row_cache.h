/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file:   shore_row_cache.h
 *
 *  @brief:  Cache for tuples (row_impl<>) used in Shore
 *
 *  @note:   row_cache_t           - class for tuple caching
 *
 *  @author: Ippokratis Pandis, January 2008
 *
 */

#ifndef __SHORE_ROW_CACHE_H
#define __SHORE_ROW_CACHE_H

// sparcv9 only at the moment
#include <atomic.h>

#include "util/guard.h"

#include "shore_row_impl.h"


// define cache stats to get some statistics about the tuple cache
#undef CACHE_STATS
//#define CACHE_STATS


ENTER_NAMESPACE(shore);


const int DEFAULT_INIT_ROW_COUNT = 50;


template <class TableDesc>
class row_cache_t : protected atomic_stack
{
    typedef row_impl<TableDesc> table_tuple;
private:

    TableDesc* _ptable; 

#ifdef CACHE_STATS    
    // stats
    int _tuple_requests; 
    tatas_lock _requests_lock;
    int _tuple_setups;
    tatas_lock _setups_lock;
#endif              

public:

    row_cache_t(TableDesc* ptable, int init_count=DEFAULT_INIT_ROW_COUNT) 
        : atomic_stack(-sizeof(ptr)),
          _ptable(ptable)
#ifdef CACHE_STATS
        , _tuple_requests(0), _tuple_setups(0)
#endif
    { 
        assert (_ptable);
        assert (init_count >= 0); 

        // start with a non-empty pool, so that threads don't race
        // at the beginning
        ptr* head = NULL;
        for (int i=0; i<init_count; i++) {                       
            vpn u = {borrow()};
            u.p->next = head;
            head = u.p;
        }

        for (int i=0; i<init_count; i++) {
            pvn p;
            p.p = head;
            head = head->next;
            giveback((table_tuple*)p.v);
        }
    }

    
    /* Destroys the cache, calling the destructor for all the objects 
     * it is hoarding.
     */
    ~row_cache_t() 
    {
        vpn val;
        void* v = NULL;
        int icount = 0;
        while ( (v=pop()) ) {
            val.v = v;
            val.n += +_offset; // back up to the real
            ((table_tuple*)v)->freevalues();
            free(val.v);
            ++icount;
        }

        TRACE( TRACE_TRX_FLOW, "Deleted: (%d) (%s)\n", icount, _ptable->name());
    }

    /* Return an unused object, if cache empty allocate and return a new one
     */
    table_tuple* borrow() 
    {
#ifdef CACHE_STATS
        CRITICAL_SECTION( rcs, _requests_lock);
        ++_tuple_requests;
#endif

        void* val = pop();
        if (val) return ((table_tuple*)(val));

#ifdef CACHE_STATS
        CRITICAL_SECTION( scs, _setups_lock);
        ++_tuple_setups;
#endif

        // allocates and setups a new table_tuple
        vpn u = { malloc(sizeof(table_tuple)) };
        if (!u.v) u.v = null();
        table_tuple* rp = (table_tuple*)prepare(u);
        rp->setup(_ptable);
	return (rp);
    }
    
    /* Returns an object to the cache. The object is reset and put on the
     * free list.
     */
    void giveback(table_tuple* ptn) 
    {        
        assert (ptn);
        assert (ptn->_ptable == _ptable);

        // reset the object
        ptn->reset();
        
        // avoid pointer aliasing problems with the optimizer
        union { table_tuple* t; void* v; } u = {ptn};
        
        // give it back
        push(u.v);
    }    


#ifdef CACHE_STATS    
    int  setup_count() { return (_tuple_setups); }
    int  request_count() { return (_tuple_requests); }
    void print_stats() {
        TRACE( TRACE_STATISTICS, "Requests: (%d)\n", _tuple_requests);
        TRACE( TRACE_STATISTICS, "Setups  : (%d)\n", _tuple_setups);
    }
#endif 

}; // EOF: row_cache_t


EXIT_NAMESPACE(shore);


#endif /* __SHORE_ROW_CACHE_H */
