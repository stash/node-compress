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
    int ret;
    
    if (blockSize100k < 1) {
      blockSize100k = 1;
    }
    if (9 < blockSize100k) {
      blockSize100k = 9;
    }

    if (workFactor < 0) {
      workFactor = 0;
    }
    if (workFactor > 250) {
      workFactor = 250;
    }

    /* allocate deflate state */
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;
    ret = BZ2_bzCompressInit(&strm, blockSize100k, 0, workFactor);
    if (ret == BZ_OK) {
      is_allocated = true;
    }
    return ret;
  }

  int BzipDeflate(char* data, int data_len, char** out, int* out_len) {
    int ret;
    char* temp;

    *out = NULL;
    *out_len = 0;

    if (!is_allocated) {
      return BZ_SEQUENCE_ERROR;
    }

    ret = 0;
    while (data_len > 0) {    
      strm.avail_in = data_len;
      strm.next_in = data;

      temp = (char *)realloc(*out, *out_len + data_len + 1);
      if (temp == NULL) {
        return BZ_MEM_ERROR;
      }
      *out = temp;
      strm.avail_out = data_len + 1;
      strm.next_out = *out + *out_len;

      ret = BZ2_bzCompress(&strm, BZ_RUN);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      *out_len += (data_len + 1 - strm.avail_out);

      data += data_len - strm.avail_in;
      data_len = strm.avail_in;
    }
    return ret;
  }


  int BzipEnd(char** out, int* out_len) {
    // Don't expect this to be large as output buffer for deflate is as large
    // as input.
    const int Chunk = 128;

    int ret;
    char* temp;

    *out = NULL;
    *out_len = 0;

    if (!is_allocated) {
      return BZ_SEQUENCE_ERROR;
    }

    strm.avail_in = 0;
    strm.next_in = NULL;

    do {
      temp = (char *)realloc(*out, *out_len + Chunk);
      if (temp == NULL) {
        return BZ_MEM_ERROR;
      }
      *out = temp;
      strm.avail_out = Chunk;
      strm.next_out = *out + *out_len;

      ret = BZ2_bzCompress(&strm, BZ_FINISH);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      *out_len += (Chunk - strm.avail_out);
    } while (ret != BZ_STREAM_END);
    
    BZ2_bzCompressEnd(&strm);
    is_allocated = false;
    return ret;
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

    char* out;
    int out_size;
    int r = bzip->BzipDeflate(buf.data(), len, &out, &out_size);

    if (out_size==0) {
      return scope.Close(String::New(""));
    }

    Local<Value> outString = Encode(out, out_size, BINARY);
    free(out);
    return scope.Close(outString);
  }

  static Handle<Value>
  BzipEnd(const Arguments& args) {
    Bzip *bzip = ObjectWrap::Unwrap<Bzip>(args.This());

    HandleScope scope;

    char* out;
    int out_size;
    if (args.Length() > 0 && args[0]->IsString()) {
      String::Utf8Value format_type(args[1]->ToString());
    }  

    int r = bzip->BzipEnd(&out, &out_size);
    if (out_size==0) {
      return String::New("");
    }
    Local<Value> outString = Encode(out, out_size, BINARY);
    free(out);
    return scope.Close(outString);
  }


  Bzip () : EventEmitter () 
  {
  }

  ~Bzip ()
  {
    if (is_allocated) {
      BZ2_bzCompressEnd(&strm);
    }
  }

 private:

  bz_stream strm;
  bool is_allocated;
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
    /* allocate inflate state */
    strm.bzalloc = NULL;
    strm.bzfree = NULL;
    strm.opaque = NULL;

    strm.avail_in = 0;
    strm.next_in = NULL;
    int ret = BZ2_bzDecompressInit(&strm, 0, small);
    if (ret == BZ_OK) {
      is_allocated = true;
    }
    return ret;
  }

  int BunzipInflate(const char* data, int data_len, char** out, int* out_len) {
    int ret;
    char* temp;

    const int InflateRatio = 3;

    *out = NULL;
    *out_len = 0;

    if (!is_allocated) {
      return BZ_SEQUENCE_ERROR;
    }

    ret = 0;

    while (data_len > 0) {    
      strm.avail_in = data_len;
      strm.next_in = (char*)data;

      temp = (char *)realloc(*out, *out_len + data_len * InflateRatio);
      if (temp == NULL) {
        return BZ_MEM_ERROR;
      }

      *out = temp;
      strm.avail_out = data_len * InflateRatio;
      strm.next_out = *out + *out_len;

      ret = BZ2_bzDecompress(&strm);
      assert(ret != BZ_SEQUENCE_ERROR);  /* state not clobbered */
      *out_len += data_len * InflateRatio - strm.avail_out;
      data += data_len - strm.avail_in;
      data_len = strm.avail_in;

      if (ret != BZ_OK) {
        if (ret == BZ_STREAM_END) {
          BunzipEnd();
        }
        return ret;
      }
    }
    return ret;
  }


  void BunzipEnd() {
    if (is_allocated) {
      BZ2_bzDecompressEnd(&strm);
      is_allocated = false;
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

    char* out;
    int out_size;
    int r = bunzip->BunzipInflate(buf.data(), len, &out, &out_size);

    Local<Value> outString = Encode(out, out_size, enc);
    free(out);
    return scope.Close(outString);
  }

  static Handle<Value>
  BunzipEnd(const Arguments& args) {
    Bunzip *bunzip = ObjectWrap::Unwrap<Bunzip>(args.This());

    HandleScope scope;

    bunzip->BunzipEnd();

    return scope.Close(String::New(""));
  }

  Bunzip () : EventEmitter () 
  {
  }

  ~Bunzip ()
  {
    this->BunzipEnd();
  }

 private:

  bz_stream strm;
  bool is_allocated;

};

