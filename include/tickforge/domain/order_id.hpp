#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <ostream>

namespace tickforge::domain {

/// Strongly typed 64-bit Order Identifier.
class OrderId {
public:
    constexpr OrderId() noexcept : id_(0) {}
    constexpr explicit OrderId(uint64_t id) noexcept : id_(id) {}

    [[nodiscard]] constexpr uint64_t value() const noexcept { return id_; }
    [[nodiscard]] constexpr bool is_valid() const noexcept { return id_ != 0; }

    [[nodiscard]] constexpr bool operator==(const OrderId& other) const noexcept { return id_ == other.id_; }
    [[nodiscard]] constexpr bool operator!=(const OrderId& other) const noexcept { return id_ != other.id_; }
    [[nodiscard]] constexpr bool operator<(const OrderId& other) const noexcept { return id_ < other.id_; }
    [[nodiscard]] constexpr bool operator<=(const OrderId& other) const noexcept { return id_ <= other.id_; }
    [[nodiscard]] constexpr bool operator>(const OrderId& other) const noexcept { return id_ > other.id_; }
    [[nodiscard]] constexpr bool operator>=(const OrderId& other) const noexcept { return id_ >= other.id_; }

    [[nodiscard]] std::string to_string() const { return std::to_string(id_); }

private:
    uint64_t id_;
};

inline std::ostream& operator<<(std::ostream& os, const OrderId& id) {
    return os << id.value();
}

} // namespace tickforge::domain

namespace std {
template <>
struct hash<tickforge::domain::OrderId> {
    size_t operator()(const tickforge::domain::OrderId& id) const noexcept {
        return hash<uint64_t>{}(id.value());
    }
};
} // namespace std
