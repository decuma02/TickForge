#include <gtest/gtest.h>
#include "tickforge/domain/order_id.hpp"
#include "tickforge/domain/side.hpp"
#include "tickforge/domain/price_in_ticks.hpp"
#include "tickforge/domain/quantity.hpp"
#include "tickforge/domain/product_specification.hpp"
#include "tickforge/domain/contract_instance.hpp"
#include "tickforge/domain/order.hpp"

using namespace tickforge::domain;

// ==============================================================================
// OrderId Tests
// ==============================================================================
TEST(OrderIdTest, DefaultAndExplicitConstruction) {
    OrderId default_id;
    EXPECT_FALSE(default_id.is_valid());
    EXPECT_EQ(default_id.value(), 0);

    OrderId valid_id(42);
    EXPECT_TRUE(valid_id.is_valid());
    EXPECT_EQ(valid_id.value(), 42);
    EXPECT_EQ(valid_id.to_string(), "42");
}

TEST(OrderIdTest, ComparisonAndHashing) {
    OrderId id1(100);
    OrderId id2(100);
    OrderId id3(200);

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
    EXPECT_LT(id1, id3);
    EXPECT_LE(id1, id2);
    EXPECT_GT(id3, id1);
    EXPECT_GE(id2, id1);

    std::hash<OrderId> hasher;
    EXPECT_EQ(hasher(id1), hasher(id2));
}

// ==============================================================================
// Side Tests
// ==============================================================================
TEST(SideTest, OppositeSideAndString) {
    EXPECT_EQ(opposite_side(Side::Buy), Side::Sell);
    EXPECT_EQ(opposite_side(Side::Sell), Side::Buy);

    EXPECT_EQ(to_string(Side::Buy), "BUY");
    EXPECT_EQ(to_string(Side::Sell), "SELL");
}

// ==============================================================================
// PriceInTicks Tests
// ==============================================================================
TEST(PriceInTicksTest, PositiveTicksValidation) {
    PriceInTicks p(20000); // 20,000 ticks
    EXPECT_EQ(p.ticks(), 20000);

    // Strictly positive validation: 0 and negative must throw
    EXPECT_THROW(PriceInTicks(0), std::invalid_argument);
    EXPECT_THROW(PriceInTicks(-5), std::invalid_argument);
}

TEST(PriceInTicksTest, ConversionFromIndexPoints) {
    // ES tick size: 0.25 (2500 e4)
    // 5000.25 index points -> 5000 * 4 + 1 = 20001 ticks
    PriceInTicks p = PriceInTicks::from_index_points(5000, 2500, 2500);
    EXPECT_EQ(p.ticks(), 20001);

    // 5000.50 index points -> 20002 ticks
    PriceInTicks p2 = PriceInTicks::from_index_points(5000, 5000, 2500);
    EXPECT_EQ(p2.ticks(), 20002);

    // Non-multiple fraction: 5000.10 is invalid for 0.25 tick size
    EXPECT_THROW((void)PriceInTicks::from_index_points(5000, 1000, 2500), std::invalid_argument);
}

TEST(PriceInTicksTest, ArithmeticAndOverflowProtection) {
    PriceInTicks p1(100);
    PriceInTicks p2(50);

    PriceInTicks sum = p1 + p2;
    EXPECT_EQ(sum.ticks(), 150);

    PriceInTicks diff = p1 - p2;
    EXPECT_EQ(diff.ticks(), 50);

    // Underflow check: subtraction resulting in negative must throw
    EXPECT_THROW((void)(p2 - p1), std::underflow_error);
}

// ==============================================================================
// Quantity Tests
// ==============================================================================
TEST(QuantityTest, ValidationAndZeroRejection) {
    Quantity q(10);
    EXPECT_EQ(q.lots(), 10);
    EXPECT_FALSE(q.is_zero());

    // Strictly positive: zero lot construction throws
    EXPECT_THROW(Quantity(0), std::invalid_argument);

    Quantity zero_q = Quantity::zero();
    EXPECT_TRUE(zero_q.is_zero());
    EXPECT_EQ(zero_q.lots(), 0);
}

