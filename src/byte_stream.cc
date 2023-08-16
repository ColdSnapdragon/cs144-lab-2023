#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), stream(), isClosed( false ), hasError( false ), pushed_cnt( 0 ), poped_cnt( 0 )
{}

void Writer::push( string data )
{
  // Your code here.
  uint64_t avail_size = available_capacity();
  for ( uint64_t i = 0; i < avail_size && i < data.size(); ++i ) {
    stream.push( data[i] );
    ++pushed_cnt;
  }
}

void Writer::close()
{
  // Your code here.
  isClosed = true;
}

void Writer::set_error()
{
  // Your code here.
  hasError = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return isClosed;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  uint64_t in_mem_size = stream.size();
  return capacity_ - in_mem_size;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return pushed_cnt;
}

string_view Reader::peek() const
{
  // Your code here.
  if ( bytes_buffered() > 0 ) {
    const char& nxt = stream.front();
    string_view res( &nxt, 1 );
    return res;
  }
  return "";
}

bool Reader::is_finished() const
{
  // Your code here.
  return isClosed && bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  // Your code here.
  return hasError;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  uint64_t avail_size = bytes_buffered();
  for ( uint64_t i = 0; i < avail_size && i < len; ++i ) {
    stream.pop();
    ++poped_cnt;
  }
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return stream.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return poped_cnt;
}
