#include <gtest/gtest.h>
#include "tickforge/order_book/order_book.hpp"
#include "tickforge/domain/contract_instance.hpp"
#include "tickforge/domain/product_specification.hpp"

#include <vector>

using namespace tickforge::domain;
using namespace tickforge::order_book;

class OrderBookTest : public ::testing::Test {
protected:
    ProductSpecification spec{ProductSpecification::cme_es_outright()};
    ContractInstance contract{"ESM4", spec, 2024, 6, 20240621};

    OrderBook create_book() {
        return OrderBook(contract);
    }
};

TEST_F(OrderBookTest, InitialStateAndContractMetadata) {
    OrderBook book = create_book();

    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.order_count(), 0);
    EXPECT_EQ(book.level_count(Side::Buy), 0);
    EXPECT_EQ(book.level_count(Side::Sell), 0);
    EXPECT_EQ(book.total_level_count(), 0);
    EXPECT_FALSE(book.best_bid_price().has_value());
    EXPECT_FALSE(book.best_ask_price().has_value());
    EXPECT_EQ(book.best_bid_level(), nullptr);
    EXPECT_EQ(book.best_ask_level(), nullptr);
    EXPECT_EQ(book.contract().symbol(), "ESM4");
    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, BidAndAskSideOrdering) {
    OrderBook book = create_book();

    // Insert bids at 5000.00 (20000 ticks), 5002.50 (20010 ticks), 4998.00 (19992 ticks)
    EXPECT_TRUE(book.insert_order(Order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(5), 1)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(2), Side::Buy, PriceInTicks(20010), Quantity(10), 2)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(3), Side::Buy, PriceInTicks(19992), Quantity(15), 3)));

    // Bids must be ordered descending: Best Bid is highest (20010)
    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(book.best_bid_price().value(), PriceInTicks(20010));
    EXPECT_EQ(book.level_count(Side::Buy), 3);
    EXPECT_EQ(book.order_count(), 3);

    // Insert asks at 5005.00 (20020 ticks), 5004.00 (20016 ticks), 5008.00 (20032 ticks)
    EXPECT_TRUE(book.insert_order(Order(OrderId(4), Side::Sell, PriceInTicks(20020), Quantity(8), 4)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(5), Side::Sell, PriceInTicks(20016), Quantity(12), 5)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(6), Side::Sell, PriceInTicks(20032), Quantity(20), 6)));

    // Asks must be ordered ascending: Best Ask is lowest (20016)
    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(book.best_ask_price().value(), PriceInTicks(20016));
    EXPECT_EQ(book.level_count(Side::Sell), 3);
    EXPECT_EQ(book.total_level_count(), 6);
    EXPECT_EQ(book.order_count(), 6);

    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, MultipleOrdersAtSamePricePreservesFIFO) {
    OrderBook book = create_book();
    PriceInTicks price(20000);

    EXPECT_TRUE(book.insert_order(Order(OrderId(10), Side::Buy, price, Quantity(5), 1)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(11), Side::Buy, price, Quantity(10), 2)));
    EXPECT_TRUE(book.insert_order(Order(OrderId(12), Side::Buy, price, Quantity(15), 3)));

    EXPECT_EQ(book.level_count(Side::Buy), 1);
    EXPECT_EQ(book.order_count(), 3);

    const PriceLevel* level = book.find_level(Side::Buy, price);
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->order_count(), 3);
    EXPECT_EQ(level->total_quantity(), Quantity(30));

    // Verify FIFO traversal
    auto it = level->begin();
    ASSERT_NE(it, level->end());
    EXPECT_EQ(it->id(), OrderId(10));
    ++it;
    ASSERT_NE(it, level->end());
    EXPECT_EQ(it->id(), OrderId(11));
    ++it;
    ASSERT_NE(it, level->end());
    EXPECT_EQ(it->id(), OrderId(12));
    ++it;
    EXPECT_EQ(it, level->end());

    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, FindOrderById) {
    OrderBook book = create_book();
    PriceInTicks bid_price(20000);
    PriceInTicks ask_price(20004);

    book.insert_order(Order(OrderId(100), Side::Buy, bid_price, Quantity(10), 1));
    book.insert_order(Order(OrderId(200), Side::Sell, ask_price, Quantity(20), 2));

    const Order* ord1 = book.find_order(OrderId(100));
    ASSERT_NE(ord1, nullptr);
    EXPECT_EQ(ord1->id(), OrderId(100));
    EXPECT_EQ(ord1->side(), Side::Buy);
    EXPECT_EQ(ord1->price(), bid_price);
    EXPECT_EQ(ord1->remaining_quantity(), Quantity(10));

    const Order* ord2 = book.find_order(OrderId(200));
    ASSERT_NE(ord2, nullptr);
    EXPECT_EQ(ord2->id(), OrderId(200));
    EXPECT_EQ(ord2->side(), Side::Sell);
    EXPECT_EQ(ord2->price(), ask_price);
    EXPECT_EQ(ord2->remaining_quantity(), Quantity(20));

    EXPECT_EQ(book.find_order(OrderId(999)), nullptr);
}

