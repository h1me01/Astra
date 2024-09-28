#include "nnue.h"
#include "accumulator.h"
#include "../chess/misc.h"

namespace NNUE {

    // HELPER
    int index(Square s, Piece p, Color view) {
        assert(p != NO_PIECE);
        assert(s != NO_SQUARE);

        Color pc = colorOf(p);
        PieceType pt = typeOf(p);
        s = relativeSquare(view, s);
        const int idx = static_cast<int>(s) + static_cast<int>(pt) * 64 + (pc != view) * 64 * 6;
        assert(idx >= 0 && idx < INPUT_SIZE);
        return idx;
    }

    // NNUE
    void NNUE::init(std::string path) {
        try {
            loadParameters(path);
        } catch (const std::exception &e) {
            std::cerr << "Failed to initialize NNUE: " << e.what() << std::endl;
            exit(1);
        }
    }

    int32_t NNUE::forward(const std::vector<int16_t *> &acc, Color stm) const {
        int32_t prediction = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++) {
            if (acc[stm][j] > 0) {
                prediction += fc2_weights[j] * acc[stm][j];
            }

            if (acc[~stm][j] > 0) {
                prediction += fc2_weights[HIDDEN_SIZE + j] * acc[~stm][j];
            }
        }

        return prediction / (512 * 16);
    }

    void NNUE::putPiece(const std::vector<int16_t *> &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] += fc1_weights[idx + w_idx];
            acc[BLACK][i] += fc1_weights[idx + b_idx];
        }
    }

    void NNUE::removePiece(const std::vector<int16_t *> &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] -= fc1_weights[idx + w_idx];
            acc[BLACK][i] -= fc1_weights[idx + b_idx];
        }
    }

    void NNUE::movePiece(const std::vector<int16_t *> &acc, Piece p, Square from, Square to) const {
        const int w_from_idx = index(from, p, WHITE);
        const int w_to_idx = index(to, p, WHITE);
        const int b_from_idx = index(from, p, BLACK);
        const int b_to_idx = index(to, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;

            acc[WHITE][i] += fc1_weights[idx + w_to_idx] - fc1_weights[idx + w_from_idx];
            acc[BLACK][i] += fc1_weights[idx + b_to_idx] - fc1_weights[idx + b_from_idx];
        }
    }

    void NNUE::loadParameters(const std::string &filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error opening NNUE file: " << filename << std::endl;
        }

        file.read(reinterpret_cast<char *>(fc1_weights), sizeof(int16_t) * INPUT_SIZE * HIDDEN_SIZE);
        file.read(reinterpret_cast<char *>(fc1_biases), sizeof(int16_t) * HIDDEN_SIZE);

        file.read(reinterpret_cast<char *>(fc2_weights), sizeof(int16_t) * 2 * HIDDEN_SIZE);
        file.read(reinterpret_cast<char *>(fc2_biases), sizeof(int32_t) * OUTPUT_SIZE);

        file.close();
    }

} // namespace NNUE
