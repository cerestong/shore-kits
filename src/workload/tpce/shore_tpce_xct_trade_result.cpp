/* -*- mode:C++; c-basic-offset:4 -*-
   Shore-kits -- Benchmark implementations for Shore-MT

   Copyright (c) 2007-2009
   Data Intensive Applications and Systems Labaratory (DIAS)
   Ecole Polytechnique Federale de Lausanne

   All Rights Reserved.

   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.

   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/** @file:   shore_tpce_xct_trade_result.cpp
 *
 *  @brief:  Implementation of the Baseline Shore TPC-E TRADE RESULT transaction
 *
 *  @author: Cansu Kaynak
 *  @author: Djordje Jevdjic
 */

#include "workload/tpce/shore_tpce_env.h"
#include "workload/tpce/tpce_const.h"
#include "workload/tpce/tpce_input.h"

#include <vector>
#include <numeric>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include "workload/tpce/egen/CE.h"
#include "workload/tpce/egen/TxnHarnessStructs.h"
#include "workload/tpce/shore_tpce_egen.h"

using namespace shore;
using namespace TPCE;

//#define TRACE_TRX_FLOW TRACE_ALWAYS
//#define TRACE_TRX_RESULT TRACE_ALWAYS

ENTER_NAMESPACE(tpce);

/******************************************************************** 
 *
 * TPC-E TRADE_RESULT
 *
 ********************************************************************/


