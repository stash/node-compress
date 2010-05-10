#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "utils.h"

#define CHUNK 16384

using namespace v8;
using namespace node;

typedef ScopedOutputBuffer<Bytef> ScopedBytesBlob;

class GzipLib {
 public:
  static Handle<Value> ReturnThisOrThrow(const Local<Object> &self,
                                         int gzipStatus) {
    if (!IsError(gzipStatus)) {
      return self;
    } else {
      return ThrowError(gzipStatus);
    }
  }


  static Handle<Value> ReturnOrThrow(HandleScope &scope,
                                     const Local<Value> &value,
                                     int gzipStatus) {
    if (!IsError(gzipStatus)) {
      return scope.Close(value);
    } else {
      return ThrowError(gzipStatus);
    }
  }


  static bool IsError(int gzipStatus) {
    return !(gzipStatus == Z_OK || gzipStatus == Z_STREAM_END);
  }


  static Handle<Value> ThrowError(int gzipStatus) {
    assert(IsError(gzipStatus));

    return ThrowException(GetException(gzipStatus));
  }

  static Local<Value> GetException(int gzipStatus) {
    if (!IsError(gzipStatus)) {
      return Local<Value>::New(Undefined());
    } else {
      switch (gzipStatus) {
        case Z_NEED_DICT: 
          return Exception::Error(String::New(NeedDictionary));
        case Z_ERRNO: 
          return Exception::Error(String::New(Errno));
        case Z_STREAM_ERROR: 
          return Exception::Error(String::New(StreamError));
        case Z_DATA_ERROR: 
          return Exception::Error(String::New(DataError));
        case Z_MEM_ERROR: 
          return Exception::Error(String::New(MemError));
        case Z_BUF_ERROR: 
          return Exception::Error(String::New(BufError));
        case Z_VERSION_ERROR: 
          return Exception::Error(String::New(VersionError));

        default:
          return Exception::Error(String::New("Unknown error"));
      }
    }
  }

  
  static Handle<Value> ThrowCallbackExpected() {
    Local<Value> exception = Exception::TypeError(
        String::New("Callback must be a function"));
    return ThrowException(exception);
  }


