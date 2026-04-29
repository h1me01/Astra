#include <algorithm>
#include <iostream>
#include <sstream>

#include "../util.h"
#include "tune_params.h"

namespace astra::search {

Param::Param(std::string name, int value, int min, int max)
    : name(std::move(name)),
      value(value),
      min(min),
      max(max) {
    if (value < min || value > max) {
        println("Value out of range for search parameter {}: {} (valid range: {}-{})", this->name, value, min, max);
        return;
    }

    if (min >= max) {
        println("Invalid range for search parameter {}: min {} >= max {}", this->name, min, max);
        return;
    }

    params.push_back(this);
}

// Global Variable

std::vector<Param*> params;

} // namespace astra::search
