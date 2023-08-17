#include "reassembler.hh"

// using namespace std;

Reassembler::Reassembler(): cache(), _rcv_last(false), _bytes_stored(0), _last_byte_num(0) {}

void Reassembler::insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  std::pair<uint64_t, std::string> cur = std::make_pair(first_index, data);
  if (is_last_substring) {
    _rcv_last = true;
    _last_byte_num = std::max(_last_byte_num, first_index + data.size());
  }
  if (is_out_of_avail_cap(cur, output))
    return;

  cut_off(cur, output);
  merge(cur);
  uint64_t cur_first_id = cur.first, avail_first_id = output.bytes_pushed();
  if (cur_first_id == avail_first_id) {
    output.push(cur.second);
    if (_rcv_last && output.bytes_pushed() == _last_byte_num) {
      output.close();
      return;
    }
  } else {
    if (cur.second.size() == 0)
      return;
    cache.insert(cur);
    _bytes_stored += cur.second.size();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return _bytes_stored;
}

// a <= b
bool Reassembler::merge_two(std::pair<uint64_t, std::string>& a, std::pair<uint64_t, std::string>& b) {
  uint64_t aid = a.first, bid = b.first;
  std::string &astr = a.second, &bstr = b.second;
  if (aid + astr.size() >= bid) {
    uint64_t dist = aid + astr.size() - bid;
    if (dist < bstr.size())
      a.second = astr + bstr.substr(dist);
    return true;
  }
  return false;
}

void Reassembler::merge(std::pair<uint64_t, std::string>& cur) {
  if (cache.empty() || cur.second.size() == 0)  
    return;
  
  auto it = cache.lower_bound(cur);
  auto pre_it = cache.end();
  if (it != cache.begin())
    pre_it = std::prev(it);
  
  while (it != cache.end()) {
    std::pair<uint64_t, std::string> nxt = *it;
    if (!merge_two(cur, nxt))
      break;
    _bytes_stored -= it->second.size();
    cache.erase(it++);
  }
  
  if (pre_it == cache.end())
    return;
  std::pair<uint64_t, std::string> pre = *pre_it;
  if (merge_two(pre, cur)) {
    _bytes_stored -= pre_it->second.size();
    cache.erase(pre_it);
    cur = pre;
  }
}

void Reassembler::cut_off(std::pair<uint64_t, std::string>& cur, const Writer& output) {
  uint64_t left_border = output.bytes_pushed(), right_border = left_border + output.available_capacity();
  uint64_t tail = cur.first + cur.second.size();
  if (cur.first >= left_border && tail <= right_border)
    return;
  uint64_t l = left_border >= cur.first ? left_border - cur.first : uint64_t(0);
  if (l >= cur.second.size())
    cur.second = "";
  else {
    cur.first = cur.first + l;
    cur.second = cur.second.substr(l, std::min(tail, right_border) - cur.first);
  }
}

bool Reassembler::is_out_of_avail_cap(std::pair<uint64_t, std::string>& cur, const Writer& output) {
  uint64_t right_border = output.bytes_pushed() + output.available_capacity();
  return cur.first >= right_border;
}