/* lz4_filters.cc
   Jeremy Barnes, 2 January 2014
   Copyright (c) 2014 Datacratic Inc.  All rights reserved.
*/

// (C) Copyright 2011 Datacratic Inc.
// (C) Copyright 2008 CodeRage, LLC (turkanis at coderage dot com)
// (C) Copyright 2003-2007 Jonathan Turkanis
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

// To configure Boost to work with zlib, see the 
// installation instructions here:
// http://boost.org/libs/iostreams/doc/index.html?path=7

// Define BOOST_IOSTREAMS_SOURCE so that <boost/iostreams/detail/config.hpp> 
// knows that we are building the library (possibly exporting code), rather 
// than using it (possibly importing code).
#define BOOST_IOSTREAMS_SOURCE 

#include "lz4_filters.h"
#include <boost/throw_exception.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include <boost/lexical_cast.hpp>

namespace boost { namespace iostreams {

//------------------Implementation of lz4_error------------------------------//
                    

lz4_error::lz4_error(Lz4MtResult error) 
    : BOOST_IOSTREAMS_FAILURE("lz4 error: " + string(lz4mtResultToString(error))),
      error_(error) 
    { }

void lz4_error::check BOOST_PREVENT_MACRO_SUBSTITUTION(int error)
{
    switch (error) {
    case LZ4_OK: 
    case LZ4_STREAM_END: 
    //case LZ4_BUF_ERROR: 
        return;
    case LZ4_MEM_ERROR: 
        boost::throw_exception(std::bad_alloc());
    default:
        boost::throw_exception(lz4_error(error));
        ;
    }
}


namespace detail {

std::string lz4_strerror(Lz4MtResult result)
{
    return lz4mtResultToString(result);
}

//------------------Implementation of lz4_base-------------------------------//


lz4_base::lz4_base(bool compress, const lz4_params & params)
    : compress_(compress)
{
    throw std::exception("init stream");
#if 0
    lz4_stream init = LZ4_STREAM_INIT;
    stream_ = init;
    
    lz4_ret res;
    if (compress_)
        res = lz4_easy_encoder(&stream_, params.level,
                                (lz4_check)params.crc);
    else
        res = lz4_stream_decoder(&stream_, 100 * 1024 * 1024, 0 /* flags */);
    
    if (res != LZ4_OK)
        boost::throw_exception(lz4_error(res));
}

lz4_base::~lz4_base()
{
    throw std::exception("init stream");
    lz4_end(&stream_);
}

void lz4_base::before( const char*& src_begin, const char* src_end,
                        char*& dest_begin, char* dest_end )
{
    stream_.next_in = reinterpret_cast<const uint8_t*>(src_begin);
    stream_.avail_in = static_cast<size_t>(src_end - src_begin);
    stream_.next_out = reinterpret_cast<uint8_t*>(dest_begin);
    stream_.avail_out= static_cast<size_t>(dest_end - dest_begin);
}

void lz4_base::after(const char*& src_begin, char*& dest_begin)
{
    const char* next_in = reinterpret_cast<const char*>(stream_.next_in);
    char* next_out = reinterpret_cast<char*>(stream_.next_out);
    src_begin = next_in;
    dest_begin = next_out;
}

int lz4_base::process(const char * & src_begin,
                        const char * & src_end,
                        char * & dest_begin,
                        char * & dest_end,
                        int flushLevel)
{
    //cerr << "processing with " << std::distance(src_begin, src_end)
    //     << " bytes in input and " << std::distance(dest_begin, dest_end)
    //     << " bytes in output with flush level " << flushLevel << endl;

    lz4_action action;
        
    if (compress_) {
        switch (flushLevel) {
        case lz4::run:          action = LZ4_RUN;         break;
        case lz4::sync_flush:   action = LZ4_SYNC_FLUSH;  break;
        case lz4::full_flush:   action = LZ4_FULL_FLUSH;  break;
        case lz4::finish:       action = LZ4_FINISH;      break;
        default:
            boost::throw_exception(lz4_error(LZ4_OPTIONS_ERROR));
        }
    } else {
        action = LZ4_RUN;
    }

    lz4_ret result = LZ4_OK;
    
    for (;;) {
        if (dest_begin == dest_end) return result;

        before(src_begin, src_end, dest_begin, dest_end);
        result = lz4_code(&stream_, action);
        after(src_begin, dest_begin);

        //cerr << "    processing with " << std::distance(src_begin, src_end)
        //     << " bytes in input and " << std::distance(dest_begin, dest_end)
        //     << " bytes in output with action " << action
        //     << " returned result " << lz4_strerror(result)
        //     << endl;


        if (result == LZ4_OK && action != LZ4_RUN) continue;
        if (result == LZ4_OK && action == LZ4_RUN) break;
        if (result == LZ4_STREAM_END) break;

        boost::throw_exception(lz4_error(result));
    }

    return result;
}

} // End namespace detail.

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, boost.
