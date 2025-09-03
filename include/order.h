#pragma once

#include <cstdint>
#include <chrono>
#include <stdexcept>

enum class Side : std::uint8_t { 
    BUY, 
    SELL 
};

enum class OrderType : std::uint8_t { 
    MARKET, 
    LIMIT 
};

enum class OrderStatus : std::uint8_t { 
    NEW, 
    PARTIALLY_FILLED, 
    FILLED, 
    CANCELLED 
};

class Order {
private:
    std::uint64_t order_id_;
    std::uint64_t timestamp_;
    double price_;
    std::uint64_t quantity_;
    std::uint64_t filled_qty_;
    Side side_;
    OrderType type_;
    OrderStatus status_;

public:
    Order(std::uint64_t id, double price, std::uint64_t qty, Side side, OrderType type);
    
    // Getters
    [[nodiscard]] std::uint64_t get_id() const noexcept { return order_id_; }
    [[nodiscard]] double get_price() const noexcept { return price_; }
    [[nodiscard]] std::uint64_t get_quantity() const noexcept { return quantity_; }
    [[nodiscard]] std::uint64_t get_filled_qty() const noexcept { return filled_qty_; }
    [[nodiscard]] std::uint64_t get_remaining_qty() const noexcept { return quantity_ - filled_qty_; }
    [[nodiscard]] Side get_side() const noexcept { return side_; }
    [[nodiscard]] OrderType get_type() const noexcept { return type_; }
    [[nodiscard]] OrderStatus get_status() const noexcept { return status_; }
    [[nodiscard]] std::uint64_t get_timestamp() const noexcept { return timestamp_; }
    
    // State management
    void fill(std::uint64_t qty);
    void cancel() noexcept;
    [[nodiscard]] bool is_fully_filled() const noexcept;

private:
    [[nodiscard]] static std::uint64_t get_current_timestamp() noexcept;
};