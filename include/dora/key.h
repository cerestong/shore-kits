/* -*- mode:C++; c-basic-offset:4 -*- */

/** @file:   key.h
 *
 *  @brief:  Implementation of a template-based Key class
 *
 *  @author: Ippokratis Pandis, Oct 2008
 */

#ifndef __DORA_KEY_H
#define __DORA_KEY_H


#ifdef __SUNPRO_CC
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#else
#include <cstdlib>
#include <cstdio>
#include <cstring>
#endif

#include <iostream>
#include <sstream>
#include <vector>

#include "dora/dora_error.h"

#include "sm/shore/shore_env.h"


ENTER_NAMESPACE(dora);

using std::vector;

const char KS_DELIMETER[] = "|";


template<typename DataType> struct key_wrapper_t;
template<typename DataType> std::ostream& operator<< (std::ostream& os,
                                                      const key_wrapper_t<DataType>& rhs);



/******************************************************************** 
 *
 * @struct: key_wrapper_t
 *
 * @brief:  Template-based class used for Keys
 *
 * @note:   - Wraps a vector of key entries. Needed for STL  
 *          - All the entries of the key of the same type
 * 
 *
 ********************************************************************/

template<typename DataType>
struct key_wrapper_t
{
    typedef vector<DataType>           DataVec;
    typedef typename DataVec::iterator DataVecIt;

    // the vector with the entries - of the same type
    DataVec _key_v;

    // empty constructor
    key_wrapper_t() { }

    // argument constructor
    key_wrapper_t(const DataVec& aVector) {
        copy_vector(aVector);
    }

    key_wrapper_t(const int keysz) {
        reserve(keysz);
    }
    
    // copy constructor
    key_wrapper_t<DataType>& operator=(const key_wrapper_t<DataType>& rhs) {
        copy_vector(rhs._key_v);
        return (*this);
    }
    

    // destructor
    ~key_wrapper_t() { }

    // reset
    void reset() {
        _key_v.clear();
    }

    // push one item
    void push_back(DataType& anitem) {
        _key_v.push_back(anitem);
    }

    // reserve vector space
    void reserve(const int keysz) {
        assert (keysz);
        _key_v.reserve(keysz);
    }

    // comparison operators
    bool operator<(const key_wrapper_t<DataType>& rhs) const;
    bool operator==(const key_wrapper_t<DataType>& rhs) const;
    bool operator<=(const key_wrapper_t<DataType>& rhs) const;

    // helper functions
    inline void copy_vector(const DataVec& aVec) {
        assert (_key_v.empty());
        _key_v = aVec; // copy vector content
    }

    string toString() {
        std::ostringstream out = string("");
        for (DataVecIt it=_key_v.begin(); it!=_key_v.end(); ++it)
            out << out << (*it) << "|";
        return (out.str());
    }

    // friend function
    template<class DataType> friend std::ostream& operator<< (std::ostream& os, 
                                                              const key_wrapper_t<DataType>& rhs);

}; // EOF: struct key_wrapper_t



template<typename DataType> 
std::ostream& operator<< (std::ostream& os,
                          const key_wrapper_t<DataType>& rhs)
{
    for (int i=0; i<rhs._key_v.size(); i++) {
        os << rhs._key_v[i] << "|";
    }
    return (os);
}




/** struct key_wrapper_t methods */

// comparison operators

// less
template<class DataType>
bool key_wrapper_t<DataType>::operator<(const key_wrapper_t<DataType>& rhs) const {
    assert (_key_v.size()<=rhs._key_v.size()); // not necesserily of the same length
    for (int i=0; i<_key_v.size(); i++) {
        // goes over the key fields until one inequality is found
        if (_key_v[i]==rhs._key_v[i])
            continue;
        return (_key_v[i]<rhs._key_v[i]);        
    }
    return (false); // irreflexivity - f(x,x) must be false
}

// equal
template<class DataType>
bool key_wrapper_t<DataType>::operator==(const key_wrapper_t<DataType>& rhs) const {
    assert (_key_v.size()<=rhs._key_v.size()); // not necesserily of the same length
    for (int i=0; i<_key_v.size(); i++) {
        // goes over the key fields until one inequality is found
        if (_key_v[i]==rhs._key_v[i])
            continue;
        return (false);        
    }
    return (true);
}

// less or equal
template<class DataType>
bool key_wrapper_t<DataType>::operator<=(const key_wrapper_t<DataType>& rhs) const {
    assert (_key_v.size()<=rhs._key_v.size()); // not necesserily of the same length
    for (int i=0; i<_key_v.size(); i++) {
        // goes over the key fields
        if (_key_v[i]==rhs._key_v[i])
            continue;
        return (_key_v[i]<rhs._key_v[i]);        
    }
    // if reached this point all fields are equal so the two keys are equal
    return (true); 
}



EXIT_NAMESPACE(dora);

#endif /* __DORA_KEY_H */