  template <class Processor>
  static Handle<Value> Write(const Arguments& args) {
    Processor *proc = ObjectWrap::Unwrap<Processor>(args.This());

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
        return GzipLib::ThrowCallbackExpected();
      }
    }

    typename Processor::Blob out;
    int out_size;
    int r = proc->Write(buffer->data(), buffer->length(), out, &out_size);

    DoCallback(cb, r, out, out_size);
    return Undefined();
  }


  template <class Processor>
  static Handle<Value> Close(const Arguments& args) {
    Processor *proc = ObjectWrap::Unwrap<Processor>(args.This());

    HandleScope scope;

    Local<Value> cb(args[0]);
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsFunction()) {
        return GzipLib::ThrowCallbackExpected();
      }
    }  

    typename Processor::Blob out;
    int out_size;

    int r = proc->Close(out, &out_size);
    DoCallback(cb, r, out, out_size);

    return Undefined();
  }

  template <class Processor>
  static Handle<Value> Destroy(const Arguments& args) {
    Processor *proc = ObjectWrap::Unwrap<Processor>(args.This());
    proc->Destroy();

    return Undefined();
  }

  static void DoCallback(Local<Value> &cb,
      int r, ScopedBytesBlob &out, int out_size) {
    if (cb->IsFunction()) {
      Local<Value> argv[2];
      argv[0] = GzipLib::GetException(r);
      argv[1] = Encode(out.data(), out_size, BINARY);

      TryCatch try_catch;

      Function *fun = Function::Cast(*cb);
      fun->Call(Context::GetCurrent()->Global(), 2, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
  }

 private:
  static const char NeedDictionary[];
  static const char Errno[];
  static const char StreamError[];
  static const char DataError[];
  static const char MemError[];
  static const char BufError[];
  static const char VersionError[];
};

const char GzipLib::NeedDictionary[] = "Dictionary must be specified. "
  "Currently this is unsupported by library.";
const char GzipLib::Errno[] = "Z_ERRNO: Input/output error.";
const char GzipLib::StreamError[] = "Z_STREAM_ERROR: Invalid arguments or "
  "stream state is inconsistent.";
const char GzipLib::DataError[] = "Z_DATA_ERROR: Input data corrupted.";
const char GzipLib::MemError[] = "Z_MEM_ERROR: Out of memory.";
const char GzipLib::BufError[] = "Z_BUF_ERROR: Buffer error.";
const char GzipLib::VersionError[] = "Z_VERSION_ERROR: "
  "Invalid library version.";


class Gzip : public EventEmitter {
  friend class GzipLib;
  typedef ScopedBytesBlob Blob;

 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "write", GzipWrite);
    NODE_SET_PROTOTYPE_METHOD(t, "close", GzipClose);
    NODE_SET_PROTOTYPE_METHOD(t, "destroy", GzipDestroy);

    target->Set(String::NewSymbol("Gzip"), t->GetFunction());
  }

 private:
  int GzipInit(int level) {
    COND_RETURN(state_ != State::Idle, Z_STREAM_ERROR);

    /* allocate deflate state */
    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;

    int ret = deflateInit2(&stream_, level,
                           Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
      state_ = State::Data;
    }
    return ret;
  }


  int Write(char *data, int data_len,
                  ScopedBytesBlob &out, int* out_len) {
    *out_len = 0;
    COND_RETURN(state_ != State::Data, Z_STREAM_ERROR);

    State::Transition t(state_, State::Error);

    int ret = Z_OK;
    while (data_len > 0) { 
      if (data_len > CHUNK) {
        stream_.avail_in = CHUNK;
      } else {
        stream_.avail_in = data_len;
      }

      stream_.next_in = (Bytef*)data;
      do {
        COND_RETURN(!out.GrowBy(CHUNK), Z_MEM_ERROR);

        stream_.avail_out = CHUNK;
        stream_.next_out = out.data() + *out_len;

        ret = deflate(&stream_, Z_NO_FLUSH);
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        COND_RETURN(ret != Z_OK, ret);

        *out_len += (CHUNK - stream_.avail_out);
      } while (stream_.avail_out == 0);

      data += CHUNK;
      data_len -= CHUNK;
    }

    t.alter(State::Data);
    return ret;
  }


  int Close(ScopedBytesBlob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ == State::Idle, Z_OK);
    assert(state_ == State::Data || state_ == State::Error);

    State::Transition t(state_, State::Error);
    int ret = Z_OK;
    if (state_ == State::Data) {
      ret = GzipEndWithData(out, out_len);
    }

    t.abort();
    this->Destroy();
    return ret;
  }


  void Destroy() {
    State::Transition t(state_, State::Idle);
    if (state_ != State::Idle) {
      deflateEnd(&stream_);
    }
  }


  int GzipEndWithData(ScopedBytesBlob &out, int *out_len) {
    int ret;

    stream_.avail_in = 0;
    stream_.next_in = NULL;
    do {
      COND_RETURN(!out.GrowBy(CHUNK), Z_MEM_ERROR);

      stream_.avail_out = CHUNK;
      stream_.next_out = out.data() + *out_len;

      ret = deflate(&stream_, Z_FINISH);
      assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
      COND_RETURN(ret != Z_OK && ret != Z_STREAM_END, ret);

      *out_len += (CHUNK - stream_.avail_out);
    } while (ret != Z_STREAM_END);
    return ret;
  }


 protected:

  static Handle<Value>
  New (const Arguments& args)
  {
    HandleScope scope;

    int level = Z_DEFAULT_COMPRESSION;
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsInt32()) {
        Local<Value> exception = Exception::TypeError(
            String::New("level must be an integer"));
        return ThrowException(exception);
      }
      level = args[0]->Int32Value();
    }

    Gzip *gzip = new Gzip();
    gzip->Wrap(args.This());

    int r = gzip->GzipInit(level);
    return GzipLib::ReturnThisOrThrow(args.This(), r);
  }

  static Handle<Value>
  GzipWrite(const Arguments& args) {
    return GzipLib::Write<Gzip>(args);
  }

  static Handle<Value>
  GzipClose(const Arguments& args) {
    return GzipLib::Close<Gzip>(args);
  }
  

  static Handle<Value>
  GzipDestroy(const Arguments& args) {
    return GzipLib::Destroy<Gzip>(args);
  }


  Gzip() 
    : EventEmitter(), state_(State::Idle)
  {}

  ~Gzip()
  {
    if (state_ != State::Idle) {
      // Release zlib structures.
      deflateEnd(&stream_);
    }
  }

 private:
  z_stream stream_;
  State::Value state_;

};


