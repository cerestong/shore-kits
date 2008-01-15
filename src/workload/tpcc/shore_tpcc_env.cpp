/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file shore_tpcc_env.cpp
 *
 *  @brief Declaration of the Shore TPC-C environment
 *
 *  @author Ippokratis Pandis (ipandis)
 */


#include "workload/workload.h"
//#include "workload/tpcc/inmem_tpcc_load.h"
#include "workload/tpcc/shore_tpcc_env.h"

using namespace workload;
using namespace tpcc;


/** Exported functions */

/** @fn savedata
 *
 *  @brief Loads the TPC-C data from a specified location
 */

int ShoreTPCCEnv::savedata(c_str saveDir) {

    TRACE( TRACE_DEBUG, "Saving data to (%s)\n", saveDir.data());


    // Start measuring loading time
    time_t tstart = time(NULL);


    time_t tstop = time(NULL);

    TRACE( TRACE_ALWAYS, "Save finished in (%d) secs\n", tstop - tstart);

    return (0);
}


/** @fn loaddata
 *
 *  @brief Loads the TPC-C data from a specified location
 */

int ShoreTPCCEnv::loaddata(c_str loadDir) {

    TRACE( TRACE_DEBUG, "Getting data from (%s)\n", loadDir.data());

    // Start measuring loading time
    time_t tstart = time(NULL);


    time_t tstop = time(NULL);

    TRACE( TRACE_ALWAYS, "Loading finished in (%d) secs\n", tstop - tstart);
    _initialized = true;
    return (0);
}



/** @fn dump
 *
 *  @brief Dumps the data
 */

void ShoreTPCCEnv::dump() {
    TRACE( TRACE_DEBUG, "~~~~~~~~~~~~~~~~~~~~~\n");    
    TRACE( TRACE_DEBUG, "Dumping Shore Data\n");

    TRACE( TRACE_DEBUG, "~~~~~~~~~~~~~~~~~~~~~\n");    
}
