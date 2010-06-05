#ifndef NODE_COMPRESS_UTILS_H__
#define NODE_COMPRESS_UTILS_H__

#include <new>

#include <assert.h>

#define COND_RETURN(cond, ret) \
    if (cond) \
      return (ret);

template <class T>
class ScopedOutputBuffer {
 public:
  ScopedOutputBuffer() 
    : data_(0), capacity_(0), length_(0)
  {}

  ScopedOutputBuffer(size_t initialCapacity)
    : data_(0), capacity_(0), length_(0)
  {
    GrowBy(initialCapacity);
  }

  ~ScopedOutputBuffer() {
    Free();
  }

  bool GrowBy(size_t sz) {
    assert(sz >= 0);
    if (sz == 0) {
      return true;
    }
    return GrowTo(capacity_ + sz);
  }


  void IncreaseLengthBy(size_t sz) {
    assert(sz >= 0);
    assert(length_ + sz <= capacity_);
    length_ += sz;
  }

  
  void ResetLength() {
    length_ = 0;
  }


  void Free() {
    free(data_);
    data_ = 0;
    capacity_ = 0;
  }


  T* data() const {
    return data_;
  }


  size_t capacity() const {
    return capacity_;
  }


  size_t length() const {
    return length_;
  }


  size_t avail() const {
    return capacity_ - length_;
  }
 
 private:
  bool GrowTo(size_t sz) {
    if (sz == 0) {
      return true;
    }

    T *tmp = (T*) realloc(data_, sz * sizeof(T));
    if (tmp == NULL) {
      return false;
    }
    data_ = tmp;
    capacity_ = sz;
    return true;
  }

 private:
  T* data_;
  size_t capacity_;
  size_t length_;

 private:
  ScopedOutputBuffer(ScopedOutputBuffer&);
  ScopedOutputBuffer(const ScopedOutputBuffer&);
  ScopedOutputBuffer& operator=(ScopedOutputBuffer&);
  ScopedOutputBuffer& operator=(const ScopedOutputBuffer&);
};

typedef ScopedOutputBuffer<char> ScopedBlob;


template <class E>
class Queue {
 public:
  Queue()
    : initial_(0), length_(0), capacity_(0), data_(0)
  {}

  bool Push(E value) {
    if (EnsureCapacity()) {
      size_t index = (initial_ + length_) % capacity_;
      data_[index] = value;
      ++length_;
      return true;
    }
    return false;
  }

  E Pop() {
    if (length_ == 0) {
      return E();
    }
    E result = data_[initial_];
    if (++initial_ == capacity_) {
      initial_ = 0;
    }
    --length_;
    return result;
  }

  ~Queue() {
    delete []data_;
  }

  size_t length() const {
    return length_;
  }

 private:
  bool EnsureCapacity() {
    if (length_ < capacity_) {
      return true;
    }

    size_t new_capacity = capacity_ + (capacity_ >> 1) + 10;
    E *data = new(std::nothrow) E[new_capacity];
    if (data == 0) {
      return false;
    }
    for (size_t i = 0; i < length_; ++i) {
      data[i] = data_[(initial_ + i) % capacity_];
    }

    delete[] data_;

    data_ = data;
    initial_ = 0;
    capacity_ = new_capacity;
    return true;
  }

 private:
  size_t initial_;
  size_t length_;
  size_t capacity_;
  E *data_;
};


template <class T>
class StateTransition {
 public:
  StateTransition(T &ref, T value)
    : reference_(ref), value_(value), abort_(false)
  {}

  ~StateTransition() {
    if (!abort_) {
      reference_ = value_;
    }
  }

  void alter(T value) {
    value_ = value;
  }

  void abort(bool value = true) {
    abort_ = value;
  }

 private:
  T &reference_;
  T value_;
  bool abort_;

 private:
  StateTransition(StateTransition&);
  StateTransition(const StateTransition&);
  StateTransition& operator=(StateTransition&);
  StateTransition& operator=(const StateTransition&);
};


#endif

