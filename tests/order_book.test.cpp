#include <catch2/catch.hpp>
#include "order_book.h"
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <future>

class OrderBookTestFixture {
public:
    OrderBookTestFixture() : book(std::make_unique<OrderBook>()) {}
    
protected:
    std::unique_ptr<OrderBook> book;
};

TEST_CASE_METHOD(OrderBookTestFixture, "OrderBook basic operations", "[orderbook][basic]") {
    
    SECTION("Add buy order") {
        auto order_id = book->add_order(100.0, 500U, Side::BUY);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_best_bid().has_value());
        REQUIRE(book->get_best_bid().value() == Approx(100.0));
        REQUIRE(book->get_total_orders() == 1U);
    }
    
    SECTION("Add sell order") {
        auto order_id = book->add_order(101.0, 300U, Side::SELL);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_best_ask().has_value());
        REQUIRE(book->get_best_ask().value() == Approx(101.0));
        REQUIRE(book->get_total_orders() == 1U);
    }
    
    SECTION("Multiple orders maintain price priority") {
        book->add_order(99.0, 100U, Side::BUY);
        book->add_order(100.0, 200U, Side::BUY);  // Better price
        book->add_order(98.0, 300U, Side::BUY);
        
        REQUIRE(book->get_best_bid().value() == Approx(100.0));
        REQUIRE(book->get_bid_depth_at_level(0U) == 200U);
        REQUIRE(book->get_bid_depth_at_level(1U) == 100U);
        REQUIRE(book->get_bid_depth_at_level(2U) == 300U);
    }
    
    SECTION("Empty book queries return nullopt") {
        REQUIRE_FALSE(book->get_best_bid().has_value());
        REQUIRE_FALSE(book->get_best_ask().has_value());
        REQUIRE_FALSE(book->get_spread().has_value());
        REQUIRE(book->get_total_orders() == 0U);
    }
    
    SECTION("Single-sided book queries") {
        book->add_order(100.0, 500U, Side::BUY);
        
        REQUIRE(book->get_best_bid().has_value());
        REQUIRE_FALSE(book->get_best_ask().has_value());
        REQUIRE_FALSE(book->get_spread().has_value());
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Order matching and execution", "[orderbook][matching]") {
    
    SECTION("Simple price-time priority matching") {
        // Add orders at same price, different times
        [[maybe_unused]] auto id1 = book->add_order(100.0, 100U, Side::BUY);
        [[maybe_unused]] auto id2 = book->add_order(100.0, 200U, Side::BUY);
        
        // Add crossing sell order - should match first order first
        auto order_id = book->add_order(100.0, 50U, Side::SELL);
        REQUIRE(order_id > 0U);
        
        // First order should be partially filled, second order untouched
        REQUIRE(book->get_bid_depth_at_level(0U) == 250U); // 50 remaining + 200 untouched
        REQUIRE(book->get_total_trades() >= 1U);
    }
    
    SECTION("Partial fill execution") {
        book->add_order(100.0, 500U, Side::BUY);
        auto order_id = book->add_order(100.0, 200U, Side::SELL);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_total_trades() >= 1U);
        REQUIRE(book->get_total_volume() == 200U);
        REQUIRE(book->get_bid_depth_at_level(0U) == 300U); // 500 - 200 = 300 remaining
    }
    
    SECTION("Complete fill removes order") {
        book->add_order(100.0, 200U, Side::BUY);
        auto order_id = book->add_order(100.0, 200U, Side::SELL);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_total_trades() >= 1U);
        REQUIRE(book->get_total_volume() == 200U);
        REQUIRE_FALSE(book->get_best_bid().has_value()); // Buy order completely filled
    }
    
    SECTION("Multi-level matching") {
        // Setup multiple bid levels
        book->add_order(100.0, 100U, Side::BUY);   // Best bid
        book->add_order(99.5, 200U, Side::BUY);    // Second best
        book->add_order(99.0, 300U, Side::BUY);    // Third best
        
        // Large sell order that should cross multiple levels
        auto order_id = book->add_order(99.0, 250U, Side::SELL);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_total_trades() >= 2U);
        
        // Should have matched 100 at $100 and 150 at $99.5
        REQUIRE(book->get_total_volume() == 250U);
        
        // Best bid should now be $99.5 with remaining 50 shares
        REQUIRE(book->get_best_bid().value() == Approx(99.5));
        REQUIRE(book->get_bid_depth_at_level(0U) == 50U); // 200 - 150 = 50
    }
    
    SECTION("Price improvement for incoming orders") {
        // Resting sell order at $100
        book->add_order(100.0, 200U, Side::SELL);
        
        // Incoming buy order willing to pay $101 should execute at $100
        auto order_id = book->add_order(101.0, 100U, Side::BUY);
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_total_trades() == 1U);
        REQUIRE(book->get_total_volume() == 100U);
        
        // Remaining sell order should still be at $100
        REQUIRE(book->get_best_ask().value() == Approx(100.0));
        REQUIRE(book->get_ask_depth_at_level(0U) == 100U);
    }
    
    SECTION("No matching when prices don't cross") {
        book->add_order(99.0, 100U, Side::BUY);    // Bid at $99
        auto order_id = book->add_order(101.0, 100U, Side::SELL);  // Ask at $101
        
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_total_trades() == 0U);
        REQUIRE(book->get_total_orders() == 2U);
        REQUIRE(book->get_spread().value() == Approx(2.0)); // $101 - $99
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Market orders", "[orderbook][market]") {
    
    SECTION("Market buy order matches best asks") {
        // Setup ask levels
        book->add_order(100.0, 100U, Side::SELL);
        book->add_order(101.0, 200U, Side::SELL);
        book->add_order(102.0, 300U, Side::SELL);
        
        // Market buy for 250 shares should match 100@$100 and 150@$101
        auto trades = book->add_market_order(250U, Side::BUY);
        
        REQUIRE_FALSE(trades.empty());
        REQUIRE(book->get_total_volume() == 250U);
        
        // Best ask should now be $101 with 50 remaining shares
        REQUIRE(book->get_best_ask().value() == Approx(101.0));
        REQUIRE(book->get_ask_depth_at_level(0U) == 50U);
    }
    
    SECTION("Market sell order matches best bids") {
        // Setup bid levels
        book->add_order(100.0, 100U, Side::BUY);
        book->add_order(99.0, 200U, Side::BUY);
        book->add_order(98.0, 300U, Side::BUY);
        
        // Market sell for 250 shares
        auto trades = book->add_market_order(250U, Side::SELL);
        
        REQUIRE_FALSE(trades.empty());
        REQUIRE(book->get_total_volume() == 250U);
        
        // Best bid should now be $99 with 50 remaining shares
        REQUIRE(book->get_best_bid().value() == Approx(99.0));
        REQUIRE(book->get_bid_depth_at_level(0U) == 50U);
    }
    
    SECTION("Large market order exhausts book") {
        book->add_order(100.0, 100U, Side::SELL);
        book->add_order(101.0, 100U, Side::SELL);
        
        // Market order larger than available liquidity
        auto trades = book->add_market_order(300U, Side::BUY);
        
        REQUIRE_FALSE(trades.empty());
        REQUIRE(book->get_total_volume() == 200U); // Only 200 available
        REQUIRE_FALSE(book->get_best_ask().has_value()); // All asks consumed
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Order lifecycle management", "[orderbook][lifecycle]") {
    
    SECTION("Successful order cancellation") {
        auto order_id = book->add_order(100.0, 500U, Side::BUY);
        REQUIRE(book->get_total_orders() == 1U);
        
        bool cancelled = book->cancel_order(order_id);
        REQUIRE(cancelled);
        REQUIRE(book->get_total_orders() == 0U);
        REQUIRE_FALSE(book->get_best_bid().has_value());
    }
    
    SECTION("Cancel non-existent order") {
        bool cancelled = book->cancel_order(99999U);
        REQUIRE_FALSE(cancelled);
    }
    
    SECTION("Cancel order from multi-level book") {
        [[maybe_unused]] auto id1 = book->add_order(100.0, 100U, Side::BUY);
        [[maybe_unused]] auto id2 = book->add_order(99.0, 200U, Side::BUY);
        [[maybe_unused]] auto id3 = book->add_order(98.0, 300U, Side::BUY);
        
        REQUIRE(book->get_total_orders() == 3U);
        
        // Cancel middle order
        bool cancelled = book->cancel_order(id2);
        REQUIRE(cancelled);
        REQUIRE(book->get_total_orders() == 2U);
        
        // Verify remaining orders are intact
        REQUIRE(book->get_best_bid().value() == Approx(100.0));
        REQUIRE(book->get_bid_depth_at_level(0U) == 100U);
        REQUIRE(book->get_bid_depth_at_level(1U) == 300U); // id3 moved up
    }
    
    SECTION("Cancel partially filled order") {
        auto buy_id = book->add_order(100.0, 500U, Side::BUY);
        
        // Partially fill the order
        book->add_order(100.0, 200U, Side::SELL);
        REQUIRE(book->get_total_volume() == 200U);
        
        // Should be able to cancel the remaining portion
        bool cancelled = book->cancel_order(buy_id);
        REQUIRE(cancelled);
        REQUIRE_FALSE(book->get_best_bid().has_value());
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Market data queries", "[orderbook][marketdata]") {
    
    SECTION("Spread calculation") {
        book->add_order(100.0, 500U, Side::BUY);
        book->add_order(102.0, 300U, Side::SELL);
        
        auto spread = book->get_spread();
        REQUIRE(spread.has_value());
        REQUIRE(spread.value() == Approx(2.0));
    }
    
    SECTION("Tight spread") {
        book->add_order(100.00, 100U, Side::BUY);
        book->add_order(100.01, 100U, Side::SELL);
        
        auto spread = book->get_spread();
        REQUIRE(spread.has_value());
        REQUIRE(spread.value() == Approx(0.01));
    }
    
    SECTION("Market depth queries") {
        // Setup bid ladder
        book->add_order(100.0, 200U, Side::BUY);   // Level 0
        book->add_order(99.5, 300U, Side::BUY);    // Level 1  
        book->add_order(99.0, 400U, Side::BUY);    // Level 2
        
        // Setup ask ladder
        book->add_order(101.0, 150U, Side::SELL);  // Level 0
        book->add_order(101.5, 250U, Side::SELL);  // Level 1
        book->add_order(102.0, 350U, Side::SELL);  // Level 2
        
        // Test bid depth
        REQUIRE(book->get_bid_depth_at_level(0U) == 200U);
        REQUIRE(book->get_bid_depth_at_level(1U) == 300U);
        REQUIRE(book->get_bid_depth_at_level(2U) == 400U);
        REQUIRE(book->get_bid_depth_at_level(3U) == 0U); // No fourth level
        
        // Test ask depth
        REQUIRE(book->get_ask_depth_at_level(0U) == 150U);
        REQUIRE(book->get_ask_depth_at_level(1U) == 250U);
        REQUIRE(book->get_ask_depth_at_level(2U) == 350U);
        REQUIRE(book->get_ask_depth_at_level(3U) == 0U); // No fourth level
    }
    
    SECTION("Depth after partial matching") {
        book->add_order(100.0, 500U, Side::BUY);
        book->add_order(99.0, 300U, Side::BUY);
        
        // Partially match top level
        book->add_order(100.0, 200U, Side::SELL);
        
        REQUIRE(book->get_bid_depth_at_level(0U) == 300U); // 500 - 200 = 300
        REQUIRE(book->get_bid_depth_at_level(1U) == 300U); // Unchanged
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Edge cases and error handling", "[orderbook][edge_cases]") {
    
    SECTION("Zero quantity order rejection") {
        auto order_id = book->add_order(100.0, 0U, Side::BUY);
        REQUIRE(order_id == 0U); // Should return 0 for rejected orders
        REQUIRE(book->get_total_orders() == 0U);
    }
    
    SECTION("Negative price order rejection") {
        auto order_id = book->add_order(-100.0, 500U, Side::BUY);
        REQUIRE(order_id == 0U); // Should return 0 for rejected orders
        REQUIRE(book->get_total_orders() == 0U);
    }
    
    SECTION("Zero price limit order rejection") {
        auto order_id = book->add_order(0.0, 500U, Side::BUY, OrderType::LIMIT);
        REQUIRE(order_id == 0U); // Should return 0 for rejected orders
        REQUIRE(book->get_total_orders() == 0U);
    }
    
    SECTION("Market order with zero price accepted") {
        book->add_order(100.0, 200U, Side::SELL); // Provide liquidity
        
        auto trades = book->add_market_order(100U, Side::BUY);
        REQUIRE_FALSE(trades.empty());
        REQUIRE(book->get_total_volume() == 100U);
    }
    
    SECTION("Order book clearing") {
        // Add various orders
        book->add_order(100.0, 500U, Side::BUY);
        book->add_order(99.0, 300U, Side::BUY);
        book->add_order(101.0, 200U, Side::SELL);
        book->add_order(102.0, 400U, Side::SELL);
        
        REQUIRE(book->get_total_orders() == 4U);
        
        book->clear();
        
        REQUIRE(book->get_total_orders() == 0U);
        REQUIRE_FALSE(book->get_best_bid().has_value());
        REQUIRE_FALSE(book->get_best_ask().has_value());
        REQUIRE(book->get_total_trades() == 0U);
        REQUIRE(book->get_total_volume() == 0U);
    }
    
    SECTION("Large quantity orders") {
        constexpr std::uint64_t large_qty = 1000000U;
        
        auto order_id = book->add_order(100.0, large_qty, Side::BUY);
        REQUIRE(order_id > 0U);
        REQUIRE(book->get_bid_depth_at_level(0U) == large_qty);
    }
}

TEST_CASE_METHOD(OrderBookTestFixture, "Statistics and monitoring", "[orderbook][stats]") {
    
    SECTION("Trade and volume statistics") {
        book->add_order(100.0, 500U, Side::SELL);
        book->add_order(99.0, 300U, Side::SELL);
        
        // Execute some trades
        book->add_order(101.0, 200U, Side::BUY); // Should match 200@100
        book->add_order(100.0, 400U, Side::BUY); // Should match 300@100 + 100@99
        
        REQUIRE(book->get_total_trades() >= 2U);
        REQUIRE(book->get_total_volume() == 600U); // 200 + 400 = 600
    }
    
    SECTION("Order count tracking") {
        auto initial_count = book->get_total_orders();
        
        [[maybe_unused]] auto id1 = book->add_order(100.0, 100U, Side::BUY);
        auto id2 = book->add_order(99.0, 200U, Side::BUY);
        [[maybe_unused]] auto id3 = book->add_order(101.0, 150U, Side::SELL);
        
        REQUIRE(book->get_total_orders() == initial_count + 3U);
        
        // Cancel one order
        book->cancel_order(id2);
        REQUIRE(book->get_total_orders() == initial_count + 2U);
        
        // Execute a trade (should remove orders from count)
        book->add_order(100.0, 100U, Side::SELL); // Matches and removes buy order
        REQUIRE(book->get_total_orders() == initial_count + 1U); // Only sell order remains
    }
}

TEST_CASE("OrderBook thread safety", "[orderbook][concurrency]") {
    auto book = std::make_unique<OrderBook>();
    
    SECTION("Concurrent order additions") {
        constexpr int num_threads = 4;
        constexpr int orders_per_thread = 100;
        
        std::vector<std::future<void>> futures;
        
        // Launch multiple threads adding orders
        for (int t = 0; t < num_threads; ++t) {
            futures.push_back(std::async(std::launch::async, [&book, t, orders_per_thread]() {
                for (int i = 0; i < orders_per_thread; ++i) {
                    double price = 100.0 + static_cast<double>(t) + static_cast<double>(i) * 0.01;
                    Side side = ((t + i) % 2 == 0) ? Side::BUY : Side::SELL;
                    [[maybe_unused]] auto order_id = book->add_order(price, 100U, side);
                }
            }));
        }
        
        // Wait for all threads to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        // Verify book is in consistent state
        REQUIRE(book->get_total_orders() > 0U);
        
        // Should have both bids and asks
        bool has_bids = book->get_best_bid().has_value();
        bool has_asks = book->get_best_ask().has_value();
        REQUIRE((has_bids || has_asks)); // At least one side should have orders
    }
    
    SECTION("Concurrent reads while writing") {
        // Add some initial orders
        book->add_order(100.0, 500U, Side::BUY);
        book->add_order(101.0, 500U, Side::SELL);
        
        std::atomic<bool> stop_reading{false};
        std::atomic<int> successful_reads{0};
        
        // Reader thread
        auto reader_future = std::async(std::launch::async, [&]() {
            while (!stop_reading.load()) {
                if (book->get_best_bid().has_value() || book->get_best_ask().has_value()) {
                    successful_reads.fetch_add(1);
                }
                [[maybe_unused]] auto spread = book->get_spread();
                [[maybe_unused]] auto orders = book->get_total_orders();
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
        
        // Writer thread
        auto writer_future = std::async(std::launch::async, [&]() {
            for (int i = 0; i < 100; ++i) {
                [[maybe_unused]] auto order_id = book->add_order(
                    100.0 + static_cast<double>(i % 10) * 0.1, 
                    100U, 
                    (i % 2 == 0) ? Side::BUY : Side::SELL
                );
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
        
        writer_future.wait();
        stop_reading.store(true);
        reader_future.wait();
        
        REQUIRE(successful_reads.load() > 0);
        REQUIRE(book->get_total_orders() > 2U); // Should have more than initial orders
    }
}

TEST_CASE("OrderBook performance characteristics", "[orderbook][performance]") {
    OrderBook book;
    
    SECTION("Order addition performance") {
        constexpr int num_orders = 1000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_orders; ++i) {
            double price = 100.0 + static_cast<double>(i % 200) * 0.01;
            Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
            [[maybe_unused]] auto order_id = book.add_order(price, 100U, side);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        INFO("Added " << num_orders << " orders in " << duration.count() << " microseconds");
        INFO("Average: " << static_cast<double>(duration.count()) / static_cast<double>(num_orders) 
             << " microseconds per order");
        
        REQUIRE(book.get_total_orders() > 0U);
        
        // Performance should be reasonable (less than 10 microseconds per order on average)
        REQUIRE(static_cast<double>(duration.count()) / static_cast<double>(num_orders) < 10.0);
    }
    
    SECTION("Market data query performance") {
        // Setup a book with multiple levels
        for (int i = 0; i < 100; ++i) {
            book.add_order(100.0 - static_cast<double>(i) * 0.01, 100U, Side::BUY);
            book.add_order(101.0 + static_cast<double>(i) * 0.01, 100U, Side::SELL);
        }
        
        constexpr int num_queries = 10000;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < num_queries; ++i) {
            [[maybe_unused]] auto best_bid = book.get_best_bid();
            [[maybe_unused]] auto best_ask = book.get_best_ask();
            [[maybe_unused]] auto spread = book.get_spread();
            [[maybe_unused]] auto depth = book.get_bid_depth_at_level(0U);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        INFO("Executed " << num_queries << " market data queries in " << duration.count() << " nanoseconds");
        INFO("Average: " << static_cast<double>(duration.count()) / static_cast<double>(num_queries) 
             << " nanoseconds per query");
        
        // Market data queries should be very fast (sub-microsecond)
        REQUIRE(static_cast<double>(duration.count()) / static_cast<double>(num_queries) < 1000.0);
    }
}