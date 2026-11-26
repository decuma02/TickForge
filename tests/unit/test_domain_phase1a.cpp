#include <gtest/gtest.h>
#include "tickforge/domain/order_id.hpp"
#include "tickforge/domain/side.hpp"
#include "tickforge/domain/price_in_ticks.hpp"
#include "tickforge/domain/quantity.hpp"
#include "tickforge/domain/product_specification.hpp"
#include "tickforge/domain/contract_instance.hpp"
#include "tickforge/domain/order.hpp"
#include <limits>
#include <cmath>

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
TEST(PriceInTicksTest, GeneralSignedIntegerTicksRepresentation) {
    // PriceInTicks as a general type allows positive, zero, and negative ticks
    PriceInTicks p_pos(20000);
    EXPECT_EQ(p_pos.ticks(), 20000);

    PriceInTicks p_zero(0);
    EXPECT_EQ(p_zero.ticks(), 0);

    PriceInTicks p_neg(-50);
    EXPECT_EQ(p_neg.ticks(), -50);

    EXPECT_EQ(PriceInTicks::zero().ticks(), 0);
    EXPECT_EQ(PriceInTicks::from_raw(-100).ticks(), -100);
}

TEST(PriceInTicksTest, ConversionFromIndexPoints) {
    // ES tick size: 0.25 (2500 e4)
    // 5000.25 index points -> 5000 * 4 + 1 = 20001 ticks
    PriceInTicks p = PriceInTicks::from_index_points(5000, 2500, 2500);
    EXPECT_EQ(p.ticks(), 20001);

    // 5000.50 index points -> 20002 ticks
    PriceInTicks p2 = PriceInTicks::from_index_points(5000, 5000, 2500);
    EXPECT_EQ(p2.ticks(), 20002);

    // Negative points support: -10.25 index points -> -41 ticks
    PriceInTicks p_neg = PriceInTicks::from_index_points(-10, 2500, 2500);
    EXPECT_EQ(p_neg.ticks(), -41);

    // Non-multiple fraction: 5000.10 is invalid for 0.25 tick size
    EXPECT_THROW((void)PriceInTicks::from_index_points(5000, 1000, 2500), std::invalid_argument);

    // Invalid parameters
    EXPECT_THROW((void)PriceInTicks::from_index_points(5000, 2500, 0), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_index_points(5000, 10000, 2500), std::invalid_argument);
}