TEST(QuantityTest, ArithmeticAndUnderflow) {
    Quantity q1(50);
    Quantity q2(20);

    Quantity sum = q1 + q2;
    EXPECT_EQ(sum.lots(), 70);

    Quantity diff = q1 - q2;
    EXPECT_EQ(diff.lots(), 30);

    // Underflow throws
    EXPECT_THROW((void)(q2 - q1), std::underflow_error);
}

// ==============================================================================
// ProductSpecification Tests
// ==============================================================================
TEST(ProductSpecificationTest, CMEGlobexESOutright) {
    auto es_spec = ProductSpecification::cme_es_outright();
    EXPECT_EQ(es_spec.commodity_code(), "ES");
    EXPECT_EQ(es_spec.name(), "E-mini S&P 500 Index Futures");
    EXPECT_EQ(es_spec.tick_size_e4(), 2500); // 0.25
    EXPECT_EQ(es_spec.contract_multiplier(), 50); // $50/pt
    EXPECT_EQ(es_spec.min_trade_vol(), Quantity(1));
    EXPECT_EQ(es_spec.max_trade_vol(), Quantity(2999));

    // Quantity validation against product limits
    EXPECT_TRUE(es_spec.validate_quantity(Quantity(1)));
    EXPECT_TRUE(es_spec.validate_quantity(Quantity(2999)));
    EXPECT_TRUE(es_spec.validate_quantity(Quantity(500)));
    EXPECT_FALSE(es_spec.validate_quantity(Quantity(3000)));

    // Price conversion via spec
    PriceInTicks p = es_spec.to_price_in_ticks(5100, 7500); // 5100.75
    EXPECT_EQ(p.ticks(), 5100 * 4 + 3);
}

TEST(ProductSpecificationTest, InvalidProductSpecsThrow) {
    EXPECT_THROW(ProductSpecification("", "Invalid", 2500, 50, Quantity(1), Quantity(100)), std::invalid_argument);
    EXPECT_THROW(ProductSpecification("ES", "Invalid", 0, 50, Quantity(1), Quantity(100)), std::invalid_argument);
    EXPECT_THROW(ProductSpecification("ES", "Invalid", 2500, 0, Quantity(1), Quantity(100)), std::invalid_argument);
    EXPECT_THROW(ProductSpecification("ES", "Invalid", 2500, 50, Quantity(100), Quantity(10)), std::invalid_argument);
}

// ==============================================================================
// ContractInstance Tests
// ==============================================================================
TEST(ContractInstanceTest, ValidContractConstruction) {
    auto es_spec = ProductSpecification::cme_es_outright();
    ContractInstance esm4("ESM4", es_spec, 2024, 6, 20240621);

    EXPECT_EQ(esm4.symbol(), "ESM4");
    EXPECT_EQ(esm4.commodity_code(), "ES");
    EXPECT_EQ(esm4.maturity_year(), 2024);
    EXPECT_EQ(esm4.maturity_month(), 6);
    EXPECT_EQ(esm4.expiration_date(), 20240621);
    EXPECT_TRUE(esm4.is_tradable());
    EXPECT_EQ(esm4.status(), ContractTradingStatus::ContinuousTrading);

    esm4.set_status(ContractTradingStatus::Halted);
    EXPECT_FALSE(esm4.is_tradable());
    EXPECT_EQ(esm4.status(), ContractTradingStatus::Halted);
}

TEST(ContractInstanceTest, ValidationFailures) {
    auto es_spec = ProductSpecification::cme_es_outright();
    EXPECT_THROW(ContractInstance("", es_spec, 2024, 6, 20240621), std::invalid_argument);
    EXPECT_THROW(ContractInstance("ESM4", es_spec, 2024, 13, 20240621), std::invalid_argument);
    EXPECT_THROW(ContractInstance("ESM4", es_spec, 1960, 6, 20240621), std::invalid_argument);
}

