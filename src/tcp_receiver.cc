#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // 无关RFC规定，其实SYN报文和FIN报文都可以携带payload，并得到正常的ACK
  // FIN报文可以携带数据，因为连接还没释放
  // 可能会有SYN和FIN都标记的情况。有SYN则stream index必为0
  if ( message.SYN ) {
    reassembler.insert( 0, message.payload, message.FIN, inbound_stream );
    _isn = Wrap32 { message.seqno };
    _isn = _isn + 1; // SYN占一个序列号(我直接加在isn上，后续的序列号计算就不用考虑减一个SYN了)
    _has_isn = true;
    return;
  }
  uint64_t index = message.seqno.unwrap( _isn, inbound_stream.bytes_pushed() );
  reassembler.insert( index, message.payload, message.FIN, inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  uint64_t index = inbound_stream.bytes_pushed();
  Wrap32 ackno = Wrap32::wrap( index, _isn ) + inbound_stream.is_closed(); // FIN占一个序列号(无形地出现在末尾)
  uint16_t window_size = static_cast<uint16_t>( min( inbound_stream.available_capacity(), (uint64_t)UINT16_MAX ) );
  if ( _has_isn )
    return { ackno, window_size };
  return { nullopt, window_size };
}