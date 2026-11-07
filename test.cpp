#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

struct Book {
  using qty = uint64_t;
  using price = uint64_t;
  using OrderId = uint64_t;
  using OrderInfo = std::pair<price, qty>;

  enum Side { BID, ASK };

  std::map<price, qty, std::greater<price>> bids;
  std::map<price, qty, std::less<price>> asks;

  std::unordered_map<OrderId, OrderInfo> bidOrders;
  std::unordered_map<OrderId, OrderInfo> askOrders;

  auto addOrder(const OrderId id, const price p, const qty q, const Side s)
      -> void {
    switch (s) {
    case BID: {
      bids[p] += q;
      bidOrders[id] = {p, q};
      break;
    }
    case ASK: {
      asks[p] += q;
      askOrders[id] = {p, q};
      break;
    }
    }
  }

  auto fill(const OrderId id, const qty q, const Side s) -> void {
    switch (s) {
    case BID: {
      if (!bidOrders.contains(id)) {
        return;
      }
      auto &data = bidOrders[id];
      if (q > data.second)
        return;
      auto &bid = bids[data.first];
      bid -= q;
      if (bid == 0) {
        bids.erase(data.first);
      }
      data.second -= q;
      if (data.second == 0) {
        bidOrders.erase(id);
      }
      break;
    }
    case ASK: {
      if (!askOrders.contains(id)) {
        return;
      }
      auto &data = askOrders[id];
      if (q > data.second)
        return;
      auto &ask = asks[data.first];
      ask -= q;
      if (ask == 0) {
        asks.erase(data.first);
      }
      data.second -= q;
      if (data.second == 0)
        askOrders.erase(id);
      break;
    }
    }
  }

  [[nodiscard]]
  auto bestBid(price &p) -> bool {
    if (bids.empty())
      return false;
    p = bids.begin()->first;
    return true;
  }

  [[nodiscard]]
  auto bestAsk(price &p) -> bool {
    if (asks.empty())
      return false;
    p = asks.begin()->first;
    return true;
  }

  auto cancelOrder(const OrderId id, const Side s) -> void {
    switch (s) {
    case BID: {
      if (!bidOrders.contains(id)) {
        return;
      }
      const auto &data = bidOrders[id];
      auto &bid = bids[data.first];
      bid -= data.second;
      if (bid == 0) {
        bids.erase(data.first);
      }
      bidOrders.erase(id);
      break;
    }
    case ASK: {
      if (!askOrders.contains(id)) {
        return;
      }
      const auto &data = askOrders[id];
      auto &ask = asks[data.first];
      ask -= data.second;
      if (ask == 0) {
        asks.erase(data.first);
      }
      askOrders.erase(id);
      break;
    }
    }
  }
};
