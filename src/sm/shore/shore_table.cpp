/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file shore_table.cpp
 *
 *  @brief Implementation of shore_table class
 *
 *  @author: Mengzhi Wang, April 2001
 *  @author: Ippokratis Pandis, January 2008
 *
 */

#include "sm/shore/shore_table.h"

using namespace shore;



/* ----------------------------------- */
/* --- @class table_desc_t methods --- */
/* ----------------------------------- */


#define VAR_SLOT(start, offset)   ((offset_t*)((start)+(offset)))
#define SET_NULL_FLAG(start, offset)                            \
    (*(char*)((start)+((offset)>>3))) &= (1<<(offset%8))
#define IS_NULL_FLAG(start, offset)                     \
    (*(char*)((start)+((offset)>>3)))&(1<<(offset%8))




/* -------------------------- */
/* --- bulkload utilities --- */
/* -------------------------- */



/********************************************************************* 
 *
 *  @fn:    load_from_file
 *  
 *  @brief: Loads a table from a datafile which has name. "tablename".tbl
 *
 *  @note:  The index file should already been created by create_index()  
 *
 *********************************************************************/

w_rc_t table_desc_t::load_from_file(ss_m* db, const char* fname)
{
    /* 0. get filename */
    char filename[MAX_FILENAME_LEN];

    if (fname == NULL) {
        // if null use "name().tbl" file
        strcpy(filename, name());
        strcat(filename, ".tbl");
    }
    else {
        int sz = (strlen(fname)>MAX_FILENAME_LEN ? strlen(fname) : MAX_FILENAME_LEN);
        strncpy(filename, fname, sz);
    }

    FILE* fd = fopen(filename, "r");
    if(fd == NULL) {
        cerr << "fopen() failed on " << filename << endl;
	return RC(se_NOT_FOUND);
    }    

    cout << "Loading " << _name << " table from (" << filename << ") file..." << endl;
    
    W_DO(db->begin_xct());
    
    /* 1. create the warehouse table */
    W_DO(create_table(db));
         
    /* 2. append the tuples */
    append_file_i file_append(_fid);    

    /* 3a. read the file line-by-line */
    /* 3b. convert each line to a tuple for this table */
    /* 3c. insert tuple to table */
    register int tuple_count = 0;
    register int mark = COMMIT_ACTION_COUNT;
    table_row_t  tuple(this);
    bool btread = false;
    char linebuffer[MAX_LINE_LENGTH];

    for(int i=0; fgets(linebuffer, MAX_LINE_LENGTH, fd); i++) {
        btread = read_tuple_from_line(tuple, linebuffer);
	W_DO(file_append.create_rec(vec_t(), 0,
				    vec_t(tuple.format(), tuple.size()),
				    tuple._rid));

	if(i >= mark) {
	    W_COERCE(db->commit_xct());
            cerr << "load(" << name() << "): " << tuple_count << endl;
	    W_COERCE(db->begin_xct());
	    mark += COMMIT_ACTION_COUNT;
	}	    
        tuple_count++;
    }
    
    /* 3. commit and print statistics */
    W_DO(db->commit_xct());
    cout << "# of records inserted: " << tuple_count << endl;
    cout << "Building indices ... " << endl;

    /* 4. update the index structures */
    return (bulkload_all_indexes(db));
}


/********************************************************************* 
 *
 *  @fn:    bulkload_index
 *  
 *  @brief: Iterates a file and inserts (one by one) the corresponding 
 *          entries to the specified index. 
 *
 *  @note:  The index file should already been created by create_index()  
 *
 *********************************************************************/

