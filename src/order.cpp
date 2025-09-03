#include "order.h"
#include <chrono>

Order::Order(std::uint64_t id, double price, std::uint64_t qty, Side side, OrderType type)
: order_id_(id)
, timestamp_(get_current_timestamp())
, price_(price)
, quantity_(qty)
, filled_qty_(0U)
, side_(side)
, type_(type)
, status_(OrderStatus::NEW) {
	if (qty == 0U) {
		throw std::invalid_argument("Order quantity cannot be zero");
	}

	if (price < 0.0) {
		throw std::invalid_argument("Order price cannot be negative");
	}

	// Market orders can have price of 0
	if (type == OrderType::LIMIT && price <= 0.0) {
		throw std::invalid_argument("Limit order price must be positive");
	}
}

void Order::fill(std::uint64_t qty) {
	if (qty == 0U) {
		return;
	}

	if (filled_qty_ + qty > quantity_) {
		throw std::invalid_argument("Cannot fill more than remaining quantity");
	}

	if (status_ == OrderStatus::CANCELLED || status_ == OrderStatus::FILLED) {
		throw std::logic_error("Cannot fill cancelled or already filled order");
	}

	filled_qty_ += qty;

	if (filled_qty_ == quantity_) {
		status_ = OrderStatus::FILLED;
	}
	else {
		status_ = OrderStatus::PARTIALLY_FILLED;
	}
}

void Order::cancel() noexcept {
	if (status_ == OrderStatus::NEW || status_ == OrderStatus::PARTIALLY_FILLED) {
		status_ = OrderStatus::CANCELLED;
	}
}

bool Order::is_fully_filled() const noexcept {
	return filled_qty_ == quantity_;
}

std::uint64_t Order::get_current_timestamp() noexcept {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                      std::chrono::high_resolution_clock::now().time_since_epoch())
	                                      .count());
}