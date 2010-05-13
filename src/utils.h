#ifndef NODE_COMPRESS_UTILS_H__
#define NODE_COMPRESS_UTILS_H__

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
#endif

