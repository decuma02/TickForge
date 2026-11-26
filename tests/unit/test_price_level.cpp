#include <gtest/gtest.h>
#include "tickforge/order_book/price_level.hpp"
#include <vector>

using namespace tickforge::domain;
using namespace tickforge::order_book;

class PriceLevelTest : public ::testing::Test {
protected:
    PriceInTicks price{500000}; // e.g. 5000.00
};

TEST_F(PriceLevelTest, ConstructionAndInitialState) {
    PriceLevel level(price);
    EXPECT_EQ(level.price(), price);
    EXPECT_EQ(level.total_quantity(), Quantity::zero());
    EXPECT_EQ(level.order_count(), 0);
    EXPECT_TRUE(level.empty());
    EXPECT_TRUE(level.check_invariants());

    EXPECT_THROW(static_cast<void>(level.front()), std::logic_error);
    EXPECT_THROW(static_cast<void>(level.back()), std::logic_error);

    // Invalid non-positive price level rejection
    EXPECT_THROW(PriceLevel(PriceInTicks(0)), std::invalid_argument);
    EXPECT_THROW(PriceLevel(PriceInTicks(-10)), std::invalid_argument);
}

TEST_F(PriceLevelTest, AppendSingleOrder) {
    PriceLevel level(price);
    Order ord(OrderId(1), Side::Buy, price, Quantity(10), 100);

    auto it = level.append(ord);
    EXPECT_FALSE(level.empty());
    EXPECT_EQ(level.order_count(), 1);
    EXPECT_EQ(level.total_quantity(), Quantity(10));
    EXPECT_EQ(it->id(), OrderId(1));
    EXPECT_EQ(level.front().id(), OrderId(1));
    EXPECT_EQ(level.back().id(), OrderId(1));
    EXPECT_TRUE(level.check_invariants());
}

TEST_F(PriceLevelTest, RejectMismatchedPriceOrInactiveOrder) {
    PriceLevel level(price);

    // Mismatched price
    Order diff_price_ord(OrderId(1), Side::Buy, PriceInTicks(500025), Quantity(10), 100);
    EXPECT_THROW(level.append(diff_price_ord), std::invalid_argument);

    // Inactive order
    Order inactive_ord(OrderId(2), Side::Buy, price, Quantity(5), 101);
    inactive_ord.cancel();
    EXPECT_THROW(level.append(inactive_ord), std::invalid_argument);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.total_quantity(), Quantity::zero());
}

TEST_F(PriceLevelTest, AppendMultipleOrdersPreservesFIFO) {
    PriceLevel level(price);

    Order o1(OrderId(1), Side::Buy, price, Quantity(5), 100);
    Order o2(OrderId(2), Side::Buy, price, Quantity(15), 101);
    Order o3(OrderId(3), Side::Buy, price, Quantity(20), 102);

    level.append(o1);
    level.append(o2);
    level.append(o3);

    EXPECT_EQ(level.order_count(), 3);
    EXPECT_EQ(level.total_quantity(), Quantity(40));
    EXPECT_TRUE(level.check_invariants());

    EXPECT_EQ(level.front().id(), OrderId(1));
    EXPECT_EQ(level.back().id(), OrderId(3));

    // Verify FIFO iteration order
    std::vector<OrderId> ids;
    for (const auto& ord : level) {
        ids.push_back(ord.id());
    }
    ASSERT_EQ(ids.size(), 3);
    EXPECT_EQ(ids[0], OrderId(1));
    EXPECT_EQ(ids[1], OrderId(2));
    EXPECT_EQ(ids[2], OrderId(3));
}

TEST_F(PriceLevelTest, EraseFrontOrder) {
    PriceLevel level(price);
    auto it1 = level.append(Order(OrderId(1), Side::Buy, price, Quantity(10), 100));
    level.append(Order(OrderId(2), Side::Buy, price, Quantity(20), 101));

    level.erase(it1);
    EXPECT_EQ(level.order_count(), 1);
    EXPECT_EQ(level.total_quantity(), Quantity(20));
    EXPECT_EQ(level.front().id(), OrderId(2));
    EXPECT_TRUE(level.check_invariants());
}

TEST_F(PriceLevelTest, EraseMiddleOrder) {
    PriceLevel level(price);
    level.append(Order(OrderId(1), Side::Buy, price, Quantity(10), 100));
    auto it2 = level.append(Order(OrderId(2), Side::Buy, price, Quantity(20), 101));
    level.append(Order(OrderId(3), Side::Buy, price, Quantity(30), 102));

    level.erase(it2);
    EXPECT_EQ(level.order_count(), 2);
    EXPECT_EQ(level.total_quantity(), Quantity(40));
    EXPECT_EQ(level.front().id(), OrderId(1));
    EXPECT_EQ(level.back().id(), OrderId(3));
    EXPECT_TRUE(level.check_invariants());
}

TEST_F(PriceLevelTest, EraseLastOrderAndEmptyLevel) {
    PriceLevel level(price);
    auto it1 = level.append(Order(OrderId(1), Side::Buy, price, Quantity(10), 100));

    level.erase(it1);
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.order_count(), 0);
    EXPECT_EQ(level.total_quantity(), Quantity::zero());
    EXPECT_TRUE(level.check_invariants());
}

TEST_F(PriceLevelTest, UpdateOrderRemainingQuantity) {
    PriceLevel level(price);
    auto it = level.append(Order(OrderId(1), Side::Buy, price, Quantity(10), 100));
    level.append(Order(OrderId(2), Side::Buy, price, Quantity(15), 101));
    EXPECT_EQ(level.total_quantity(), Quantity(25));

    // Simulate reduction of order 1 from 10 to 6
    Quantity old_qty = it->remaining_quantity();
    it->record_quantity_reduction(Quantity(6));
    level.update_order_remaining(old_qty, it->remaining_quantity());

    EXPECT_EQ(level.total_quantity(), Quantity(21));
    EXPECT_TRUE(level.check_invariants());

    // Simulate execution of 4 contracts on order 1
    old_qty = it->remaining_quantity();
    it->record_execution(Quantity(4));
    level.update_order_remaining(old_qty, it->remaining_quantity());

    EXPECT_EQ(it->remaining_quantity(), Quantity(2));
    EXPECT_EQ(level.total_quantity(), Quantity(17));
    EXPECT_TRUE(level.check_invariants());
}
