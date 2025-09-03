#include "order_book.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

void print_separator() {
	std::cout << std::string(60, '=') << std::endl;
}

void demonstrate_basic_operations() {
	std::cout << "=== Order Book Engine Demo ===" << std::endl;

	OrderBook book;

	print_separator();
	std::cout << "1. Adding initial limit orders..." << std::endl;

	[[maybe_unused]] auto buy1 = book.add_order(100.0, 500U, Side::BUY);
	auto buy2 = book.add_order(99.5, 300U, Side::BUY);
	[[maybe_unused]] auto buy3 = book.add_order(99.0, 200U, Side::BUY);

	[[maybe_unused]] auto sell1 = book.add_order(101.0, 400U, Side::SELL);
	[[maybe_unused]] auto sell2 = book.add_order(101.5, 250U, Side::SELL);
	[[maybe_unused]] auto sell3 = book.add_order(102.0, 150U, Side::SELL);

	std::cout << "Added " << book.get_total_orders() << " orders" << std::endl;
	book.print_book();

	print_separator();
	std::cout << "2. Adding crossing limit order (Buy 250 @ $101.25)..." << std::endl;

	[[maybe_unused]] auto crossing_buy = book.add_order(101.25, 250U, Side::BUY);

	std::cout << "Order executed with " << book.get_total_trades() << " total trades" << std::endl;
	std::cout << "Total volume traded: " << book.get_total_volume() << std::endl;
	book.print_book();

	print_separator();
	std::cout << "3. Adding market order (Market Sell 150)..." << std::endl;

	auto market_trades = book.add_market_order(150U, Side::SELL);

	std::cout << "Market order generated " << market_trades.size() << " trades:" << std::endl;
	for (const auto& trade : market_trades) {
		std::cout << "  Trade: " << trade.quantity << " shares @ $" << std::fixed << std::setprecision(2) << trade.price
		          << " (Trade ID: " << trade.trade_id << ")" << std::endl;
	}

	book.print_book();

	print_separator();
	std::cout << "4. Order cancellation..." << std::endl;

	if (book.cancel_order(buy2)) {
		std::cout << "Successfully cancelled order " << buy2 << std::endl;
	}
	else {
		std::cout << "Failed to cancel order " << buy2 << std::endl;
	}

	book.print_book();

	print_separator();
	std::cout << "5. Market data queries..." << std::endl;

	if (auto best_bid = book.get_best_bid()) {
		std::cout << "Best Bid: $" << std::fixed << std::setprecision(2) << *best_bid << std::endl;
	}

	if (auto best_ask = book.get_best_ask()) {
		std::cout << "Best Ask: $" << std::fixed << std::setprecision(2) << *best_ask << std::endl;
	}

	if (auto spread = book.get_spread()) {
		std::cout << "Spread: $" << std::fixed << std::setprecision(2) << *spread << std::endl;
	}

	// Show depth
	std::cout << "\nBid Depth:" << std::endl;
	for (std::size_t level = 0U; level < 3U; ++level) {
		auto depth = book.get_bid_depth_at_level(level);
		if (depth > 0U) {
			std::cout << "  Level " << level << ": " << depth << " shares" << std::endl;
		}
	}

	std::cout << "\nAsk Depth:" << std::endl;
	for (std::size_t level = 0U; level < 3U; ++level) {
		auto depth = book.get_ask_depth_at_level(level);
		if (depth > 0U) {
			std::cout << "  Level " << level << ": " << depth << " shares" << std::endl;
		}
	}

	print_separator();
	std::cout << "6. Final Statistics:" << std::endl;
	std::cout << "Total Orders in Book: " << book.get_total_orders() << std::endl;
	std::cout << "Total Trades Executed: " << book.get_total_trades() << std::endl;
	std::cout << "Total Volume Traded: " << book.get_total_volume() << " shares" << std::endl;
}

void demonstrate_performance() {
	std::cout << "\n=== Performance Test ===" << std::endl;

	OrderBook book;
	constexpr int num_orders = 1000; // Reduced for demo

	auto start = std::chrono::high_resolution_clock::now();

	// Add orders with slight price variations to create realistic book
	for (int i = 0; i < num_orders; ++i) {
		double base_price = 100.0;
		double price_variation = static_cast<double>(i % 200) * 0.01; // 0-2 dollar range
		Side side = ((i % 2) == 0) ? Side::BUY : Side::SELL;

		if (side == Side::BUY) {
			[[maybe_unused]] auto order_id = book.add_order(base_price - price_variation, 100U, side);
		}
		else {
			[[maybe_unused]] auto order_id = book.add_order(base_price + price_variation, 100U, side);
		}
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	std::cout << "Performance Results:" << std::endl;
	std::cout << "Added " << num_orders << " orders in " << duration.count() << " microseconds" << std::endl;
	std::cout << "Average: " << static_cast<double>(duration.count()) / static_cast<double>(num_orders)
	          << " microseconds per order" << std::endl;
	std::cout << "Final book state:" << std::endl;
	std::cout << "  Orders in book: " << book.get_total_orders() << std::endl;
	std::cout << "  Total trades: " << book.get_total_trades() << std::endl;
	std::cout << "  Total volume: " << book.get_total_volume() << std::endl;
}

int main() {
	try {
		demonstrate_basic_operations();
		demonstrate_performance();

		std::cout << "\n=== Demo Complete ===" << std::endl;

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}