TEST(PriceInTicksTest, ConversionFromDouble) {
    // Exact grid points
    PriceInTicks p1 = PriceInTicks::from_double(5000.25, 0.25);
    EXPECT_EQ(p1.ticks(), 20001);

    PriceInTicks p2 = PriceInTicks::from_double(5000.50, 0.25);
    EXPECT_EQ(p2.ticks(), 20002);

    // Negative price
    PriceInTicks p_neg = PriceInTicks::from_double(-10.25, 0.25);
    EXPECT_EQ(p_neg.ticks(), -41);

    // Off-grid price must throw std::invalid_argument (no silent rounding!)
    EXPECT_THROW((void)PriceInTicks::from_double(5000.10, 0.25), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_double(5000.2505, 0.25), std::invalid_argument);

    // NaN and Inf must throw
    EXPECT_THROW((void)PriceInTicks::from_double(std::numeric_limits<double>::quiet_NaN(), 0.25), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_double(std::numeric_limits<double>::infinity(), 0.25), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_double(-std::numeric_limits<double>::infinity(), 0.25), std::invalid_argument);

    // Invalid tick size
    EXPECT_THROW((void)PriceInTicks::from_double(5000.0, 0.0), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_double(5000.0, -0.25), std::invalid_argument);
    EXPECT_THROW((void)PriceInTicks::from_double(5000.0, std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}

TEST(PriceInTicksTest, ArithmeticAndOverflowProtection) {
    PriceInTicks p1(100);
    PriceInTicks p2(50);

    PriceInTicks sum = p1 + p2;
    EXPECT_EQ(sum.ticks(), 150);

    PriceInTicks diff = p1 - p2;
    EXPECT_EQ(diff.ticks(), 50);

    PriceInTicks neg_diff = p2 - p1;
    EXPECT_EQ(neg_diff.ticks(), -50);

    // Overflow check on addition
    PriceInTicks p_max(std::numeric_limits<int64_t>::max());
    EXPECT_THROW((void)(p_max + PriceInTicks(1)), std::overflow_error);

    // Underflow check on subtraction
    PriceInTicks p_min(std::numeric_limits<int64_t>::min());
    EXPECT_THROW((void)(p_min - PriceInTicks(1)), std::underflow_error);
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

    // Overflow throws
    Quantity q_max(std::numeric_limits<uint32_t>::max());
    EXPECT_THROW((void)(q_max + Quantity(1)), std::overflow_error);
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

    // Price validation: ES prices must be strictly positive
    EXPECT_TRUE(es_spec.validate_price(PriceInTicks(1)));
    EXPECT_FALSE(es_spec.validate_price(PriceInTicks(0)));
    EXPECT_FALSE(es_spec.validate_price(PriceInTicks(-10)));

    // Price conversion via spec
    PriceInTicks p = es_spec.to_price_in_ticks(5100, 7500); // 5100.75
    EXPECT_EQ(p.ticks(), 5100 * 4 + 3);

    PriceInTicks pd = es_spec.to_price_in_ticks(5100.75);
    EXPECT_EQ(pd.ticks(), 5100 * 4 + 3);

    // Configurable max trade vol
    auto custom_spec = ProductSpecification::cme_es_outright(Quantity(5000));
    EXPECT_EQ(custom_spec.max_trade_vol(), Quantity(5000));
    EXPECT_TRUE(custom_spec.validate_quantity(Quantity(4000)));
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
    // Invalid date format checks
    EXPECT_THROW(ContractInstance("ESM4", es_spec, 2024, 6, 20241301), std::invalid_argument);
    EXPECT_THROW(ContractInstance("ESM4", es_spec, 2024, 6, 20240632), std::invalid_argument);
}

TEST(ContractInstanceTest, CalendarDateAndLeapYearValidation) {
    auto es_spec = ProductSpecification::cme_es_outright();

    // Specific invalid dates requested for validation:
    // 2027-02-29 (non-leap year February 29)
    EXPECT_THROW(ContractInstance("ESZ7", es_spec, 2027, 2, 20270229), std::invalid_argument);
    // 2027-02-31 (February 31 doesn't exist)
    EXPECT_THROW(ContractInstance("ESZ7", es_spec, 2027, 2, 20270231), std::invalid_argument);
    // 2027-04-31 (April has only 30 days)
    EXPECT_THROW(ContractInstance("ESM7", es_spec, 2027, 4, 20270431), std::invalid_argument);
    // 2027-13-01 (Month 13 does not exist)
    EXPECT_THROW(ContractInstance("ESZ7", es_spec, 2027, 12, 20271301), std::invalid_argument);

    // Valid leap year dates (2024 and 2000 are leap years)
    EXPECT_NO_THROW(ContractInstance("ESH4", es_spec, 2024, 2, 20240229));
    EXPECT_NO_THROW(ContractInstance("ESH0", es_spec, 2000, 2, 20000229));

    // Invalid leap year dates (1900 and 2100 are NOT leap years under Gregorian 400-year rule; 2100 is in range)
    EXPECT_THROW(ContractInstance("ESH0", es_spec, 2100, 2, 21000229), std::invalid_argument);

    // Historical contract instances remain representable within [1970, 2100]
    EXPECT_NO_THROW(ContractInstance("ESH70", es_spec, 1970, 3, 19700320));
    EXPECT_NO_THROW(ContractInstance("ESM82", es_spec, 1982, 6, 19820625));
    EXPECT_NO_THROW(ContractInstance("ESZ99", es_spec, 1999, 12, 19991217));
}

TEST(ContractInstanceTest, ExplicitReferenceTimeExpiration) {
    auto es_spec = ProductSpecification::cme_es_outright();
    ContractInstance esm4("ESM4", es_spec, 2024, 6, 20240621);

    // Static metadata checks
    EXPECT_EQ(esm4.symbol(), "ESM4");
    EXPECT_EQ(esm4.expiration_date(), 20240621);
    EXPECT_TRUE(esm4.is_tradable());

    // Reference date before expiration: not expired
    EXPECT_FALSE(esm4.is_expired_at(20240620));
    EXPECT_FALSE(esm4.is_expired_at(20240621)); // Expiration day itself

    // Reference date after expiration: expired
    EXPECT_TRUE(esm4.is_expired_at(20240622));

    // Update status explicitly based on reference date
    esm4.update_expiration_status(20240620);
    EXPECT_TRUE(esm4.is_tradable());
    EXPECT_EQ(esm4.status(), ContractTradingStatus::ContinuousTrading);

    esm4.update_expiration_status(20240622);
    EXPECT_FALSE(esm4.is_tradable());
    EXPECT_EQ(esm4.status(), ContractTradingStatus::Expired);
}

// ==============================================================================
// Order & Accounting Invariant Lifecycle Tests
// ==============================================================================
TEST(OrderTest, InitialStateAndAccountingInvariant) {
    Order order(OrderId(1), Side::Buy, PriceInTicks(20000), Quantity(100), 1);

    EXPECT_EQ(order.id(), OrderId(1));
    EXPECT_EQ(order.side(), Side::Buy);
    EXPECT_EQ(order.price(), PriceInTicks(20000));
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(100));
    EXPECT_EQ(order.initial_quantity(), Quantity(100)); // alias check
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity::zero());
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(100));
    EXPECT_EQ(order.remaining_quantity(), Quantity(100));
    EXPECT_EQ(order.executed_quantity(), Quantity::zero());
    EXPECT_EQ(order.reduced_quantity(), Quantity::zero());
    EXPECT_EQ(order.cancelled_quantity(), Quantity::zero());
    EXPECT_EQ(order.priority_sequence(), 1);
    EXPECT_EQ(order.status(), OrderStatus::New);
    EXPECT_TRUE(order.is_active());
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, CompletePrescribedLifecycle) {
    // Step 1: Initial quantity 10
    Order order(OrderId(42), Side::Buy, PriceInTicks(20000), Quantity(10), 1);
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.remaining_quantity(), Quantity(10));
    EXPECT_TRUE(order.check_quantity_invariant());

    // Step 2: Execute 2
    order.record_execution(Quantity(2));
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.remaining_quantity(), Quantity(8));
    EXPECT_EQ(order.executed_quantity(), Quantity(2));
    EXPECT_EQ(order.status(), OrderStatus::PartiallyFilled);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Step 3: Reduce remaining quantity by 3 (from 8 to 5)
    order.record_quantity_reduction(Quantity(5));
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity::zero());
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.remaining_quantity(), Quantity(5));
    EXPECT_EQ(order.reduced_quantity(), Quantity(3));
    EXPECT_EQ(order.executed_quantity(), Quantity(2));
    EXPECT_EQ(order.priority_sequence(), 1); // priority retained
    EXPECT_TRUE(order.check_quantity_invariant());

    // Step 4: Increase remaining quantity by 5 (from 5 to 10)
    order.record_quantity_increase(Quantity(10), 99);
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(10)); // original NOT mutated!
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(5));
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(15));
    EXPECT_EQ(order.remaining_quantity(), Quantity(10));
    EXPECT_EQ(order.reduced_quantity(), Quantity(3));
    EXPECT_EQ(order.executed_quantity(), Quantity(2));
    EXPECT_EQ(order.priority_sequence(), 99); // priority lost
    EXPECT_TRUE(order.check_quantity_invariant());

    // Step 5: Execute the remaining quantity (10)
    order.record_execution(Quantity(10));
    EXPECT_EQ(order.remaining_quantity(), Quantity::zero());
    EXPECT_EQ(order.executed_quantity(), Quantity(12));
    EXPECT_EQ(order.reduced_quantity(), Quantity(3));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(5));
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(10));
    EXPECT_EQ(order.status(), OrderStatus::Filled);
    EXPECT_TRUE(order.is_filled());
    EXPECT_FALSE(order.is_active());
    // 10 orig + 5 added = 15 total == 12 executed + 0 remaining + 3 reduced + 0 cancelled
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, CancelAfterPartialFill) {
    Order order(OrderId(10), Side::Sell, PriceInTicks(20000), Quantity(20), 5);
    order.record_execution(Quantity(8));
    EXPECT_EQ(order.remaining_quantity(), Quantity(12));
    EXPECT_EQ(order.executed_quantity(), Quantity(8));

    order.cancel();
    EXPECT_TRUE(order.is_cancelled());
    EXPECT_FALSE(order.is_active());
    EXPECT_EQ(order.remaining_quantity(), Quantity::zero());
    EXPECT_EQ(order.executed_quantity(), Quantity(8));
    EXPECT_EQ(order.cancelled_quantity(), Quantity(12));
    // 20 total == 8 executed + 12 cancelled
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, MultipleReductionsAndIncreases) {
    Order order(OrderId(101), Side::Buy, PriceInTicks(20000), Quantity(50), 1);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Reduction 1: 50 -> 40 (reduced by 10)
    order.record_quantity_reduction(Quantity(40));
    EXPECT_EQ(order.remaining_quantity(), Quantity(40));
    EXPECT_EQ(order.reduced_quantity(), Quantity(10));
    EXPECT_EQ(order.priority_sequence(), 1);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Reduction 2: 40 -> 25 (reduced by another 15, total reduced 25)
    order.record_quantity_reduction(Quantity(25));
    EXPECT_EQ(order.remaining_quantity(), Quantity(25));
    EXPECT_EQ(order.reduced_quantity(), Quantity(25));
    EXPECT_EQ(order.priority_sequence(), 1);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Increase 1: 25 -> 60 (added 35)
    order.record_quantity_increase(Quantity(60), 10);
    EXPECT_EQ(order.remaining_quantity(), Quantity(60));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(35));
    EXPECT_EQ(order.priority_sequence(), 10);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Increase 2: 60 -> 100 (added another 40, total added 75)
    order.record_quantity_increase(Quantity(100), 20);
    EXPECT_EQ(order.remaining_quantity(), Quantity(100));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(75));
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(125));
    EXPECT_EQ(order.priority_sequence(), 20);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Reduction after increase: 100 -> 70 (reduced by 30, total reduced 55)
    order.record_quantity_reduction(Quantity(70));
    EXPECT_EQ(order.remaining_quantity(), Quantity(70));
    EXPECT_EQ(order.reduced_quantity(), Quantity(55));
    EXPECT_EQ(order.priority_sequence(), 20);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Partial execution: execute 20 (remaining 50, executed 20)
    order.record_execution(Quantity(20));
    EXPECT_EQ(order.remaining_quantity(), Quantity(50));
    EXPECT_EQ(order.executed_quantity(), Quantity(20));
    EXPECT_TRUE(order.check_quantity_invariant());

    // Increase after partial fill: 50 -> 80 (added 30, total added 105)
    order.record_quantity_increase(Quantity(80), 30);
    EXPECT_EQ(order.remaining_quantity(), Quantity(80));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(105));
    EXPECT_EQ(order.total_submitted_quantity(), Quantity(155));
    EXPECT_EQ(order.priority_sequence(), 30);
    EXPECT_TRUE(order.check_quantity_invariant());

    // Final invariant check:
    // Original (50) + Added (105) = 155 Total
    // Executed (20) + Remaining (80) + Reduced (55) + Cancelled (0) = 155
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, CancellationAfterIncreasesAndReductions) {
    Order order(OrderId(102), Side::Sell, PriceInTicks(20000), Quantity(100), 1);

    // Initial fill: 25 lots
    order.record_execution(Quantity(25)); // remaining: 75, exec: 25

    // Reduction: 75 -> 50 (reduced: 25)
    order.record_quantity_reduction(Quantity(50));

    // Increase: 50 -> 90 (added: 40)
    order.record_quantity_increase(Quantity(90), 5);

    // Partial fill after increase: 30 lots (remaining: 60, exec: 55)
    order.record_execution(Quantity(30));

    // Cancel remainder (cancelled: 60)
    order.cancel();
    EXPECT_TRUE(order.is_cancelled());
    EXPECT_FALSE(order.is_active());
    EXPECT_EQ(order.remaining_quantity(), Quantity::zero());
    EXPECT_EQ(order.cancelled_quantity(), Quantity(60));
    EXPECT_EQ(order.executed_quantity(), Quantity(55));
    EXPECT_EQ(order.reduced_quantity(), Quantity(25));
    EXPECT_EQ(order.cumulative_added_quantity(), Quantity(40));
    EXPECT_EQ(order.original_submitted_quantity(), Quantity(100));

    // Invariant:
    // 100 orig + 40 added = 140 total == 55 exec + 0 rem + 25 red + 60 canc = 140
    EXPECT_TRUE(order.check_quantity_invariant());
}

