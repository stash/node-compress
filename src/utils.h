#ifndef NODE_COMPRESS_UTILS_H__
#define NODE_COMPRESS_UTILS_H__

#define COND_RETURN(cond, ret) \
    if (cond) \
      return (ret);

template <class T>
class ScopedOutputBuffer {
 public:
  ScopedOutputBuffer() 
    : data_(0), length_(0)
  {}

  ScopedOutputBuffer(size_t initial)
    : data_(0), length_(0)
  {
    GrowTo(initial);
  }

  ~ScopedOutputBuffer() {
    Free();
  }

  bool GrowBy(size_t sz) {
    return GrowTo(length_ + sz);
  }


  bool GrowTo(size_t sz) {
    if (sz == 0) {
      return true;
    }

    T *tmp = (T*) realloc(data_, sz * sizeof(T));
    if (tmp == NULL) {
      return false;
    }
    data_ = tmp;
    length_ = sz;
    return true;
  }

  void Free() {
    free(data_);
    data_ = 0;
    length_ = 0;
  }

  T* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }
 
 private:
  T* data_;
  size_t length_;

 private:
  ScopedOutputBuffer(ScopedOutputBuffer&);
  ScopedOutputBuffer(const ScopedOutputBuffer&);
  ScopedOutputBuffer& operator=(ScopedOutputBuffer&);
  ScopedOutputBuffer& operator=(const ScopedOutputBuffer&);
};

typedef ScopedOutputBuffer<char> ScopedBlob;
#endif

