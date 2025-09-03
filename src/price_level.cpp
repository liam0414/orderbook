#include "price_level.h"
#include <algorithm>

void PriceLevel::add_order(std::shared_ptr<Order> order) {
    if (!order) {
        return;
    }
    
    total_quantity_ += order->get_remaining_qty();
    orders_.push(order);
}

bool PriceLevel::remove_order(std::uint64_t order_id) {
    if (orders_.empty()) {
        return false;
    }
    
    std::queue<std::shared_ptr<Order>> temp_queue;
    bool found = false;
    
    while (!orders_.empty()) {
        auto order = orders_.front();
        orders_.pop();
        
        if (order && order->get_id() == order_id) {
            total_quantity_ -= order->get_remaining_qty();
            found = true;
        } else if (order) {
            temp_queue.push(order);
        }
    }
    
    orders_ = std::move(temp_queue);
    return found;
}

std::shared_ptr<Order> PriceLevel::get_front_order() {
    if (orders_.empty()) {
        return nullptr;
    }
    
    return orders_.front();
}

void PriceLevel::pop_front_order() {
    if (!orders_.empty()) {
        auto order = orders_.front();
        if (order) {
            total_quantity_ -= order->get_remaining_qty();
        }
        orders_.pop();
    }
}

void PriceLevel::update_quantity(std::uint64_t old_remaining, std::uint64_t new_remaining) noexcept {
    if (old_remaining >= new_remaining) {
        total_quantity_ -= (old_remaining - new_remaining);
    } else {
        total_quantity_ += (new_remaining - old_remaining);
    }
}