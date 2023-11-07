#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <queue>

class Timer
{
  bool _is_running {false};
  uint64_t _start_ms {0}, _RTO_ms {0};

public:
  void start(uint64_t cur_ms, uint64_t RTO_ms);
  void stop();
  bool expired(uint64_t cur_ms);
  bool is_running() const;
};


class TCPSenderMessageCmp
{
public:
  static Wrap32 _isn;
  static uint64_t _abs_ackno;
  TCPSenderMessage _msg;

  TCPSenderMessageCmp(TCPSenderMessage msg);
  bool operator<(const TCPSenderMessageCmp& other) const;
};


class TCPSender
{
  Wrap32 _isn, _cur_seq_no {0};
  uint16_t _window_size {1};
  uint64_t _initial_RTO_ms, _cur_RTO_ms, _ms_in_total {0};
  uint64_t _abs_ackno {0}, _sequence_numbers_in_flight {0}, _consecutive_retransmissions {0};
  Timer _timer {};
  bool _set_syn {false}, _set_fin {false};

  std::queue<TCPSenderMessage> _msg_to_send {};
  std::priority_queue<TCPSenderMessageCmp> _outstanding {};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( const uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