w_rc_t table_desc_t::bulkload_index(ss_m* db,
                                    index_desc_t* index)
{
    assert (index);

    cout << "Starting to build index: " << index->name() << endl;
    
    W_DO(db->begin_xct());
    
    /* 1. open a (table) scan iterator over the table */
    table_scan_iter_impl* iter;
    W_DO(get_iter_for_file_scan(db, iter));

    bool        eof;
    register int tuple_count = 0;
    register int mark = COMMIT_ACTION_COUNT;
    table_row_t row(this);
    time_t tstart = time(NULL);

    /* 2. iterate over the whole table and insert the corresponding index entries */    
    W_DO(iter->next(db, eof, row));
    while (!eof) {
	W_DO(db->create_assoc(index->fid(),
                              vec_t(format_key(index, &row), key_size(index, &row)),
                              vec_t(&(row._rid), sizeof(rid_t))));
	W_DO(iter->next(db, eof, row));
	tuple_count++;

	if (tuple_count >= mark) { 
            W_DO(db->commit_xct());
            cerr << "index(" << index->name() << "): " << tuple_count << endl;
            W_DO(db->begin_xct());
            mark += COMMIT_ACTION_COUNT;
	}
    }
    W_DO(db->commit_xct());
    delete iter;


    /* 5. print stats */
    time_t tstop = time(NULL);
    cout << "Index " << index->name() << " loaded in " << (tstop - tstart) << " secs..." << endl;

    return (RCOK);
}


/* bulkload an index - find index by name */
w_rc_t table_desc_t::bulkload_index(ss_m* db,
                                    const char* name)
{
    index_desc_t* index = (_indexes ? _indexes->find_by_name(name) : NULL);

    if (index)
        return bulkload_index(db, index);
    return RC(se_INDEX_NOT_FOUND);
}


/* bulkloads all indexes */
w_rc_t table_desc_t::bulkload_all_indexes(ss_m* db)
{
    cout << "Start building indices for: " << _name << endl;
  
    index_desc_t* index = _indexes;

    while (index) {
	// build one index at each iteration.
	W_DO(bulkload_index(db, index));
	index = index->next();
    }

    cout << "End building indices" << endl;
    return RCOK;
}


/* --------------------------------- */
/* --- access data through index --- */
/* --------------------------------- */


/********************************************************************* 
 *
 *  @fn:    index_probe
 *  
 *  @brief: Finds the rid of the specified key using a certain index
 *
 *  @note:  The key is parsed from the tuple that it is passed as parameter
 *
 *********************************************************************/

w_rc_t  table_desc_t::index_probe(ss_m* db,
                                  index_desc_t* index,
                                  table_row_t* ptuple,
                                  lock_mode_t lmode)
{
    assert (index);
    assert (ptuple); 

    bool     found = false;
    smsize_t len = sizeof(rid_t);

    /* 1. ensure valid index */
    W_DO(index->check_fid(db));

    /* 2. find the tuple in the B+tree */
    W_DO(ss_m::find_assoc(index->fid(),
			  vec_t(format_key(index, ptuple), key_size(index, ptuple)),
			  &(ptuple->_rid),
			  len,
			  found));

    if (!found) return RC(se_TUPLE_NOT_FOUND);

    /* 3. read the tuple */
    pin_i pin;
    W_DO(pin.pin(ptuple->rid(), 0, lmode));
    if (!ptuple->load(pin.body())) return RC(se_WRONG_DISK_DATA);
    pin.unpin();
  
    return RCOK;
}



/* ----------------------------------------- */
/* --- create physical table and indexes --- */
/* ----------------------------------------- */


/********************************************************************* 
 *
 *  @fn:    create_table
 *
 *  @brief: Creates the physical table and all the corresponding indexes
 *
 *********************************************************************/

w_rc_t table_desc_t::create_table(ss_m* db)
{
    if (!is_vid_valid() || !is_root_valid())
	W_DO(find_root_iid(db));

    /* create the table */
    W_DO(db->create_file(vid(), _fid, smlevel_3::t_regular));

    /* add table entry to the metadata tree */
    file_info_t file;
    file.set_ftype(FT_REGULAR);
    file.set_fid(_fid);
    W_DO(ss_m::create_assoc(root_iid(),
			    vec_t(name(), strlen(name())),
			    vec_t(&file, sizeof(file_info_t))));
    

    /* create all the indexes of the table */
    index_desc_t* index = _indexes;
    while (index) {
	stid_t iid;

        /* create index */
	W_DO(db->create_index(_vid,
                              (index->is_unique() ? ss_m::t_uni_btree : ss_m::t_btree),
                              ss_m::t_regular,
                              index_keydesc(index),
                              ss_m::t_cc_kvl,
                              iid));
	index->set_fid(iid);

        /* add index entry to the metadata tree */
        if (index->is_primary())
            file.set_ftype(FT_PRIMARY_IDX);
        else
            file.set_ftype(FT_IDX);

	file.set_fid(iid);
	W_DO(db->create_assoc(root_iid(),
                              vec_t(index->name(), strlen(index->name())),
                              vec_t(&file, sizeof(file_info_t))));
				
        /* move to next index */
	index = index->next();
    }
    
    return (RCOK);
}