class Gunzip : public EventEmitter {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", GunzipInit);
    NODE_SET_PROTOTYPE_METHOD(t, "inflate", GunzipInflate);
    NODE_SET_PROTOTYPE_METHOD(t, "end", GunzipEnd);

    target->Set(String::NewSymbol("Gunzip"), t->GetFunction());
  }

 private:
  int GunzipInit() {
    COND_RETURN(state_ != State::Idle, Z_STREAM_ERROR);

    /* allocate inflate state */
    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;
    stream_.avail_in = 0;
    stream_.next_in = Z_NULL;

    int ret = inflateInit2(&stream_, 16 + MAX_WBITS);
    if (ret == Z_OK) {
      state_ = State::Data;
    }
    return ret;
  }


  int GunzipInflate(const char* data, int data_len,
                    ScopedBytesBlob &out, int* out_len) {
    *out_len = 0;
    COND_RETURN(state_ == State::Eos, Z_OK);
    COND_RETURN(state_ != State::Data, Z_STREAM_ERROR);

    State::Transition t(state_, State::Error);

    int ret = Z_OK;
    while (data_len > 0) { 
      if (data_len > CHUNK) {
        stream_.avail_in = CHUNK;
      } else {
        stream_.avail_in = data_len;
      }

      stream_.next_in = (Bytef*)data;
      do {
        COND_RETURN(!out.GrowBy(CHUNK), Z_MEM_ERROR);
        
        stream_.avail_out = CHUNK;
        stream_.next_out = out.data() + *out_len;

        ret = inflate(&stream_, Z_NO_FLUSH);
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */

        switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;     /* and fall through */
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          t.abort();
          GunzipEnd();
          return ret;
        }
        COND_RETURN(ret != Z_OK && ret != Z_STREAM_END, ret);
        
        *out_len += (CHUNK - stream_.avail_out);

        if (ret == Z_STREAM_END) {
          t.alter(State::Eos);
          return ret;
        }
      } while (stream_.avail_out == 0);
      data += CHUNK;
      data_len -= CHUNK;
    }
    t.alter(State::Data);
    return ret;
  }


  void GunzipEnd() {
    if (state_ != State::Idle) {
      state_ = State::Idle;
      inflateEnd(&stream_);
    }
  }

 protected:

  static Handle<Value>
  New(const Arguments& args) {
    HandleScope scope;

    Gunzip *gunzip = new Gunzip();
    gunzip->Wrap(args.This());

    return args.This();
  }

  static Handle<Value>
  GunzipInit(const Arguments& args) {
    Gunzip *gunzip = ObjectWrap::Unwrap<Gunzip>(args.This());

    HandleScope scope;

    int r = gunzip->GunzipInit();

    return scope.Close(Integer::New(r));
  }


  static Handle<Value>
  GunzipInflate(const Arguments& args) {
    Gunzip *gunzip = ObjectWrap::Unwrap<Gunzip>(args.This());

    HandleScope scope;

    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    ScopedArray<char> buf(len);
    ssize_t written = DecodeWrite(buf.data(), len, args[0], BINARY);
    assert(written == len);

    ScopedBytesBlob out;
    int out_size;
    int r = gunzip->GunzipInflate(buf.data(), len, out, &out_size);

    Local<Value> outString = Encode(out.data(), out_size, enc);
    return scope.Close(outString);
  }

  static Handle<Value>
  GunzipEnd(const Arguments& args) {
    Gunzip *gunzip = ObjectWrap::Unwrap<Gunzip>(args.This());

    HandleScope scope;

    gunzip->GunzipEnd();

    return scope.Close(String::New(""));
  }

  Gunzip() 
    : EventEmitter(), state_(State::Idle)
  {}

  ~Gunzip()
  {
    this->GunzipEnd();
  }

 private:
  z_stream stream_;
  State::Value state_;
};

