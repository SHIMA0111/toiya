#pragma once

#include <iostream>
#include <fstream>
#include <string>
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
                           size_t chunk_size)-> Result;

extern "C" {
    typedef struct {
        const void* data;
        const char* name;
        void (*release)(void*) noexcept;
    } CResult;

    CResult read_from_hyper_query_c(const char* path, const char* query, size_t chunk_size) {
        std::ofstream log_file("/tmp/hyper_debug.log", std::ios_base::app);
        try {
            log_file << "Starting read_from_hyper_query_c" << std::endl;

            std::string path_str(path);
            std::string query_str(query);

            log_file << "Path: " << path_str << std::endl;
            log_file << "Query: " << query_str << std::endl;

            Result result = read_from_hyper_query(path_str, query_str, chunk_size);
            log_file << "Query executed successfully" << std::endl;

            std::cout << "Finished read_from_hyper_query_c" << std::endl;
            return {result.data, result.name, result.release};
        } catch (const std::exception& e) {
            log_file << "Caught exception: " << e.what() << std::endl;
            return {nullptr, nullptr, nullptr};
        } catch (...) {
            log_file << "Caught unknown exception" << std::endl;
            return {nullptr, nullptr, nullptr};
        }
    }
}