/****************************************************************** 
 *  
 *  @fn:    create_index
 *
 *  @brief: Create a regular or primary index on the table
 *
 *  @note:  This only creates the index decription for the index in memory. 
 *          Call bulkload_index to create and populate the index on disks.
 *
 ******************************************************************/

bool table_desc_t::create_index(const char* name,
                                const int* fields,
                                const int num,
                                const bool unique,
                                const bool primary)
{
    index_desc_t* p_index = new index_desc_t(name, num, fields, unique);

    /* check the validity of the index */
    for (int i=0; i<num; i++)  {
        assert(fields[i] >= 0 && fields[i] < _field_count);

        /* only the last field in the index can be variable lengthed */
        if (_desc[fields[i]].is_variable_length() && i != num-1) {
            assert(false);
        }
    }

    /* link it to the list */
    if (_indexes == NULL) _indexes = p_index;
    else _indexes->insert(p_index);

    /* add as primary */
    if (p_index->is_unique() && p_index->is_primary())
        _primary_idx = p_index;

    return true;
}


bool  table_desc_t::create_primary_idx(const char* name,
                                       const int* fields,
                                       const int num)
{
    index_desc_t* p_index = new index_desc_t(name, num, fields, true, true);

    /* check the validity of the index */
    for (int i=0; i<num; i++) {
        assert(fields[i] >= 0 && fields[i] < _field_count);

        /* only the last field in the index can be variable lengthed */
        if (_desc[fields[i]].is_variable_length() && i != num-1) {
            assert(false);
        }
    }

    /* link it to the list of indexes */
    if (_indexes == NULL) _indexes = p_index;
    else _indexes->insert(p_index);

    /* make it the primary index */
    _primary_idx = p_index;

    return (true);
}



/* -------------------------- */
/* --- tuple manipulation --- */
/* -------------------------- */


/********************************************************************* 
 *
 *  @fn:    add_tuple
 *
 *  @brief: Inserts a tuple to a table and all the indexes of the table
 *
 *  @note:  This function should be called in the context of a trx.
 *          The passed tuple should be formed. If everything goes as
 *          expected the _rid of the tuple will be set. 
 *
 *********************************************************************/

w_rc_t table_desc_t::add_tuple(ss_m* db, table_row_t* tuple)
{
    assert (tuple);

    /* 1. find the file */
    W_DO(check_fid(db));

    /* 2. append the tuple */
    W_DO(db->create_rec(fid(), vec_t(), tuple->size(),
                        vec_t(tuple->format(), tuple->size()),
                        tuple->_rid));

    /* 3. update the indexes */
    index_desc_t* index = _indexes;
    while (index) {
	W_DO(db->create_assoc(index->fid(),
                              vec_t(format_key(index, tuple), key_size(index, tuple)),
                              vec_t(&(tuple->_rid), sizeof(rid_t))));

        /* move to next index */
	index = index->next();
    }

    return (RCOK);
}



/********************************************************************* 
 *
 *  @fn:    update_tuple
 *
 *  @brief: Upldates a tuple from a table
 *
 *  @note:  This function should be called in the context of a trx.
 *          The passed tuple should be valid.
 *          There is no need of updating the indexes.
 *
 *********************************************************************/

w_rc_t table_desc_t::update_tuple(ss_m* db, table_row_t* tuple)
{
    assert (tuple);

    if (!tuple->is_rid_valid()) return RC(se_NO_CURRENT_TUPLE);

    /* 1. pin record */
    pin_i pin;
    W_DO(pin.pin(tuple->rid(), 0, EX));

    int current_size = pin.body_size();

    /* 2. update record */
    if (current_size < tuple->size()) {
        w_rc_t rc = db->append_rec(tuple->rid(),
                                   zvec_t(tuple->size() - current_size),
                                   true);
        // on error unpin 
        if (rc!=(RCOK)) pin.unpin();
        W_DO(rc);
    }
    w_rc_t rc = db->update_rec(tuple->rid(), 
                               0, 
                               vec_t(tuple->format(), tuple->size()));

    /* 3. unpin */
    pin.unpin();
    return (rc);
}



