#pragma once

#include <sstream>
#include <string>

#include "../search/search.h"
#include "options.h"

namespace astra::uci {

const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

class UCI {
  public:
    UCI();

    void loop(int argc, char** argv);

  private:
    Options options_;
    Board board_{STARTING_FEN};

    void update_position(std::istringstream& is);
    void new_game();
    void go(std::istringstream& is);
    void bench();

    Move parse_move(const std::string& str_move) const;
};

} // namespace astra::uci
