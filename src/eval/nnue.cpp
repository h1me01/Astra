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
    void NNUE::init(const std::string& filename) {
        try {
            loadParameters(filename);
        } catch (const std::exception &e) {
            std::cerr << "Failed to initialize NNUE: " << e.what() << std::endl;
        }
    }

    int32_t NNUE::forward(const Accumulator &acc, Color stm) const {
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

    void NNUE::putPiece(Accumulator &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] += fc1_weights[idx + w_idx];
            acc[BLACK][i] += fc1_weights[idx + b_idx];
        }
    }

    void NNUE::removePiece(Accumulator &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] -= fc1_weights[idx + w_idx];
            acc[BLACK][i] -= fc1_weights[idx + b_idx];
        }
    }

    void NNUE::movePiece(Accumulator &acc, Piece p, Square from, Square to) const {
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

    void NNUE::loadParameters(const std::string& filename) {
        FILE *f = fopen(filename.c_str(), "rb");

        if(!f) {
            std::cerr << "Failed to open file: " << filename << std::endl;
            return;
        }

        std::size_t elements_size = 0;
        elements_size += fread(fc1_weights, sizeof(int16_t), INPUT_SIZE * HIDDEN_SIZE, f);
        elements_size += fread(fc1_biases, sizeof(int16_t), HIDDEN_SIZE, f);
        elements_size += fread(fc2_weights, sizeof(int16_t), 2 * HIDDEN_SIZE, f);
        elements_size += fread(fc2_biases, sizeof(int32_t), OUTPUT_SIZE, f);

        std::size_t expected_size = INPUT_SIZE * HIDDEN_SIZE + HIDDEN_SIZE + 2 * HIDDEN_SIZE + OUTPUT_SIZE;
        if (elements_size != expected_size) {
            std::cerr << "Error loading network: Expected " << expected_size << " elements, got " << elements_size << " elements." << std::endl;
            fclose(f);
            exit(1);
        }

        fclose(f);
    }

} // namespace NNUE