TEST(OrderTest, OverflowSafetyAndStrongExceptionGuarantee) {
    Order order(OrderId(103), Side::Buy, PriceInTicks(20000), Quantity(10), 1);

    // Initial order with near max submitted quantity
    Order big_order(OrderId(104), Side::Buy, PriceInTicks(20000), Quantity(std::numeric_limits<uint32_t>::max() - 10), 1);

    // First increase by 5 (remaining becomes max - 5, added becomes 5)
    big_order.record_quantity_increase(Quantity(std::numeric_limits<uint32_t>::max() - 5), 2);
    EXPECT_EQ(big_order.cumulative_added_quantity(), Quantity(5));
    EXPECT_TRUE(big_order.check_quantity_invariant());

    // Partial execution to reduce remaining_quantity without reducing total submitted
    // Execute (max - 20) lots. Remaining becomes 15 lots.
    Quantity big_fill(std::numeric_limits<uint32_t>::max() - 20);
    big_order.record_execution(big_fill);
    EXPECT_EQ(big_order.remaining_quantity(), Quantity(15));
    EXPECT_EQ(big_order.executed_quantity(), big_fill);
    EXPECT_EQ(big_order.cumulative_added_quantity(), Quantity(5));

    // Current state:
    // original = max - 10
    // added = 5
    // total submitted = max - 5
    // remaining = 15
    // executed = max - 20
    // Now if an increase is attempted where (cumulative_added + increase) would overflow uint32_t:
    // If we request new_working_qty = 15 + (max - 5 + 1) -> increase is (max - 4)
    // Then cumulative_added (5) + increase (max - 4) = max + 1 -> overflows uint32_t!
    // But since new_working_qty cannot exceed uint32_t, notice:
    // Suppose cumulative_added is already near max:
    Order overflow_order(OrderId(105), Side::Buy, PriceInTicks(20000), Quantity(10), 1);
    // Increase to 100 (added = 90)
    overflow_order.record_quantity_increase(Quantity(100), 2);
    // Fill 95 (remaining = 5)
    overflow_order.record_execution(Quantity(95));
    // Increase to max - 50 (added = 90 + (max - 55) = max + 35 -> overflows!)
    Quantity prev_rem = overflow_order.remaining_quantity();
    Quantity prev_added = overflow_order.cumulative_added_quantity();
    uint64_t prev_seq = overflow_order.priority_sequence();

    EXPECT_THROW(overflow_order.record_quantity_increase(Quantity(std::numeric_limits<uint32_t>::max() - 50), 3), std::overflow_error);
    // Strong exception guarantee: verify all fields completely unchanged
    EXPECT_EQ(overflow_order.remaining_quantity(), prev_rem);
    EXPECT_EQ(overflow_order.cumulative_added_quantity(), prev_added);
    EXPECT_EQ(overflow_order.priority_sequence(), prev_seq);
    EXPECT_TRUE(overflow_order.check_quantity_invariant());

    // Invalid reduction attempt (new_working_qty >= remaining_qty):
    EXPECT_THROW(overflow_order.record_quantity_reduction(prev_rem), std::invalid_argument);
    EXPECT_EQ(overflow_order.remaining_quantity(), prev_rem);
    EXPECT_EQ(overflow_order.cumulative_added_quantity(), prev_added);
    EXPECT_EQ(overflow_order.priority_sequence(), prev_seq);

    // Invalid fill attempt (fill > remaining):
    EXPECT_THROW(overflow_order.record_execution(Quantity(std::numeric_limits<uint32_t>::max())), std::invalid_argument);
    EXPECT_EQ(overflow_order.remaining_quantity(), prev_rem);
    EXPECT_EQ(overflow_order.executed_quantity(), Quantity(95));
    EXPECT_EQ(overflow_order.status(), OrderStatus::PartiallyFilled);

    // Invalid price update (price <= 0):
    PriceInTicks prev_price = overflow_order.price();
    EXPECT_THROW(overflow_order.update_price(PriceInTicks(0), 4), std::invalid_argument);
    EXPECT_THROW(overflow_order.update_price(PriceInTicks(-100), 4), std::invalid_argument);
    EXPECT_EQ(overflow_order.price(), prev_price);
    EXPECT_EQ(overflow_order.priority_sequence(), prev_seq);
}

