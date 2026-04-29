#include <algorithm>
#include <iostream>
#include <sstream>

#include "../util.h"
#include "tune_params.h"

namespace astra::search {

std::vector<Param*> params;

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

bool set_param(const std::string& name, int value) {
    bool found = false;

    for (auto* param : params) {
        if (param->name != name)
            continue;

        if (value < param->min || value > param->max) {
            println(
                "Value out of range for search parameter {}: {} (valid range: {}-{})",
                name,
                value,
                param->min,
                param->max
            );
            return true;
        }

        param->value = value;
        found = true;
        break;
    }

    if (!found)
        println("Unknown search parameter {}", name);

    return found;
}

void print_params() {
    for (auto param : params) {
        println("option name {} type spin default {} min {} max {}", param->name, param->value, param->min, param->max);
    }
}

void params_to_spsa() {
    for (const auto& param : params) {
        println(
            "{}, int, {}, {}, {}, {}, {}",
            param->name,
            param->value,
            param->min,
            param->max,
            std::max<double>(0.5, (param->max - param->min) / 20.0),
            0.002
        );
    }
}

} // namespace astra::search
