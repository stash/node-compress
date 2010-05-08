#include <node.h>

#include "gzip.cc"

#ifndef COMPRESS_WITHOUT_BZIP
#include "bzip.cc"
#endif

extern "C" void
init (Handle<Object> target) 
{
  HandleScope scope;
  Gzip::Initialize(target);
  Gunzip::Initialize(target);

#ifndef COMPRESS_WITHOUT_BZIP
  Bzip::Initialize(target);
  Bunzip::Initialize(target);
#endif
}
