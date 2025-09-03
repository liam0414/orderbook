#include "order_book.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>

OrderBook::OrderBook() noexcept
: next_order_id_(1U)
, next_trade_id_(1U)
, total_trades_(0U)
, total_volume_(0U) {}

std::uint64_t OrderBook::add_order(double price, std::uint64_t quantity, Side side, OrderType type) {
	if (quantity == 0U) {
		return 0U; // Reject zero quantity orders
	}

	if (price < 0.0) {
		return 0U; // Reject negative price orders
	}

	if (type == OrderType::LIMIT && price <= 0.0) {
		return 0U; // Reject zero/negative price limit orders
	}

	std::unique_lock<std::shared_mutex> lock(mutex_);

	auto order = std::make_shared<Order>(next_order_id_++, price, quantity, side, type);
	std::uint64_t order_id = order->get_id();

	// Try to match the order first
	auto trades = match_order(order);

	// If order has remaining quantity and is a limit order, add to book
	if (order->get_remaining_qty() > 0U && type == OrderType::LIMIT && order->get_status() != OrderStatus::CANCELLED) {
		order_map_[order_id] = order;

		if (side == Side::BUY) {
			if (bids_.find(price) == bids_.end()) {
				bids_[price] = PriceLevel(price); // Explicit construction
			}
			bids_[price].add_order(order);
		}
		else {
			if (asks_.find(price) == asks_.end()) {
				asks_[price] = PriceLevel(price); // Explicit construction
			}
			asks_[price].add_order(order);
		}
	}

	// Update statistics
	total_trades_ += trades.size();
	for (const auto& trade : trades) {
		total_volume_ += trade.quantity;
	}

	return order_id;
}

std::vector<Trade> OrderBook::add_market_order(std::uint64_t quantity, Side side) {
	if (quantity == 0U) {
		return {};
	}

	std::unique_lock<std::shared_mutex> lock(mutex_);

	auto order = std::make_shared<Order>(next_order_id_++, 0.0, quantity, side, OrderType::MARKET);

	auto trades = match_order(order);

	// Update statistics
	total_trades_ += trades.size();
	for (const auto& trade : trades) {
		total_volume_ += trade.quantity;
	}

	return trades;
}

bool OrderBook::cancel_order(std::uint64_t order_id) {
	std::unique_lock<std::shared_mutex> lock(mutex_);

	auto it = order_map_.find(order_id);
	if (it == order_map_.end()) {
		return false;
	}

	auto order = it->second;
	order->cancel();

	// Remove from price level
	bool removed = false;
	if (order->get_side() == Side::BUY) {
		auto bid_it = bids_.find(order->get_price());
		if (bid_it != bids_.end()) {
			removed = bid_it->second.remove_order(order_id);
			if (bid_it->second.empty()) {
				bids_.erase(bid_it);
			}
		}
	}
	else {
		auto ask_it = asks_.find(order->get_price());
		if (ask_it != asks_.end()) {
			removed = ask_it->second.remove_order(order_id);
			if (ask_it->second.empty()) {
				asks_.erase(ask_it);
			}
		}
	}

	order_map_.erase(it);
	return removed;
}

std::vector<Trade> OrderBook::match_order(std::shared_ptr<Order> incoming_order) {
	std::vector<Trade> trades;

	if (!incoming_order) {
		return trades;
	}

	// Handle the different map types separately to avoid ternary operator type mismatch
	if (incoming_order->get_side() == Side::BUY) {
		// Buy order matches against asks (ascending price order)
		while (!asks_.empty() && incoming_order->get_remaining_qty() > 0U) {
			auto& best_level = asks_.begin()->second;

			// Check if buy order can match at this ask price
			if (incoming_order->get_type() == OrderType::MARKET || incoming_order->get_price() >= asks_.begin()->first) {
				// Process matches at this price level
				process_matches_at_level(incoming_order, best_level, asks_.begin()->first, trades);

				// Remove empty price level
				if (best_level.empty()) {
					asks_.erase(asks_.begin());
				}
			}
			else {
				break; // No more matches possible
			}
		}
	}
	else {
		// Sell order matches against bids (descending price order)
		while (!bids_.empty() && incoming_order->get_remaining_qty() > 0U) {
			auto& best_level = bids_.begin()->second;

			// Check if sell order can match at this bid price
			if (incoming_order->get_type() == OrderType::MARKET || incoming_order->get_price() <= bids_.begin()->first) {
				// Process matches at this price level
				process_matches_at_level(incoming_order, best_level, bids_.begin()->first, trades);

				// Remove empty price level
				if (best_level.empty()) {
					bids_.erase(bids_.begin());
				}
			}
			else {
				break; // No more matches possible
			}
		}
	}

	return trades;
}

