#pragma once

#include <fstream>
#include <unordered_map>

#include "uci_options.h"
#include "../engine/search.h"

namespace UCI {

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

} // namespace UCI
