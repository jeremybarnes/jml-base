/* circular_buffer.h                                               -*- C++ -*-
   Jeremy Barnes, 7 December 2009
   Copyright (c) 2009 Jeremy Barnes.  All rights reserved.

   Circular buffer structure, that will grow to hold whatever we put in it.
   O(1) insertion and deletion at the front and at the back.  Kind of like
   a deque, but much lighter weight.
*/

#ifndef __jml__utils__circular_buffer_h__
#define __jml__utils__circular_buffer_h__

#include <vector>
#include "arch/exception.h"
#include <boost/iterator/iterator_facade.hpp>
#include <iostream> // debug

namespace ML {

using namespace std; // debug

template<typename T, class CircularBuffer>
struct Circular_Buffer_Iterator
    : public boost::iterator_facade<Circular_Buffer_Iterator<T, CircularBuffer>,
                                    T,
                                    boost::random_access_traversal_tag> {
    CircularBuffer * buffer;
    int index;
    bool wrapped;
    
    typedef boost::iterator_facade<Circular_Buffer_Iterator<T, CircularBuffer>,
                                   T,
                                   boost::random_access_traversal_tag> Base;

public:
    typedef T & reference;
    typedef int difference_type;
    
    Circular_Buffer_Iterator()
        : buffer(0), index(0), wrapped(false)
    {
    }
    
    Circular_Buffer_Iterator(CircularBuffer * buffer, int idx, bool wrapped)
        : buffer(buffer), index(idx), wrapped(wrapped)
    {
    }
    
    template<typename T2, class CircularBuffer2>
    difference_type
    operator - (const Circular_Buffer_Iterator<T2, CircularBuffer2>
                    & other) const
    {
            return distance_to(other);
    }
    
    template<typename T2, class CircularBuffer2>
    bool operator == (const Circular_Buffer_Iterator<T2, CircularBuffer2>
                          & other) const
    {
        return equal(other);
    }
    
    template<typename T2, class CircularBuffer2>
    bool operator != (const Circular_Buffer_Iterator<T2, CircularBuffer2>
                          & other) const
    {
        return ! operator == (other);
    }
    
    using Base::operator -;
    
    std::string print() const
    {
        return format("Circular_Buffer_Iterator: buffer %p (start %d size %d capacity %d) index %d wrapped %d",
                      buffer,
                      (buffer ? buffer->start_ : 0),
                      (buffer ? buffer->size_ : 0),
                      (buffer ? buffer->capacity_ : 0),
                      index, wrapped);
    }
    
private:
    friend class boost::iterator_core_access;
    
    template<typename T2, class CircularBuffer2> friend class Iterator;
    
    template<typename T2, class CircularBuffer2>
    bool equal(const Circular_Buffer_Iterator<T2, CircularBuffer2>
                   & other) const
    {
        if (buffer != other.buffer)
            throw Exception("wrong buffer");
        return index == other.index && wrapped == other.wrapped;
    }
    
    T & dereference() const
    {
        if (!buffer)
            throw Exception("defererencing null iterator");
        return buffer->vals_[index] ;
        }
    
    void increment()
    {
        ++index;
        if (index == buffer->capacity_) {
            index = 0;
            wrapped = true;
        }
    }
    
    void decrement()
    {
        if (index == buffer->start_ && !wrapped)
            throw Exception("decrementing off the end");

        //cerr << "decrement: " << print() << endl;

        --index;
        if (index < 0) {
            wrapped = false;
            index += buffer->capacity_;
        }

        //cerr << "after decrement: " << print() << endl;
    }
    
    void advance(int nelements)
    {
        //cerr << "advance: before " << print() << endl;
        //cerr << "nelements = " << nelements << endl;

        index += nelements;
        if (index >= buffer->capacity_) {
            index -= buffer->capacity_;
            wrapped = true;
        }
        if (index < 0) {
            index += buffer->capacity_;
            wrapped = false;
        }

        //err << "advance: after " << print() << endl;
    }
    
    template<typename T2, class CircularBuffer2>
    int distance_to(const Circular_Buffer_Iterator<T2, CircularBuffer2>
                        & other) const
    {
        if (buffer != other.buffer)
            throw Exception("other buffer wrong");

        //cerr << "distance_to:" << endl;
        //cerr << " this:   " << print() << endl;
        //cerr << " other:  " << other.print() << endl;
        
        // What is the distance from the start to this element?
        int dstart1 = index - buffer->start_;
        if (wrapped) dstart1 += buffer->capacity_;

        int dstart2 = other.index - buffer->start_;
        if (other.wrapped) dstart2 += buffer->capacity_;

        //cerr << " dist1:  " << dstart1 << endl;
        //cerr << " dist2:  " << dstart2 << endl;
        //cerr << " result: " << dstart1 - dstart2 << endl;

        return dstart1 - dstart2;
    }
};


template<typename T, bool Safe = false>
struct Circular_Buffer {
    Circular_Buffer(int initial_capacity = 0)
        : vals_(0), start_(0), size_(0), capacity_(0)
    {
        if (initial_capacity != 0) reserve(initial_capacity);
    }

