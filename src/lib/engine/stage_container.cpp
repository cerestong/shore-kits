/* -*- mode:C++; c-basic-offset:4 -*- */

#include "stage_container.h"
#include "trace.h"
#include "qpipe_panic.h"
#include "utils.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>
#include <cstring>



// include me last!!!
#include "namespace.h"




// container methods

/**
 *  @brief Stage constructor.
 *
 *  @param container_name The name of this stage container. Useful for
 *  printing debug information. The constructor will create a copy of
 *  this string, so the caller should deallocate it if necessary.
 */
stage_container_t::stage_container_t(const char* container_name,
				     stage_factory_t* stage_maker)
    : _stage_maker(stage_maker)
{

    // copy container name
    if ( asprintf(&_container_name, "%s", container_name) == -1 ) {
	TRACE(TRACE_ALWAYS, "asprintf() failed on stage: %s\n",
	      container_name);
	QPIPE_PANIC();
    }
  
    // mutex that serializes the creation of the worker threads
    pthread_mutex_init_wrapper(&_container_lock, NULL);
  
    // condition variable that worker threads can wait on to
    // deschedule until more packets arrive
    pthread_cond_init_wrapper(&_container_queue_nonempty, NULL);
}



/**
 *  @brief Stage container destructor. Should only be invoked after a
 *  shutdown() has successfully returned and no more worker threads
 *  are in the system. We will delete every entry of the container
 *  queue.
 */
stage_container_t::~stage_container_t(void) {

    // the container owns its name
    free(_container_name);

    // There should be no worker threads accessing the packet queue when
    // this function is called. Otherwise, we get race conditions and
    // invalid memory accesses.
  
    // destroy synch vars    
    pthread_mutex_destroy_wrapper(&_container_lock);
    pthread_cond_destroy_wrapper (&_container_queue_nonempty);
}



/**
 *  THE CALLER MUST BE HOLDING THE _container_lock MUTEX.
 */
void stage_container_t::container_queue_enqueue_no_merge(packet_list_t* packets) {
    _container_queue.push_back(packets);
    pthread_cond_signal_wrapper(&_container_queue_nonempty);
}



/**
 *  @brief Helper function used to add the specified packet as a new
 *  element in the _container_queue and signal a waiting worker
 *  thread.
 *
 *  THE CALLER MUST BE HOLDING THE _container_lock MUTEX.
 */
void stage_container_t::container_queue_enqueue_no_merge(packet_t* packet) {
    packet_list_t* packets = new packet_list_t();
    packets->push_back(packet);
    container_queue_enqueue_no_merge(packets);
}



/**
 *  @brief Helper function used to remove the next packet in this
 *  container queue. If no packets are available, wait for one to
 *  appear.
 *
 *  THE CALLER MUST BE HOLDING THE stage_lock MUTEX.
 */
packet_list_t* stage_container_t::container_queue_dequeue() {

    // wait for a packet to appear
    while ( _container_queue.empty() ) {
	pthread_cond_wait_wrapper( &_container_queue_nonempty, &_container_lock );
    }

    // remove available packet
    packet_list_t* plist = _container_queue.front();
    _container_queue.pop_front();
  
    return plist;
}



/**
 *  @brief Send the specified packet to this container. We will try to
 *  merge the packet into a running stage or into an already enqueued
 *  packet packet list. If we fail, we will wrap the packet in a
 *  packet_list_t and insert the list as a new entry in the container
 *  queue.
 *
 *  Merging will fail if the packet is already marked as non-mergeable
 *  (if its is_merge_enabled() method returns false). It will also
 *  fail if there are no "similar" packets (1) currently being
 *  processed by any stage AND (2) currently enqueued in the container
 *  queue.
 *
 *  @param packet The packet to send to this stage.
 */
