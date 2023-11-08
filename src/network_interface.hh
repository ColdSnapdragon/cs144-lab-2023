#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <iostream>
#include <list>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>

struct PairHash {
  template <typename T1, typename T2>
  std::size_t operator()(const std::pair<T1, T2>& p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ h2;
  }
};

struct PairEqual {
  template <typename T1, typename T2>
  bool operator()(const std::pair<T1, T2>& p1, const std::pair<T1, T2>& p2) const {
    return p1.first == p2.first && p1.second == p2.second;
  }
};

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
private:
  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress _ethernet_address;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address _ip_address;

  size_t _ms_in_total {0};
  const size_t _arp_avoid_flood_time {5 * 1000}, _map_expire_time {30 * 1000};
  std::queue<EthernetFrame> _to_send {};
  std::unordered_map<uint32_t, std::pair<size_t, InternetDatagram>> _unkown_ipdgram_info {};
  std::unordered_map<uint32_t, EthernetAddress> _ip_to_eth_map {};
  std::unordered_map<std::pair<uint32_t, std::string>, uint32_t, PairHash, PairEqual> _map_set_time {};
  std::queue<std::tuple<uint32_t, std::string, uint32_t>> _ip_eth_time_q {};

  void update_ip_eth_map(uint32_t ip, EthernetAddress eth);
  void resend_unknown_ip_dgram(const uint32_t ip, const EthernetAddress& eth);
  
  template<class T>
  EthernetFrame construct_frame(const T& data, const EthernetAddress& eth_addr, uint16_t type) {
    EthernetHeader eth_header {eth_addr, _ethernet_address, type};
    std::vector<Buffer> eth_payload = serialize(data);
    return {eth_header, eth_payload};
  }

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( const size_t ms_since_last_tick );
};