/********************************************************************* 
 *
 *  @fn:    delete_tuple
 *
 *  @brief: Deletes a tuple from a table and the corresponding entries
 *          on all the indexes of the table
 *
 *  @note:  This function should be called in the context of a trx
 *          The passed tuple should be valid.
 *
 *********************************************************************/

w_rc_t  table_desc_t::delete_tuple(ss_m* db, table_row_t* ptuple)
{
    assert (ptuple);

    if (!ptuple->is_rid_valid()) return RC(se_NO_CURRENT_TUPLE);

    /* 1. delete the tuple */
    W_DO(db->destroy_rec(ptuple->rid()));

    /* 2. delete all the corresponding index entries */
    index_desc_t* index = _indexes;
    while (index) {
	W_DO(index->find_fid(db));
	W_DO(db->destroy_assoc(index->fid(),
                               vec_t(format_key(index, ptuple), key_size(index,ptuple)),
                               vec_t(&(ptuple->_rid), sizeof(rid_t))));

        /* move to next index */
	index = index->next();
    }

    /* invalidate tuple */
    ptuple->set_rid(rid_t::null);

    return RCOK;
}



/* ---------------------- */
/* --- tuple iterator --- */
/* ---------------------- */


w_rc_t  table_desc_t::get_iter_for_file_scan(ss_m* db,
                                             table_scan_iter_impl* &iter)
{
    iter = new table_scan_iter_impl(db, this);
    if (iter->opened()) return RCOK;
    return RC(se_OPEN_SCAN_ERROR);
}


w_rc_t table_desc_t::get_iter_for_index_scan(ss_m* db,
                                             index_desc_t* index,
                                             index_scan_iter_impl* &iter,
                                             scan_index_i::cmp_t c1,
                                             const cvec_t& bound1,
                                             scan_index_i::cmp_t c2,
                                             const cvec_t& bound2,
                                             bool need_tuple)
{
    iter = new index_scan_iter_impl(db, index, need_tuple );
    W_DO( iter->open_scan(db, c1, bound1, c2, bound2, 
                          maxkeysize(index)));

    if (iter->opened())  return RCOK;
    return RC(se_OPEN_SCAN_ERROR);
}



/* ------------------- */
/* --- check index --- */
/* ------------------- */


/** @fn:    check_all_indexes
 *
 *  @brief: Check all indexes
 */

bool table_desc_t::check_all_indexes(ss_m* db)
{
    index_desc_t* index = _indexes;

    cout << "Checking consistency of the indexes on table " << _name << endl;

    while (index) {
	w_rc_t rc = check_index(db, index);
	if (rc) {
	    cout << "Index checking error in " << name() << " " << index->name() << endl;
	    cout << "Due to " << rc << endl;
	    return false;
	}
	index = index->next();
    }
    return true;
}


/********************************************************************* 
 *
 *  @fn:    check_index
 *
 *  @brief: Checks all the values on an index. It first gets the rid from
 *          the table (by scanning) and then probes the index for the same
 *          tuple. It reports error if the two rids do not match.
 *
 *********************************************************************/

w_rc_t table_desc_t::check_index(ss_m* db,
                                 index_desc_t* pindex)
{
    cout << "Start to check index " << pindex->name() << endl;

    // XCT_BEGIN
    W_DO(db->begin_xct());

    table_scan_iter_impl* iter;
    W_DO(get_iter_for_file_scan(db, iter));

    bool        eof = false;
    table_row_t tablerow(this);
    W_DO(iter->next(db, eof, tablerow));
    while (!eof) {
        // remember the rid just scanned
        rid_t tablerid = tablerow.rid();
	W_DO(index_probe(db, pindex, &tablerow));
	if (tablerid != tablerow.rid()) {
            cerr << " Inconsistent index... " << endl;
            return RC(se_INCONSISTENT_INDEX);
	}
	W_DO(iter->next(db, eof, tablerow));
    }

    W_DO(db->commit_xct());
    // XCT_COMMIT

    delete iter;
    return (RCOK);
}



/* ------------------ */
/* --- scan index --- */
/* ------------------ */


/** @fn:    scan_all_indexes
 *
 *  @brief: Scan all indexes
 */

