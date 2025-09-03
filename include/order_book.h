#pragma once

#include "order.h"
#include "trade.h"
#include "price_level.h"
#include <map>
#include <unordered_map>
#include <memory>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <cstddef>

class OrderBook {
private:
    // Price-ordered containers - bids descending, asks ascending
    std::map<double, PriceLevel, std::greater<double>> bids_;
    std::map<double, PriceLevel, std::less<double>> asks_;
    
    // Fast order lookup
    std::unordered_map<std::uint64_t, std::shared_ptr<Order>> order_map_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // Internal counters
    std::uint64_t next_order_id_;
    std::uint64_t next_trade_id_;
    std::uint64_t total_trades_;
    std::uint64_t total_volume_;
    
    // Helper methods
    std::vector<Trade> match_order(std::shared_ptr<Order> order);
    void process_matches_at_level(std::shared_ptr<Order> incoming_order, 
                                 PriceLevel& level, 
                                 double trade_price, 
                                 std::vector<Trade>& trades);
    [[nodiscard]] bool can_match(const Order& order) const noexcept;
    [[nodiscard]] static std::uint64_t get_timestamp() noexcept;

public:
    OrderBook() noexcept;
    ~OrderBook() = default;
    
    // Non-copyable, non-movable (for thread safety)
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;
    
    // Core operations
    std::uint64_t add_order(double price, std::uint64_t quantity, Side side, 
                           OrderType type = OrderType::LIMIT);
    bool cancel_order(std::uint64_t order_id);
    std::vector<Trade> add_market_order(std::uint64_t quantity, Side side);
    
    // Market data queries (thread-safe reads)
    [[nodiscard]] std::optional<double> get_best_bid() const;
    [[nodiscard]] std::optional<double> get_best_ask() const;
    [[nodiscard]] std::optional<double> get_spread() const;
    [[nodiscard]] std::uint64_t get_bid_depth_at_level(std::size_t level) const;
    [[nodiscard]] std::uint64_t get_ask_depth_at_level(std::size_t level) const;
    
    // Statistics
    [[nodiscard]] std::size_t get_total_orders() const;
    [[nodiscard]] std::uint64_t get_total_trades() const noexcept { return total_trades_; }
    [[nodiscard]] std::uint64_t get_total_volume() const noexcept { return total_volume_; }
    
    // Utility
    void clear();
    void print_book(std::size_t levels = 5U) const;
};