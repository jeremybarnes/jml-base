// -*- C++ -*-
// (C) Copyright 2011 Datacratic Inc.
// (C) Copyright 2008 CodeRage, LLC (turkanis at coderage dot com)
// (C) Copyright 2003-2007 Jonathan Turkanis
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.)

// See http://www.boost.org/libs/iostreams for documentation.

// Note: custom allocators are not supported on VC6, since that compiler
// had trouble finding the function zlib_base::do_init.

#ifndef __utils__lz4_h__
#define __utils__lz4_h__

#include <iostream>

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif              

#include <cassert>                            
#include <iosfwd>            // streamsize.                 
#include <memory>            // allocator, bad_alloc.
#include <new>          
#include <boost/config.hpp>  // MSVC, STATIC_CONSTANT, DEDUCED_TYPENAME, DINKUM.
#include <boost/cstdint.hpp> // uint*_t
#include <boost/detail/workaround.hpp>
#include <boost/iostreams/constants.hpp>   // buffer size.
#include <boost/iostreams/detail/config/auto_link.hpp>
#include <boost/iostreams/detail/config/dyn_link.hpp>
#include <boost/iostreams/detail/config/wide_streams.hpp>
#include <boost/iostreams/detail/config/zlib.hpp>
#include <boost/iostreams/detail/ios.hpp>  // failure, streamsize.
#include <boost/iostreams/filter/symmetric.hpp>                
#include <boost/iostreams/pipeline.hpp>                
#include <boost/type_traits/is_same.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include "jml/lz4mt/src/lz4mt.h"

// Must come last.
#ifdef BOOST_MSVC
# pragma warning(push)
# pragma warning(disable:4251 4231 4660)         // Dependencies not exported.
#endif
#include <boost/config/abi_prefix.hpp>           

namespace boost { namespace iostreams {

typedef Lz4MtStreamDescriptor lz4_params;


//
// Class name: lz4_error.
// Description: Subclass of std::ios::failure thrown to indicate
//     lz4 errors other than out-of-memory conditions.
//
class BOOST_IOSTREAMS_DECL lz4_error : public BOOST_IOSTREAMS_FAILURE {
public:
    explicit lz4_error(Lz4MtResult error);
    Lz4MtResult error() const { return error_; }
    static void check BOOST_PREVENT_MACRO_SUBSTITUTION(Lz4MtResult error);
private:
    Lz4MtResult error_;
};

namespace detail {

class BOOST_IOSTREAMS_DECL lz4_base { 
public:
    typedef char char_type;
protected:
    lz4_base(bool compress, const lz4_params & params = lz4_params());

    ~lz4_base();

    void before( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end );

    void after( const char*& src_begin, char*& dest_begin );

