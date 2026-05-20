#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>

namespace astra::uci {

enum class OptionType {
    NONE,
    SPIN,
    STRING,
    CHECK,
};

class Option {
  public:
    Option(OptionType type = OptionType::NONE, std::string val = "", int min = 0, int max = 0)
        : type_(type),
          default_val_(val),
          val_(val),
          min_(min),
          max_(max) {}

    void set(const std::string& value) {
        assert(type_ != OptionType::NONE);

        if (type_ == OptionType::SPIN) {
            int n = std::stoi(value);
            if (n >= min_ && n <= max_)
                val_ = value;
            else
                assert(false);
        } else {
            val_ = value;
        }
    }

    OptionType type() const { return type_; }
    std::string default_val() const { return default_val_; }
    int min() const { return min_; }
    int max() const { return max_; }
    bool operator==(const std::string& other) const { return val_ == other; }
    operator std::string() const { return val_; }

  private:
    OptionType type_;
    std::string default_val_, val_;
    int min_, max_;
};

class Options {
  public:
    void print() const;
    void set(const std::string& info);

    void add(std::string name, const Option& option) {
        options_[name] = option;
        apply(name);
    }

    Option& get(const std::string& name) {
        auto it = options_.find(name);
        assert(it != options_.end());
        return it->second;
    }

    const Option& get(const std::string& name) const {
        auto it = options_.find(name);
        assert(it != options_.end());
        static const Option empty_option;
        return (it != options_.end()) ? it->second : empty_option;
    }

  private:
    std::unordered_map<std::string, Option> options_;

    void update_syzygy_path(const std::string& path);
    void apply(const std::string& name);
};

} // namespace astra::uci
