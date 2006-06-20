
#ifndef _TUPLE_SOURCE_H
#define _TUPLE_SOURCE_H

#include "engine/core/packet.h"



// include me last!!!
#include "engine/namespace.h"



class tuple_source_t {

 public:
  
  /**
   *  @brief Produce another packet that we can dispatch to receive
   *  another stream of tuples. Since we only consider left-deep query
   *  plans, it is useful to specify the right/inner relation of join
   *  as a tuple_source_t.
   */
  virtual packet_t* reset()=0;
};


#include "engine/namespace.h"


#endif