TEST_F(OrderBookTest, RemoveOrderByIdAndLevelCleanup) {
    OrderBook book = create_book();
    PriceInTicks p1(20000);
    PriceInTicks p2(20004);

    book.insert_order(Order(OrderId(1), Side::Buy, p1, Quantity(10), 1));
    book.insert_order(Order(OrderId(2), Side::Buy, p1, Quantity(15), 2));
    book.insert_order(Order(OrderId(3), Side::Buy, p2, Quantity(20), 3));

    EXPECT_EQ(book.level_count(Side::Buy), 2);
    EXPECT_EQ(book.order_count(), 3);

    // Remove first order at p1
    EXPECT_TRUE(book.remove_order(OrderId(1)));
    EXPECT_EQ(book.order_count(), 2);
    EXPECT_EQ(book.find_order(OrderId(1)), nullptr);
    EXPECT_NE(book.find_level(Side::Buy, p1), nullptr);
    EXPECT_EQ(book.find_level(Side::Buy, p1)->order_count(), 1);
    EXPECT_TRUE(book.check_invariants());

    // Remove remaining order at p1 -> level p1 must disappear
    EXPECT_TRUE(book.remove_order(OrderId(2)));
    EXPECT_EQ(book.order_count(), 1);
    EXPECT_EQ(book.find_order(OrderId(2)), nullptr);
    EXPECT_EQ(book.find_level(Side::Buy, p1), nullptr);
    EXPECT_EQ(book.level_count(Side::Buy), 1);
    EXPECT_TRUE(book.check_invariants());

    // Remove order at p2 -> book becomes empty
    EXPECT_TRUE(book.remove_order(OrderId(3)));
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.level_count(Side::Buy), 0);
    EXPECT_FALSE(book.best_bid_price().has_value());
    EXPECT_TRUE(book.check_invariants());

    // Second removal of already removed order returns false
    EXPECT_FALSE(book.remove_order(OrderId(3)));
    EXPECT_FALSE(book.remove_order(OrderId(999)));
}

