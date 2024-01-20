#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <list>
#include <queue>

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool _start = false; // 是否已经发送SYN报文
  bool _end = false;   // 是否已经发送FIN报文
  Wrap32 seqno { 0 };
  Wrap32 expect_ack { 0 };
  uint64_t index { 0 };
  uint64_t ack_index { 0 };
  uint16_t rwindow { 0 };
  uint64_t timer { 0 };
  uint64_t rto { 0 };
  bool zerowin = false;
  std::queue<TCPSenderMessage> toack_segments {};
  std::queue<TCPSenderMessage> tosend_segments {};
  std::queue<TCPSenderMessage> resend_segments {};
  uint64_t outstanding_bytes { 0 };
  uint64_t l_index { 0 };
  uint64_t r_index { 1 }; // message的最小长度为1(比如在第一次获取接收窗口大小前的SYN报文)
  uint64_t retransmissions { 0 };

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
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
