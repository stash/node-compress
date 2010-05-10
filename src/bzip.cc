#include <node.h>
#include <node_events.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define BZ_NO_STDIO
#include <bzlib.h>
#undef BZ_NO_STDIO

#include "utils.h"

using namespace v8;
using namespace node;


class Bzip : public EventEmitter {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", BzipInit);
    NODE_SET_PROTOTYPE_METHOD(t, "deflate", BzipDeflate);
    NODE_SET_PROTOTYPE_METHOD(t, "end", BzipEnd);

    target->Set(String::NewSymbol("Bzip"), t->GetFunction());
  }

  int BzipInit(int blockSize100k, int workFactor) {
    COND_RETURN(state_ != State::Idle, BZ_SEQUENCE_ERROR);

    /* allocate deflate state */
    stream_.bzalloc = NULL;
    stream_.bzfree = NULL;
    stream_.opaque = NULL;

    int ret = BZ2_bzCompressInit(&stream_, blockSize100k, 0, workFactor);
    if (ret == BZ_OK) {
      state_ = State::Data;
    }
    return ret;
  }


  int BzipDeflate(char *data, int data_len, ScopedBlob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ != State::Data, BZ_SEQUENCE_ERROR);

    State::Transition t(state_, State::Error);

    int ret = BZ_OK;
    while (data_len > 0) {    
      COND_RETURN(!out.GrowBy(data_len + 1), BZ_MEM_ERROR);
      
      stream_.next_in = data;
      stream_.next_out = out.data() + *out_len;
      stream_.avail_in = data_len;
      stream_.avail_out = data_len + 1;

      ret = BZ2_bzCompress(&stream_, BZ_RUN);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      COND_RETURN(ret != BZ_RUN_OK, ret);

      *out_len += (data_len + 1 - stream_.avail_out);
      data += data_len - stream_.avail_in;
      data_len = stream_.avail_in;
    }
    t.alter(State::Data);
    return BZ_OK;
  }


  int BzipEnd(ScopedBlob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ == State::Idle, BZ_OK);
    assert(state_ == State::Data || state_ == State::Error);

    State::Transition t(state_, State::Idle);

    int ret = BZ_OK;
    if (state_ == State::Data) {
      ret = BzipEndWithData(out, out_len);
    }

    BZ2_bzCompressEnd(&stream_);
    return ret;
  }

  int BzipEndWithData(ScopedBlob &out, int *out_len) {
    // Don't expect data to be large as output buffer for deflate is as large
    // as input.
    const int Chunk = 128;

    int ret;
    do {
      COND_RETURN(!out.GrowBy(Chunk), BZ_MEM_ERROR);
      
      stream_.avail_out = Chunk;
      stream_.next_out = out.data() + *out_len;

      ret = BZ2_bzCompress(&stream_, BZ_FINISH);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      COND_RETURN(ret != BZ_FINISH_OK && ret != BZ_STREAM_END, ret);

      *out_len += (Chunk - stream_.avail_out);
    } while (ret != BZ_STREAM_END);
    return BZ_OK;
  }


 protected:

  static Handle<Value>
  New (const Arguments& args)
  {
    HandleScope scope;

    Bzip *bzip = new Bzip();
    bzip->Wrap(args.This());

    return args.This();
  }

  static Handle<Value>
  BzipInit (const Arguments& args)
  {
    Bzip *bzip = ObjectWrap::Unwrap<Bzip>(args.This());

    HandleScope scope;

    int blockSize100k = 1;
    int workFactor = 0;

    int length = args.Length();
    if (length >= 1 && !args[0]->IsUndefined()) {
      if (!args[0]->IsInt32()) {
        Local<Value> exception = Exception::TypeError(
            String::New("blockSize must be an integer"));
        return ThrowException(exception);
      }
      blockSize100k = args[0]->Int32Value();
    }
    if (length >= 2 && !args[1]->IsUndefined()) {
      if (!args[1]->IsInt32()) {
        Local<Value> exception = Exception::TypeError(
            String::New("workFactor must be an integer"));
        return ThrowException(exception);
      }
      workFactor = args[1]->Int32Value();
    }
    int r = bzip->BzipInit(blockSize100k, workFactor);
    return scope.Close(Integer::New(r));
  }

  static Handle<Value>
  BzipDeflate(const Arguments& args) {
    Bzip *bzip = ObjectWrap::Unwrap<Bzip>(args.This());

    HandleScope scope;

    enum encoding enc = ParseEncoding(args[1]);
    ssize_t len = DecodeBytes(args[0], enc);

    if (len < 0) {
      Local<Value> exception = Exception::TypeError(
          String::New("Bad argument"));
      return ThrowException(exception);
    }
    ScopedArray<char> buf(len);
    ssize_t written = DecodeWrite(buf.data(), len, args[0], enc);
    assert(written == len);

    ScopedBlob out;
    int out_size;
    int r = bzip->BzipDeflate(buf.data(), len, out, &out_size);

    if (out_size==0) {
      return scope.Close(String::New(""));
    }

    Local<Value> outString = Encode(out.data(), out_size, BINARY);
    return scope.Close(outString);
  }

  static Handle<Value>
  BzipEnd(const Arguments& args) {
    Bzip *bzip = ObjectWrap::Unwrap<Bzip>(args.This());

    HandleScope scope;

    ScopedBlob out;
    int out_size;
    if (args.Length() > 0 && args[0]->IsString()) {
      String::Utf8Value format_type(args[1]->ToString());
    }  

    int r = bzip->BzipEnd(out, &out_size);
    if (out_size==0) {
      return String::New("");
    }
    Local<Value> outString = Encode(out.data(), out_size, BINARY);
    return scope.Close(outString);
  }


  Bzip()
    : EventEmitter(), state_(State::Idle)
  {
  }

  ~Bzip()
  {
    if (state_ != State::Idle) {
      BZ2_bzCompressEnd(&stream_);
    }
  }

 private:
  bz_stream stream_;
  State::Value state_;

};


