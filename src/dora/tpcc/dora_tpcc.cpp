/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file:   dora_tpcc.cpp
 *
 *  @brief:  Implementation of the DORA TPC-C class
 *
 *  @author: Ippokratis Pandis, Oct 2008
 */


#include "dora/tpcc/dora_tpcc.h"

#include "dora/tpcc/dora_mbench.h"
#include "dora/tpcc/dora_payment.h"
#include "dora/tpcc/dora_order_status.h"
#include "dora/tpcc/dora_stock_level.h"
#include "dora/tpcc/dora_delivery.h"
#include "dora/tpcc/dora_new_order.h"

#include "tls.h"


using namespace shore;
using namespace tpcc;



ENTER_NAMESPACE(dora);



// max field counts for (int) keys of tpc-c tables
const int whs_IRP_KEY = 1;
const int dis_IRP_KEY = 2;
const int cus_IRP_KEY = 3;
const int his_IRP_KEY = 6;
const int nor_IRP_KEY = 3;
const int ord_IRP_KEY = 4;
const int ite_IRP_KEY = 1;
const int oli_IRP_KEY = 4;
const int sto_IRP_KEY = 2;

// key estimations for each partition of the tpc-c tables
const int whs_KEY_EST = 100;
const int dis_KEY_EST = 1000;
const int cus_KEY_EST = 30000;
const int his_KEY_EST = 100;
const int nor_KEY_EST = 3;
const int ord_KEY_EST = 4;
const int ite_KEY_EST = 1;
const int oli_KEY_EST = 4;
const int sto_KEY_EST = 2;



/****************************************************************** 
 *
 * @fn:    start()
 *
 * @brief: Starts the DORA TPC-C
 *
 * @note:  Creates a corresponding number of partitions per table.
 *         The decision about the number of partitions per table may 
 *         be based among others on:
 *         - _env->_sf : the database scaling factor
 *         - _env->_{max,active}_cpu_count: {hard,soft} cpu counts
 *
 ******************************************************************/

const int DoraTPCCEnv::start()
{
    // 1. Creates partitioned tables
    // 2. Adds them to the vector
    // 3. Resets each table

    conf(); // re-configure

    processorid_t icpu(_starting_cpu);

    TRACE( TRACE_STATISTICS, "Creating tables. SF=(%d)...\n", _sf);    


    // WAREHOUSE
    GENERATE_DORA_PARTS(whs,warehouse);


    // DISTRICT
    GENERATE_DORA_PARTS(dis,district);


    // HISTORY
    GENERATE_DORA_PARTS(his,history);


    // CUSTOMER
    GENERATE_DORA_PARTS(cus,customer);

   
    // NEW-ORDER
    GENERATE_DORA_PARTS(nor,new_order);


    // ORDER
    GENERATE_DORA_PARTS(ord,order);


    // ITEM
    GENERATE_DORA_PARTS(ite,item);


    // ORDER-LINE
    GENERATE_DORA_PARTS(oli,order_line);


    // STOCK
    GENERATE_DORA_PARTS(sto,stock);


    TRACE( TRACE_DEBUG, "Starting tables...\n");
    for (int i=0; i<_irptp_vec.size(); i++) {
        _irptp_vec[i]->reset();
    }

    set_dbc(DBC_ACTIVE);
    return (0);
}



/****************************************************************** 
 *
 * @fn:    stop()
 *
 * @brief: Stops the DORA TPC-C
 *
 ******************************************************************/

const int DoraTPCCEnv::stop()
{
    TRACE( TRACE_ALWAYS, "Stopping...\n");
    for (int i=0; i<_irptp_vec.size(); i++) {
        _irptp_vec[i]->stop();
    }
    _irptp_vec.clear();
    set_dbc(DBC_STOPPED);
    return (0);
}




/****************************************************************** 
 *
 * @fn:    resume()
 *
 * @brief: Resumes the DORA TPC-C
 *
 ******************************************************************/

const int DoraTPCCEnv::resume()
{
    assert (0); // TODO (ip)
    set_dbc(DBC_ACTIVE);
    return (0);
}



/****************************************************************** 
 *
 * @fn:    pause()
 *
 * @brief: Pauses the DORA TPC-C
 *
 ******************************************************************/

