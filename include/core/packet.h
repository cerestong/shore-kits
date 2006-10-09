/* -*- mode:C++; c-basic-offset:4 -*- */

#ifndef __PACKET_H
#define __PACKET_H

#include <list>
#include "core/tuple.h"
#include "core/tuple_fifo.h"
#include "core/functors.h"
#include "core/cpu_bind.h"


ENTER_NAMESPACE(qpipe);

using std::list;



/* exported datatypes */

class   packet_t;
typedef list<packet_t*> packet_list_t;

struct query_plan {
    c_str action;
    c_str filter;
    query_plan const** child_plans;
    int child_count;
    
    query_plan(const c_str &a, const c_str &f, query_plan const** children, int count)
        : action(a), filter(f), child_plans(children), child_count(count)
    {
    }
};


/**
 *  @brief A packet in QPIPE is a unit of work that can be processed
 *  by a stage's worker thread.
 *
 *  After a packet it created, it is given to the dispatcher, which
 *  inserts the packet into some stage's work queue. The packet is
 *  eventually dequeued by a worker thread for that stage, inserted
 *  into that stage's working set, and processed.
 *
 *  Before the dispatcher inserts the next packet into a stage's work
 *  queue, it will check the working set to see if the new packet can
 *  be merged with an existing one that is already being processed.
 */


class packet_t
{

protected:
    query_plan* _plan; 

    bool _merge_enabled;

    static bool is_compatible(query_plan const* a, query_plan const* b) {
        if(!a || !b || strcmp(a->action, b->action))
            return false;

        assert(a->child_count == b->child_count);
        for(int i=0; i < a->child_count; i++) {
            query_plan const* ca = a->child_plans[i];
            query_plan const* cb = b->child_plans[i];
            if(!ca || !cb)
                return false;
            if(strcmp(ca->filter, cb->filter) || !is_compatible(ca, cb))
                return false;
        }

        return true;
    }
    
    virtual bool is_compatible(packet_t* other) {
        return is_compatible(plan(), other->plan());
    }

public:
    
    c_str _packet_id;
    c_str _packet_type;
private:    
    guard<tuple_fifo> _output_buffer;
public:
    guard<tuple_filter_t> _output_filter;

    
    /** Should be set to the stage's _stage_next_tuple field when this
	packet is merged into the stage. Should be initialized to 0
	when the packet is first created. */
    unsigned int _next_tuple_on_merge;

    /** Should be set to _stage_next_tuple_on_merge when a packet is
	re-enqued. This lets the stage processing it know to
	send_eof() when its internal _stage_next_tuple counter reaches
	this number. A value of 0 should indicate that the packet must
	receive all tuples produced by the stage. Should be
	initialized to 0. */
    unsigned int _next_tuple_needed;

    cpu_bind* _cpu_bind;
    


    /* see packet.cpp for documentation */
    packet_t(const c_str    &packet_id,
             const c_str    &packet_type,
             tuple_fifo*     output_buffer,
             tuple_filter_t* output_filter,
             query_plan*     plan,
             bool            merge_enabled=true);


    virtual ~packet_t(void);

    tuple_fifo* release_output_buffer() {
        return _output_buffer.release();
    }
    tuple_fifo* output_buffer() {
        return _output_buffer;
    }
    
    /**
     *  @brief Check whether this packet can be merged with the
     *  specified one.
     *
     *  @return false
     */  
    
    bool is_merge_enabled() {
	return _merge_enabled;
    }

    query_plan const* plan() const {
        return _plan;
    }

    /**
     *  @brief Check whether this packet can be merged with the
     *  specified one.
     *
     *  @return false
     */  
    
    bool is_mergeable(packet_t* other) {
	return is_merge_enabled() && is_compatible(other);
    }


    void disable_merging() {
	_merge_enabled = false;	
    }

};


EXIT_NAMESPACE(qpipe);

#endif