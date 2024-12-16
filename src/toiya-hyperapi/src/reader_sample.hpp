#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <variant>
#include <unordered_map>

template <std::size_t N> constexpr auto to_integral_variant(std::size_t  n) {
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        using ResType = std::variant<std::integral_constant<std::size_t, Is>...>;
        std::array<ResType, N> all {
            ResType {std::integral_constant<std::size_t, Is>{}}...};

        return all[n];
    }(std::make_index_sequence<N>());
}

struct Result {
    const void* data{};
    const char* name{};
    void (*release)(void*) noexcept = nullptr;
};

auto read_from_hyper_query(const std::string& path,
                           const std::string& query,
                           std::unordered_map<std::string, std::string>&& process_params,
                           size_t chunk_size)-> Result;
