#ifndef NODE_COMPRESS_UTILS_H__
#define NODE_COMPRESS_UTILS_H__

template <class T>
class ScopedArray {
 public:
  ScopedArray(size_t size)
    : data_(new T[size])
  {}

  ~ScopedArray() {
    delete []data_;
  }

  T *data() const {
    return data_;
  }

 private:
  T *data_;

 private:
  ScopedArray(ScopedArray&);
  ScopedArray(const ScopedArray&);
  ScopedArray& operator=(ScopedArray&);
  ScopedArray& operator=(const ScopedArray&);
};


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