w_rc_t table_desc_t::scan_all_indexes(ss_m* db)
{
    index_desc_t* index = _indexes;
    while (index) {
	W_DO(scan_index(db, index));
	index = index->next();
    }
    return (RCOK);
}


/********************************************************************* 
 *
 *  @fn:    scan_index
 *
 *  @brief: Iterates over all the values on an index
 *
 *********************************************************************/

w_rc_t table_desc_t::scan_index(ss_m* db, index_desc_t* index)
{
    assert (index);

    cout << "Scanning index " << index->name() << " for table " << name() << endl;

    /* 1. open a index scanner */
    index_scan_iter_impl* iter;
    char* high_key = new char [maxkeysize(index)];
    char* low_key = new char [maxkeysize(index)];

    table_row_t lowtuple(this);
    table_row_t hightuple(this);

    memcpy(low_key, min_key(index, &lowtuple), key_size(index, &lowtuple));
    memcpy(high_key, max_key(index, &hightuple), key_size(index, &hightuple));

    W_DO(get_iter_for_index_scan(db, index, iter,
				 scan_index_i::ge,
				 vec_t(low_key, key_size(index, &lowtuple)),
				 scan_index_i::le,
				 vec_t(high_key, key_size(index, &hightuple)),
				 false));
    delete high_key;
    delete low_key;

    /* 2. iterate over all index records */
    bool        eof;
    int         count = 0;    
    table_row_t row(this);
    W_DO(iter->next(db, eof, row));
    while (!eof) {	
	pin_i  pin;
	W_DO(pin.pin(row.rid(), 0));
	if (!row.load(pin.body())) return RC(se_WRONG_DISK_DATA);
	pin.unpin();
        row.print_value();

	W_DO(iter->next(db, eof, row));
	count++;
    }
    delete iter;

    /* 3. print out some statistics */
    cout << count << " tuples found!" << endl;
    cout << "Scan finished!" << endl;

    return (RCOK);
}


/* ----------------- */
/* --- debugging --- */
/* ----------------- */

w_rc_t table_desc_t::print_table(ss_m* db)
{
    char   filename[MAX_FILENAME_LEN];
    strcpy(filename, name());
    strcat(filename, ".tbl.tmp");
    ofstream fout(filename);

    W_DO(db->begin_xct());

    table_scan_iter_impl* iter;
    W_DO(get_iter_for_file_scan(db, iter));

    bool        eof = false;
    table_row_t row(this);
    W_DO(iter->next(db, eof, row));
    while (!eof) {
	row.print_value(fout);
	W_DO(iter->next(db, eof, row));
    }

    W_DO(db->commit_xct());
    delete iter;

    return RCOK;
}


/* For debug use only: print the description for all the field. */
void table_desc_t::print_desc(ostream& os)
{
    os << "Schema for table " << _name << endl;
    os << "Numer of fields: " << _field_count << endl;
    for (int i=0; i<_field_count; i++) {
	_desc[i].print_desc(os);
    }
}







//////////////////////////////////////////////////////////////////////
// 
// class table_desc_t methods 
//
//////////////////////////////////////////////////////////////////////


/* ----------------- */
/* --- formating --- */
/* ----------------- */


/********************************************************************* 
 *
 *  @fn:    format
 *
 *  @brief: For a given tuple in memory (data in _value array), we
 *          format the tuple in disk format so that we can push it 
 *          down to data pages in Shore.
 *
 *  @note:  convert: memory -> disk format
 *
 *********************************************************************/

