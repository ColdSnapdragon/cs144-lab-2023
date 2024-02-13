#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  if(arp_table.contains(next_hop.ipv4_numeric()) && timer < arp_ttl[next_hop.ipv4_numeric()]) {
    EthernetFrame frame{};
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = ethernet_address_;
    frame.header.dst = arp_table[next_hop.ipv4_numeric()];
    frame.payload = serialize(dgram); // 序列化
    to_send.push(frame);
  }
  else {
    pending_dgrams.push({dgram, next_hop});
    if(timer < arp_retransmit[next_hop.ipv4_numeric()])
      return;
    arp_retransmit[next_hop.ipv4_numeric()] = timer + 5000;
    EthernetFrame frame{};
    ARPMessage arp_dgram;
    arp_dgram.opcode = ARPMessage::OPCODE_REQUEST;
    arp_dgram.sender_ethernet_address = ethernet_address_;
    arp_dgram.sender_ip_address = ip_address_.ipv4_numeric();
    arp_dgram.target_ip_address = next_hop.ipv4_numeric();
    frame.header.type = EthernetHeader::TYPE_ARP; // 在头部标记类型
    frame.header.src = ethernet_address_;
    frame.header.dst = ETHERNET_BROADCAST; // 广播地址（不过按书上说应该是全0吧？）
    frame.payload = serialize(arp_dgram);
    to_send.push(frame);
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if(frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp_dgram;
    parse(arp_dgram, frame.payload); // 反序列化
    arp_table[arp_dgram.sender_ip_address] = arp_dgram.sender_ethernet_address;
    arp_ttl[arp_dgram.sender_ip_address] = timer + 30000;
    if(arp_dgram.opcode == ARPMessage::OPCODE_REQUEST && arp_dgram.target_ip_address == ip_address_.ipv4_numeric()){
        EthernetFrame ef;
        arp_dgram.opcode = ARPMessage::OPCODE_REPLY;
        arp_dgram.target_ip_address = arp_dgram.sender_ip_address;
        arp_dgram.target_ethernet_address = arp_dgram.sender_ethernet_address;
        arp_dgram.sender_ip_address = ip_address_.ipv4_numeric();
        arp_dgram.sender_ethernet_address = ethernet_address_;
        ef.header.type = EthernetHeader::TYPE_ARP;
        ef.header.src = ethernet_address_;
        ef.header.dst = frame.header.src;
        ef.payload = serialize(arp_dgram);
        to_send.push(ef);
    }
    while(!pending_dgrams.empty() && pending_dgrams.front().second.ipv4_numeric() == arp_dgram.sender_ip_address) {
      auto [dgram, next_hop] = pending_dgrams.front();
      EthernetFrame ef;
      ef.header.type = EthernetHeader::TYPE_IPv4;
      ef.header.src = ethernet_address_;
      ef.header.dst = frame.header.src;
      ef.payload = serialize(dgram);
      to_send.push(ef);
      pending_dgrams.pop();
    }
    return std::nullopt;
  }
  InternetDatagram dgram;
  parse(dgram, frame.payload); 
  if(frame.header.dst == ethernet_address_) // 尽管dst ip可能与当前网卡ip不同
    return dgram;
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  timer += ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if(!to_send.empty()) {
    EthernetFrame frame = to_send.front();
    to_send.pop();
    return frame;
  }
  return std::nullopt;
}
