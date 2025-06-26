#pragma once

#include "search/search.h"
#include <fstream>
#include <unordered_map>

#include "search/search.h"

namespace UCI {

struct Option {
    std::string type, default_val, val;
    int min, max;

    Option() : min(0), max(0) {}

    Option(std::string type, std::string default_val, std::string val, int min, int max)
        : type(std::move(type)), default_val(std::move(default_val)), val(std::move(val)), min(min), max(max) {};

    Option &operator=(const std::string &v) {
        val = v;
        return *this;
    }
};

class Options {
  private:
    std::unordered_map<std::string, Option> options;

  public:
    int num_workers = 1; // default number of threads
    bool use_tb = false;

    void print() const;
    void apply();

    void add(std::string name, const Option &option) {
        options[name] = option;
    }

    void set(std::istringstream &is);

    std::string get(std::string str) const {
        auto it = options.find(str);
        if(it != options.end())
            return it->second.val;
        return "";
    }
};

class Uci {
  private:
    Board board;
    Options options;
    std::ofstream logFile;

    void updatePosition(std::istringstream &is);
    void go(std::istringstream &is);

    Move getMove(const std::string &str_move) const;

    void openLog(const std::string &filename = "astra_log.txt") {
        logFile.open(filename, std::ios::app);
        writeLog("=== New Session Started ===");
    }

    void closeLog() {
        if(!logFile.is_open())
            return;

        writeLog("=== Session Ended ===");
        logFile.close();
    }

    void writeLog(const std::string &message) {
        if(logFile.is_open())
            logFile << message << std::endl;
    }

  public:
    Uci();
    ~Uci();

    void loop(int argc, char **argv);
};

} // namespace UCI
