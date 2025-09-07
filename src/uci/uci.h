#pragma once

#include <fstream>
#include <unordered_map>

#include "../engine/search.h"

namespace UCI {

struct Option {
    std::string type, default_val, val;
    int min, max;

    Option() : min(0), max(0) {}

    Option(std::string type, std::string default_val, std::string val, int min, int max)
        : type(type), default_val(default_val), val(val), min(min), max(max) {}

    Option &operator=(const std::string &v) {
        val = v;
        return *this;
    }
};

class Options {
  public:
    // public variables

    bool use_tb = false;

    // public functions

    void print() const;

    void apply(bool init_tt = true);

    void set(std::istringstream &is);

    void add(std::string name, const Option &option) {
        options[name] = option;
    }

    std::string get(std::string str) const {
        auto it = options.find(str);
        if(it != options.end())
            return it->second.val;
        return "";
    }

  private:
    std::unordered_map<std::string, Option> options;
};

class UCI {
  public:
    UCI();

    void loop(int argc, char **argv);

  private:
    // private variables

    Board board;
    Options options;

    // private member functions

    void update_position(std::istringstream &is);
    void go(std::istringstream &is);
    void bench();

    Move get_move(const std::string &str_move) const;
};

} // namespace UCI