const int DoraTPCCEnv::pause()
{
    assert (0); // TODO (ip)
    set_dbc(DBC_PAUSED);
    return (0);
}



/****************************************************************** 
 *
 * @fn:    conf()
 *
 * @brief: Re-reads configuration
 *
 ******************************************************************/

const int DoraTPCCEnv::conf()
{
    ShoreTPCCEnv::conf();

    TRACE( TRACE_DEBUG, "configuring dora-tpcc\n");

    envVar* ev = envVar::instance();

    _starting_cpu = ev->getVarInt("dora-cpu-starting",DF_CPU_STEP_PARTITIONS);
    _cpu_table_step = ev->getVarInt("dora-cpu-table-step",DF_CPU_STEP_TABLES);

    _cpu_range = get_active_cpu_count();
    _sf = upd_sf();


    _sf_per_part_whs = ev->getVarInt("dora-tpcc-wh-per-part-wh",0);
    _sf_per_part_dis = ev->getVarInt("dora-tpcc-wh-per-part-dist",0);
    _sf_per_part_cus = ev->getVarInt("dora-tpcc-wh-per-part-cust",0);
    _sf_per_part_his = ev->getVarInt("dora-tpcc-wh-per-part-hist",0);
    _sf_per_part_nor = ev->getVarInt("dora-tpcc-wh-per-part-nord",0);
    _sf_per_part_ord = ev->getVarInt("dora-tpcc-wh-per-part-ord",0);
    _sf_per_part_ite = ev->getVarInt("dora-tpcc-wh-per-part-item",0);
    _sf_per_part_oli = ev->getVarInt("dora-tpcc-wh-per-part-oline",0);
    _sf_per_part_sto = ev->getVarInt("dora-tpcc-wh-per-part-stock",0);
    
    return (0);
}





/****************************************************************** 
 *
 * @fn:    newrun()
 *
 * @brief: Prepares the DORA TPC-C DB for a new run
 *
 ******************************************************************/

const int DoraTPCCEnv::newrun()
{
    TRACE( TRACE_DEBUG, "Preparing for new run...\n");
    for (int i=0; i<_irptp_vec.size(); i++) {
        _irptp_vec[i]->prepareNewRun();
    }
    return (0);
}



/****************************************************************** 
 *
 * @fn:    set()
 *
 * @brief: Given a map of strings it updates the db environment
 *
 ******************************************************************/

const int DoraTPCCEnv::set(envVarMap* vars)
{
    TRACE( TRACE_DEBUG, "Reading set...\n");
    for (envVarConstIt cit = vars->begin(); cit != vars->end(); ++cit)
        TRACE( TRACE_DEBUG, "(%s) (%s)\n", 
               cit->first.c_str(), 
               cit->second.c_str());
    TRACE( TRACE_DEBUG, "*** unimplemented ***\n");
    return (0);
}


/****************************************************************** 
 *
 * @fn:    dump()
 *
 * @brief: Dumps information about all the tables and partitions
 *
 ******************************************************************/

const int DoraTPCCEnv::dump()
{
    int sz=_irptp_vec.size();
    TRACE( TRACE_ALWAYS, "Tables  = (%d)\n", sz);
    for (int i=0; i<sz; i++) {
        _irptp_vec[i]->dump();
    }
    return (0);
}


/****************************************************************** 
 *
 * @fn:    info()
 *
 * @brief: Information about the current state of DORA
 *
 ******************************************************************/

const int DoraTPCCEnv::info()
{
    TRACE( TRACE_ALWAYS, "SF      = (%d)\n", _scaling_factor);
    int sz=_irptp_vec.size();
    TRACE( TRACE_ALWAYS, "Tables  = (%d)\n", sz);
    for (int i=0;i<sz;++i) {
        _irptp_vec[i]->info();
    }
    return (0);
}



/******************************************************************** 
 *
 *  @fn:    statistics
 *
 *  @brief: Prints statistics for DORA 
 *
 ********************************************************************/

