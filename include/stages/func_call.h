/* -*- mode:C++; c-basic-offset:4 -*- */

#ifndef _FUNC_CALL_H
#define _FUNC_CALL_H

#include <cstdio>

#include "tuple.h"
#include "packet.h"
#include "stage.h"
#include "dispatcher.h"
#include "trace.h"
#include "qpipe_panic.h"



using namespace qpipe;



/* exported datatypes */


class func_call_packet_t : public packet_t {
  
public:

    static const char* PACKET_TYPE;
    
    
    stage_t::result_t (*_func) (void*);
    
    
    void* _func_arg;
    
    
    /**
     *  @brief func_call_packet_t constructor.
     *
     *  @param packet_id The ID of this packet. This should point to a
     *  block of bytes allocated with malloc(). This packet will take
     *  ownership of this block and invoke free() when it is
     *  destroyed.
     *
     *  @param output_buffer This packet's output buffer. This buffer
     *  is not used directly. The container will send_eof() and close
     *  this buffer when process_packet() returns. However, this will
     *  not be done until func is called with func_arg. So a test
     *  program may pass in a tuple_buffer_t for both output_buffer
     *  and func_arg. A packet DOES NOT own its output buffer (we will
     *  not invoke delete or free() on this field in our packet
     *  destructor).
     *
     *  @param output_filter This filter is not used, but it cannot be
     *  NULL. Pass in a tuple_filter_t instance. The packet OWNS this
     *  filter. It will be deleted in the packet destructor.
     *
     *  @param func The function that should be called with func_arg.
     *
     *  @param func_arg func will be called with this value. This
     *  packet does not own this value.
     */
    func_call_packet_t(char*           packet_id,
                       tuple_buffer_t* output_buffer,
                       tuple_filter_t* output_filter,
                       stage_t::result_t (*func) (void*),
                       void* func_arg)
      : packet_t(packet_id, PACKET_TYPE, output_buffer, output_filter, false),
      _func(func),
      _func_arg(func_arg)
      {
        // error checking
        assert(func != NULL);
      }

    
    virtual ~func_call_packet_t() {
      // This packet has nothing to destroy.
    }
    
    
    virtual void destroy_subpackets() {
        // No input buffers or subpackets ... do nothing.
    }

    virtual void terminate_inputs() {
        // No input buffers ... do nothing.
    }

};



/**
 *  @brief FUNC_CALL stage. Simply invokes a packet's specified
 *  function with the packet's tuple buffer. Useful for unit testing.
 */
class func_call_stage_t : public stage_t {

public:
  
    static const char* DEFAULT_STAGE_NAME;
    
    func_call_stage_t() { }
    
    virtual ~func_call_stage_t() { }
    
protected:

    virtual result_t process_packet();
};



#endif