#include "datagen.h"

std::vector<Dataset> loadDataset(int data_size) {
   std::vector<Dataset> dataset;
   std::ifstream file(DATA_PATH);

   if (!file) {
      std::cerr << "Error opening file" << std::endl;
      return dataset;
   }

   std::string header;
   std::getline(file, header);

   std::string line;
   for (int i = 0; std::getline(file, line) && data_size > i; ++i) {
      std::istringstream iss(line);
      std::string fen, eval;

      std::getline(iss, fen, ',');
      std::getline(iss, eval, ',');

      if(fen.empty()) {
         std::cerr << "Empty FEN at line " << i << std::endl;
         exit(0);
      }

      Dataset data{fen.substr(1), static_cast<int16_t>(std::stoi(eval))};
      dataset.push_back(data);
   }

   std::cout << "Loaded " << dataset.size() << " positions" << std::endl;
   return dataset;
}

std::vector<NetInput> fenToInput(const std::vector<Dataset> &dataset) {
   std::vector<NetInput> net_inputs;
   net_inputs.reserve(dataset.size());

   for (const auto &i: dataset) {
      NetInput input;
      input.target = i.eval;

      Board board(i.fen);
      for (const PieceType pt: {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
         input.pieces[WHITE][pt] = board.getPieceBB(WHITE, pt);
         input.pieces[BLACK][pt] = board.getPieceBB(BLACK, pt);
      }

      net_inputs.push_back(input);
   }

   return net_inputs;
}

void saveNetInput(const std::vector<NetInput>& data) {
   std::ofstream file(NET_DATA_PATH, std::ios::binary);
   if (!file.is_open()) {
      std::cerr << "Error writing in file" << std::endl;
   }

   for (const NetInput &input: data) {
      file.write(reinterpret_cast<const char *>(&input), sizeof(NetInput));
   }

   file.close();

   std::cout << "Saved positions at " << NET_DATA_PATH << std::endl;
   exit(0);
}