TEST(OrderTest, InvalidReductionsAndIncreases) {
    Order order(OrderId(10), Side::Buy, PriceInTicks(20000), Quantity(10), 1);

    // Reduction to 0 is invalid (must use cancel instead)
    EXPECT_THROW(order.record_quantity_reduction(Quantity::zero()), std::invalid_argument);

    // Reduction to >= remaining is invalid
    EXPECT_THROW(order.record_quantity_reduction(Quantity(10)), std::invalid_argument);
    EXPECT_THROW(order.record_quantity_reduction(Quantity(15)), std::invalid_argument);

    // Increase to <= remaining is invalid
    EXPECT_THROW(order.record_quantity_increase(Quantity(10), 2), std::invalid_argument);
    EXPECT_THROW(order.record_quantity_increase(Quantity(5), 2), std::invalid_argument);

    // Operations on inactive order must throw
    order.cancel();
    EXPECT_THROW(order.record_execution(Quantity(1)), std::invalid_argument);
    EXPECT_THROW(order.record_quantity_reduction(Quantity(5)), std::invalid_argument);
    EXPECT_THROW(order.record_quantity_increase(Quantity(15), 3), std::invalid_argument);
    EXPECT_THROW(order.update_price(PriceInTicks(20004), 4), std::invalid_argument);
    EXPECT_THROW(order.cancel(), std::invalid_argument);
}
