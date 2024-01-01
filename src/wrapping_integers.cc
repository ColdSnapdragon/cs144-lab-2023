#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( n + zero_point.raw_value_ ) }; // 截断代替取模
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t n = static_cast<uint32_t>( ( 1ULL << 32 ) + raw_value_ - zero_point.raw_value_ );
  uint64_t m = static_cast<uint32_t>( checkpoint );
  uint64_t p = ( checkpoint >> 32 );
  p += ( m > n + ( 1ULL << 31 ) );
  p -= ( !!p && ( m + ( 1ULL << 31 ) < n ) ); // 前提p不为0
  return n + ( p << 32 );
}
// 当p做加法时感觉要先判断if ( p ^ ( ( 1ULL << 32 ) - 1 ) )，虽然测试没事