w_rc_t ShoreTPCEEnv::xct_trade_result(const int xct_id, trade_result_input_t& ptrin)
{
    // ensure a valid environment
    assert (_pssm);
    assert (_initialized);
    assert (_loaded);

    table_row_t* prtrade = _ptrade_man->get_tuple();	//149B
    assert (prtrade);

    table_row_t* prtradetype = _ptrade_type_man->get_tuple();
    assert (prtradetype);

    table_row_t* prholdingsummary = _pholding_summary_man->get_tuple();
    assert (prholdingsummary);

    table_row_t* prcustaccount = _pcustomer_account_man->get_tuple();
    assert (prcustaccount);

    table_row_t* prholding = _pholding_man->get_tuple();
    assert (prholding);

    table_row_t* prholdinghistory = _pholding_history_man->get_tuple();
    assert (prholdinghistory);

    table_row_t* prsecurity = _psecurity_man->get_tuple();	//194B
    assert (prsecurity);

    table_row_t* prbroker = _pbroker_man->get_tuple();
    assert (prbroker);

    table_row_t* prsettlement = _psettlement_man->get_tuple();
    assert (prsettlement);

    table_row_t* prcashtrans = _pcash_transaction_man->get_tuple();
    assert (prcashtrans);

    table_row_t* prcommissionrate = _pcommission_rate_man->get_tuple();
    assert (prcommissionrate);

    table_row_t* prcusttaxrate = _pcustomer_taxrate_man->get_tuple();
    assert (prcusttaxrate);

    table_row_t* prtaxrate = _ptaxrate_man->get_tuple();
    assert (prtaxrate);

    table_row_t* prcustomer = _pcustomer_man->get_tuple();	//280B
    assert (prcustomer);

    table_row_t* prtradehist = _ptrade_history_man->get_tuple();
    assert (prtradehist);

    w_rc_t e = RCOK;
    rep_row_t areprow(_pcustomer_man->ts());

    areprow.set(_pcustomer_desc->maxsize());

    prtrade->_rep = &areprow;
    prtradetype->_rep = &areprow;
    prholdingsummary->_rep = &areprow;
    prcustaccount->_rep = &areprow;
    prholding->_rep = &areprow;
    prholdinghistory->_rep = &areprow;
    prsecurity->_rep = &areprow;
    prbroker->_rep = &areprow;
    prsettlement->_rep = &areprow;
    prcashtrans->_rep = &areprow;
    prcommissionrate->_rep = &areprow;
    prcusttaxrate->_rep = &areprow;
    prtaxrate->_rep = &areprow;
    prcustomer->_rep = &areprow;
    prtradehist->_rep = &areprow;

    rep_row_t lowrep(_pcustomer_man->ts());
    rep_row_t highrep(_pcustomer_man->ts());

    lowrep.set(_pcustomer_desc->maxsize());
    highrep.set(_pcustomer_desc->maxsize());
    { // make gotos safe

	int trade_qty;
	TIdent acct_id = -1;
	bool type_is_sell;
	int hs_qty = -1;
	char symbol[16]; //15
	bool is_lifo;
	char type_id[4]; //3
	bool trade_is_cash;
	char type_name[13];	//12
	double charge;

	//BEGIN FRAME1
	{
	    /**
	     * 	SELECT 	acct_id = T_CA_ID, type_id = T_TT_ID, symbol = T_S_SYMB, trade_qty = T_QTY,
	     *		charge = T_CHRG, is_lifo = T_LIFO, trade_is_cash = T_IS_CASH
	     *	FROM	TRADE
	     *	WHERE	T_ID = trade_id
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:t-idx-probe (%ld) \n", xct_id,  ptrin._trade_id);
	    e =  _ptrade_man->t_index_probe(_pssm, prtrade, ptrin._trade_id);
	    if (e.is_error()) {  goto done; }

	    prtrade->get_value(8, acct_id);
	    prtrade->get_value(3, type_id, 4); 	//3
	    prtrade->get_value(5, symbol, 16);	//15
	    prtrade->get_value(6, trade_qty);
	    prtrade->get_value(11, charge);
	    prtrade->get_value(14, is_lifo);
	    prtrade->get_value(4, trade_is_cash);

	    assert(acct_id != -1); //Harness control

	    /**
	     * 	SELECT 	type_name = TT_NAME, type_is_sell = TT_IS_SELL, type_is_market = TT_IS_MRKT
	     *	FROM	TRADE_TYPE
	     *   	WHERE	TT_ID = type_id
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:tt-idx-probe (%s) \n", xct_id,  type_id);
	    e =  _ptrade_type_man->tt_index_probe(_pssm, prtradetype, type_id);
	    if (e.is_error()) {  goto done; }

	    prtradetype->get_value(1, type_name, 13); //12
	    prtradetype->get_value(2, type_is_sell);
	    bool type_is_market;
	    prtradetype->get_value(3, type_is_market);

	    /**
	     * 	SELECT 	hs_qty = HS_QTY
	     *	FROM	HOLDING_SUMMARY
	     *  	WHERE	HS_CA_ID = acct_id and HS_S_SYMB = symbol
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-idx-probe (%ld) (%s) \n", xct_id,  acct_id, symbol);
	    //e =  _pholding_summary_man->hs_index_probe_forupdate(_pssm, prholdingsummary, acct_id, symbol);
	    e =  _pholding_summary_man->hs_index_probe(_pssm, prholdingsummary, acct_id, symbol);
	    if (e.is_error())  hs_qty=0;
	    else  prholdingsummary->get_value(2, hs_qty);
	    if(hs_qty == -1){ //-1 = NULL, no prior holdings exist
		hs_qty = 0;
	    }
	}
	//END FRAME1

	TIdent cust_id;
	double buy_value = 0;
	double sell_value = 0;
	myTime trade_dts = time(NULL);
	TIdent broker_id;
	short tax_status;

	//BEGIN FRAME2
	{
	    TIdent hold_id;
	    double hold_price;
	    int hold_qty;
	    int needed_qty;

	    needed_qty = trade_qty;
	    /**
	     *
	     * 	SELECT 	broker_id = CA_B_ID, cust_id = CA_C_ID, tax_status = CA_TAX_ST
	     *	FROM	CUSTOMER_ACCOUNT
	     *	WHERE	CA_ID = acct_id
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:ca-idx-probe (%ld) \n", xct_id,  acct_id);
	    e =  _pcustomer_account_man->ca_index_probe_forupdate(_pssm, prcustaccount, acct_id);
	    if (e.is_error()) {  goto done; }

	    prcustaccount->get_value(1, broker_id);
	    prcustaccount->get_value(2, cust_id);
	    prcustaccount->get_value(4, tax_status);

	    if(type_is_sell){
		if(hs_qty == 0){
		    /**
		     *	INSERT INTO HOLDING_SUMMARY (
		     *					HS_CA_ID,
		     *					HS_S_SYMB,
		     *					HS_QTY
		     *				)
		     *	VALUES 			(
		     *					acct_id,
		     *					symbol,
		     *					-trade_qty
		     *				)
		     */
		    prholdingsummary->set_value(0, acct_id);
		    prholdingsummary->set_value(1, symbol);
		    prholdingsummary->set_value(2, -1*trade_qty);

		    TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-add-tuple (%ld) \n", xct_id, acct_id);
		    e = _pholding_summary_man->add_tuple(_pssm, prholdingsummary);
		    if (e.is_error()) {  goto done; }
		}
		else{
		    if(hs_qty != trade_qty){
			/**
			 *	UPDATE	HOLDING_SUMMARY
			 *	SET		HS_QTY = hs_qty - trade_qty
			 *	WHERE	HS_CA_ID = acct_id and HS_S_SYMB = symbol
			 */

			TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty - trade_qty));
			e = _pholding_summary_man->hs_update_qty(_pssm, prholdingsummary, acct_id, symbol, (hs_qty - trade_qty));
			if (e.is_error()) {  goto done; }
		    }
		}
		if (hs_qty > 0) {
		    guard<index_scan_iter_impl<holding_t> > h_iter;
		    sort_scan_t* h_list_sort_iter;
		    if (is_lifo){
			/**
			 *
			 * 	SELECT	H_T_ID, H_QTY, H_PRICE
			 *  	FROM	HOLDING
			 *	WHERE	H_CA_ID = acct_id and H_S_SYMB = symbol
			 *	ORDER BY H_DTS DESC
			 */
			index_scan_iter_impl<holding_t>* tmp_h_iter;
			TRACE( TRACE_TRX_FLOW, "App: %d TR:h-get-iter-by-idx2 \n", xct_id);
			e = _pholding_man->h_get_iter_by_index2(_pssm, tmp_h_iter, prholding, lowrep, highrep, acct_id, symbol);
			h_iter = tmp_h_iter;
			if (e.is_error()) {  goto done; }

			//descending order
			rep_row_t sortrep(_pholding_man->ts());
			sortrep.set(_pholding_desc->maxsize());
			desc_sort_buffer_t h_list(4);

			h_list.setup(0, SQL_LONG);	//dts
			h_list.setup(1, SQL_LONG); 	//id
			h_list.setup(2, SQL_INT); 	//qty
			h_list.setup(3, SQL_FLOAT);     //price

			table_row_t rsb(&h_list);
			desc_sort_man_impl h_sorter(&h_list, &sortrep);
			bool eof;
			e = h_iter->next(_pssm, eof, *prholding);
			if (e.is_error()) {  goto done; }
			while (!eof) {
			    /* put the value into the sorted buffer */
			    int temp_qty;
			    myTime temp_dts;
			    TIdent temp_tid;
			    double temp_price;

			    prholding->get_value(5, temp_qty);
			    prholding->get_value(4, temp_price);
			    prholding->get_value(3, temp_dts);
			    prholding->get_value(0, temp_tid);

			    rsb.set_value(0, temp_dts);
			    rsb.set_value(1, temp_tid);
			    rsb.set_value(2, temp_qty);
			    rsb.set_value(3, temp_price);

			    h_sorter.add_tuple(rsb);
			    TRACE( TRACE_TRX_FLOW, "App: %d TR:hold-list (%ld)  \n", xct_id, temp_tid);
			    e = h_iter->next(_pssm, eof, *prholding);
			    if (e.is_error()) {  goto done; }
			}
			assert (h_sorter.count());

			desc_sort_iter_impl h_list_sort_iter(_pssm, &h_list, &h_sorter);
			e = h_list_sort_iter.next(_pssm, eof, rsb);
			if (e.is_error()) {  goto done; }
			while(needed_qty != 0 && !eof){
			    TIdent hold_id;
			    int hold_qty;
			    double hold_price;

			    rsb.get_value(1, hold_id);
			    rsb.get_value(2, hold_qty);
			    rsb.get_value(3, hold_price);

			    if(hold_qty > needed_qty){
				/**
				 * 	INSERT INTO
				 *		HOLDING_HISTORY (
				 *		HH_H_T_ID,
				 *		HH_T_ID,
				 *		HH_BEFORE_QTY,
				 *		HH_AFTER_QTY
				 *	)
				 *	VALUES(
				 *		hold_id, // H_T_ID of original trade
				 *		trade_id, // T_ID current trade
				 *		hold_qty, // H_QTY now
				 *		hold_qty - needed_qty // H_QTY after update
				 *	)
				 */

				prholdinghistory->set_value(0, hold_id);
				prholdinghistory->set_value(1, ptrin._trade_id);
				prholdinghistory->set_value(2, hold_qty);
				prholdinghistory->set_value(3, (hold_qty - needed_qty));

				TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, hold_id, ptrin._trade_id, hold_price, (hold_qty - needed_qty));
				e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
				if (e.is_error()) {  goto done; }

				/**
				 * 	UPDATE 	HOLDING
				 *	SET	H_QTY = hold_qty - needed_qty
				 *	WHERE	current of hold_list
				 */

				TRACE( TRACE_TRX_FLOW, "App: %d TR:hold-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty - trade_qty));
				e = _pholding_man->h_update_qty(_pssm, prholding, hold_id, (hold_qty - needed_qty));
				if (e.is_error()) {  goto done; }

				buy_value += needed_qty * hold_price;
				sell_value += needed_qty * ptrin._trade_price;
				needed_qty = 0;
			    }
			    else{
				/**
				 * 	INSERT INTO
				 *		HOLDING_HISTORY (
				 *		HH_H_T_ID,
				 *		HH_T_ID,
				 *		HH_BEFORE_QTY,
				 *		HH_AFTER_QTY
				 *	)
				 *	VALUES(
				 *		hold_id, // H_T_ID of original trade
				 *		trade_id, // T_ID current trade
				 *		hold_qty, // H_QTY now
				 *		0 // H_QTY after update
				 *	)
				 */

				prholdinghistory->set_value(0, hold_id);
				prholdinghistory->set_value(1, ptrin._trade_id);
				prholdinghistory->set_value(2, hold_qty);
				prholdinghistory->set_value(3, 0);

				TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, hold_id, ptrin._trade_id, hold_price, 0);
				e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
				if (e.is_error()) {  goto done; }

				/**
				 * 	DELETE 	FROM	HOLDING
				 *	WHERE	current of hold_list
				 */

				TRACE( TRACE_TRX_FLOW, "App: %d TR:h-delete-by-index (%ld) \n", xct_id, hold_id);
				e = _pholding_man->h_delete_by_index(_pssm, prholding, hold_id);
				if (e.is_error()) {  goto done; }

				buy_value += hold_qty * hold_price;
				sell_value += hold_qty * ptrin._trade_price;
				needed_qty = needed_qty - hold_qty;
			    }

			    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-iter-next \n", xct_id);
			    e = h_list_sort_iter.next(_pssm, eof, rsb);
			    if (e.is_error()) {  goto done; }
			}
		    }
		    else{
			/**
			 * 	SELECT	H_T_ID, H_QTY, H_PRICE
			 *  	FROM	HOLDING
			 *	WHERE	H_CA_ID = acct_id and H_S_SYMB = symbol
			 *	ORDER BY H_DTS ASC
			 */

			{
			    index_scan_iter_impl<holding_t>* tmp_h_iter;
			    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-get-iter-by-idx2 \n", xct_id);
			    e = _pholding_man->h_get_iter_by_index2(_pssm, tmp_h_iter, prholding, lowrep, highrep, acct_id, symbol);
			    if (e.is_error()) {  goto done; }
			    h_iter = tmp_h_iter;
			}
			//already sorted in ascending order because of its index

			bool eof;
			e = h_iter->next(_pssm, eof, *prholding);
			if (e.is_error()) {  goto done; }
			while(needed_qty != 0 && !eof){
			    char hs_s_symb[16];
			    prholding->get_value(2, hs_s_symb, 16);

			    if(strcmp(hs_s_symb, symbol) == 0)//FIX_ME
				{
				    TIdent hold_id;
				    int hold_qty;
				    double hold_price;

				    prholding->get_value(5, hold_qty);
				    prholding->get_value(4, hold_price);
				    prholding->get_value(0, hold_id);

				    if(hold_qty > needed_qty){
					//Selling some of the holdings
					/**
					 *
					 * 	INSERT INTO
					 *		HOLDING_HISTORY (
					 *		HH_H_T_ID,
					 *		HH_T_ID,
					 *		HH_BEFORE_QTY,
					 *		HH_AFTER_QTY
					 *	)
					 *	VALUES(
					 *		hold_id, // H_T_ID of original trade
					 *		trade_id, // T_ID current trade
					 *		hold_qty, // H_QTY now
					 *		hold_qty - needed_qty // H_QTY after update
					 *	)
					 */

					prholdinghistory->set_value(0, hold_id);
					prholdinghistory->set_value(1, ptrin._trade_id);
					prholdinghistory->set_value(2, hold_qty);
					prholdinghistory->set_value(3, (hold_qty - needed_qty));

					TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d) \n", xct_id, hold_id, ptrin._trade_id, hold_price, (hold_qty - needed_qty));
					e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
					if (e.is_error()) {  goto done; }

					/**
					 * 	UPDATE 	HOLDING
					 *	SET	H_QTY = hold_qty - needed_qty
					 *	WHERE	current of hold_list
					 */

					TRACE( TRACE_TRX_FLOW, "App: %d TR:hold-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty - trade_qty));
					e = _pholding_man->h_update_qty(_pssm, prholding, hold_id, (hold_qty - needed_qty));
					if (e.is_error()) {  goto done; }

					buy_value += needed_qty * hold_price;
					sell_value += needed_qty * ptrin._trade_price;
					needed_qty = 0;
				    }
				    else{
					// Selling all holdings
					/**
					 * 	INSERT INTO
					 *		HOLDING_HISTORY (
					 *		HH_H_T_ID,
					 *		HH_T_ID,
					 *		HH_BEFORE_QTY,
					 *		HH_AFTER_QTY
					 *	)
					 *	VALUES(
					 *		hold_id, // H_T_ID of original trade
					 *		trade_id, // T_ID current trade
					 *		hold_qty, // H_QTY now
					 *		0 // H_QTY after update
					 *	)
					 */

					prholdinghistory->set_value(0, hold_id);
					prholdinghistory->set_value(1, ptrin._trade_id);
					prholdinghistory->set_value(2, hold_qty);
					prholdinghistory->set_value(3, 0);

					TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%d) (%ld) (%ld) (%d) \n", xct_id, hold_id, ptrin._trade_id, hold_price, 0);
					e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
					if (e.is_error()) {  goto done; }

					/**
					 * 	DELETE 	FROM	HOLDING
					 *	WHERE	current of hold_list
					 */

					TRACE( TRACE_TRX_FLOW, "App: %d TR:h-delete-by-index (%ld) \n", xct_id, hold_id);
					e = _pholding_man->h_delete_by_index(_pssm, prholding, hold_id);
					if (e.is_error()) {  goto done; }

					buy_value += hold_qty * hold_price;
					sell_value += hold_qty * ptrin._trade_price;
					needed_qty = needed_qty - hold_qty;
				    }
				}
			    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-iter-next \n", xct_id);
			    e = h_iter->next(_pssm, eof, *prholding);
			    if (e.is_error()) {  goto done; }
			}
		    }
		}
		if (needed_qty > 0){
		    /**
		     * 	INSERT INTO
		     *		HOLDING_HISTORY (
		     *		HH_H_T_ID,
		     *		HH_T_ID,
		     *		HH_BEFORE_QTY,
		     *		HH_AFTER_QTY
		     *	)
		     *	VALUES(
		     *		trade_id, // T_ID current is original trade
		     *		trade_id, // T_ID current trade
		     *		0, // H_QTY before
		     *		(-1) * needed_qty // H_QTY after insert
		     *	)
		     */

		    prholdinghistory->set_value(0, ptrin._trade_id);
		    prholdinghistory->set_value(1, ptrin._trade_id);
		    prholdinghistory->set_value(2, 0);
		    prholdinghistory->set_value(3, (-1) * needed_qty);

		    TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d) \n", xct_id, ptrin._trade_id, ptrin._trade_id, 0, (-1) * needed_qty);
		    e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
		    if (e.is_error()) {  goto done; }

		    /**
		     * 	INSERT INTO
		     *		HOLDING (
		     *				H_T_ID,
		     *				H_CA_ID,
		     *				H_S_SYMB,
		     *				H_DTS,
		     *				H_PRICE,
		     *				H_QTY
		     *			)
		     *	VALUES 		(
		     *				trade_id, // H_T_ID
		     *				acct_id, // H_CA_ID
		     *				symbol, // H_S_SYMB
		     *				trade_dts, // H_DTS
		     *				trade_price, // H_PRICE
		     *				(-1) * needed_qty //* H_QTY
		     *			)
		     */

		    prholding->set_value(0, ptrin._trade_id);
		    prholding->set_value(1, acct_id);
		    prholding->set_value(2, symbol);
		    prholding->set_value(3, trade_dts);
		    prholding->set_value(4, ptrin._trade_price);
		    prholding->set_value(5, (-1) * needed_qty);

		    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-add-tuple (%ld) (%ld) (%s) (%ld) (%d) (%d) \n", xct_id, ptrin._trade_id, acct_id, symbol, trade_dts, ptrin._trade_price, (-1) * needed_qty);
		    e = _pholding_man->add_tuple(_pssm, prholding);
		    if (e.is_error()) {  goto done; }
		}
		else{
		    if (hs_qty == trade_qty){
			/**
			 * DELETE FROM	HOLDING_SUMMARY
			 * WHERE HS_CA_ID = acct_id and HS_S_SYMB = symbol
			 */

			TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-delete-by-index (%ld) (%s) \n", xct_id, acct_id, symbol);
			e = _pholding_summary_man->hs_delete_by_index(_pssm, prholdingsummary, acct_id, symbol);
			if (e.is_error()) {  goto done; }
		    }
		}
	    }
	    else{
		if (hs_qty == 0){
		    /**
		     *	INSERT INTO HOLDING_SUMMARY (
		     *					HS_CA_ID,
		     *					HS_S_SYMB,
		     *					HS_QTY
		     *				   )
		     *	VALUES 			(
		     *					acct_id,
		     *					symbol,
		     *					trade_qty
		     *				)
		     **/

		    prholdingsummary->set_value(0, acct_id);
		    prholdingsummary->set_value(1, symbol);
		    prholdingsummary->set_value(2, trade_qty);

		    TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-add-tuple (%ld) \n", xct_id, acct_id);
		    e = _pholding_summary_man->add_tuple(_pssm, prholdingsummary);
		    if (e.is_error()) {  goto done; }
		}
		else{
		    if (-hs_qty != trade_qty){
			/**
			 *	UPDATE	HOLDING_SUMMARY
			 *	SET	HS_QTY = hs_qty + trade_qty
			 *	WHERE	HS_CA_ID = acct_id and HS_S_SYMB = symbol
			 */

			TRACE( TRACE_TRX_FLOW, "App: %d TR:holdsumm-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty + trade_qty));
			e = _pholding_summary_man->hs_update_qty(_pssm, prholdingsummary, acct_id, symbol, (hs_qty + trade_qty));
			if (e.is_error()) {  goto done; }
		    }

		    if(hs_qty < 0){
			guard<index_scan_iter_impl<holding_t> > h_iter;
			sort_scan_t* h_list_sort_iter;
			if(is_lifo){
			    /**
			     * 	SELECT	H_T_ID, H_QTY, H_PRICE
			     *  	FROM	HOLDING
			     *	WHERE	H_CA_ID = acct_id and H_S_SYMB = symbol
			     *	ORDER BY H_DTS DESC
			     */

			    index_scan_iter_impl<holding_t>* tmp_h_iter;
			    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-get-iter-by-idx2 \n", xct_id);
			    e = _pholding_man->h_get_iter_by_index2(_pssm, tmp_h_iter, prholding, lowrep, highrep, acct_id, symbol);
			    if (e.is_error()) {  goto done; }
			    h_iter = tmp_h_iter;

			    //descending order
			    rep_row_t sortrep(_pholding_man->ts());
			    sortrep.set(_pholding_desc->maxsize());
			    desc_sort_buffer_t h_list(4);

			    h_list.setup(0, SQL_LONG);
			    h_list.setup(1, SQL_LONG);
			    h_list.setup(2, SQL_INT);
			    h_list.setup(3, SQL_FLOAT);

			    table_row_t rsb(&h_list);
			    desc_sort_man_impl h_sorter(&h_list, &sortrep);

			    bool eof;
			    e = h_iter->next(_pssm, eof, *prholding);
			    if (e.is_error()) {  goto done; }
			    while (!eof) {
				/* put the value into the sorted buffer */
				int temp_qty;
				myTime temp_dts;
				TIdent temp_tid;
				double temp_price;

				prholding->get_value(5, temp_qty);
				prholding->get_value(4, temp_price);
				prholding->get_value(3, temp_dts);
				prholding->get_value(0, temp_tid);

				rsb.set_value(0, temp_dts);
				rsb.set_value(1, temp_tid);
				rsb.set_value(2, temp_qty);
				rsb.set_value(3, temp_price);

				h_sorter.add_tuple(rsb);

				e = h_iter->next(_pssm, eof, *prholding);
				if (e.is_error()) {  goto done; }
			    }
			    assert (h_sorter.count());

			    desc_sort_iter_impl h_list_sort_iter(_pssm, &h_list, &h_sorter);
			    e = h_list_sort_iter.next(_pssm, eof, rsb);
			    if (e.is_error()) {  goto done; }
			    while(needed_qty != 0 && !eof){
				TIdent hold_id;
				int hold_qty;
				double hold_price;

				rsb.get_value(1, hold_id);
				rsb.get_value(2, hold_qty);
				rsb.get_value(3, hold_price);

				if(hold_qty + needed_qty < 0){
				    /**
				     *
				     * 	INSERT INTO
				     *		HOLDING_HISTORY (
				     *		HH_H_T_ID,
				     *		HH_T_ID,
				     *		HH_BEFORE_QTY,
				     *		HH_AFTER_QTY
				     *	)
				     *	VALUES(
				     *		hold_id, // H_T_ID of original trade
				     *		trade_id, // T_ID current trade
				     *		hold_qty, // H_QTY now
				     *		hold_qty + needed_qty // H_QTY after update
				     *	)
				     *
				     */

				    prholdinghistory->set_value(0, hold_id);
				    prholdinghistory->set_value(1, ptrin._trade_id);
				    prholdinghistory->set_value(2, hold_qty);
				    prholdinghistory->set_value(3, (hold_qty + needed_qty));

				    TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, hold_id, ptrin._trade_id, hold_price, (hold_qty + needed_qty));
				    e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
				    if (e.is_error()) {  goto done; }

				    /**
				     * 	UPDATE 	HOLDING
				     *	SET	H_QTY = hold_qty + needed_qty
				     *	WHERE	current of hold_list
				     */

				    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty + trade_qty));
				    e = _pholding_man->h_update_qty(_pssm, prholding, hold_id, (hold_qty + needed_qty));
				    if (e.is_error()) {  goto done; }

				    sell_value += needed_qty * hold_price;
				    buy_value += needed_qty * ptrin._trade_price;
				    needed_qty = 0;
				}
				else{
				    /**
				     *
				     * 	INSERT INTO
				     *		HOLDING_HISTORY (
				     *		HH_H_T_ID,
				     *		HH_T_ID,
				     *		HH_BEFORE_QTY,
				     *		HH_AFTER_QTY
				     *	)
				     *	VALUES(
				     *		hold_id, // H_T_ID of original trade
				     *		trade_id, // T_ID current trade
				     *		hold_qty, // H_QTY now
				     *		0 // H_QTY after update
				     *	)
				     */

				    TIdent ttt = hold_id;
				    prholdinghistory->set_value(0, hold_id);
				    prholdinghistory->set_value(1, ptrin._trade_id);
				    prholdinghistory->set_value(2, hold_qty);
				    prholdinghistory->set_value(3, 0);

				    TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d) \n", xct_id, hold_id, ptrin._trade_id, hold_price, 0);
				    e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
				    if (e.is_error()) {  goto done; }

				    /**
				     * DELETE FROM	HOLDING
				     * WHERE current of hold_list
				     */

				    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-delete-by-index (%ld) \n", ttt);
				    e = _pholding_man->h_delete_by_index(_pssm, prholding,ttt);
				    if (e.is_error()) {  goto done; }

				    hold_qty = -hold_qty;
				    sell_value += hold_qty * hold_price;
				    buy_value += hold_qty * ptrin._trade_price;
				    needed_qty = needed_qty - hold_qty;
				}

				TRACE( TRACE_TRX_FLOW, "App: %d TR:h-iter-next \n", xct_id);
				e = h_list_sort_iter.next(_pssm, eof, rsb);
				if (e.is_error()) {  goto done; }
			    }
			}
			else{
			    /**
			     * 	SELECT	H_T_ID, H_QTY, H_PRICE
			     *  FROM	HOLDING
			     *	WHERE	H_CA_ID = acct_id and H_S_SYMB = symbol
			     *	ORDER  BY H_DTS ASC
			     */

			    index_scan_iter_impl<holding_t>* tmp_h_iter;
			    TRACE( TRACE_TRX_FLOW, "App: %d TR:h-get-iter-by-idx2\n", xct_id);
			    e = _pholding_man->h_get_iter_by_index2(_pssm, tmp_h_iter, prholding, lowrep, highrep, acct_id, symbol);
			    if (e.is_error()) {  goto done; }
			    h_iter = tmp_h_iter;
			    //already sorted in ascending order because of index

			    bool eof;
			    e = h_iter->next(_pssm, eof, *prholding);
			    if (e.is_error()) {  goto done; }
			    while(needed_qty != 0 && !eof){
				char hs_s_symb[16];
				prholding->get_value(2, hs_s_symb, 16);

				if(strcmp(hs_s_symb, symbol) == 0){//FIX_ME
				    TIdent hold_id;
				    int hold_qty;
				    double hold_price;

				    prholding->get_value(5, hold_qty);
				    prholding->get_value(4, hold_price);
				    prholding->get_value(0, hold_id);

				    if(hold_qty + needed_qty < 0){
					/**
					 * 	INSERT INTO
					 *		HOLDING_HISTORY (
					 *		HH_H_T_ID,
					 *		HH_T_ID,
					 *		HH_BEFORE_QTY,
					 *		HH_AFTER_QTY
					 *	)
					 *	VALUES(
					 *		hold_id, // H_T_ID of original trade
					 *		trade_id, // T_ID current trade
					 *		hold_qty, // H_QTY now
					 *		hold_qty + needed_qty // H_QTY after update
					 *	)
					 */

					prholdinghistory->set_value(0, hold_id);
					prholdinghistory->set_value(1, ptrin._trade_id);
					prholdinghistory->set_value(2, hold_qty);
					prholdinghistory->set_value(3, (hold_qty + needed_qty));

					TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, hold_id, ptrin._trade_id, hold_price, (hold_qty + needed_qty));
					e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
					if (e.is_error()) {  goto done; }

					/**
					 * 	UPDATE 	HOLDING
					 *	SET	H_QTY = hold_qty + needed_qty
					 *	WHERE	current of hold_list
					 */

					TRACE( TRACE_TRX_FLOW, "App: %d TR:h-update (%ld) (%s) (%d) \n", xct_id, acct_id, symbol, (hs_qty + trade_qty));
					e = _pholding_man->h_update_qty(_pssm, prholding, hold_id, (hold_qty + needed_qty));
					if (e.is_error()) {  goto done; }

					sell_value += needed_qty * hold_price;
					buy_value += needed_qty * ptrin._trade_price;
					needed_qty = 0;
				    }
				    else{
					/**
					 * 	INSERT INTO
					 *		HOLDING_HISTORY (
					 *		HH_H_T_ID,
					 *		HH_T_ID,
					 *		HH_BEFORE_QTY,
					 *		HH_AFTER_QTY
					 *	)
					 *	VALUES(
					 *		hold_id, // H_T_ID of original trade
					 *		trade_id, // T_ID current trade
					 *		hold_qty, // H_QTY now
					 *		0 // H_QTY after update
					 *	)
					 */

					prholdinghistory->set_value(0, hold_id);
					prholdinghistory->set_value(1, ptrin._trade_id);
					prholdinghistory->set_value(2, hold_qty);
					prholdinghistory->set_value(3, 0);

					TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, hold_id, ptrin._trade_id, hold_price, 0);
					e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
					if (e.is_error()) {  goto done; }

					/**
					 * DELETE FROM	HOLDING
					 * WHERE current of hold_list
					 */

					TRACE( TRACE_TRX_FLOW, "App: %d TR:h-delete-by-index (%ld) (%s) \n", xct_id, acct_id, symbol);
					e = _pholding_man->h_delete_by_index(_pssm, prholding, hold_id);
					if (e.is_error()) {  goto done; }

					hold_qty = -hold_qty;
					sell_value += hold_qty * hold_price;
					buy_value += hold_qty * ptrin._trade_price;
					needed_qty = needed_qty - hold_qty;
				    }
				}
				TRACE( TRACE_TRX_FLOW, "App: %d TR:h-iter-next \n", xct_id);
				e = h_iter->next(_pssm, eof, *prholding);
				if (e.is_error()) {  goto done; }
			    }
			}
		    }
		    if (needed_qty > 0){
			/**
			 * 	INSERT INTO
			 *		HOLDING_HISTORY (
			 *		HH_H_T_ID,
			 *		HH_T_ID,
			 *		HH_BEFORE_QTY,
			 *		HH_AFTER_QTY
			 *	)
			 *	VALUES(
			 *		trade_id, // T_ID current is original trade
			 *		trade_id, // T_ID current trade
			 *		0, // H_QTY before
			 *		needed_qty // H_QTY after insert
			 *	)
			 */

			prholdinghistory->set_value(0, ptrin._trade_id);
			prholdinghistory->set_value(1, ptrin._trade_id);
			prholdinghistory->set_value(2, 0);
			prholdinghistory->set_value(3, needed_qty);

			TRACE( TRACE_TRX_FLOW, "App: %d TR:hh-add-tuple (%ld) (%ld) (%d) (%d)\n", xct_id, ptrin._trade_id, ptrin._trade_id, 0, needed_qty);
			e = _pholding_history_man->add_tuple(_pssm, prholdinghistory);
			if (e.is_error()) {  goto done; }

			/**
			 *
			 * 	INSERT INTO
			 *		HOLDING (
			 *				H_T_ID,
			 *				H_CA_ID,
			 *				H_S_SYMB,
			 *				H_DTS,
			 *				H_PRICE,
			 *				H_QTY
			 *			)
			 *	VALUES 		(
			 *				trade_id, // H_T_ID
			 *				acct_id, // H_CA_ID
			 *				symbol, // H_S_SYMB
			 *				trade_dts, // H_DTS
			 *				trade_price, // H_PRICE
			 *				needed_qty // H_QTY
			 *			)
			 */

			prholding->set_value(0, ptrin._trade_id);
			prholding->set_value(1, acct_id);
			prholding->set_value(2, symbol);
			prholding->set_value(3, trade_dts);
			prholding->set_value(4, ptrin._trade_price);
			prholding->set_value(5, needed_qty);

			TRACE( TRACE_TRX_FLOW, "App: %d TR:h-add-tuple (%ld) (%ld) (%s) (%d) (%d) (%d) \n", xct_id, ptrin._trade_id, acct_id, symbol, trade_dts,  ptrin._trade_price, needed_qty);
			e = _pholding_man->add_tuple(_pssm, prholding);
			if (e.is_error()) {  goto done; }
		    }
		    else if ((-hs_qty) == trade_qty){
			/**
			 * DELETE FROM	HOLDING_SUMMARY
			 * WHERE HS_CA_ID = acct_id and HS_S_SYMB = symbol
			 */

			TRACE( TRACE_TRX_FLOW, "App: %d TR:hs-delete-by-index (%ld) (%s) \n", xct_id, acct_id, symbol);
			e = _pholding_summary_man->hs_delete_by_index(_pssm, prholdingsummary, acct_id, symbol);
			if (e.is_error()) {  goto done; }
		    }
		}
	    }
	}
	//END FRAME2


	//BEGIN FRAME3
	double tax_amount = 0;
	if((tax_status == 1 || tax_status == 2) && (sell_value > buy_value))
	    {
		double tax_rates = 0;
		/**
		 *	SELECT 	tax_rates = sum(TX_RATE)
		 *	FROM	TAXRATE
		 *	WHERE  	TX_ID in ( 	SELECT	CX_TX_ID
		 *				FROM	CUSTOMER_TAXRATE
		 *				WHERE	CX_C_ID = cust_id)
		 */

		guard< index_scan_iter_impl<customer_taxrate_t> > cx_iter;
		{
		    index_scan_iter_impl<customer_taxrate_t>* tmp_cx_iter;
		    TRACE( TRACE_TRX_FLOW, "App: %d TR:ct-iter-by-idx (%ld) \n", xct_id, cust_id);
		    e = _pcustomer_taxrate_man->cx_get_iter_by_index(_pssm, tmp_cx_iter, prcusttaxrate, lowrep, highrep, cust_id);
		    if (e.is_error()) {  goto done; }
		    cx_iter = tmp_cx_iter;
		}
		bool eof;
		e = cx_iter->next(_pssm, eof, *prcusttaxrate);
		if (e.is_error()) {  goto done; }
		while (!eof) {
		    char tax_id[5]; //4
		    prcusttaxrate->get_value(0, tax_id, 5); //4

		    TRACE( TRACE_TRX_FLOW, "App: %d TR:tx-idx-probe (%s) \n", xct_id, tax_id);
		    e =  _ptaxrate_man->tx_index_probe(_pssm, prtaxrate, tax_id);
		    if (e.is_error()) {  goto done; }

		    double rate;
		    prtaxrate->get_value(2, rate);

		    tax_rates += rate;

		    e = cx_iter->next(_pssm, eof, *prcusttaxrate);
		    if (e.is_error()) {  goto done; }
		}
		tax_amount = (sell_value - buy_value) * tax_rates;

		/**
		 *	UPDATE	TRADE
		 *	SET	T_TAX = tax_amount
		 *	WHERE	T_ID = trade_id
		 */

		TRACE( TRACE_TRX_FLOW, "App: %d TR:t-upd-tax-by-ind (%ld) (%d)\n", xct_id, ptrin._trade_id, tax_amount);
		e = _ptrade_man->t_update_tax_by_index(_pssm, prtrade, ptrin._trade_id, tax_amount);
		if (e.is_error()) {  goto done; }

		assert(tax_amount > 0); //Harness control
	    }

	//END FRAME3

	double comm_rate = 0;
	char s_name[51]; //50
	//BEGIN FRAME4
	{
	    /**
	     *	SELECT	s_ex_id = S_EX_ID, s_name = S_NAME
	     *	FROM	SECURITY
	     *	WHERE	S_SYMB = symbol
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:security-idx-probe (%s) \n", xct_id,  symbol);
	    e =  _psecurity_man->s_index_probe(_pssm, prsecurity, symbol);
	    if (e.is_error()) {  goto done; }

	    char s_ex_id[7]; //6
	    prsecurity->get_value(4, s_ex_id, 7); //7
	    prsecurity->get_value(3, s_name, 51); //50

	    /**
	     *	SELECT	c_tier = C_TIER
	     *	FROM	CUSTOMER
	     *	WHERE	C_ID = cust_id
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:c-idx-probe (%ld) \n", xct_id,  cust_id);
	    e = _pcustomer_man->c_index_probe(_pssm, prcustomer, cust_id);
	    if (e.is_error()) {  goto done; }

	    short c_tier;
	    prcustomer->get_value(7, c_tier);

	    /**
	     *	SELECT	1 row comm_rate = CR_RATE
	     *	FROM	COMMISSION_RATE
	     *	WHERE	CR_C_TIER = c_tier and CR_TT_ID = type_id and CR_EX_ID = s_ex_id and
	     *			CR_FROM_QTY <= trade_qty and CR_TO_QTY >= trade_qty
	     */

	    guard< index_scan_iter_impl<commission_rate_t> > cr_iter;
	    {
		index_scan_iter_impl<commission_rate_t>* tmp_cr_iter;
		TRACE( TRACE_TRX_FLOW, "App: %d TR:cr-iter-by-idx (%d) (%s) (%ld) (%d) \n", xct_id, c_tier, type_id, s_ex_id, trade_qty);
		e = _pcommission_rate_man->cr_get_iter_by_index(_pssm, tmp_cr_iter, prcommissionrate, lowrep, highrep, c_tier, type_id, s_ex_id, trade_qty);
		if (e.is_error()) {  goto done; }
		cr_iter = tmp_cr_iter;
	    }

	    bool eof;
	    TRACE( TRACE_TRX_FLOW, "App: %d TR:cr-iter-next \n", xct_id);
	    e = cr_iter->next(_pssm, eof, *prcommissionrate);
	    if (e.is_error()) {  goto done; }
	    while (!eof) {			  
		int to_qty;
		prcommissionrate->get_value(4, to_qty);
		if(to_qty >= trade_qty){				
		    prcommissionrate->get_value(5, comm_rate);
		    break;
		}

		TRACE( TRACE_TRX_FLOW, "App: %d TR:cr-iter-next \n", xct_id);
		e = cr_iter->next(_pssm, eof, *prcommissionrate);
		if (e.is_error()) {  goto done; }
	    }
	}
	//END FRAME4
	assert(comm_rate > 0.00);  //Harness control

	double comm_amount = (comm_rate / 100) * (trade_qty * ptrin._trade_price);
	char st_completed_id[5] = "CMPT"; //4
	//BEGIN FRAME5
	{
	    /**
	     *	UPDATE	TRADE
	     * 	SET	T_COMM = comm_amount, T_DTS = trade_dts, T_ST_ID = st_completed_id,
	     *		T_TRADE_PRICE = trade_price
	     *	WHERE	T_ID = trade_id
	     */

	    prtrade->set_value(0, ptrin._trade_id);
	    TRACE( TRACE_TRX_FLOW, "App: %d TR:t-upd-ca_td_sci_tp-by-ind (%ld) (%lf) (%ld) (%s) (%lf) \n",
		   xct_id, ptrin._trade_id, comm_amount, trade_dts, st_completed_id, ptrin._trade_price);
	    e = _ptrade_man->t_update_ca_td_sci_tp_by_index(_pssm, prtrade, ptrin._trade_id, comm_amount, trade_dts, st_completed_id, ptrin._trade_price);
	    if (e.is_error()) {  goto done; }

	    /**
	     *	INSERT INTO TRADE_HISTORY(
	     *					TH_T_ID, TH_DTS, TH_ST_ID
	     *				)
	     *	VALUES 			(
	     *					trade_id, // TH_T_ID
	     *					now_dts, // TH_DTS
	     *					st_completed_id // TH_ST_ID
	     *				)
	     */

	    myTime now_dts = time(NULL);

	    prtradehist->set_value(0, ptrin._trade_id);
	    prtradehist->set_value(1, now_dts);
	    prtradehist->set_value(2, st_completed_id);

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:th-add-tuple (%ld) \n", xct_id, ptrin._trade_id);
	    e = _ptrade_history_man->add_tuple(_pssm, prtradehist);
	    if (e.is_error()) {  goto done; }

	    /**
	     *	UPDATE	BROKER
	     *	SET	B_COMM_TOTAL = B_COMM_TOTAL + comm_amount, B_NUM_TRADES = B_NUM_TRADES + 1
	     *	WHERE	B_ID = broker_id
	     */

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:broker-upd-ca_td_sci_tp-by-ind (%ld) (%d) \n", xct_id, broker_id, comm_amount);
	    e = _pbroker_man->broker_update_ca_nt_by_index(_pssm, prbroker, broker_id, comm_amount);
	    if (e.is_error()) {  goto done; }
	}
	//END FRAME5

	myTime due_date = time(NULL) + 48*60*60; //add 2 days
	double se_amount;
	if(type_is_sell){
	    se_amount = (trade_qty * ptrin._trade_price) - charge - comm_amount;
	}
	else{
	    se_amount = -((trade_qty * ptrin._trade_price) + charge + comm_amount);
	}
	if (tax_status == 1){
	    se_amount = se_amount - tax_amount;
	}

	//BEGIN FRAME6
	{
	    char cash_type[41] = "\0"; //40
	    if(trade_is_cash){
		strcpy (cash_type, "Cash Account");
	    }
	    else{
		strcpy(cash_type,"Margin");
	    }

	    /**
	     *	INSERT INTO SETTLEMENT (
	     *					SE_T_ID,
	     *					SE_CASH_TYPE,
	     *					SE_CASH_DUE_DATE,
	     *					SE_AMT
	     *				)
	     *	VALUES 			(
	     * 					trade_id,
	     *					cash_type,
	     *					due_date,
	     *					se_amount
	     *				)
	     */

	    prsettlement->set_value(0, ptrin._trade_id);
	    prsettlement->set_value(1, cash_type);
	    prsettlement->set_value(2, due_date);
	    prsettlement->set_value(3, se_amount);

	    TRACE( TRACE_TRX_FLOW, "App: %d TR:se-add-tuple (%ld)\n", xct_id, ptrin._trade_id);
	    e = _psettlement_man->add_tuple(_pssm, prsettlement);
	    if (e.is_error()) {  goto done; }

	    if(trade_is_cash){
		/**
		 *	update 	CUSTOMER_ACCOUNT
		 *	set	CA_BAL = CA_BAL + se_amount
		 *	where	CA_ID = acct_id
		 */

		TRACE( TRACE_TRX_FLOW, "App: %d TR:ca-upd-tuple \n", xct_id);
		prcustaccount->set_value(0, acct_id);
		e =  _pcustomer_account_man->ca_index_probe_forupdate(_pssm, prcustaccount, acct_id);
		if (e.is_error()) { return e; }	
		e = _pcustomer_account_man->ca_update_bal(_pssm, prcustaccount, se_amount);
		if (e.is_error()) {  goto done; }

		/**
		 *	insert into	CASH_TRANSACTION 	(
		 *							CT_DTS,
		 *							CT_T_ID,
		 *							CT_AMT,
		 *							CT_NAME
		 *						)
		 *	values 					(
		 *							trade_dts,
		 *							trade_id,
		 *							se_amount,
		 *							type_name + " " + trade_qty + " shares of " + s_name
		 *						)
		 */

		prcashtrans->set_value(0, ptrin._trade_id);
		prcashtrans->set_value(1, trade_dts);
		prcashtrans->set_value(2, se_amount);

		stringstream ss;
		ss << type_name << " " << trade_qty << " shares of " << s_name;
		prcashtrans->set_value(3, ss.str().c_str());

		TRACE( TRACE_TRX_FLOW, "App: %d TR:ct-add-tuple (%ld) (%ld) (%lf) (%s)\n", xct_id, trade_dts, ptrin._trade_id, se_amount, ss.str().c_str());
		e = _pcash_transaction_man->add_tuple(_pssm, prcashtrans);
		if (e.is_error()) {  goto done; }
	    }

	    /**
	     *	select	acct_bal = CA_BAL
	     *	from	CUSTOMER_ACCOUNT
	     *	where	CA_ID = acct_id
	     */

	    double acct_bal;
	    prcustaccount->get_value(5, acct_bal);
	}
	//END FRAME6
    }