TEST_F(OrderBookTest, RejectDuplicateOrderId) {
    OrderBook book = create_book();

    book.insert_order(Order(OrderId(42), Side::Buy, PriceInTicks(20000), Quantity(10), 1));

    // Same side duplicate
    EXPECT_THROW(
        book.insert_order(Order(OrderId(42), Side::Buy, PriceInTicks(20000), Quantity(5), 2)),
        std::invalid_argument
    );

    // Opposite side duplicate
    EXPECT_THROW(
        book.insert_order(Order(OrderId(42), Side::Sell, PriceInTicks(20004), Quantity(5), 3)),
        std::invalid_argument
    );

    EXPECT_EQ(book.order_count(), 1);
    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, RejectInvalidOrderInputs) {
    OrderBook book = create_book();

    // Inactive order
    Order inactive_ord(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(10), 1);
    inactive_ord.cancel();
    EXPECT_THROW(book.insert_order(inactive_ord), std::invalid_argument);

    EXPECT_TRUE(book.empty());
    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, SideIndependenceAndTopLevelUpdates) {
    OrderBook book = create_book();

    book.insert_order(Order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(10), 1));
    book.insert_order(Order(OrderId(2), Side::Buy, PriceInTicks(20002), Quantity(10), 2));
    book.insert_order(Order(OrderId(3), Side::Sell, PriceInTicks(20008), Quantity(10), 3));
    book.insert_order(Order(OrderId(4), Side::Sell, PriceInTicks(20006), Quantity(10), 4));

    EXPECT_EQ(book.best_bid_price().value(), PriceInTicks(20002));
    EXPECT_EQ(book.best_ask_price().value(), PriceInTicks(20006));

    // Removing best bid updates top of bid book without affecting asks
    EXPECT_TRUE(book.remove_order(OrderId(2)));
    EXPECT_EQ(book.best_bid_price().value(), PriceInTicks(20000));
    EXPECT_EQ(book.best_ask_price().value(), PriceInTicks(20006));

    // Removing best ask updates top of ask book without affecting bids
    EXPECT_TRUE(book.remove_order(OrderId(4)));
    EXPECT_EQ(book.best_bid_price().value(), PriceInTicks(20000));
    EXPECT_EQ(book.best_ask_price().value(), PriceInTicks(20008));

    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, HighVolumeInsertionsAndRemovalsConsistency) {
    OrderBook book = create_book();
    constexpr uint64_t num_orders = 1000;

    // Insert orders spanning 50 price levels on both sides
    for (uint64_t i = 1; i <= num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        PriceInTicks price = (side == Side::Buy)
            ? PriceInTicks(20000 - static_cast<int64_t>((i % 50) * 4))
            : PriceInTicks(20100 + static_cast<int64_t>((i % 50) * 4));

        book.insert_order(Order(OrderId(i), side, price, Quantity(static_cast<uint32_t>(1 + (i % 10))), i));
    }

    EXPECT_EQ(book.order_count(), num_orders);
    EXPECT_TRUE(book.check_invariants());

    // Remove every other order
    for (uint64_t i = 1; i <= num_orders; i += 2) {
        EXPECT_TRUE(book.remove_order(OrderId(i)));
    }

    EXPECT_EQ(book.order_count(), num_orders / 2);
    EXPECT_TRUE(book.check_invariants());

    // Verify remaining orders can still be located
    for (uint64_t i = 2; i <= num_orders; i += 2) {
        const Order* ord = book.find_order(OrderId(i));
        ASSERT_NE(ord, nullptr);
        EXPECT_EQ(ord->id(), OrderId(i));
    }

    // Remove remaining orders
    for (uint64_t i = 2; i <= num_orders; i += 2) {
        EXPECT_TRUE(book.remove_order(OrderId(i)));
    }

    EXPECT_TRUE(book.empty());
    EXPECT_EQ(book.order_count(), 0);
    EXPECT_EQ(book.total_level_count(), 0);
    EXPECT_TRUE(book.check_invariants());
}

TEST_F(OrderBookTest, ExplicitRvalueOrderInsertion) {
    OrderBook book = create_book();
    PriceInTicks price(20000);
    Quantity qty(25);
    OrderId id(777);
    uint64_t seq = 42;

    Order order(id, Side::Buy, price, qty, seq);

    // Explicitly pass named Order object with std::move
    bool inserted = book.insert_order(std::move(order));
    EXPECT_TRUE(inserted);

    // Verify order is retrievable by its ID
    const Order* stored = book.find_order(id);
    ASSERT_NE(stored, nullptr);

    // Verify stored order has expected side, price, ID and remaining quantity
    EXPECT_EQ(stored->id(), id);
    EXPECT_EQ(stored->side(), Side::Buy);
    EXPECT_EQ(stored->price(), price);
    EXPECT_EQ(stored->remaining_quantity(), qty);
    EXPECT_EQ(stored->priority_sequence(), seq);

    // Verify correct price level and aggregate quantity are updated
    const PriceLevel* level = book.find_level(Side::Buy, price);
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->price(), price);
    EXPECT_EQ(level->order_count(), 1);
    EXPECT_EQ(level->total_quantity(), qty);
    EXPECT_EQ(level->front().id(), id);

    EXPECT_EQ(book.order_count(), 1);
    EXPECT_EQ(book.level_count(Side::Buy), 1);
    ASSERT_TRUE(book.best_bid_price().has_value());
    EXPECT_EQ(book.best_bid_price().value(), price);

    // Verify OrderBook invariants remain valid
    EXPECT_TRUE(book.check_invariants());
}
