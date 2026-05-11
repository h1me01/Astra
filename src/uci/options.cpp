#include <algorithm>
#include <cctype>

#include "../search/threads.h"
#include "../search/tune_params.h"
#include "../third_party/fathom/src/tbprobe.h"
#include "../util.h"
#include "options.h"

namespace astra::uci {

void Options::print() const {
    for (const auto& elem : options_) {
        Option option = elem.second;
        const std::string& default_val = option.default_val();

        std::string type_str = "unknown";
        switch (option.type()) {
        case OptionType::SPIN:
            type_str = "spin";
            break;
        case OptionType::STRING:
            type_str = "string";
            break;
        case OptionType::CHECK:
            type_str = "check";
            break;
        default:
            assert(false);
            break;
        }

        astra::print(
            "option name {} type {} default {}", elem.first, type_str, (default_val.empty() ? "<empty>" : default_val)
        );

        if (option.min() != 0 || option.max() != 0)
            astra::print(" min {} max {}", option.min(), option.max());
        astra::println("");
    }

    for (auto param : search::params) {
        println("option name {} type spin default {} min {} max {}", param->name, param->value, param->min, param->max);
    }
}

void Options::set(const std::string& info) {
    std::vector<std::string> tokens = split(info, ' ');

    if (tokens.size() < 5 || tokens[1] != "name" || tokens[3] != "value") {
        println("Invalid setoption command format: {}", info);
        return;
    }

    const std::string& value = tokens[4];

    if (value == "<empty>" || value.empty())
        return;

    std::string name;
    const std::string lower_input = to_lower(tokens[2]);
    for (const auto& [key, _] : options_) {
        if (to_lower(key) == lower_input) {
            name = key;
            break;
        }
    }

    if (!name.empty()) {
        options_[name].set(value);
        apply(name);
        return;
    }

    auto it = std::ranges::find_if(search::params, [&](auto* p) { return to_lower(p->name) == lower_input; });

    if (it != std::ranges::end(search::params)) {
        auto* param = *it;
        int n = std::stoi(value);
        if (n < param->min || n > param->max) {
            println(
                "Value out of range for search parameter {}: {} (valid range: {}-{})",
                param->name,
                n,
                param->min,
                param->max
            );
        } else {
            param->value = n;
        }
        return;
    }

    println("Unknown option {}", tokens[2]);
}

void Options::update_syzygy_path(const std::string& path) {
    if (!path.empty() && path != "<empty>") {
        bool success = tb_init(path.c_str());
        if (success && TB_LARGEST > 0)
            println("info string Successfully loaded syzygy path with {} largest tablebases", TB_LARGEST);
        else
            println("info string Failed to load syzygy path {}", path);
    }
}

void Options::apply(const std::string& name) {
    if (to_lower(name) == "syzygypath")
        update_syzygy_path(options_[name]);
    else if (to_lower(name) == "hash")
        search::tt.init(std::stoi(options_[name]));
    else if (to_lower(name) == "threads")
        search::thread_pool.set_count(std::stoi(get("Threads")));
}

} // namespace astra::uci
