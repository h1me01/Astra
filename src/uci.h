#ifndef UCI_H
#define UCI_H

#include <unordered_map>
#include "search/search.h"

namespace UCI
{
    struct Option
    {
        std::string type, default_val, val;
        int min, max;

        Option() : min(0), max(0) {}

        Option(std::string type, std::string default_val, std::string val, int min, int max) :
            type(std::move(type)),
            default_val(std::move(default_val)),
            val(std::move(val)),
            min(min),
            max(max) {};

        Option& operator=(const std::string& v)
        {
            val = v;
            return *this;
        }
    };

    class Uci
    {
    public :
        Uci();

        void loop();

    private:
        int num_workers = 1; // default number of threads
        bool use_tb = false;
        Board board;

        std::unordered_map<std::string, Option> options;

        void updatePosition(std::istringstream& is);
        void go(std::istringstream& is);

        void printOptions() const;
        void applyOptions();
        void setOption(std::istringstream& is);
        std::string getOption(const std::string& str) const;

        Move getMove(const std::string& str_move) const;
    };

} // namespace UCI

#endif //UCI_H