const char* table_row_t::format()
{
    int i;
    int var_count = 0;
    int fixed_size = 0;
    int null_count = 0;

    /* 1. calculate the total space occupied by the fixed
     * length fields and null flags.
     */
    for (i=0; i<_field_cnt; i++) {
	if (_pvalues[i].is_variable_length()) {
	    var_count++;
	}
	else fixed_size += _ptable->maxsize();
	if (_pvalues[i].field_desc()->allow_null())  null_count++;
    }

    /* 2. allocate space for formatted data */
    if (!_formatted_data) {
	_formatted_data = new char[_ptable->maxsize()+var_count*sizeof(offset_t)+null_count];
    }

    /* 3. format the data */

    // current offset for fixed length field values
    offset_t    fixed_offset = 0;
    if (null_count) fixed_offset = ((null_count-1) >> 3) + 1;
    // current offset for variable length field slots
    offset_t    var_slot_offset = fixed_offset + fixed_size; 
    // current offset for variable length field values
    offset_t    var_offset = var_slot_offset + sizeof(offset_t)*var_count;

    int    null_index = -1;
    for (i=0; i<_field_cnt; i++) {
	if (_pvalues[i].field_desc()->allow_null()) {
	    null_index++;
	    if (_pvalues[i].is_null()) {
		SET_NULL_FLAG(_formatted_data, null_index);
	    }
	}

	if (_pvalues[i].is_variable_length()) {
	    _pvalues[i].copy_value(_formatted_data + var_offset);
	    var_offset += _pvalues[i].realsize();

	    // set the offset
	    offset_t   len = _pvalues[i].realsize();
	    memcpy(VAR_SLOT(_formatted_data, var_slot_offset), &len, sizeof(offset_t));
	    var_slot_offset += sizeof(offset_t);
	}
	else {
	    _pvalues[i].copy_value(_formatted_data + fixed_offset);
	    fixed_offset += _pvalues[i].field_desc()->maxsize();
	}
    }

    return _formatted_data;
}


/** @fn    load
 *
 *  @brief Given a tuple in disk format, we read it back into memory
 *         (_value array).
 *
 *  @note  convert: disk -> memory format
 */

bool table_row_t::load(const char* data)
{
    int i;
    int var_count = 0;
    int fixed_size = 0;
    int null_count = 0;

    /* 1. calculate the total space occupied by the fixed
     * length fields.
     */
    for (i=0; i<_field_cnt; i++) {
	if (_pvalues[i].is_variable_length())
	    var_count++;
	else 
            fixed_size += _pvalues[i].field_desc()->maxsize();

	if (_pvalues[i].field_desc()->allow_null())  
            null_count++;
    }


    /* 2. read the data field by field */

    // current offset for fixed length field values
    offset_t    fixed_offset = 0;
    if (null_count) fixed_offset = ((null_count-1) >> 3) + 1;
    // current offset for variable length field slots
    offset_t    var_slot_offset = fixed_offset + fixed_size; 
    // current offset for variable length field values
    offset_t    var_offset = var_slot_offset + sizeof(offset_t)*var_count;

    int null_index = -1;
    for (i=0; i<_field_cnt; i++) {
	if (_pvalues[i].field_desc()->allow_null()) {
	    null_index++;
	    if (IS_NULL_FLAG(data, null_index)) {
		_pvalues[i].set_null();
		continue;
	    }
	}

	if (_pvalues[i].is_variable_length()) {
	    offset_t var_len;
	    memcpy(&var_len,  VAR_SLOT(data, var_slot_offset), sizeof(offset_t));
	    _pvalues[i].set_value(data+var_offset,
                                  var_len);
	    var_offset += var_len;
	    var_slot_offset += sizeof(offset_t);
	}
	else {
	    _pvalues[i].set_value(data+fixed_offset,
                                  _pvalues[i].field_desc()->maxsize());
	    fixed_offset += _pvalues[i].field_desc()->maxsize();
	}
    }
    return true;
}


bool   table_row_t::load_keyvalue(const unsigned char* string,
                                  index_desc_t* index)
{
    int offset = 0;
    for (int i=0; i<index->field_count(); i++) {
	int field_index = index->key_index(i);
	if (_pvalues[field_index].is_variable_length()) {
	    _pvalues[field_index].set_value(string + offset,
                                            _pvalues[field_index].realsize());
	    offset += _pvalues[field_index].field_desc()->maxsize();
	}
	else {
	    int size = _pvalues[field_index].field_desc()->maxsize();
	    _pvalues[field_index].set_value(string + offset,
                                            size);
	    offset += size;
	}
    }
    return true;
}


/* ----------------- */
/* --- debugging --- */
/* ----------------- */

/* For debug use only: print the value for all the fields. */
void table_row_t::print_value(ostream& os)
{
    //  cout << "Numer of fields: " << _field_count << endl;
    for (int i=0; i<_field_cnt; i++) {
	_pvalues[i].print_value(os);
	if (i != _field_cnt) os << DELIM_CHAR;
    }
    os << endl;
}