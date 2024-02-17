#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  route_table.push_back({route_prefix, prefix_length, next_hop, interface_num});
}

void Router::route() {
  for(auto &interface: interfaces_) {
    optional<InternetDatagram> opt_dgram;
    while(opt_dgram = interface.maybe_receive()){
      InternetDatagram dgram = opt_dgram.value();
      if(dgram.header.ttl <= 1)
        continue;
      dgram.header.ttl--; // TTL减一
      size_t max_len = 0;
      int target_pos = -1;
      for(size_t i=0;i<route_table.size();++i){
        auto& [route_prefix, prefix_length, next_hop, interface_num] = route_table[i];
        if(prefix_length < max_len)
          continue;
        // 前缀长度为0，表示默认路由，能够匹配所有地址
        if(prefix_length && ((dgram.header.dst ^ route_prefix) >> (32 - prefix_length)))
          continue;
        max_len = prefix_length;
        target_pos = i;
      }
      if(target_pos == -1)
        continue;
      RouteItem& route_item = route_table[target_pos];
      interfaces_[route_item.interface_num].send_datagram(dgram, 
                              route_item.next_hop.value_or(Address::from_ipv4_numeric(dgram.header.dst)));
    }
  }
}