void OrderBook::process_matches_at_level(std::shared_ptr<Order> incoming_order,
                                         PriceLevel& level,
                                         double trade_price,
                                         std::vector<Trade>& trades) {
	while (!level.empty() && incoming_order->get_remaining_qty() > 0U) {
		auto resting_order = level.get_front_order();
		if (!resting_order) {
			level.pop_front_order();
			continue;
		}

		std::uint64_t trade_qty = std::min(incoming_order->get_remaining_qty(), resting_order->get_remaining_qty());

		std::uint64_t trade_timestamp = get_timestamp();

		// Create trade
		Trade trade(next_trade_id_++,
		            (incoming_order->get_side() == Side::BUY) ? incoming_order->get_id() : resting_order->get_id(),
		            (incoming_order->get_side() == Side::SELL) ? incoming_order->get_id() : resting_order->get_id(),
		            trade_price,
		            trade_qty,
		            trade_timestamp);

		// Update orders
		std::uint64_t old_resting_remaining = resting_order->get_remaining_qty();
		incoming_order->fill(trade_qty);
		resting_order->fill(trade_qty);

		// Update price level quantity
		level.update_quantity(old_resting_remaining, resting_order->get_remaining_qty());

		trades.push_back(trade);

		// Remove fully filled resting order
		if (resting_order->is_fully_filled()) {
			level.pop_front_order();
			order_map_.erase(resting_order->get_id());
		}
	}
}

bool OrderBook::can_match(const Order& order) const noexcept {
	if (order.get_type() == OrderType::MARKET) {
		return true; // Market orders match at any price
	}

	if (order.get_side() == Side::BUY && !asks_.empty()) {
		return order.get_price() >= asks_.begin()->first;
	}
	else if (order.get_side() == Side::SELL && !bids_.empty()) {
		return order.get_price() <= bids_.begin()->first;
	}

	return false;
}

std::optional<double> OrderBook::get_best_bid() const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	return bids_.empty() ? std::nullopt : std::make_optional(bids_.begin()->first);
}

std::optional<double> OrderBook::get_best_ask() const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	return asks_.empty() ? std::nullopt : std::make_optional(asks_.begin()->first);
}

std::optional<double> OrderBook::get_spread() const {
	std::shared_lock<std::shared_mutex> lock(mutex_);

	if (bids_.empty() || asks_.empty()) {
		return std::nullopt;
	}

	return asks_.begin()->first - bids_.begin()->first;
}

std::uint64_t OrderBook::get_bid_depth_at_level(std::size_t level) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);

	if (level >= bids_.size()) {
		return 0U;
	}

	auto it = bids_.begin();
	std::advance(it, static_cast<std::ptrdiff_t>(level));
	return it->second.get_total_quantity();
}

std::uint64_t OrderBook::get_ask_depth_at_level(std::size_t level) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);

	if (level >= asks_.size()) {
		return 0U;
	}

	auto it = asks_.begin();
	std::advance(it, static_cast<std::ptrdiff_t>(level));
	return it->second.get_total_quantity();
}

std::size_t OrderBook::get_total_orders() const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	return order_map_.size();
}

void OrderBook::clear() {
	std::unique_lock<std::shared_mutex> lock(mutex_);

	bids_.clear();
	asks_.clear();
	order_map_.clear();
	total_trades_ = 0U;
	total_volume_ = 0U;
}

void OrderBook::print_book(std::size_t levels) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);

	std::cout << "=== ORDER BOOK ===" << std::endl;
	std::cout << "BIDS" << std::setw(20) << "ASKS" << std::endl;

	auto bid_it = bids_.begin();
	auto ask_it = asks_.begin();

	for (std::size_t i = 0U; i < levels && (bid_it != bids_.end() || ask_it != asks_.end()); ++i) {
		// Print bid side
		if (bid_it != bids_.end()) {
			std::cout << std::fixed << std::setprecision(2) << bid_it->second.get_total_quantity() << "@"
			          << bid_it->first;
			++bid_it;
		}
		else {
			std::cout << std::setw(12) << "";
		}

		std::cout << std::setw(8) << "";

		// Print ask side
		if (ask_it != asks_.end()) {
			std::cout << std::fixed << std::setprecision(2) << ask_it->second.get_total_quantity() << "@"
			          << ask_it->first;
			++ask_it;
		}

		std::cout << std::endl;
	}

	// Print statistics
	std::cout << "\nStatistics:" << std::endl;
	std::cout << "Total Orders: " << order_map_.size() << std::endl;
	std::cout << "Total Trades: " << total_trades_ << std::endl;
	std::cout << "Total Volume: " << total_volume_ << std::endl;

	if (auto spread = get_spread()) {
		std::cout << "Spread: $" << std::fixed << std::setprecision(2) << *spread << std::endl;
	}
}

std::uint64_t OrderBook::get_timestamp() noexcept {
	return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
	                                      std::chrono::high_resolution_clock::now().time_since_epoch())
	                                      .count());
}