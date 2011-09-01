/*
 * Copyright 2009, Acknack Ltd. All rights reserved.
 * Copyright 2010, Ivan Egorov (egorich.3.04@gmail.com).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#include "utils.h"
#include "zlib.h"

using namespace v8;
using namespace node;

class GzipUtils {
 public:
  typedef ScopedOutputBuffer<Bytef> Blob;

 public:
  static int StatusOk() {
    return Z_OK;
  }


  static int StatusSequenceError() {
    return Z_STREAM_ERROR;
  }


  static int StatusMemoryError() {
    return Z_MEM_ERROR;
  }


  static int StatusEndOfStream() {
    return Z_STREAM_END;
  }

 public:
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


class GzipImpl {
#ifdef NEED_PUBLIC_FRIEND
 public:
#endif
  friend class ZipLib<GzipImpl>;

  typedef GzipUtils Utils;
  typedef GzipUtils::Blob Blob;

 private:
  static const char Name[];

 private:
  Handle<Value> Init(const Arguments &args) {
    HandleScope scope;

    int level = Z_DEFAULT_COMPRESSION;
    int gzip_header = 16;

    want_buffer_ = false;
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsInt32()) {
        Local<Value> exception = Exception::TypeError(
            String::New("level must be an integer"));
        return ThrowException(exception);
      }
      level = args[0]->Int32Value();

      if(args.Length() > 1 && !args[1]->IsUndefined()) {
        if(!args[1]->IsBoolean()) {
            Local<Value> exception = Exception::TypeError(
                String::New("want_buffer must be a boolean"));
            return ThrowException(exception);
        }
        if(args[1]->BooleanValue()) want_buffer_ = true;

        if(args.Length() > 2 && !args[2]->IsUndefined()) {
          if(!args[2]->IsBoolean()) {
            Local<Value> exception = Exception::TypeError(
                String::New("gzip_header must be a boolean"));
            return ThrowException(exception);
          }
          if(!args[2]->BooleanValue()) gzip_header = 0;
        }
      }
    }

    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;
    DEBUG_P("strm:%p", (void*)&stream_);
    int ret = deflateInit2(&stream_, level,
                           Z_DEFLATED, gzip_header + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (Utils::IsError(ret)) {
      return ThrowException(Utils::GetException(ret));
    }
    return Undefined();
  }


  int Write(char *data, int &dataLength, Blob &out, bool flush) {
    out.setUseBufferOut(want_buffer_);
    stream_.next_in = reinterpret_cast<Bytef*>(data);
    stream_.avail_in = dataLength;
    stream_.next_out = out.data() + out.length();
    size_t initAvail = stream_.avail_out = out.avail();

    DEBUG_P("deflate strm:%p flush?%s stream{next_in:%p avail_in:%d next_out:%p avail_out:%d} out{length():%d capacity():%d}", (void*)&stream_, flush?"yes":"no", stream_.next_in, stream_.avail_in, stream_.next_out, stream_.avail_out, out.length(), out.capacity());
    int ret = deflate(&stream_, flush ? Z_FINISH : Z_NO_FLUSH);
    DEBUG_P("post-deflate strm:%p flush?%s stream{next_in:%p avail_in:%d next_out:%p avail_out:%d} wrote:%d ret:%d out{length():%d capacity():%d}", (void*)&stream_, flush?"yes":"no", stream_.next_in, stream_.avail_in, stream_.next_out, stream_.avail_out, initAvail - stream_.avail_out, ret, out.length(), out.capacity());

    if (!Utils::IsError(ret)) {
      DEBUG_P("post-deflate strm:%p ret is OK will assert %d+%d <= %d", (void*)&stream_, out.length(), initAvail - stream_.avail_out, out.capacity());
      dataLength = stream_.avail_in;
      out.IncreaseLengthBy(initAvail - stream_.avail_out);
    }
    return ret;
  }


  int Finish(Blob &out) {
    out.setUseBufferOut(want_buffer_);
    stream_.avail_in = 0;
    stream_.next_in = NULL;
    stream_.next_out = out.data() + out.length();
    int initAvail = stream_.avail_out = out.avail();

    DEBUG_P("deflate strm:%p stream{next_in:%p avail_in:%d next_out:%p avail_out:%d} out{length():%d capacity():%d}", (void*)&stream_, stream_.next_in, stream_.avail_in, stream_.next_out, stream_.avail_out, out.length(), out.capacity());
    int ret = deflate(&stream_, Z_FINISH);
    DEBUG_P("post-deflate strm:%p stream{next_in:%p avail_in:%d next_out:%p avail_out:%d} wrote:%d ret:%d out{length():%d capacity():%d}", (void*)&stream_, stream_.next_in, stream_.avail_in, stream_.next_out, stream_.avail_out, initAvail - stream_.avail_out, ret, out.length(), out.capacity());
    if (!Utils::IsError(ret)) {
      out.IncreaseLengthBy(initAvail - stream_.avail_out);
    }
    return ret;
  }


  void Destroy() {
    deflateEnd(&stream_);
  }

 private:
  bool want_buffer_;
  z_stream stream_;
};
const char GzipImpl::Name[] = "Gzip";
typedef ZipLib<GzipImpl> Gzip;


class GunzipImpl {
#ifdef NEED_PUBLIC_FRIEND
 public:
#endif
  friend class ZipLib<GunzipImpl>;

  typedef GzipUtils Utils;
  typedef GzipUtils::Blob Blob;

 private:
  static const char Name[];

 private:
  Handle<Value> Init(const Arguments &args) {
    int gzip_header = 32; // auto-detect by default
    stream_.zalloc = Z_NULL;
    stream_.zfree = Z_NULL;
    stream_.opaque = Z_NULL;
    stream_.avail_in = 0;
    stream_.next_in = Z_NULL;

    want_buffer_ = false;
    if (args.Length() > 0) {
      if(!args[0]->IsBoolean()) {
        Local<Value> exception = Exception::TypeError(
            String::New("want_buffer must be a boolean"));
        return ThrowException(exception);
      }
      if(args[0]->BooleanValue()) want_buffer_ = true;

      if(args.Length() > 1) {
        if(!args[1]->IsBoolean()) {
          Local<Value> exception = Exception::TypeError(
              String::New("gzip_header must be a boolean"));
          return ThrowException(exception);
        }
        gzip_header = (args[1]->BooleanValue()) ? 16 : 0;
      }
    }
    int ret = inflateInit2(&stream_, gzip_header + MAX_WBITS);
    if (Utils::IsError(ret)) {
      return ThrowException(Utils::GetException(ret));
    }
    return Undefined();
  }


  int Write(char* data, int &dataLength, Blob &out, bool flush) {
    out.setUseBufferOut(want_buffer_);
    stream_.next_in = reinterpret_cast<Bytef*>(data);
    stream_.avail_in = dataLength;
    stream_.next_out = out.data() + out.length();
    size_t initAvail = stream_.avail_out = out.avail();

    int ret = inflate(&stream_, Z_NO_FLUSH); // flush means nothing here.
    dataLength = stream_.avail_in;
    if (!Utils::IsError(ret)) {
      out.IncreaseLengthBy(initAvail - stream_.avail_out);
    }
    return ret;
  }


  int Finish(Blob &out) {
    out.setUseBufferOut(want_buffer_);
    return Z_OK;
  }


  void Destroy() {
    inflateEnd(&stream_);
  }

 private:
  bool want_buffer_;
  z_stream stream_;
};
const char GunzipImpl::Name[] = "Gunzip";
typedef ZipLib<GunzipImpl> Gunzip;

