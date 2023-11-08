#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : _ethernet_address( ethernet_address ), _ip_address( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( _ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::update_ip_eth_map(uint32_t ip, EthernetAddress eth) {
  _ip_to_eth_map[ip] = eth;
  _map_set_time[{ip, to_string(eth)}] = _ms_in_total;
  _ip_eth_time_q.push({ip, to_string(eth), _ms_in_total});
}

void NetworkInterface::resend_unknown_ip_dgram(const uint32_t ip, const EthernetAddress& eth) {
  if (_unkown_ipdgram_info.count(ip)) {
    EthernetFrame cur = construct_frame(_unkown_ipdgram_info[ip].second, eth, EthernetHeader::TYPE_IPv4);
    _unkown_ipdgram_info.erase(ip);
    _to_send.push(cur);
  }
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t ip = next_hop.ipv4_numeric();
  if (_ip_to_eth_map.count(ip)) {
    EthernetAddress dest = _ip_to_eth_map[ip];
    EthernetFrame cur = construct_frame(dgram, dest, EthernetHeader::TYPE_IPv4);
    _to_send.push(cur);
  } else {
    if (_unkown_ipdgram_info.count(ip)) {
      auto info = _unkown_ipdgram_info[ip];
      size_t last_time = info.first;
      size_t duration = _ms_in_total - last_time;
      if (duration <= _arp_avoid_flood_time) return;
    }
    _unkown_ipdgram_info[ip] = {_ms_in_total, dgram};

    ARPMessage msg;
    msg.opcode = msg.OPCODE_REQUEST;
    msg.sender_ethernet_address = _ethernet_address;
    msg.sender_ip_address = _ip_address.ipv4_numeric();
    msg.target_ip_address = ip;

    EthernetFrame cur = construct_frame(msg, ETHERNET_BROADCAST, EthernetHeader::TYPE_ARP);
    _to_send.push(cur);
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  EthernetHeader eth_header = frame.header;
  InternetDatagram datagram;
  if (eth_header.type == EthernetHeader::TYPE_IPv4) {
    if (eth_header.dst == _ethernet_address)
      if (parse(datagram, frame.payload))
        return datagram;
  
  } else if (eth_header.type == EthernetHeader::TYPE_ARP) {
    if (eth_header.dst == ETHERNET_BROADCAST || eth_header.dst == _ethernet_address) {
      ARPMessage msg;
      if (parse(msg, frame.payload)) {
        update_ip_eth_map(msg.sender_ip_address, msg.sender_ethernet_address);
        resend_unknown_ip_dgram(msg.sender_ip_address, msg.sender_ethernet_address);

        if (msg.opcode == msg.OPCODE_REPLY) {
          update_ip_eth_map(msg.target_ip_address, msg.target_ethernet_address);
          resend_unknown_ip_dgram(msg.target_ip_address, msg.target_ethernet_address);
       
        } else if (msg.opcode == msg.OPCODE_REQUEST) {
          if (msg.target_ip_address == _ip_address.ipv4_numeric()) {
            swap(msg.target_ip_address, msg.sender_ip_address);
            msg.target_ethernet_address = msg.sender_ethernet_address;
            msg.sender_ethernet_address = _ethernet_address;
            msg.opcode = msg.OPCODE_REPLY;
            EthernetFrame cur = construct_frame(msg, eth_header.src, EthernetHeader::TYPE_ARP);
            _to_send.push(cur);
          }
        }
      }
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  _ms_in_total += ms_since_last_tick;
  while (_ip_eth_time_q.size()) {
    auto cur = _ip_eth_time_q.front();
    uint32_t ip, time;
    string eth;
    tie(ip, eth, time) = cur;
    if (time + _map_expire_time > _ms_in_total) break;

    _ip_eth_time_q.pop();
    auto ip_eth_map = make_pair(ip, eth);
    if (_map_set_time.count(ip_eth_map) && _map_set_time[ip_eth_map] == time) {
      _ip_to_eth_map.erase(ip);
      _map_set_time.erase(ip_eth_map);
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (_to_send.size()) {
    EthernetFrame cur = _to_send.front();
    _to_send.pop();
    return cur;
  }
  return nullopt;
}
