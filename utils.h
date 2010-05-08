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

#endif

