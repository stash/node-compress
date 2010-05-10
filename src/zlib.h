#ifndef NODE_COMPRESS_ZLIB_H__
#define NODE_COMPRESS_ZLIB_H__

#include <node.h>
#include <node_events.h>
#include <node_buffer.h>
#include <assert.h>

using namespace v8;
using namespace node;

template <class Processor>
class ZipLib {
 private:
  typedef typename Processor::Utils Utils;
  typedef typename Processor::Blob Blob;

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
        return ThrowCallbackExpected();
      }
    }

    Blob out;
    int out_size;
    int r = proc->Write(buffer->data(), buffer->length(), out, &out_size);

    DoCallback(cb, r, out, out_size);
    return Undefined();
  }


  static Handle<Value> Close(const Arguments& args) {
    Processor *proc = ObjectWrap::Unwrap<Processor>(args.This());

    HandleScope scope;

    Local<Value> cb(args[0]);
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
      if (!args[0]->IsFunction()) {
        return ThrowCallbackExpected();
      }
    }  

    Blob out;
    int out_size;

    int r = proc->Close(out, &out_size);
    DoCallback(cb, r, out, out_size);

    return Undefined();
  }


  static Handle<Value> Destroy(const Arguments& args) {
    Processor *proc = ObjectWrap::Unwrap<Processor>(args.This());
    proc->Destroy();

    return Undefined();
  }


  static void DoCallback(Local<Value> &cb, int r, Blob &out, int out_size) {
    if (cb->IsFunction()) {
      Local<Value> argv[2];
      argv[0] = Utils::GetException(r);
      argv[1] = Encode(out.data(), out_size, BINARY);

      TryCatch try_catch;

      Function *fun = Function::Cast(*cb);
      fun->Call(Context::GetCurrent()->Global(), 2, argv);

      if (try_catch.HasCaught()) {
        FatalException(try_catch);
      }
    }
  }

};

#endif

