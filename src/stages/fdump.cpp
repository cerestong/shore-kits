/* -*- mode:C++; c-basic-offset:4 -*- */

#include "stages/fdump.h"
#include "qpipe_panic.h"
#include "trace/trace.h"
#include "tuple.h"



const char* fdump_packet_t::PACKET_TYPE = "FDUMP";



const char* fdump_stage_t::DEFAULT_STAGE_NAME = "FDUMP_STAGE";



/**
 *  @brief Write the file specified by fdump_packet_t p.
 *
 *  @param p An fdump_packet_t.
 *
 *  @return 0 on success. Non-zero on unrecoverable error. The stage
 *  should terminate all queries it is processing.
 */

int fdump_stage_t::process_packet() {

    adaptor_t* adaptor = _adaptor;
    fdump_packet_t* packet = (fdump_packet_t*)adaptor->get_packet();


    char* filename = packet->_filename;
    FILE* file = fopen(filename, "w+");
    if (file == NULL) {
	TRACE(TRACE_ALWAYS, "fopen() failed on %s\n", filename);
	return -1;
    }


    tuple_buffer_t* input_buffer = packet->_input_buffer;
    input_buffer->init_buffer();

    
    // read the file
    tuple_page_t* tuple_page;
    while ( (tuple_page = input_buffer->get_page()) != NULL ) {
	if ( !tuple_page->fwrite_full_page(file) ) {
	    TRACE(TRACE_ALWAYS, "fwrite_full_page() failed\n");
	    free(tuple_page);
	    if ( fclose(file) )
		TRACE(TRACE_ALWAYS, "fclose() failed on %s\n", filename);
	    return -1;
	}

	// When we invoked get_page(), we took ownership of the page
	// from the buffer. It is our responsibility to free it when
	// we're done with it.
	free(tuple_page);
    }


    if ( fclose(file) ) {
	TRACE(TRACE_ALWAYS, "fclose() failed on %s\n", filename);
	return -1;
    }


    return 0;
}