    void swap(Circular_Buffer & other)
    {
        std::swap(vals_, other.vals_);
        std::swap(start_, other.start_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    Circular_Buffer(const Circular_Buffer & other)
        : vals_(0), start_(0), size_(0), capacity_(0)
    {
        reserve(other.size());
        for (unsigned i = 0;  i < other.size();  ++i)
            push_back(other[i]);
    }

    Circular_Buffer & operator = (const Circular_Buffer & other)
    {
        Circular_Buffer new_me(other);
        swap(new_me);
        return *this;
    }

    ~Circular_Buffer()
    {
        destroy();
    }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    void reserve(int new_capacity)
    {
        //cerr << "reserve: capacity = " << capacity_ << " new_capacity = "
        //     << new_capacity << endl;

        if (new_capacity <= capacity_) return;
        new_capacity = std::max(capacity_ * 2, new_capacity);

        T * new_vals = new T[new_capacity];
        memset(new_vals, 0xaa, sizeof(T) * new_capacity);

        int nfirst_half = std::min(capacity_ - start_, size_);
        int nsecond_half = std::max(0, size_ - nfirst_half);

        //cerr << "start_ = " << start_ << " size_ = " << size_ << endl;
        //cerr << "nfirst_half = " << nfirst_half << endl;
        //cerr << "nsecond_half = " << nsecond_half << endl;

        std::copy(vals_ + start_, vals_ + start_ + nfirst_half,
                  new_vals);
        std::copy(vals_ + start_ - nsecond_half, vals_ + start_,
                  new_vals + nfirst_half);

        memset(vals_, 0xaa, sizeof(T) * capacity_);

        delete[] vals_;

        vals_ = new_vals;
        capacity_ = new_capacity;
    }

    void resize(size_t new_size, const T & el = T())
    {
        while (new_size < size_)
            pop_back();
        while (new_size > size_)
            push_back(el);
    }

    void destroy()
    {
        delete[] vals_;
        vals_ = 0;
        start_ = size_ = capacity_ = 0;
    }

    void clear(int start = 0)
    {
        size_ = 0;
        start_ = start;
        if (start < 0 || start > capacity_ || (start == capacity_ && capacity_))
            throw Exception("invalid start");
        memset(vals_, 0xaa, sizeof(T) * capacity_);
    }

    const T & operator [](int index) const
    {
        if (size_ == 0)
            throw Exception("Circular_Buffer: empty array");
        if (index < -size_ || index >= size_)
            throw Exception("Circular_Buffer: invalid size");
        return *element_at(index);
    }

    T & operator [](int index)
    {
        const Circular_Buffer * cthis = this;
        return const_cast<T &>(cthis->operator [] (index));
    }

    const T & at(int index) const
    {
        if (size_ == 0)
            throw Exception("Circular_Buffer: empty array");
        if (index < -size_ || index >= size_)
            throw Exception("Circular_Buffer: invalid size");
        return *element_at(index);
    }

    T & at(int index)
    {
        const Circular_Buffer * cthis = this;
        return const_cast<T &>(cthis->operator [] (index));
    }

    const T & front() const
    {
        if (empty())
            throw Exception("front() with empty circular array");
        return vals_[start_];
    }

    T & front()
    {
        if (empty())
            throw Exception("front() with empty circular array");
        return vals_[start_];
    }

    const T & back() const
    {
        if (empty())
            throw Exception("back() with empty circular array");
        return *element_at(size_ - 1);
    }

    T & back()
    {
        if (empty())
            throw Exception("back() with empty circular array");
        return *element_at(size_ - 1);
    }

    void push_back(const T & val)
    {
        if (size_ == capacity_) reserve(std::max(1, capacity_ * 2));
        ++size_;
        *element_at(size_ - 1) = val;
    }

    void push_front(const T & val)
    {
        if (size_ == capacity_) reserve(std::max(1, capacity_ * 2));
        *element_at(-1) = val;
        if (start_ == 0) start_ = capacity_ - 1;
        else --start_;
    }

    void pop_back()
    {
        if (empty())
            throw Exception("pop_back with empty circular array");
        memset(element_at(size_ - 1), 0xaa, sizeof(T));
        --size_;
    }

    void pop_front()
    {
        if (empty())
            throw Exception("pop_front with empty circular array");
        memset(vals_ + start_, 0xaa, sizeof(T));
        ++start_;
        --size_;

        // Point back to the start if empty
        if (start_ == capacity_) start_ = 0;
    }

    void erase_element(int el)
    {
        //cerr << "erase_element: el = " << el << " size = " << size()
        //     << endl;

        if (el >= size() || el < -(int)size())
            throw Exception("erase_element(): invalid value");
        if (el < 0) el += size_;

        if (capacity_ == 0)
            throw Exception("empty circular buffer");

        int offset = (start_ + el) % capacity_;

        //cerr << "offset = " << offset << " start_ = " << start_
        //     << " size_ = " << size_ << " capacity_ = " << capacity_
        //     << endl;

        // TODO: could be done more efficiently

        if (el == 0) {
            pop_front();
        }
        else if (el == size() - 1) {
            pop_back();
        }
        else if (offset < start_) {
            // slide everything 
            int num_to_do = size_ - el - 1;
            std::copy(vals_ + offset + 1, vals_ + offset + 1 + num_to_do,
                      vals_ + offset);
            --size_;
        }
        else {
            for (int i = offset;  i > 0;  --i)
                vals_[i] = vals_[i - 1];
            --size_;
            ++start_;
        }
    }

    typedef Circular_Buffer_Iterator<T, Circular_Buffer> iterator;
    typedef Circular_Buffer_Iterator<const T, const Circular_Buffer>
        const_iterator;

    iterator begin()
    {
        return iterator(this, start_, capacity_ == 0 /* wrapped */);
    }

    iterator end()
    {
        return begin() + size_;
    }
    
    const_iterator begin() const
    {
        return const_iterator(this, start_, capacity_ == 0 /* wrapped */);
    }

    const_iterator end() const
    {
        return begin() + size_;
    }
    
private:
    template<typename T2, class CB> friend class Circular_Buffer_Iterator;

    T * vals_;
    int start_;
    int size_;
    int capacity_;

    void validate() const
    {
        if (!vals_ && capacity_ != 0)
            throw Exception("null vals but capacity");
        if (start_ < 0)
            throw Exception("negative start");
        if (size_ < 0)
            throw Exception("negative size");
        if (capacity_ < 0)
            throw Exception("negaive capacity");
        if (size_ > capacity)
            throw Exception("capacity too high");
        if (start_ > size_ || (start_ == size_ && size_ != 0))
            throw Exception("start too far");
    }

    T * element_at(int index)
    {
        if (capacity_ == 0)
            throw Exception("empty circular buffer");

        //cerr << "element_at: index " << index << " start_ " << start_
        //     << " capacity_ " << capacity_ << " size_ " << size_;

        if (index < 0) index += size_;

        int offset = (start_ + index) % capacity_;

        //cerr << "  offset " << offset << endl;

        return vals_ + offset;
    }
    
    const T * element_at(int index) const
    {
        if (capacity_ == 0)
            throw Exception("empty circular buffer");

        //cerr << "element_at: index " << index << " start_ " << start_
        //     << " capacity_ " << capacity_ << " size_ " << size_;

        if (index < 0) index += size_;

        int offset = (start_ + index) % capacity_;

        //cerr << "  offset " << offset << endl;

        return vals_ + offset;
    }
};

template<typename T, bool S>
bool
operator == (const Circular_Buffer<T, S> & cb1,
             const Circular_Buffer<T, S> & cb2)
{
    return cb1.size() == cb2.size()
        && std::equal(cb1.begin(), cb1.end(), cb2.begin());
}

template<typename T, bool S>
bool
operator < (const Circular_Buffer<T, S> & cb1,
            const Circular_Buffer<T, S> & cb2)
{
    return std::lexicographical_compare(cb1.begin(), cb1.end(),
                                        cb2.begin(), cb2.end());
}

template<typename T, bool S>
std::ostream & operator << (std::ostream & stream,
                            const Circular_Buffer<T, S> & buf)
{
    stream << "[";
    for (unsigned i = 0;  i < buf.size();  ++i)
        stream << " " << buf[i];
    return stream << " ]";
}

template<typename T, class CB>
std::ostream &
operator << (std::ostream & stream,
             const Circular_Buffer_Iterator<T, CB> & it)
{
    return stream << it.print();
}


} // namespace ML


#endif /* __jml__utils__circular_buffer_h__ */