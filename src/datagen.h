#ifndef DATAGEN_H
#define DATAGEN_H

#include <fstream>
#include "chess/board.h"

using namespace Chess;

const std::string DATA_PATH = "C:/Users/semio/Documents/programming/Astra-Data/TrainingData/chess_data1_d9.csv";
const std::string NET_DATA_PATH = "C:/Users/semio/Downloads/chess_data1.bin";

struct Dataset {
    std::string fen;
    int16_t eval;

    Dataset(std::string fen, int16_t eval) {
        this->fen = std::move(fen);
        this->eval = eval;
    }
};

struct NetInput {
    U64 pieces[NUM_COLORS][6]{};
    int16_t target;

    NetInput() {
        target = 0;
    }
};

std::vector<Dataset> loadDataset(int data_size = INT_MAX);
std::vector<NetInput> fenToInput(const std::vector<Dataset> &dataset);
void saveNetInput(const std::vector<NetInput>& data);

#endif //DATAGEN_H
