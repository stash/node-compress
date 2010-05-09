#include <node.h>

#ifdef WITH_GZIP
#include "gzip.cc"
#endif

#ifdef WITH_BZIP
#include "bzip.cc"
#endif

extern "C" void
init (Handle<Object> target) 
{
  HandleScope scope;

#ifdef WITH_GZIP
  Gzip::Initialize(target);
  Gunzip::Initialize(target);
#endif

#ifdef WITH_BZIP
  Bzip::Initialize(target);
  Bunzip::Initialize(target);
#endif
}

