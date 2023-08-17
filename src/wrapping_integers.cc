#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint32_t diff = raw_value_ - zero_point.raw_value_;
  uint32_t ckp_mod = static_cast<uint32_t>(checkpoint);
  uint32_t add = diff - ckp_mod, subtract = ckp_mod - diff;
  uint64_t added = checkpoint + add, subtracted = checkpoint - subtract;
  if (subtracted > checkpoint || add <= subtract)
    return added;
  return subtracted;
}