#ifdef PRINT_TRX_RESULTS
    // at the end of the transaction
    // dumps the status of all the table rows used
    rtrade.print_tuple();
    rtradetype.print_tuple();
    rholdingsummary.print_tuple();
    rcustaccount.print_tuple();
    rholding.print_tuple();
    rsecurity.print_tuple();
    rbroker.print_tuple();
    rsettlement.print_tuple();
    rcashtrans.print_tuple();
    rcommussionrate.print_tuple();
    rcusttaxrate.print_tuple();
    rtaxrate.print_tuple();
    rcustomer.print_tuple();
    rtradehist.print_tuple();
#endif

 done:
	

#ifdef TESTING_TPCE           
    int exec=++trxs_cnt_executed[XCT_TPCE_TRADE_RESULT - XCT_TPCE_MIX - 1];
    if(e.is_error()) trxs_cnt_failed[XCT_TPCE_TRADE_RESULT - XCT_TPCE_MIX-1]++;
    if(exec%100==99) printf("TRADE_RESULT executed: %d, failed: %d\n", exec, trxs_cnt_failed[XCT_TPCE_TRADE_RESULT - XCT_TPCE_MIX-1]);
#endif

    // return the tuples to the cache
    _ptrade_man->give_tuple(prtrade);
    _ptrade_type_man->give_tuple(prtradetype);
    _pholding_summary_man->give_tuple(prholdingsummary);
    _pcustomer_account_man->give_tuple(prcustaccount);
    _pholding_man->give_tuple(prholding);
    _pholding_history_man->give_tuple(prholdinghistory);
    _psecurity_man->give_tuple(prsecurity);
    _pbroker_man->give_tuple(prbroker);
    _psettlement_man->give_tuple(prsettlement);
    _pcash_transaction_man->give_tuple(prcashtrans);
    _pcommission_rate_man->give_tuple(prcommissionrate);
    _pcustomer_taxrate_man->give_tuple(prcusttaxrate);
    _ptaxrate_man->give_tuple(prtaxrate);
    _pcustomer_man->give_tuple(prcustomer);
    _ptrade_history_man->give_tuple(prtradehist);

    return (e);
}

EXIT_NAMESPACE(tpce);    
