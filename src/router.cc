#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;


IPTrie::IPTrie() {
  tree.push_back({false, {0, 0}, 0, 0});
}

void IPTrie::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num ) 
{
  size_t cur = 0;
  for (int i = 31, j = 0; i >= 0 && j < prefix_length; --i, ++j) {
    int bit = (route_prefix >> i) & 1;
    size_t nxt = tree[cur]._child[bit];
    if (nxt == 0) {
      tree[cur]._child[bit] = tree.size();
      tree.push_back({false, {0, 0}, 0, 0});
    }
    cur = tree[cur]._child[bit];
  }

  tree[cur]._interfaces_id = interface_num;
  if (next_hop.has_value()) {
    tree[cur]._type = NODETYPE_LAST;
    tree[cur]._next_hop_ip = next_hop.value().ipv4_numeric();
  } else {
    tree[cur]._type = NODETYPE_DIRECT;
  }
}

optional<node> IPTrie::query(uint32_t ip) {
  bool ok = false;
  size_t cur = 0, res_id = 0;
  if (tree[cur]._type == NODETYPE_LAST || tree[cur]._type == NODETYPE_DIRECT) {
    ok = true;
    res_id = cur;
  }
  
  for (int i = 31; i >= 0; --i) {
    int bit = (ip >> i) & 1;
    size_t nxt = tree[cur]._child[bit];
    if (nxt == 0) break;
    if (tree[nxt]._type == NODETYPE_LAST || tree[nxt]._type == NODETYPE_DIRECT) {
      ok = true;
      res_id = nxt;
    }
    cur = nxt;
  }
  if (ok) return tree[res_id];
  return {};
}


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
  trie.add_route(route_prefix, prefix_length, next_hop, interface_num);
}

void Router::route() {
  for (auto& itf : interfaces_) {
    auto ipdgram = itf.maybe_receive();
    if (ipdgram.has_value()) {
      InternetDatagram data = ipdgram.value();
      uint32_t dst_ip = data.header.dst;
      if (data.header.ttl > 0)  --data.header.ttl;
      // s.b bug! (don't forget to update the checksum after decrement the ttl)
      data.header.compute_checksum();

      if (data.header.ttl > 0) {
        auto nxt = trie.query(dst_ip);
        if (nxt.has_value()) {
          node nd = nxt.value();
          uint32_t nxt_hop_ip = nd._next_hop_ip;
          if (nd._type == IPTrie::NODETYPE_DIRECT) nxt_hop_ip = dst_ip;
          interface(nd._interfaces_id).send_datagram(data, Address::from_ipv4_numeric(nxt_hop_ip));
        }
      }
    }
  }
}