    int process(const char * & src_begin,
                const char * & src_end,
                char * & dest_begin,
                char * & dest_end,
                int flushLevel);

public:
    int total_in();
    int total_out();
private:
    bool compress_;
    Lz4MtContext context_;
    Lz4MtStreamDescriptor stream_;
};

//
// Template name: lz4_compressor_impl
// Description: Model of C-Style Filter implementing compression by
//      delegating to the lz4 function run.
//
template<typename Alloc = std::allocator<char> >
class lz4_compressor_impl : public lz4_base { 
public: 
    lz4_compressor_impl(const lz4_params& = lz4_params());
    ~lz4_compressor_impl();
    bool filter( const char*& src_begin, const char* src_end,
                 char*& dest_begin, char* dest_end, bool flush );
    void close();
};

//
// Template name: lz4_compressor
// Description: Model of C-Style Filte implementing decompression by
//      delegating to the lz4 function inflate.
//
template<typename Alloc = std::allocator<char> >
class lz4_decompressor_impl : public lz4_base {
public:
    lz4_decompressor_impl();
    ~lz4_decompressor_impl();
    bool filter( const char*& begin_in, const char* end_in,
                 char*& begin_out, char* end_out, bool flush );
    void close();
    bool eof() const
    {
        return eof_;
    }
private:
    bool eof_;
};

} // End namespace detail.

//
// Template name: lz4_compressor
// Description: Model of InputFilter and OutputFilter implementing
//      compression using lz4.
//
template<typename Alloc = std::allocator<char> >
struct basic_lz4_compressor 
    : symmetric_filter<detail::lz4_compressor_impl<Alloc>, Alloc> 
{
private:
    typedef detail::lz4_compressor_impl<Alloc>         impl_type;
    typedef symmetric_filter<impl_type, Alloc>  base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
    basic_lz4_compressor( const lz4_params& = lz4_params(),
                           int buffer_size = default_device_buffer_size);
    int total_in() {  return this->filter().total_in(); }
};
BOOST_IOSTREAMS_PIPABLE(basic_lz4_compressor, 1)

typedef basic_lz4_compressor<> lz4_compressor;

//
// Template name: lz4_decompressor
// Description: Model of InputFilter and OutputFilter implementing
//      decompression using lz4.
//
template<typename Alloc = std::allocator<char> >
struct basic_lz4_decompressor 
    : symmetric_filter<detail::lz4_decompressor_impl<Alloc>, Alloc> 
{
private:
    typedef detail::lz4_decompressor_impl<Alloc>       impl_type;
    typedef symmetric_filter<impl_type, Alloc>  base_type;
public:
    typedef typename base_type::char_type               char_type;
    typedef typename base_type::category                category;
    basic_lz4_decompressor(int buffer_size = default_device_buffer_size);
    int total_out() {  return this->filter().total_out(); }
    bool eof() { return this->filter().eof(); }
};
BOOST_IOSTREAMS_PIPABLE(basic_lz4_decompressor, 1)

typedef basic_lz4_decompressor<> lz4_decompressor;

//----------------------------------------------------------------------------//

namespace detail {

//------------------Implementation of lz4_compressor_impl--------------------//

template<typename Alloc>
lz4_compressor_impl<Alloc>::lz4_compressor_impl(const lz4_params& p)
    : lz4_base(true, p)
{ }

template<typename Alloc>
lz4_compressor_impl<Alloc>::~lz4_compressor_impl()
{ }

template<typename Alloc>
bool lz4_compressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool flush )
{
    int result = process(src_begin, src_end, dest_begin, dest_end,
                         flush);
    return result != 0;
}

template<typename Alloc>
void lz4_compressor_impl<Alloc>::close()
{
}

//------------------Implementation of lz4_decompressor_impl------------------//

template<typename Alloc>
lz4_decompressor_impl<Alloc>::~lz4_decompressor_impl()
{
}

template<typename Alloc>
lz4_decompressor_impl<Alloc>::lz4_decompressor_impl()
    : lz4_base(false), eof_(false)
{ 
}

template<typename Alloc>
bool lz4_decompressor_impl<Alloc>::filter
    ( const char*& src_begin, const char* src_end,
      char*& dest_begin, char* dest_end, bool flush )
{
    int result = process(src_begin, src_end, dest_begin, dest_end,
                         flush);
    return !(eof_ = result == 0);
}

template<typename Alloc>
void lz4_decompressor_impl<Alloc>::close() {
    // no real way to close or reopen this...
    //eof_ = false;
    //reset(false, true);
}

} // End namespace detail.

//------------------Implementation of lz4_decompressor-----------------------//

template<typename Alloc>
basic_lz4_compressor<Alloc>::basic_lz4_compressor
(const lz4_params& p, int buffer_size ) 
    : base_type(buffer_size, p) { }

//------------------Implementation of lz4_decompressor-----------------------//

template<typename Alloc>
basic_lz4_decompressor<Alloc>::basic_lz4_decompressor(int buffer_size)
    : base_type(buffer_size) { }

//----------------------------------------------------------------------------//

} } // End namespaces iostreams, boost.

#include <boost/config/abi_suffix.hpp> // Pops abi_suffix.hpp pragmas.
#ifdef BOOST_MSVC
# pragma warning(pop)
#endif

#endif
