#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "utils.h"
#include "zlib.h"

#define CHUNK 16384

using namespace v8;
using namespace node;

class GzipUtils {
 public:
  typedef ScopedOutputBuffer<Bytef> Blob;


  static bool IsError(int gzipStatus) {
    return !(gzipStatus == Z_OK || gzipStatus == Z_STREAM_END);
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

 private:
  static const char NeedDictionary[];
  static const char Errno[];
  static const char StreamError[];
  static const char DataError[];
  static const char MemError[];
  static const char BufError[];
  static const char VersionError[];
};

const char GzipUtils::NeedDictionary[] = "Dictionary must be specified. "
  "Currently this is unsupported by library.";
const char GzipUtils::Errno[] = "Z_ERRNO: Input/output error.";
const char GzipUtils::StreamError[] = "Z_STREAM_ERROR: Invalid arguments or "
  "stream state is inconsistent.";
const char GzipUtils::DataError[] = "Z_DATA_ERROR: Input data corrupted.";
const char GzipUtils::MemError[] = "Z_MEM_ERROR: Out of memory.";
const char GzipUtils::BufError[] = "Z_BUF_ERROR: Buffer error.";
const char GzipUtils::VersionError[] = "Z_VERSION_ERROR: "
  "Invalid library version.";


class GzipImpl : public EventEmitter {
  friend class ZipLib<GzipImpl>;
  typedef GzipUtils Utils;
  typedef GzipUtils::Blob Blob;

  typedef ZipLib<GzipImpl> GzipLib;

 public:
  static const char Name[];

 public:
  int GzipInit(int level) {
    COND_RETURN(state_ != GzipLib::Idle, Z_STREAM_ERROR);

    /* allocate deflate state */
    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;

    int ret = deflateInit2(&stream_, level,
                           Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_OK) {
      state_ = GzipLib::Data;
    }
    return ret;
  }


  int Write(char *data, int data_len, Blob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ != GzipLib::Data, Z_STREAM_ERROR);

    GzipLib::Transition t(state_, GzipLib::Error);

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

    t.alter(GzipLib::Data);
    return ret;
  }


  int Close(Blob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ == GzipLib::Idle, Z_OK);
    assert(state_ == GzipLib::Data || state_ == GzipLib::Error);

    GzipLib::Transition t(state_, GzipLib::Error);
    int ret = Z_OK;
    if (state_ == GzipLib::Data) {
      ret = GzipEndWithData(out, out_len);
    }

    t.abort();
    this->Destroy();
    return ret;
  }


  void Destroy() {
    GzipLib::Transition t(state_, GzipLib::Idle);
    if (state_ != GzipLib::Idle) {
      deflateEnd(&stream_);
    }
  }


  int GzipEndWithData(Blob &out, int *out_len) {
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


 public:
  static Handle<Value> New(const Arguments& args)
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

    GzipImpl *gzip = new GzipImpl();
    gzip->Wrap(args.This());

    int r = gzip->GzipInit(level);
    return GzipLib::ReturnThisOrThrow(args, r);
  }

 protected:
  GzipImpl() 
    : EventEmitter(), state_(GzipLib::Idle)
  {}


  ~GzipImpl()
  {
    this->Destroy();
  }

 private:
  z_stream stream_;
  GzipLib::State state_;

};
const char GzipImpl::Name[] = "Gzip";
typedef ZipLib<GzipImpl> Gzip;


class GunzipImpl : public EventEmitter {
  friend class ZipLib<GunzipImpl>;
  typedef GzipUtils Utils;
  typedef GzipUtils::Blob Blob;

  typedef ZipLib<GunzipImpl> GzipLib;
 public:
  static const char Name[];

 private:
  int GunzipInit() {
    COND_RETURN(state_ != GzipLib::Idle, Z_STREAM_ERROR);

    /* allocate inflate state */
    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;
    stream_.avail_in = 0;
    stream_.next_in = Z_NULL;

    int ret = inflateInit2(&stream_, 16 + MAX_WBITS);
    if (ret == Z_OK) {
      state_ = GzipLib::Data;
    }
    return ret;
  }


  int Write(const char* data, int data_len, Blob &out, int *out_len) {
    *out_len = 0;
    COND_RETURN(state_ == GzipLib::Eos, Z_OK);
    COND_RETURN(state_ != GzipLib::Data, Z_STREAM_ERROR);

    GzipLib::Transition t(state_, GzipLib::Error);

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
          this->Destroy();
          return ret;
        }
        COND_RETURN(ret != Z_OK && ret != Z_STREAM_END, ret);
        
        *out_len += (CHUNK - stream_.avail_out);

        if (ret == Z_STREAM_END) {
          t.alter(GzipLib::Eos);
          return ret;
        }
      } while (stream_.avail_out == 0);
      data += CHUNK;
      data_len -= CHUNK;
    }
    t.alter(GzipLib::Data);
    return ret;
  }


  int Close(Blob &out, int *out_len) {
    *out_len = 0;
    this->Destroy();
    return Z_OK;
  }


  void Destroy() {
    if (state_ != GzipLib::Idle) {
      state_ = GzipLib::Idle;
      inflateEnd(&stream_);
    }
  }

 public:
  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;

    GunzipImpl *gunzip = new GunzipImpl();
    gunzip->Wrap(args.This());

    int r = gunzip->GunzipInit();
    return GzipLib::ReturnThisOrThrow(args, r);
  }


 protected:
  GunzipImpl() 
    : EventEmitter(), state_(GzipLib::Idle)
  {}


  ~GunzipImpl()
  {
    this->Destroy();
  }

 private:
  z_stream stream_;
  GzipLib::State state_;
};
const char GunzipImpl::Name[] = "Gunzip";
typedef ZipLib<GunzipImpl> Gunzip;

