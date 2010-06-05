#ifndef NODE_COMPRESS_ZLIB_H__
#define NODE_COMPRESS_ZLIB_H__

#include <pthread.h>

#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>

#include "utils.h"

using namespace v8;
using namespace node;

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


template <class Processor>
class ZipLib : ObjectWrap {
 private:
  typedef typename Processor::Utils Utils;
  typedef typename Processor::Blob Blob;

  typedef ZipLib<Processor> Self;

 public:
  static void Initialize(v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "write", Write);
    NODE_SET_PROTOTYPE_METHOD(t, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(t, "destroy", Destroy);

    target->Set(String::NewSymbol(Processor::Name), t->GetFunction());
  }

 private:
  struct Request {
   public:
    enum Kind {
      RWrite,
      RClose,
      RDestroy
    };
   private:
    Request(Kind kind, ZipLib *self, Local<Value> callback)
      : kind_(kind), self_(self), callback_(callback)
    {}

    Request(ZipLib *self, Local<Value> inputBuffer, Local<Value> callback)
      : kind_(RWrite), self_(self),
      buffer_(inputBuffer), data_(GetBuffer(inputBuffer)->data()),
      length_(GetBuffer(inputBuffer)->length()), callback_(callback) 
    {}
    
    static Buffer *GetBuffer(Local<Value> &buffer) {
      return ObjectWrap::Unwrap<Buffer>(buffer->ToObject());
    }

   public:
    static Request* Write(Self *self, Local<Value> inputBuffer,
        Local<Value> callback) {
      return new Request(self, inputBuffer, callback);
    }

    static Request* Close(Self *self, Local<Value> callback) {
      return new Request(RClose, self, callback);
    }

    static Request* Destroy(Self *self) {
      return new Request(RDestroy, self, Local<Value>::New(Undefined()));
    }

   public:
    void setStatus(int status) {
      status_ = status;
    }
    
    char* buffer() const {
      return data_;
    }

    int length() const {
      return length_;
    }

    Self *self() const {
      return self_;
    }

    Blob &output() {
      return out_;
    }

    Kind kind() const {
      return kind_;
    }

    int status() const {
      return status_;
    }

    Persistent<Value> callback() const {
      return callback_;
    }

   private:
    Kind kind_;

    ZipLib *self_;

    // We store persistent Buffer object reference to avoid garbage collection,
    // but it's not thread-safe to reference it from non-JS script, so we also
    // store raw buffer data and length.
    Persistent<Value> buffer_;
    char *data_;
    int length_;

    Persistent<Value> callback_;

    // Output structures.
    Blob out_;
    int status_;
  };

 public:
  static Handle<Value> New(const Arguments &args) {
    Self *result = new Self();
    result->Wrap(args.This());

    Transition t(result->state_, Self::Error);

    Handle<Value> exception = result->processor_.Init(args);
    if (!exception->IsUndefined()) {
      return exception;
    }

    t.alter(Self::Data);
    return args.This();
  }


  static Handle<Value> Write(const Arguments& args) {
    Self *proc = ObjectWrap::Unwrap<Self>(args.This());

    HandleScope scope;

    if (!Buffer::HasInstance(args[0])) {
      Local<Value> exception = Exception::TypeError(
          String::New("Input must be of type Buffer"));
      return ThrowException(exception);
    }

    Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

    Local<Value> cb(args[1]);
    if (args.Length() > 1 && !args[1]->IsUndefined()) {
      if (!args[1]->IsFunction()) {
        return ThrowCallbackExpected();
      }
    }

    Request *request = Request::Write(proc, args[0], cb);
    
    eio_custom(Self::DoPushRequest, EIO_PRI_DEFAULT,
        Self::DoHandleCallbacks, request);

    ev_ref(EV_DEFAULT_UC);
    proc->Ref();

    return Undefined();
  }


