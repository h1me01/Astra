#pragma once

#include <iostream>
#include <string>
#include <unordered_map>

namespace uci {

enum class OptionType { NONE, SPIN, STRING, CHECK };

class Option {
  public:
    Option(OptionType type = OptionType::NONE, std::string val = "", int min = 0, int max = 0)
        : type(type),
          default_val(val),
          val(val),
          min(min),
          max(max) {}

    void set(const std::string& value) {
        assert(type != OptionType::NONE);

        if (type == OptionType::SPIN) {
            int n = std::stoi(value);
            if (n >= min && n <= max)
                val = value;
            else
                assert(false);
        } else {
            val = value;
        }
    }

    OptionType get_type() const {
        return type;
    }

    std::string get_default_val() const {
        return default_val;
    }

    int get_min() const {
        return min;
    }

    int get_max() const {
        return max;
    }

    bool operator==(const std::string& other) const {
        return val == other;
    }

    operator std::string() const {
        return val;
    }

  private:
    OptionType type;
    std::string default_val, val;
    int min, max;
};

class Options {
  public:
    void print() const;
    void set(const std::string& info);

    void add(std::string name, const Option& option) {
        options[name] = option;
        apply(name);
    }

    Option& get(const std::string& name) {
        auto it = options.find(name);
        if (it != options.end())
            return it->second;
        assert(false);
        options[name] = Option();
        return options[name];
    }

    const Option& get(const std::string& name) const {
        auto it = options.find(name);
        if (it != options.end())
            return it->second;
        assert(false);
        static const Option empty_option;
        return empty_option;
    }

  private:
    std::unordered_map<std::string, Option> options;

    void update_syzygy_path(const std::string& path);
    void apply(const std::string& name);
};

} // namespace uci