void stage_container_t::enqueue(packet_t* packet) {


    assert(packet != NULL);


    // * * * BEGIN CRITICAL SECTION * * *
    critical_section_t cs(&_container_lock);


    // check for non-mergeable packets
    if ( !packet->is_merge_enabled() )  {
	// We are forcing the new packet to not merge with others.
	container_queue_enqueue_no_merge(packet);
	return; 
	// * * * END CRITICAL SECTION * * *
    }


    // try merging with packets in merge_candidates before they
    // disappear or become non-mergeable
    list<stage_adaptor_t*>::iterator sit = _container_current_stages.begin();
    for( ; sit != _container_current_stages.end(); ++sit) {
	// try to merge with this stage
	stage_container_t::stage_adaptor_t* ad = *sit;
	if ( ad->try_merge(packet) ) {
	    /* packet was merged with this existing stage */
	    return;
	    // * * * END CRITICAL SECTION * * *
	}
    }


    // If we are here, we could not merge with any of the running
    // stages. Don't give up. We can still try merging with a packet in
    // the container_queue.
    list<packet_list_t*>::iterator cit = _container_queue.begin();
    for ( ; cit != _container_queue.end(); ++cit) {
	
	packet_list_t* cq_plist = *cit;
	packet_t* cq_packet = cq_plist->front();


	// No need to acquire queue_pack's merge mutex. No other
	// thread can touch it until we release _container_lock.
    
	// We need to check queue_pack's mergeability flag since some
	// packets are non-mergeable from the beginning. Don't need to
	// grab its merge_mutex because its mergeability status could
	// not have changed while it was in the queue.
	if ( cq_packet->is_mergeable(packet) ) {
	    // add this packet to the list of already merged packets
	    // in the container queue
	    cq_plist->push_back(packet);
	    return;
	    // * * * END CRITICAL SECTION * * *
	}
    }


    // No work sharing detected. We can now give up and insert the new
    // packet into the stage_queue.
    container_queue_enqueue_no_merge(packet);
    // * * * END CRITICAL SECTION * * *
};



/**
 *  @brief Worker threads for this stage should invoke this
 *  function. It will return when the stage shuts down.
 */
void stage_container_t::run() {

    while (1) {
	
	// wait for a packet to become available
	critical_section_t cs(&_container_lock);
	packet_list_t* packets = container_queue_dequeue();
	cs.exit();
	
	
	// error checking
	assert( packets != NULL );
	assert( !packets->empty() );


	// TODO: process rebinding instructions here

    
 	pointer_guard_t <stage_t> stage = _stage_maker->create_stage();
        size_t tuple_size = packets->front()->_output_filter->input_tuple_size();
	stage_adaptor_t(this, packets, tuple_size).run_stage(stage);
	

	// TODO: check for container shutdown
    }
}




// stage adaptor methods

/**
 *  @brief Try to merge the specified packet into this stage. This
 *  function assumes that packet has its mergeable flag set to
 *  true.
 *
 *  This method relies on the stage's current state (whether
 *  _stage_accepting_packets is true) as well as the
 *  is_mergeable() method used to compare two packets.
 *
 *  @param packet The packet to try and merge.
 *
 *  @return true if the merge was successful. false otherwise.
 */
bool stage_container_t::stage_adaptor_t::try_merge(packet_t* packet) {
 
    // * * * BEGIN CRITICAL SECTION * * *
    critical_section_t cs(&_stage_adaptor_lock);


    if ( !_still_accepting_packets ) {
	// stage not longer in a state where it can accept new packets
	return false;
	// * * * END CRITICAL SECTION * * *	
    }

    // The _still_accepting_packets flag cannot change since we are
    // holding the _stage_adaptor_lock. We can also safely access
    // _packet for the same reason.
 

    // Mergeability is an equality relation. We can check whether
    // packet is similar to the packets in this stage by simply
    // comparing it with the stage's primary packet.
    if ( !_packet->is_mergeable(packet) ) {
	// packet cannot share work with this stage
	return false;
	// * * * END CRITICAL SECTION * * *
    }
    
    
    // If we are here, we detected work sharing!
    _packet_list->push_front(packet);
    packet->_stage_next_tuple_on_merge = _next_tuple;
    if ( _next_tuple == 1 ) {
	// Stage has not produced any results yet. No difference
	// between (1) merging the packet now and (2) if the packet
	// had been with this stage's packet list from the beginning.
	packet->destroy_subpackets();
    }
    // else: Need to keep the packet's subtree alive until the stage
    // is done. At that point, we need to combine the unfinished
    // packets and resubmit them. We may end up choosing this packet
    // as the primary for that packet set.
    

    // * * * END CRITICAL SECTION * * *
    return true;
}



