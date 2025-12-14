#pragma once

#include <fstream>
#include <unordered_map>

#include "../search/search.h"
#include "uci_options.h"

namespace uci {

class UCI {
  public:
    UCI();

    void loop(int argc, char **argv);

  private:
    Board board{STARTING_FEN};
    Options options;

    void update_position(std::istringstream &is);
    void new_game();
    void go(std::istringstream &is);
    void bench();

    Move parse_move(const std::string &str_move) const;
};

} // namespace uci
