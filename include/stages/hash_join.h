/* -*- mode:C++; c-basic-offset:4 -*- */

/* hash_join_stage.h */
/* Declaration of the hash_join_stage hash_join_packet classes */
/* History: 
   3/3/2006: Removed the static hash_join_cnt variable of the hash_join_packet_t class, instead it  uses the singleton stage_packet_counter class.
*/


#ifndef __HASH_JOIN_STAGE_H
#define __HASH_JOIN_STAGE_H

#include "core.h"

#include <ext/hash_set>


using namespace qpipe;

#define HASH_JOIN_STAGE_NAME  "HASH_JOIN"
#define HASH_JOIN_PACKET_TYPE "HASH_JOIN"


/********************
 * hash_join_packet *
 ********************/

class hash_join_packet_t : public packet_t {
public:
    static const c_str PACKET_TYPE;

    guard<packet_t> _left;
    guard<packet_t> _right;
    guard<tuple_fifo> _left_buffer;
    guard<tuple_fifo> _right_buffer;

    guard<tuple_join_t> _join;
    bool _outer;
    bool _distinct;
    
    int count_out;
    int count_left;
    int count_right;
  
    /**
     *  @brief sort_packet_t constructor.
     *
     *  @param packet_id The ID of this packet. This should point to a
     *  block of bytes allocated with malloc(). This packet will take
     *  ownership of this block and invoke free() when it is
     *  destroyed.
     *
     *  @param output_buffer The buffer where this packet should send
     *  its data. A packet DOES NOT own its output buffer (we will not
     *  invoke delete or free() on this field in our packet
     *  destructor).
     *
     *  @param output_filter The filter that will be applied to any
     *  tuple sent to output_buffer. The packet OWNS this filter. It
     *  will be deleted in the packet destructor.
     *
     *  @param left Left side-input packet. This will be dispatched
     *  second and become the outer relation of the join. This should
     *  be the larger input.
     *
     *  @param right Right-side packet. This will be dispatched first
     *  and will become the inner relation of the join. It should be
     *  the smaller input.
     *
     *  @param join The joiner  we will be using for this
     *  packet. The packet OWNS this joiner. It will be deleted in
     *  the packet destructor.
     *
     */
    hash_join_packet_t(const c_str &packet_id,
                       tuple_fifo* out_buffer,
                       tuple_filter_t *output_filter,
                       packet_t* left,
                       packet_t* right,
                       tuple_join_t *join,
                       bool outer=false,
                       bool distinct=false)
        : packet_t(packet_id, PACKET_TYPE, out_buffer, output_filter,
                   create_plan(output_filter, join, outer, distinct, left, right)),
          _left(left),
          _right(right),
          _left_buffer(left->output_buffer()),
          _right_buffer(right->output_buffer()),
          _join(join),
          _outer(outer), _distinct(distinct)
    {
    }
  
    static query_plan* create_plan(tuple_filter_t* filter, tuple_join_t* join,
                                   bool outer, bool distinct,
                                   packet_t* left, packet_t* right)
    {
        c_str action("%s:%s:%d:%d", PACKET_TYPE.data(),
                     join->to_string().data(), outer, distinct);

        query_plan const** children = new query_plan const*[2];
        children[0] = left->plan();
        children[1] = right->plan();
        return new query_plan(action, filter->to_string(), children, 2);
    }
};


/*******************
 * hash_join_stage *
 *******************/
using __gnu_cxx::hashtable;
using std::string;
using std::vector;

class hash_join_stage_t : public stage_t {
    struct hash_key_t;
    
    template <bool left>
    struct extract_key_t;
    
    struct equal_key_t;

    /**
     * Abuse the flexibility of the base STL hashtable implementation
     * to avoid storing tuple keys.
     *
     * The key extractor maintains internal space to hold the
     * key. When asked to extract a key it returns a pointer to this
     * initial space after filling it based on the tuple it was
     * given. Since this may be called more than once per expression
     * (see the implementation) the extractor actually maintains space
     * for two keys, alternating which pointer it returns.
     *
     * The key equality test simply performs a bytewise comparison of
     * the two keys it is given.
     */
    typedef hashtable<char *, const char *,
                      hash_key_t,
                      extract_key_t<true>,
                      equal_key_t> tuple_hash_t;
  
    /* Reads tuples from the given source to build an in-memory hash table
     */
    template<class TupleSource>
    page* build_hash(tuple_hash_t &probe_table,
                       TupleSource source);
    
    template <class TupleSource>
    hash_join_packet_t *probe_matches(tuple_hash_t &probe_table,
                                      hash_join_packet_t *packet,
                                      TupleSource source);

    struct partition_t {
        page* _page;
        int size;
        FILE *file;
        c_str file_name1;
        c_str file_name2;
        partition_t()
            : _page(NULL), size(0), file(NULL)
        {
        }
    };

    typedef vector<partition_t> partition_list_t;
    partition_list_t partitions;
    
    int page_quota;
    int page_count;

    tuple_join_t *_join;
public:

    static const c_str DEFAULT_STAGE_NAME;
    typedef hash_join_packet_t stage_packet_t;
    virtual void process_packet();
    
    // set up the partitions in memory
    hash_join_stage_t()
        : partitions(512),
          page_quota(10000), page_count(0)
    {
    }

    ~hash_join_stage_t() {
    }

private:
    void test_overflow(int partition);

    struct left_action_t;
    struct right_action_t;
    template<class Action>
    void close_file(partition_list_t::iterator it, Action a);
};

#endif	// __HASH_JOIN_H