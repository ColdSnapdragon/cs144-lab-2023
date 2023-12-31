#include "reassembler.hh"

#include <iostream>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  R = L + output.available_capacity();
  uint64_t lb, rb;
  lb = first_index;
  rb = lb + data.size();
  if ( is_last_substring ) {
    _end = rb;
    if ( L == _end )
      output.close();
  }
  rb = std::min( R, rb );
  lb = std::max( L, lb );
  if ( rb <= L || lb >= R )
    return;
  auto r_obj = storage.upper_bound( DataSeg { inf, rb } ); // 相当于rb<rhs.r
  auto l_obj = storage.lower_bound( DataSeg { lb, 0 } );   // 相当于lb<rhs.l
  {
    auto it = r_obj;
    if ( it != storage.end() && ++it == l_obj ) // 如果[lb,rb)被包含在一个已有的区间中，那么++l_obj等于r_obj
      return;
  }
  while ( l_obj != r_obj ) {
    _pending -= l_obj->second.size();
    storage.erase( l_obj++ );
  }
  if ( r_obj != storage.end() && r_obj->first.l < rb ) {
    rb = r_obj->first.l; // 调整后缀
  }
  l_obj = storage.upper_bound( DataSeg { inf, lb } );
  if ( l_obj != storage.end() && l_obj->first.l < lb ) {
    lb = l_obj->first.r; // 调整前缀
  }
  data = std::move( data.substr( lb - first_index, rb - lb ) );
  _pending += data.size();
  storage[DataSeg { lb, rb }] = std::move( data );
  for ( auto it = storage.begin(); it != storage.end(); ) {
    if ( it->first.l == L ) {
      L = it->first.r;
      _pending -= it->second.size();
      output.push( std::move( it->second ) ); // 使用移动语义构造形参
      storage.erase( it++ );
    } else {
      break;
    }
  }
  if ( L == _end ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return _pending;
}
