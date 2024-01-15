#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <iostream>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // return outstanding_bytes;
  return min( index, r_index ) - l_index;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( tosend_segments.empty() )
    return nullopt;
  TCPSenderMessage message = tosend_segments.front();
  if ( message.seqno.unwrap( isn_, index ) + message.sequence_length() > r_index )
    return nullopt;
  // if ( toack_segments.empty() )
  //  expect_ack = expect_ack + message.sequence_length();
  // outstanding_bytes += message.sequence_length();
  toack_segments.push( message );
  tosend_segments.pop();
  return message;
}

void TCPSender::push( Reader& outbound_stream )
{
  if ( !_start ) {
    // seqno = isn_;
    // expect_ack = isn_;
  }
  string_view sv = outbound_stream.peek();
  if ( sv.size() || !_start ) {
    uint16_t win = min( rwindow, (uint16_t)1452 );
    do {
      uint16_t len = min( (uint16_t)min( (uint64_t)win, r_index - index ), (uint16_t)sv.size() );
      string_view seg = sv.substr( 0, len );
      TCPSenderMessage msg
        = TCPSenderMessage { Wrap32::wrap( index, isn_ ), !_start, string( seg ), outbound_stream.is_finished() };
      tosend_segments.push( msg );
      outbound_stream.pop( len );
      index += msg.sequence_length();
      // seqno = seqno + msg.sequence_length();
      // outstanding_bytes += msg.sequence_length();
      sv.remove_prefix( len );
    } while ( sv.size() );
  }
  _start = true;
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( min( index, r_index ), isn_ );
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  rwindow = msg.window_size;
  if ( !msg.ackno.has_value() )
    return;
  while ( !toack_segments.empty() && msg.ackno.value().unwrap( isn_, index ) > l_index ) {
    // outstanding_bytes -= toack_segments.front().sequence_length();
    l_index += toack_segments.front().sequence_length();
    toack_segments.pop();
  }
  r_index = l_index + rwindow;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer += ms_since_last_tick;
  if ( timer >= initial_RTO_ms_ ) {
    timer = 0;
  }
}