class Bunzip : public EventEmitter {
 public:
  static void
  Initialize (v8::Handle<v8::Object> target)
  {
    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);

    t->Inherit(EventEmitter::constructor_template);
    t->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(t, "init", BunzipInit);
    NODE_SET_PROTOTYPE_METHOD(t, "inflate", BunzipInflate);
    NODE_SET_PROTOTYPE_METHOD(t, "end", BunzipEnd);

    target->Set(String::NewSymbol("Bunzip"), t->GetFunction());
  }

  int BunzipInit(int small) {
    COND_RETURN(state_ != State::Idle, BZ_SEQUENCE_ERROR);

    /* allocate inflate state */
    stream_.bzalloc = NULL;
    stream_.bzfree = NULL;
    stream_.opaque = NULL;
    stream_.avail_in = 0;
    stream_.next_in = NULL;

    int ret = BZ2_bzDecompressInit(&stream_, 0, small);
    if (ret == BZ_OK) {
      state_ = State::Data;
    }
    return ret;
  }


  int BunzipInflate(const char *data, int data_len,
                    ScopedBlob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ == State::Eos, BZ_OK);
    COND_RETURN(state_ != State::Data, BZ_SEQUENCE_ERROR);

    int ret = BZ_OK;

    State::Transition t(state_, State::Error);
    while (data_len > 0) { 
      COND_RETURN(!out.GrowBy(data_len), BZ_MEM_ERROR);

      stream_.next_in = (char*)data;
      stream_.next_out = out.data() + *out_len;
      stream_.avail_in = data_len;
      stream_.avail_out = data_len;

      ret = BZ2_bzDecompress(&stream_);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      COND_RETURN(ret != BZ_OK && ret != BZ_STREAM_END, ret);

      *out_len += data_len - stream_.avail_out;
      data += data_len - stream_.avail_in;
      data_len = stream_.avail_in;

      if (ret == BZ_STREAM_END) {
        t.alter(State::Eos);
        return ret;
      }
    }
    t.alter(State::Data);
    return ret;
  }


  void BunzipEnd() {
    if (state_ != State::Idle) {
      state_ = State::Idle;
      BZ2_bzDecompressEnd(&stream_);
    }
  }

 protected:

  static Handle<Value>
  New(const Arguments& args) {
    HandleScope scope;

    Bunzip *bunzip = new Bunzip();
    bunzip->Wrap(args.This());

    return args.This();
  }

  static Handle<Value>
  BunzipInit(const Arguments& args) {
    Bunzip *bunzip = ObjectWrap::Unwrap<Bunzip>(args.This());

    HandleScope scope;

    int small = 0;
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      small = args[0]->BooleanValue() ? 1 : 0;
    }
    int r = bunzip->BunzipInit(small);

    return scope.Close(Integer::New(r));
  }


  static Handle<Value>
  BunzipInflate(const Arguments& args) {
    Bunzip *bunzip = ObjectWrap::Unwrap<Bunzip>(args.This());

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

    ScopedBlob out;
    int out_size;
    int r = bunzip->BunzipInflate(buf.data(), len, out, &out_size);

    Local<Value> outString = Encode(out.data(), out_size, enc);
    return scope.Close(outString);
  }

  static Handle<Value>
  BunzipEnd(const Arguments& args) {
    Bunzip *bunzip = ObjectWrap::Unwrap<Bunzip>(args.This());

    HandleScope scope;

    bunzip->BunzipEnd();

    return scope.Close(String::New(""));
  }

  Bunzip()
    : EventEmitter(), state_(State::Idle)
  {}

  ~Bunzip ()
  {
    this->BunzipEnd();
  }

 private:
  bz_stream stream_;
  State::Value state_;

};