const int DoraTPCCEnv::statistics() 
{
    int sz=_irptp_vec.size();
    TRACE( TRACE_ALWAYS, "Tables  = (%d)\n", sz);
    for (int i=0;i<sz;++i) {
        _irptp_vec[i]->statistics();
    }

    // first print any parent statistics
    ShoreTPCCEnv::statistics();

    return (0);
}





/****************************************************************** 
 *
 * @fn:    _next_cpu()
 *
 * @brief: Deciding the distribution of tables
 *
 * @note:  Very simple (just increases processor id by DF_CPU_STEP) 

 * @note:  This decision  can be based among others on:
 *
 *         - aprd                    - the current cpu
 *         - this->_max_cpu_count    - the maximum cpu count (hard-limit)
 *         - this->_active_cpu_count - the active cpu count (soft-limit)
 *         - this->_start_prs_id     - the first assigned cpu for the table
 *         - this->_prs_range        - a range of cpus assigned for the table
 *         - this->_sf               - the scaling factor of the tpcc-db
 *         - this->_vec.size()       - the number of tables
 *         - atable                  - the current table
 *
 ******************************************************************/

const processorid_t DoraTPCCEnv::_next_cpu(const processorid_t& aprd,
                                           const irpTableImpl* atable,
                                           const int step)
{    
    int binding = envVar::instance()->getVarInt("dora-cpu-binding",0);
    if (binding==0)
        return (PBIND_NONE);

    processorid_t nextprs = ((aprd+step) % this->get_active_cpu_count());
    //TRACE( TRACE_DEBUG, "(%d) -> (%d)\n", aprd, nextprs);
    return (nextprs);
}






/******************************************************************** 
 *
 *  Thread-local action and rvp object caches
 *
 ********************************************************************/


// MBenches RVP //

DEFINE_DORA_FINAL_RVP_GEN_FUNC(final_mb_rvp,DoraTPCCEnv);

DEFINE_DORA_ACTION_GEN_FUNC(upd_wh_mb_action,rvp_t,mbench_wh_input_t,int,DoraTPCCEnv);
DEFINE_DORA_ACTION_GEN_FUNC(upd_cust_mb_action,rvp_t,mbench_cust_input_t,int,DoraTPCCEnv);


///////////////////////////////////////////////////////////////////////////////////////

// TPC-C PAYMENT


DEFINE_DORA_MIDWAY_RVP_GEN_FUNC(midway_pay_rvp,payment_input_t,DoraTPCCEnv);

DEFINE_DORA_FINAL_RVP_WITH_PREV_GEN_FUNC(final_pay_rvp,DoraTPCCEnv);


DEFINE_DORA_ACTION_GEN_FUNC(upd_wh_pay_action,midway_pay_rvp,payment_input_t,int,DoraTPCCEnv);
DEFINE_DORA_ACTION_GEN_FUNC(upd_dist_pay_action,midway_pay_rvp,payment_input_t,int,DoraTPCCEnv);
DEFINE_DORA_ACTION_GEN_FUNC(upd_cust_pay_action,midway_pay_rvp,payment_input_t,int,DoraTPCCEnv);

DEFINE_DORA_ACTION_GEN_FUNC(ins_hist_pay_action,rvp_t,payment_input_t,int,DoraTPCCEnv);
//const tpcc_warehouse_tuple& awh
//const tpcc_district_tuple& adist


///////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////

// TPC-C ORDER STATUS


DEFINE_DORA_FINAL_RVP_GEN_FUNC(final_ordst_rvp,DoraTPCCEnv);

DEFINE_DORA_ACTION_GEN_FUNC(r_cust_ordst_action,rvp_t,order_status_input_t,int,DoraTPCCEnv);
DEFINE_DORA_ACTION_GEN_FUNC(r_ol_ordst_action,rvp_t,order_status_input_t,int,DoraTPCCEnv);
//const tpcc_order_tuple& aorder


///////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////

// TPC-C STOCK LEVEL

DEFINE_DORA_MIDWAY_RVP_GEN_FUNC(mid1_stock_rvp,stock_level_input_t,DoraTPCCEnv);
DEFINE_DORA_MIDWAY_RVP_WITH_PREV_GEN_FUNC(mid2_stock_rvp,stock_level_input_t,DoraTPCCEnv);

