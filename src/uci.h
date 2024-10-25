#ifndef UCI_H
#define UCI_H

#include <unordered_map>
#include "ucioptions.h"
#include "search/search.h"

namespace UCI {

    class Uci {
    public :
        Uci();

        void loop();

    private:
        int num_workers = 1; // default number of threads
        bool use_tb = false;
        Board board;

        std::unordered_map<std::string, Option> options;

        void updatePosition(std::istringstream &is);
        void go(std::istringstream &is);

        void printOptions() const;
        void applyOptions();
        void setOption(std::istringstream &is);
        std::string getOption(const std::string& str) const;

        Move getMove(const std::string &str_move) const;
    };

} // namespace UCI

#endif //UCI_H