/**
 *  @brief Outputs a page of tuples to this stage's packet set. The
 *  caller retains ownership of the page.
 */
stage_t::result_t stage_container_t::stage_adaptor_t::output_page(tuple_page_t *page) {

    packet_list_t::iterator it, end;
    unsigned int next_tuple;
    
    
    critical_section_t cs(&_stage_adaptor_lock);
    it  = _packet_list->begin();
    end = _packet_list->end();
    _next_tuple += page->tuple_count();
    next_tuple = _next_tuple;
    cs.exit();

    
    // Any new packets which merge after this point will not 
    // receive this page.
    

    bool packets_remaining = false;
    tuple_t out_tup;

    while (it != end) {


        packet_t* curr_packet = *it;
        bool terminate_curr_packet = false;


        // Drain all tuples in output page into the current packet's
        // output buffer.
        tuple_page_t::iterator page_it;
        for( page_it = page->begin();  page_it != page->end(); ++page_it) {

            // apply current packet's filter to this tuple
            if(curr_packet->_output_filter->select(*page_it)) {

                // this tuple selected by filter!

                // allocate space in the output buffer
                if ( curr_packet->_output_buffer->alloc_tuple(out_tup) ) {
                    // The consumer of the current packet's output
                    // buffer has terminated the buffer! No need to
                    // continue iterating over output page.
                    terminate_curr_packet = true;
                    break;
                }
        
                // write tuple to allocated space
                curr_packet->_output_filter->project(out_tup, *page_it);
            }
        }
        
        
        // If this packet has run more than once, it may have received
        // all the tuples it needs. Check for this case.
        if ( next_tuple == curr_packet->_stage_next_tuple_needed )
            // This packet has received all tuples it needs! Another
            // reason to terminate this packet!
            terminate_curr_packet = true;
        

        // check for packet termination
        if (terminate_curr_packet) {
            // Finishing up a stage packet is tricky. We must treat
            // terminating the primary packet as a special case. The
            // good news is that the finish_packet() method handle all
            // special cases for us.
            finish_packet(curr_packet);
            it = _packet_list->erase(it);
            continue;
        }
 

        ++it;
        packets_remaining = true;
    }
    
    
    if ( packets_remaining )
        // still have packets that need tuples
	return stage_t::RESULT_CONTINUE;
    return stage_t::RESULT_STOP;
}



/**
 *  @brief Send EOF to the packet's output buffer. Delete the buffer
 *  if the consumer has already terminated it. If packet is not the
 *  primary packet for this stage, destroy its subpackets and delete
 *  it.
 *
 *  @param packet The packet to terminate.
 */
void stage_container_t::stage_adaptor_t::finish_packet(packet_t* packet) {

    
    // packet output buffer
    tuple_buffer_t* output_buffer = packet->_output_buffer;
    if ( !output_buffer->send_eof() ) {
        // Consumer has already terminated this buffer! We are now
        // responsible for deleting it.
        delete output_buffer;
    }
    // The producer should perform no other operations with this
    // buffer. Setting the output buffer to NULL helps catch
    // programming errors as well as detect when the primary packet's
    // output buffer has been already closed.
    packet->_output_buffer = NULL;
   

    // packet input buffer(s)
    if ( packet == _packet ) {
        // Trying to terminate this stage's primary
        // packet. Terminating the output buffer was ok, but we cannot
        // yet terminate the inputs because other packets in the stage
        // may still be reading them.
        return;
    }
    else {
        // since we are not the primary, can happily destroy packet
        // subtrees
        packet->destroy_subpackets();
        delete packet;
    }
}



/**
 *  @brief When a worker thread dequeues a new packet list from the
 *  container queue, it should create a stage_adaptor_t around that
 *  list, create a stage to work with, and invoke this method with the
 *  stage. This function invokes the stage's process() method with
 *  itself and "cleans up" the stage's packet list when process()
 *  returns.
 *
 *  @param stage The stage providing us with a process().
 */
