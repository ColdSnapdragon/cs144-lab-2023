#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

void Timer::start(uint64_t cur_ms, uint64_t RTO_ms) {
  _is_running = true;
  _start_ms = cur_ms;
  _RTO_ms = RTO_ms;
}

void Timer::stop() {
  _is_running = false;
}

bool Timer::expired(uint64_t cur_ms) {
  if (!_is_running)
    return false;
  uint64_t duration = cur_ms - _start_ms;
  return duration >= _RTO_ms;
}

bool Timer::is_running() const {
  return _is_running;
}

Wrap32 TCPSenderMessageCmp::_isn {0};
uint64_t TCPSenderMessageCmp::_abs_ackno {0};
TCPSenderMessageCmp::TCPSenderMessageCmp(TCPSenderMessage msg) : _msg(msg) {}

bool TCPSenderMessageCmp::operator<(const TCPSenderMessageCmp& other) const {
  return _msg.seqno.unwrap(_isn, _abs_ackno) > other._msg.seqno.unwrap(_isn, _abs_ackno);
}


/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn )
  : _isn( fixed_isn.value_or( Wrap32 { std::random_device()() } ) ), _initial_RTO_ms( initial_RTO_ms ), _cur_RTO_ms(initial_RTO_ms)
{
  TCPSenderMessageCmp::_isn = _cur_seq_no = _isn;
  TCPSenderMessageCmp::_abs_ackno = 0;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return _sequence_numbers_in_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return _consecutive_retransmissions;
}

std::optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if (_set_syn && !_msg_to_send.empty()) {
    TCPSenderMessage msg = _msg_to_send.front();
    _msg_to_send.pop();
    _outstanding.push(msg);
    if (!_timer.is_running())
      _timer.start(_ms_in_total, _cur_RTO_ms);
    return msg;
  }
  return std::nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  while (true) {
    uint16_t cur_window = _window_size ? _window_size : 1;
    uint64_t avail_size = _abs_ackno + cur_window - _cur_seq_no.unwrap(_isn, _abs_ackno);
    uint64_t stream_size = outbound_stream.bytes_buffered();
    if (avail_size == 0 || _set_fin || (_set_syn && !outbound_stream.is_finished() && stream_size == 0))  return;

    std::string str {};
    size_t read_size = std::min({stream_size, static_cast<size_t>(avail_size - (!_set_syn)), TCPConfig::MAX_PAYLOAD_SIZE});
    read(outbound_stream, read_size, str);
    
    if (avail_size > str.size()) _set_fin = outbound_stream.is_finished();
    TCPSenderMessage msg {_cur_seq_no, !_set_syn, str, _set_fin};
    _msg_to_send.push(msg);
    _cur_seq_no = _cur_seq_no + msg.sequence_length();
    _set_syn = true;
    _sequence_numbers_in_flight += msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return TCPSenderMessage {_cur_seq_no, false, {}, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  if (!msg.ackno.has_value()) return;
  uint64_t recv_abs_ackno = msg.ackno.value().unwrap(_isn, _abs_ackno);
  if (recv_abs_ackno > _cur_seq_no.unwrap(_isn, _abs_ackno) || recv_abs_ackno < _abs_ackno) return;
  
  bool good_ack = false;
  while (_outstanding.size()) {
    TCPSenderMessage cur = _outstanding.top()._msg;
    uint64_t cur_tail = (cur.seqno + cur.sequence_length()).unwrap(_isn, _abs_ackno);
    if (cur_tail <= recv_abs_ackno) {
      good_ack = true;
      _outstanding.pop();
      _sequence_numbers_in_flight -= cur.sequence_length();
    }
    else
      break;
  }

  _window_size = msg.window_size;
  TCPSenderMessageCmp::_abs_ackno = _abs_ackno = recv_abs_ackno;
  if (good_ack) {
    _cur_RTO_ms = _initial_RTO_ms;
    _consecutive_retransmissions = 0;
    if (_outstanding.size())  _timer.start(_ms_in_total, _cur_RTO_ms);
    else _timer.stop();
  }
}

void TCPSender::tick( const uint64_t ms_since_last_tick )
{
  // Your code here.
  _ms_in_total += ms_since_last_tick;
  if (_timer.expired(_ms_in_total) && _outstanding.size()) {
    _msg_to_send.push(_outstanding.top()._msg);
    _outstanding.pop();
    if (_window_size > 0) {
      ++_consecutive_retransmissions;
      _cur_RTO_ms *= 2;
    }
    _timer.start(_ms_in_total, _cur_RTO_ms);
  }
}
