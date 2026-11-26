#include <benchmark/benchmark.h>
#include "tickforge/order_book/order_book.hpp"
#include "tickforge/domain/contract_instance.hpp"
#include "tickforge/domain/product_specification.hpp"
#include <vector>

using namespace tickforge::domain;
using namespace tickforge::order_book;

static ContractInstance create_bench_contract() {
    auto spec = ProductSpecification::cme_es_outright();
    return ContractInstance("ESM4", spec, 2024, 6, 20240621);
}

// Benchmark inserting orders into an existing price level (FIFO append)
static void BM_OrderBookInsertExistingLevel(benchmark::State& state) {
    auto contract = create_bench_contract();
    OrderBook book(contract);
    PriceInTicks price(20000);

    // Seed initial order
    book.insert_order(Order(OrderId(1), Side::Buy, price, Quantity(10), 1));

    uint64_t next_id = 2;
    for (auto _ : state) {
        book.insert_order(Order(OrderId(next_id), Side::Buy, price, Quantity(10), next_id));
        ++next_id;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsertExistingLevel);

// Benchmark inserting orders creating new price levels
static void BM_OrderBookInsertNewPriceLevel(benchmark::State& state) {
    auto contract = create_bench_contract();
    OrderBook book(contract);

    int64_t tick_offset = 1;
    uint64_t next_id = 1;
    for (auto _ : state) {
        PriceInTicks price(20000 + (tick_offset % 500000) * 4);
        book.insert_order(Order(OrderId(next_id), Side::Buy, price, Quantity(10), next_id));
        ++next_id;
        ++tick_offset;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookInsertNewPriceLevel);

// Benchmark lookup by OrderId in a book populated with 10,000 orders
static void BM_OrderBookFindOrderById(benchmark::State& state) {
    auto contract = create_bench_contract();
    OrderBook book(contract);
    constexpr uint64_t num_orders = 10000;

    for (uint64_t i = 1; i <= num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        PriceInTicks price(20000 + static_cast<int64_t>((i % 100) * 4));
        book.insert_order(Order(OrderId(i), side, price, Quantity(5), i));
    }

    uint64_t query_id = 1;
    for (auto _ : state) {
        const Order* ord = book.find_order(OrderId(query_id));
        benchmark::DoNotOptimize(ord);
        query_id = (query_id % num_orders) + 1;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_OrderBookFindOrderById);

// Benchmark removal of orders by OrderId
static void BM_OrderBookRemoveOrderById(benchmark::State& state) {
    auto contract = create_bench_contract();

    for (auto _ : state) {
        state.PauseTiming();
        OrderBook book(contract);
        constexpr uint64_t num_orders = 1000;
        for (uint64_t i = 1; i <= num_orders; ++i) {
            PriceInTicks price(20000 + static_cast<int64_t>((i % 50) * 4));
            book.insert_order(Order(OrderId(i), Side::Buy, price, Quantity(5), i));
        }
        state.ResumeTiming();

        for (uint64_t i = 1; i <= num_orders; ++i) {
            book.remove_order(OrderId(i));
        }
    }

    state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(BM_OrderBookRemoveOrderById);