// ==============================================================================
// Order & Accounting Invariant Tests
// ==============================================================================
TEST(OrderTest, InitialStateAndAccountingInvariant) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 1);

    EXPECT_EQ(order.id(), OrderId(1));
    EXPECT_EQ(order.side(), Side::Buy);
    EXPECT_EQ(order.price(), PriceInTicks(20000));
    EXPECT_EQ(order.initial_quantity(), Quantity(100));
    EXPECT_EQ(order.remaining_quantity(), Quantity(100));
    EXPECT_EQ(order.executed_quantity(), Quantity::zero());
    EXPECT_EQ(order.reduced_quantity(), Quantity::zero());
    EXPECT_EQ(order.cancelled_quantity(), Quantity::zero());
    EXPECT_EQ(order.priority_sequence(), 1);
    EXPECT_EQ(order.status(), OrderStatus::New);
    EXPECT_TRUE(order.is_active());
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, PartialAndFullExecutions) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 1);

    // Partial fill of 40 lots
    order.record_execution(Quantity(40));
    EXPECT_EQ(order.remaining_quantity(), Quantity(60));
    EXPECT_EQ(order.executed_quantity(), Quantity(40));
    EXPECT_EQ(order.status(), OrderStatus::PartiallyFilled);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Remaining fill of 60 lots
    order.record_execution(Quantity(60));
    EXPECT_EQ(order.remaining_quantity(), Quantity::zero());
    EXPECT_EQ(order.executed_quantity(), Quantity(100));
    EXPECT_EQ(order.status(), OrderStatus::Filled);
    EXPECT_TRUE(order.is_filled());
    EXPECT_FALSE(order.is_active());
    EXPECT_TRUE(order.check_quantity_invariant());

    // Excess execution throws
    EXPECT_THROW(order.record_execution(Quantity(1)), std::invalid_argument);
}

TEST(OrderTest, ModifyQuantityReductionRetainsPriority) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 10);

    // Reduce working quantity from 100 to 70 lots
    order.record_quantity_reduction(Quantity(70));
    EXPECT_EQ(order.remaining_quantity(), Quantity(70));
    EXPECT_EQ(order.reduced_quantity(), Quantity(30));
    EXPECT_EQ(order.initial_quantity(), Quantity(100));
    // Priority sequence unchanged!
    EXPECT_EQ(order.priority_sequence(), 10);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Invariant holds after partial execution and then reduction:
    order.record_execution(Quantity(20)); // remaining: 50, executed: 20, reduced: 30
    EXPECT_EQ(order.remaining_quantity(), Quantity(50));
    EXPECT_EQ(order.executed_quantity(), Quantity(20));
    EXPECT_TRUE(order.check_quantity_invariant());

    order.record_quantity_reduction(Quantity(35)); // remaining: 35, executed: 20, reduced: 45
    EXPECT_EQ(order.remaining_quantity(), Quantity(35));
    EXPECT_EQ(order.reduced_quantity(), Quantity(45));
    EXPECT_EQ(order.priority_sequence(), 10);
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, ModifyQuantityIncreaseLosesPriority) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 10);

    // Increase working quantity from 100 to 150 lots with new sequence number 99
    order.record_quantity_increase(Quantity(150), 99);
    EXPECT_EQ(order.remaining_quantity(), Quantity(150));
    EXPECT_EQ(order.initial_quantity(), Quantity(150));
    EXPECT_EQ(order.priority_sequence(), 99);
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, CancellationAccounting) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 10);

    order.record_execution(Quantity(30));
    order.record_quantity_reduction(Quantity(50)); // reduced by 20; remaining is 50
    EXPECT_EQ(order.remaining_quantity(), Quantity(50));

    order.cancel();
    EXPECT_EQ(order.remaining_quantity(), Quantity::zero());
    EXPECT_EQ(order.executed_quantity(), Quantity(30));
    EXPECT_EQ(order.reduced_quantity(), Quantity(20));
    EXPECT_EQ(order.cancelled_quantity(), Quantity(50));
    EXPECT_TRUE(order.is_cancelled());
    EXPECT_FALSE(order.is_active());
    // 30 executed + 50 cancelled + 20 reduced == 100 initial
    EXPECT_TRUE(order.check_quantity_invariant());
}
