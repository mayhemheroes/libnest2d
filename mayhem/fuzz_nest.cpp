//
// Created by bailey on 11/26/22.
//
#include <cstdint>
#include <optional>

#include <libnest2d/libnest2d.hpp>

#include "FuzzedDataProvider.h"


// Some constants needed for fuzzing decisions

using namespace libnest2d;

auto generate_random_points(FuzzedDataProvider &fdp) -> std::vector<ClipperLib::IntPoint> {
    std::vector<ClipperLib::IntPoint> points;
    auto num_points = fdp.ConsumeIntegralInRange(0, 100);
    for (int i = 0; i < num_points; i++) {
        points.emplace_back(fdp.ConsumeIntegral<int64_t>(), fdp.ConsumeIntegral<int64_t>());
    }
    return points;
}

auto generate_random_item_vector(FuzzedDataProvider& fdp) -> std::vector<Item> {
    std::vector<Item> random_items{};
    std::size_t item_count = fdp.ConsumeIntegralInRange(0, 100);
    for (std::size_t i = 0; i < item_count; i++) {
        auto points = generate_random_points(fdp);
        random_items.emplace_back(Item{points});
    }
    return random_items;
}

extern "C" [[maybe_unused]] int LLVMFuzzerTestOneInput(const uint8_t *data, std::size_t size) {
    FuzzedDataProvider fdp(data, size);
    try {
        auto input1 = generate_random_item_vector(fdp);
        nest(input1, Box(fdp.ConsumeIntegral<uint16_t>(), fdp.ConsumeIntegral<uint16_t>()));
    } catch (const ClipperLib::clipperException &e) {
        // Ignore clipper exceptions
    }
    return 0;
}