DEFINE_DORA_FINAL_RVP_WITH_PREV_GEN_FUNC(final_stock_rvp,DoraTPCCEnv);


DEFINE_DORA_ACTION_GEN_FUNC(r_dist_stock_action,mid1_stock_rvp,stock_level_input_t,int,DoraTPCCEnv);
DEFINE_DORA_ACTION_GEN_FUNC(r_ol_stock_action,mid2_stock_rvp,stock_level_input_t,int,DoraTPCCEnv);
//const int nextid

DEFINE_DORA_ACTION_GEN_FUNC(r_st_stock_action,rvp_t,stock_level_input_t,int,DoraTPCCEnv);
//TwoIntVec* pvwi


///////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////

// TPC-C DELIVERY

DEFINE_DORA_MIDWAY_RVP_GEN_FUNC(mid1_del_rvp,delivery_input_t,DoraTPCCEnv);
//final_del_rvp* frvp
//const int d_id

DEFINE_DORA_MIDWAY_RVP_WITH_PREV_GEN_FUNC(mid2_del_rvp,delivery_input_t,DoraTPCCEnv);
//final_del_rvp* frvp
//const int d_id

DEFINE_DORA_FINAL_RVP_GEN_FUNC(final_del_rvp,DoraTPCCEnv);


DEFINE_DORA_ACTION_GEN_FUNC(del_nord_del_action,mid1_del_rvp,delivery_input_t,int,DoraTPCCEnv);
//const int d_id

DEFINE_DORA_ACTION_GEN_FUNC(upd_ord_del_action,mid2_del_rvp,delivery_input_t,int,DoraTPCCEnv);
//const int d_id
//const int o_id

DEFINE_DORA_ACTION_GEN_FUNC(upd_oline_del_action,mid2_del_rvp,delivery_input_t,int,DoraTPCCEnv);
//const int d_id
//const int o_id

DEFINE_DORA_ACTION_GEN_FUNC(upd_cust_del_action,rvp_t,delivery_input_t,int,DoraTPCCEnv);
//const int d_id
//const int o_id
//const int amount


///////////////////////////////////////////////////////////////////////////////////////




///////////////////////////////////////////////////////////////////////////////////////

// TPC-C NEWORDER


DEFINE_DORA_MIDWAY_RVP_GEN_FUNC(midway_nord_rvp,new_order_input_t,DoraTPCCEnv);
//final_nord_rvp* frvp

DEFINE_DORA_FINAL_RVP_GEN_FUNC(final_nord_rvp,DoraTPCCEnv);
// const int ol_cnt


DEFINE_DORA_ACTION_GEN_FUNC(r_wh_nord_action,rvp_t,int,int,DoraTPCCEnv);
//const int d_id

DEFINE_DORA_ACTION_GEN_FUNC(r_cust_nord_action,rvp_t,int,int,DoraTPCCEnv);
//const int d_id
//const int c_id


DEFINE_DORA_ACTION_GEN_FUNC(upd_dist_nord_action,midway_nord_rvp,int,int,DoraTPCCEnv);
//const int d_id

DEFINE_DORA_ACTION_GEN_FUNC(r_item_nord_action,midway_nord_rvp,int,int,DoraTPCCEnv);
//const int d_id
//const int olidx

DEFINE_DORA_ACTION_GEN_FUNC(upd_sto_nord_action,midway_nord_rvp,int,int,DoraTPCCEnv);
//const int d_id
//const int olidx

DEFINE_DORA_ACTION_GEN_FUNC(ins_ord_nord_action,rvp_t,int,int,DoraTPCCEnv);
//const int d_id
//const int nextoid
//const cid
//const tstamp
//const int olcnt
//const int alllocal

DEFINE_DORA_ACTION_GEN_FUNC(ins_nord_nord_action,rvp_t,int,int,DoraTPCCEnv);
//const int d_id
//const int nextoid

DEFINE_DORA_ACTION_GEN_FUNC(ins_ol_nord_action,rvp_t,int,int,DoraTPCCEnv);
//const int d_id
//const int nextoid
//const int olidx
//const ol_item_info& iteminfo
//time_t tstamp



///////////////////////////////////////////////////////////////////////////////////////




EXIT_NAMESPACE(dora);

