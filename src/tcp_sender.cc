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
  return retransmissions; // 连续重传数。用于给使用者判断是否该断开连接
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( !resend_segments.empty() ) {
    TCPSenderMessage message = resend_segments.front();
    resend_segments.pop();
    return message;
  }
  if ( tosend_segments.empty() )
    return nullopt;
  TCPSenderMessage message = tosend_segments.front();
  if ( message.seqno.unwrap( isn_, index ) + message.sequence_length() > r_index )
    return nullopt;
  toack_segments.push( message );
  tosend_segments.pop();
  return message;
}

void TCPSender::push( Reader& outbound_stream )
{
  bool close = outbound_stream.is_finished();
  string_view sv = outbound_stream.peek();
  if ( sv.size() || !_start || close ) {
    do {
      uint16_t win = r_index - index; // 发送窗口的可用空间
      uint16_t len
        = min( win, (uint16_t)min( sv.size(), TCPConfig::MAX_PAYLOAD_SIZE ) ); // 最大为1000(和书上的1452不太合?)
      if ( _end || ( _start && len == 0 && !close ) || win == 0 )
        return;
      string seg { sv.substr( 0, len ) };
      outbound_stream.pop( len );
      _end = ( close && win > len ); // 若无可用序列号，则FIN留到下个数据段再发送
      TCPSenderMessage msg = TCPSenderMessage { Wrap32::wrap( index, isn_ ), !_start, seg, _end };
      tosend_segments.push( msg );
      index += msg.sequence_length();
      sv.remove_prefix( len );
      _start = true;
    } while ( sv.size() );
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = Wrap32::wrap( min( index, r_index ), isn_ ); // 就是发一个payload为空串的message而已
  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  rwindow = msg.window_size;
  zerowin = ( !rwindow );
  r_index = l_index + max( 1, (int)rwindow );
  if ( !msg.ackno.has_value() )
    return;
  uint64_t ackno = msg.ackno.value().unwrap( isn_, index );
  if ( ackno > index ) // 忽略错误的ackno
    return;
  while ( !toack_segments.empty() && ackno > l_index ) {
    TCPSenderMessage message = toack_segments.front();
    if ( l_index + message.sequence_length() > ackno )
      break; // 忽略错误的ackno(范围在ackno之前的数据段一定已经正确接收)。对于仅确认的一部分的数据段，则认为错误并完整重发，让重组器去合并
    l_index += message.sequence_length();
    toack_segments.pop();
    // 刷新重传计时器
    retransmissions = 0;
    rto = initial_RTO_ms_;
    timer = 0;
  }
  r_index = l_index + max( 1, (int)rwindow );
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timer += ms_since_last_tick;
  if ( rto == 0 )
    rto = initial_RTO_ms_;
  if ( timer >= rto ) {
    if ( !toack_segments.empty() )
      resend_segments.push( toack_segments.front() );
    if ( !zerowin ) // 接收方窗口为0时，定时探测，不改rto
      rto <<= 1;
    retransmissions += 1;
    timer = 0;
  }
}
