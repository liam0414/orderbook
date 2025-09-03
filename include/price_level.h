#pragma once

#include "order.h"
#include <queue>
#include <memory>
#include <cstddef>

class PriceLevel {
private:
    double price_;
    std::uint64_t total_quantity_;
    std::queue<std::shared_ptr<Order>> orders_;

public:
    // Default constructor for std::map::operator[]
    PriceLevel() noexcept : price_(0.0), total_quantity_(0U) {}
    
    // Explicit constructor with price
    explicit PriceLevel(double price) noexcept : price_(price), total_quantity_(0U) {}
    
    // Order management
    void add_order(std::shared_ptr<Order> order);
    bool remove_order(std::uint64_t order_id);
    [[nodiscard]] std::shared_ptr<Order> get_front_order();
    void pop_front_order();
    
    // Queries
    [[nodiscard]] bool empty() const noexcept { return orders_.empty(); }
    [[nodiscard]] double get_price() const noexcept { return price_; }
    [[nodiscard]] std::uint64_t get_total_quantity() const noexcept { return total_quantity_; }
    [[nodiscard]] std::size_t get_order_count() const noexcept { return orders_.size(); }
    
    // Update quantity when orders are partially filled
    void update_quantity(std::uint64_t old_remaining, std::uint64_t new_remaining) noexcept;
    
    // Set price (for default constructed objects)
    void set_price(double price) noexcept { price_ = price; }
};