  static Handle<Value> Close(const Arguments& args) {
    Self *proc = ObjectWrap::Unwrap<Self>(args.This());

    HandleScope scope;

    Local<Value> cb(args[0]);
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsFunction()) {
        return ThrowCallbackExpected();
      }
    }  

    Request *request = Request::Close(proc, cb);
    eio_custom(Self::DoPushRequest, EIO_PRI_DEFAULT,
        Self::DoHandleCallbacks, request);

    ev_ref(EV_DEFAULT_UC);
    proc->Ref();

    return Undefined();
  }


  static Handle<Value> Destroy(const Arguments& args) {
    Self *proc = ObjectWrap::Unwrap<Self>(args.This());

    Request *request = Request::Destroy(proc);
    eio_custom(Self::DoPushRequest, EIO_PRI_DEFAULT,
        Self::DoHandleCallbacks, request);

    ev_ref(EV_DEFAULT_UC);
    proc->Ref();

    return Undefined();
  }


 private:
  static int DoPushRequest(eio_req *req) {
    Request *request = static_cast<Request*>(req->data);

    Self *self = request->self();
    pthread_mutex_lock(&self->requestsMutex_);
    self->requestsQueue_.Push(request);
    bool startProcessing = !self->processorActive_;
    if (!startProcessing) self->processorActive_ = true;
    pthread_mutex_unlock(&self->requestsMutex_);

    if (startProcessing) {
      self->DoProcess();
    }
    return 0;
  }

  static int DoHandleCallbacks(eio_req *req) {
    Request *request;

    HandleScope scope;

    while (ReentrantPop(callbackQueue_, callbackMutex_, request)) {
      Self *self = request->self();
      Local<Value> cb(*request->callback());
      self->DoCallback(cb, request->status(), request->output());

      ev_unref(EV_DEFAULT_UC);
      self->Unref();
    }
    return 0;
  }

  static void DoHandleCallbacks2(EV_P_ ev_async *evt, int revents) {
    DoHandleCallbacks(0);
  }

  void DoProcess() {
    Request *request;

    while (ReentrantPop(requestsQueue_, requestsMutex_, request)) {
      switch (request->kind()) {
        case Request::RWrite:
          request->setStatus(
              this->Write(request->buffer(), request->length(),
                request->output()));
          break;

        case Request::RClose:
          request->setStatus(this->Close(request->output()));
          break;

        case Request::RDestroy:
          this->Destroy();
          request->setStatus(Utils::StatusOk());
          break;
      }
    }

    pthread_mutex_lock(&requestsMutex_);
    processorActive_ = false;
    pthread_mutex_unlock(&requestsMutex_);

    ev_async_send(&callbackNotify_);
  }

  static bool ReentrantPop(Queue<Request*> &queue, pthread_mutex_t &mutex,
      Request*& request) {
    request = 0;

    pthread_mutex_lock(&mutex);
    bool result = queue.length() != 0;
    if (result) {
      request = queue.Pop();
    }
    pthread_mutex_unlock(&mutex);

    return result;
  }

 private:

  ZipLib()
    : ObjectWrap(), state_(Self::Idle), processorActive_(false)
  {
    pthread_mutex_init(&requestsMutex_, 0);

    // Lazy init. Safe to do it here as this always happen in JS-thread.
    if (!callbackInitialized_) {
      pthread_mutex_init(&callbackMutex_, 0);
      ev_async_init(&callbackNotify_, Self::DoHandleCallbacks2);
      ev_async_start(&callbackNotify_);
      ev_unref();
    }
  }


  ~ZipLib() {
    this->Destroy();
  }


  int Write(char *data, int dataLength, Blob &out) {
    COND_RETURN(state_ != Self::Data, Utils::StatusSequenceError());

    Transition t(state_, Self::Error);

    data += dataLength;
    int ret = Utils::StatusOk();
    while (dataLength > 0) { 
      COND_RETURN(!out.GrowBy(dataLength + 1), Utils::StatusMemoryError());
      
      ret = this->processor_.Write(data - dataLength, dataLength, out);
      COND_RETURN(Utils::IsError(ret), ret);
      if (ret == Utils::StatusEndOfStream()) {
        t.alter(Self::Eos);
        return ret;
      }
    }
    t.abort();
    return Utils::StatusOk();
  }


  int Close(Blob &out) {
    COND_RETURN(state_ == Self::Idle || state_ == Self::Destroyed,
        Utils::StatusOk());

    Transition t(state_, Self::Error);

    int ret = Utils::StatusOk();
    if (state_ == Self::Data) {
      ret = Finish(out);
    }

    t.abort();
    this->Destroy();
    return ret;
  }


  void Destroy() {
    if (state_ != Self::Idle && state_ != Self::Destroyed) {
      this->processor_.Destroy();
    }
    state_ = Self::Destroyed;
  }


  int Finish(Blob &out) {
    const int Chunk = 128;

    int ret;
    do {
      COND_RETURN(!out.GrowBy(Chunk), Utils::StatusMemoryError());
      
      ret = this->processor_.Finish(out);
      COND_RETURN(Utils::IsError(ret), ret);
    } while (ret != Utils::StatusEndOfStream());
    return Utils::StatusOk();
  }


 public:
  static Handle<Value> ReturnThisOrThrow(const Arguments &args,
                                         int zipStatus) {
    if (!Utils::IsError(zipStatus)) {
      return args.This();
    } else {
      return ThrowError(zipStatus);
    }
  }


  static Handle<Value> ReturnOrThrow(HandleScope &scope,
                                     const Local<Value> &value,
                                     int zipStatus) {
    if (!Utils::IsError(zipStatus)) {
      return scope.Close(value);
    } else {
      return ThrowError(zipStatus);
    }
  }


  static Handle<Value> ThrowError(int zipStatus) {
    assert(Utils::IsError(zipStatus));

    return ThrowException(Utils::GetException(zipStatus));
  }
 
  static Handle<Value> ThrowCallbackExpected() {
    Local<Value> exception = Exception::TypeError(
        String::New("Callback must be a function"));
    return ThrowException(exception);
  }


  static void DoCallback(Local<Value> &cb, int r, Blob &out) {
    if (cb->IsFunction()) {
      Local<Value> argv[2];
      argv[0] = Utils::GetException(r);
      argv[1] = Encode(out.data(), out.length(), BINARY);

      TryCatch try_catch;

      Function *fun = Function::Cast(*cb);
      fun->Call(Context::GetCurrent()->Global(), 2, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
  }

  enum State {
    Idle,
    Destroyed,
    Data,
    Eos,
    Error
  };

  typedef StateTransition<State> Transition;

 private:
  Processor processor_;
  State state_;

  pthread_mutex_t requestsMutex_;
  Queue<Request*> requestsQueue_;

  static bool callbackInitialized_;
  static pthread_mutex_t callbackMutex_;
  static Queue<Request*> callbackQueue_;
  static ev_async callbackNotify_;

  volatile bool processorActive_;
};

template <class T> bool ZipLib<T>::callbackInitialized_ = false;

#endif

