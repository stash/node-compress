#ifndef NODE_COMPRESS_UTILS_H__
#define NODE_COMPRESS_UTILS_H__

#define COND_RETURN(cond, ret) \
    if (cond) \
      return (ret);

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

class State {
 public:
  enum Value {
    Idle,
    Data,
    Eos,
    Error
  };

  typedef StateTransition<Value> Transition;
};

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