void stage_container_t::stage_adaptor_t::run_stage(stage_t* stage) {


    // error checking
    assert( stage != NULL );

    
    // run stage-specific processing function
    stage->init(this);
    stage_t::result_t process_ret = stage->process();


    // if we are still accepting packets, stop now
    stop_accepting_packets();

    TRACE(TRACE_DEBUG, "process returned %d\n", process_ret);
    
    switch(process_ret) {

    case stage_t::RESULT_STOP:
        
        // flush remaining tuples
        if ( flush() == stage_t::RESULT_ERROR ) {
            TRACE(TRACE_ALWAYS, "flush() returned error. Aborting queries...\n");
            abort_queries();
            return;
        }

        // done!
        cleanup();
        return;
        
    case stage_t::RESULT_ERROR:
	TRACE(TRACE_ALWAYS, "process() returned error. Aborting queries...\n");
        abort_queries();
        return;

    default:
        TRACE(TRACE_ALWAYS, "process() returned an invalid result %d\n",
	      process_ret);
        QPIPE_PANIC();
    }
}



/**
 *  @brief Cleanup after successful processing of a stage.
 *
 *  Walk through packet list. Invoke finish_packet() on packets that
 *  merged when _next_tuple == 1. Erase these packets from the packet
 *  list.
 *
 *  Take the remain packet list (of "unfinished" packets) and
 *  re-enqueue it.
 *
 *  Invoke terminate_inputs() on the primary packet and delete it.
 */
void stage_container_t::stage_adaptor_t::cleanup() {


    // walk through our packet list
    packet_list_t::iterator it;
    for (it = _packet_list->begin(); it != _packet_list->end(); ) {
        
	packet_t* curr_packet = *it;

        // Check for all packets that have been with this stage from
        // the beginning. If we haven't invoked finish_packet() on the
        // primary yet, we're going to do it now.
        if ( curr_packet->_stage_next_tuple_on_merge == 1 ) {

            // The packet is finished. If it is not the primary
            // packet, finish_packet() will delete it.
            finish_packet(curr_packet);

            it = _packet_list->erase(it);
            continue;
        }
        
        // The packet is not finished. Simply update its progress
        // counter(s).
        curr_packet->_stage_next_tuple_needed =
            curr_packet->_stage_next_tuple_on_merge;
        curr_packet->_stage_next_tuple_on_merge = 0; // reinitialize
    }


    // Re-enqueue incomplete packets if we have them
    if ( _packet_list->empty() ) {
	delete _packet_list;
    }
    else {
        // Hand off ownership of the packet list back to the
        // container.
	_container->container_queue_enqueue_no_merge(_packet_list);
    }
    _packet_list = NULL;

    
    // Terminate inputs of primary packet. finish_packet() already
    // took care of its output buffer.
    assert(_packet != NULL);
    _packet->terminate_inputs();
    delete _packet;
    _packet = NULL;
}



/**
 *  @brief Cleanup after unsuccessful processing of a stage.
 *
 *  Walk through packet list. For each non-primary packet, invoke
 *  terminate() on its output buffer, invoke destroy_subpackets() to
 *  destroy its inputs, and delete the packet.
 *
 *  Invoke terminate() on the primary packet's output buffer, invoke
 *  terminate_inputs() to terminate its input buffers, and delete it.
 */
void stage_container_t::stage_adaptor_t::abort_queries() {


    // handle non-primary packets in packet list
    packet_list_t::iterator it;
    for (it = _packet_list->begin(); it != _packet_list->end(); ) {
        

	packet_t* curr_packet = *it;


        if ( curr_packet != _packet ) {
            // packet is non-primary

            // output
            if ( !curr_packet->_output_buffer->terminate() ) {
                // Consumer has already terminated this buffer! We are
                // now responsible for deleting it.
                delete curr_packet->_output_buffer;
            }

            // input(s)
            curr_packet->destroy_subpackets();
            delete curr_packet;
        }
    }

    
    // handle primary packet

    // output
    
    //If the primary packet has been finished, its output
    // buffer will be set to NULL. If this is the case, there is no
    // reason to invoke terminate() on it.
    if ( _packet->_output_buffer != NULL ) {
        if ( !_packet->_output_buffer->terminate() ) {
            // Consumer has already terminated this buffer! We are
            // now responsible for deleting it.
            delete _packet->_output_buffer;
        }
    }


    // input(s)
    
    // Terminate inputs of primary packet. finish_packet() already
    // took care of its output buffer.
    _packet->terminate_inputs();
    delete _packet;
}



#include "namespace.h"