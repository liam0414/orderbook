#pragma once

#include <cstdint>

struct Trade {
    const std::uint64_t trade_id;
    const std::uint64_t buy_order_id;
    const std::uint64_t sell_order_id;
    const double price;
    const std::uint64_t quantity;
    const std::uint64_t timestamp;
    
    Trade(std::uint64_t t_id, std::uint64_t buy_id, std::uint64_t sell_id,
          double p, std::uint64_t qty, std::uint64_t ts) noexcept
        : trade_id(t_id), buy_order_id(buy_id), sell_order_id(sell_id),
          price(p), quantity(qty), timestamp(ts) {}
};