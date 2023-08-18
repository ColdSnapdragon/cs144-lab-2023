#include "tcp_receiver.hh"

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.
  if (message.SYN) {
    _isn = message.seqno;
    _rcv_syn = true;
  }
  if (!_rcv_syn)
    return;
    
  uint64_t abs_seqno = message.seqno.unwrap(_isn, inbound_stream.bytes_pushed());
  if (abs_seqno == 0 && !message.SYN)
    return;
  uint64_t first_index = abs_seqno ? abs_seqno - 1 : 0;
  reassembler.insert(first_index, message.payload, message.FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  uint16_t window_size = static_cast<uint16_t>(std::min(inbound_stream.available_capacity(), uint64_t(UINT16_MAX)));
  std::optional<Wrap32> ackno = Wrap32::wrap(inbound_stream.bytes_pushed() + _rcv_syn + inbound_stream.is_closed(), _isn);
  return TCPReceiverMessage({_rcv_syn ? ackno : std::nullopt, window_size});
}
