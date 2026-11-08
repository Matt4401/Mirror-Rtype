#include "metrics/MetricsBoard.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace metrics {

void MetricsBoard::set(const std::string_view name, std::string value, const MetricLevel level) {
    const auto entry = std::ranges::find(_entries, name, &Metric::name);
    if (entry == _entries.end()) {
        _entries.push_back({.name = std::string(name), .value = std::move(value), .level = level});
        ++_version;
        return;
    }
    if (entry->value == value && entry->level == level) {
        return;
    }
    entry->value = std::move(value);
    entry->level = level;
    ++_version;
}

const Metric* MetricsBoard::find(const std::string_view name) const noexcept {
    const auto entry = std::ranges::find(_entries, name, &Metric::name);
    return entry == _entries.end() ? nullptr : &*entry;
}

}  // namespace metrics
