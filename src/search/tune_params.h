#pragma once

#include <string>
#include <vector>

#include "../chess/types.h"

// #define TUNE

namespace astra::search {

struct Param {
    std::string name;
    int value, min, max;

    Param(std::string name, int value, int min, int max);

    operator int() const { return value; }
};

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

PARAM(pawn_value, 100, 1, 200);
PARAM(knight_value, 360, 1, 600);
PARAM(bishop_value, 385, 1, 600);
PARAM(rook_value, 635, 1, 1000);
PARAM(queen_value, 1200, 1, 2000);

PARAM(pawn_value_see, 98, 1, 200);
PARAM(knight_value_see, 318, 1, 600);
PARAM(bishop_value_see, 335, 1, 600);
PARAM(rook_value_see, 504, 1, 1000);
PARAM(queen_value_see, 997, 1, 2000);

PARAM(ms_pawn, 100, 1, 200);
PARAM(ms_knight, 300, 1, 600);
PARAM(ms_bishop, 300, 1, 600);
PARAM(ms_rook, 500, 1, 1000);
PARAM(ms_queen, 1000, 1, 2000);
PARAM(ms_base, 22806, 1, 45612);
PARAM(ms_div, 32768, 1, 65536);

PARAM(tm_stability_base, 157, 100, 200);
PARAM(tm_stability_mult, 35, 0, 250);
PARAM(tm_stability_max, 4, 1, 20);
PARAM(tm_results_base, 70, 0, 160);
PARAM(tm_results_mult1, 15, 0, 27);
PARAM(tm_results_mult2, 29, 0, 45);
PARAM(tm_results_min, 101, 0, 150);
PARAM(tm_results_max, 147, 50, 220);
PARAM(tm_node_mult, 219, 100, 350);
PARAM(tm_node_base, 58, 10, 90);

PARAM(asp_delta, 11, 1, 30);

PARAM(tt_hist_bonus_mult, 308, 1, 1536);
PARAM(tt_hist_bonus_bias, 4, -500, 500);
PARAM(tt_hist_bonus_max, 2445, 1, 4096);

PARAM(tt_hist_malus_mult, 311, 1, 1536);
PARAM(tt_hist_malus_bias, 104, -500, 500);
PARAM(tt_hist_malus_max, 1646, 1, 4096);

PARAM(static_hist_mult, -51, -200, -1);
PARAM(static_hist_min, -108, -400, -1);
PARAM(static_hist_max, 226, 1, 1000);

PARAM(rzr_base, 318, 50, 750);
PARAM(rzr_mult, 234, 1, 500);

PARAM(rfp_depth_mult, 103, 40, 200);
PARAM(rfp_improving_mult, 85, 40, 200);

PARAM(nmp_depth_mult, 26, 1, 58);
PARAM(nmp_base, 192, 50, 400);
PARAM(nmp_eval_div, 193, 50, 350);

PARAM(pc_margin, 217, 1, 400);
PARAM(pc_improving_mult, 60, 1, 120);

PARAM(hist_div, 7778, 1, 16384);

PARAM(hp_depth_mult, -5429, -12500, -2500);

PARAM(fp_base, 88, 1, 300);
PARAM(fp_mult, 104, 5, 200);

PARAM(see_noisy_margin, -103, -200, -20);
PARAM(see_quiet_margin, -15, -100, -1);

PARAM(double_ext_margin, 13, 1, 30);
PARAM(triple_ext_margin, 93, 10, 250);

PARAM(lmr_base, 1134, 1, 2048);
PARAM(lmr_mul, 355, 1, 512);
PARAM(lmr_improving, 1024, 1, 2048);
PARAM(lmr_cut_node, 2048, 1, 4096);
PARAM(lmr_cut_node_no_tt_move, 1024, 1, 2048);
PARAM(lmr_tt_pv, 1024, 1, 2048);
PARAM(lmr_tt_depth, 1024, 1, 2048);
PARAM(lmr_tt_score, 655, 1, 2048);
PARAM(lmr_tt_move_noisy, 1024, 1, 2048);
PARAM(lmr_in_check, 1024, 1, 2048);
PARAM(lmr_quiet_hist_mul, 160, 1, 300);
PARAM(lmr_noisy_hist_mul, 100, 1, 300);

PARAM(dp_margin, 60, 10, 120);
PARAM(ds_margin, 8, 1, 20);

PARAM(sdr_min_depth, 2, 0, 6);
PARAM(sdr_max_depth, 14, 0, 20);

PARAM(qfp_margin, 111, 40, 280);
PARAM(qsee_margin, -20, -200, 50);

PARAM(hist_bonus_margin, 52, 10, 200);

PARAM(hist_bonus_mult, 308, 1, 1536);
PARAM(hist_bonus_bias, 4, -500, 500);
PARAM(hist_bonus_max, 2445, 1, 4096);

PARAM(hist_malus_mult, 311, 1, 1536);
PARAM(hist_malus_bias, 104, -500, 500);
PARAM(hist_malus_max, 1646, 1, 4096);

PARAM(quiet_hist_bonus_mult, 1024, 1, 2048);
PARAM(quiet_hist_malus_mult, 1024, 1, 2048);

PARAM(noisy_hist_bonus_mult, 1024, 1, 2048);
PARAM(noisy_hist_malus_mult, 1024, 1, 2048);

PARAM(quiet_checker_bonus, 75, 0, 150);

PARAM(p_corr_weight, 4, 1, 168);
PARAM(m_corr_weight, 4, 1, 168);
PARAM(np_corr_weight, 4, 1, 168);
PARAM(cont_corr_weight, 4, 1, 168);

PARAM(mp_threat_mul, 19, 1, 40);

#ifdef TUNE
inline int piece_values(PieceType pt) {
#else
constexpr int piece_values(PieceType pt) {
#endif
    assert(is_valid(pt) || pt == NO_PIECE_TYPE);

    switch (pt) {
    case PAWN:
        return pawn_value;
    case KNIGHT:
        return knight_value;
    case BISHOP:
        return bishop_value;
    case ROOK:
        return rook_value;
    case QUEEN:
        return queen_value;
    default:
        return 0;
    }
}

// Global Variable

extern std::vector<Param*> params;

} // namespace astra::search
