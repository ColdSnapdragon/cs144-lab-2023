#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // Your code here.
  if ( closed_ )
    return;
  if ( size_ == capacity_ )
    return;
  if ( data.empty() )
    return;
  uint64_t len = data.size();
  uint64_t allow_len = min( len, capacity_ - size_ );
  data.resize( allow_len );
  stream.push( std::move( data ) );
  if ( buffer.empty() )
    buffer = stream.front();
  size_ += allow_len;
  total_ += allow_len;
}

void Writer::close()
{
  // Your code here.
  closed_ = true;
}

void Writer::set_error()
{
  // Your code here.
  error_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return closed_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - size_;
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return total_;
}

string_view Reader::peek() const
{
  // Your code here.
  return buffer;
}

bool Reader::is_finished() const
{
  // Your code here.
  return closed_ && size_ == 0;
}

bool Reader::has_error() const
{
  // Your code here.
  return error_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  if ( len == 0 )
    return;
  while ( len >= buffer.size() ) {
    stream.pop();
    len -= buffer.size();
    size_ -= buffer.size();
    if ( stream.empty() ) {
      size_ = 0;
      buffer = std::string_view();
      return;
    }
    buffer = stream.front();
  }
  buffer.remove_prefix( len );
  size_ -= len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return size_;
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return total_ - size_;
}
