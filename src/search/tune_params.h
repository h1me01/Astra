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

PARAM(pawn_value, 69, 1, 200);
PARAM(knight_value, 475, 1, 600);
PARAM(bishop_value, 456, 1, 600);
PARAM(rook_value, 705, 1, 1000);
PARAM(queen_value, 1247, 1, 2000);

PARAM(pawn_value_see, 87, 1, 200);
PARAM(knight_value_see, 379, 1, 600);
PARAM(bishop_value_see, 366, 1, 600);
PARAM(rook_value_see, 560, 1, 1000);
PARAM(queen_value_see, 1109, 1, 2000);

PARAM(ms_pawn, 102, 1, 200);
PARAM(ms_knight, 334, 1, 600);
PARAM(ms_bishop, 245, 1, 600);
PARAM(ms_rook, 515, 1, 1000);
PARAM(ms_queen, 1184, 1, 2000);
PARAM(ms_base, 14767, 1, 45612);
PARAM(ms_div, 25332, 1, 65536);

PARAM(tm_stability_base, 155, 100, 200);
PARAM(tm_stability_mult, 27, 0, 250);
PARAM(tm_stability_max, 5, 1, 20);
PARAM(tm_results_base, 82, 0, 160);
PARAM(tm_results_mult1, 15, 0, 27);
PARAM(tm_results_mult2, 23, 0, 45);
PARAM(tm_results_min, 120, 0, 150);
PARAM(tm_results_max, 158, 50, 220);
PARAM(tm_node_mult, 242, 100, 350);
PARAM(tm_node_base, 55, 10, 90);

PARAM(asp_delta, 9, 1, 30);

PARAM(tt_hist_bonus_mult, 456, 1, 1536);
PARAM(tt_hist_bonus_bias, -57, -500, 500);
PARAM(tt_hist_bonus_max, 1791, 1, 4096);

PARAM(tt_hist_malus_mult, 313, 1, 1536);
PARAM(tt_hist_malus_bias, 1, -500, 500);
PARAM(tt_hist_malus_max, 2227, 1, 4096);

PARAM(static_hist_mult, -70, -200, -1);
PARAM(static_hist_min, -162, -400, -1);
PARAM(static_hist_max, 318, 1, 1000);

PARAM(rzr_base, 299, 50, 750);
PARAM(rzr_mult, 198, 1, 500);

PARAM(rfp_depth_mult, 100, 40, 200);
PARAM(rfp_improving_mult, 79, 40, 200);

PARAM(nmp_depth_mult, 21, 1, 58);
PARAM(nmp_base, 171, 50, 400);
PARAM(nmp_eval_div, 167, 50, 350);

PARAM(pc_margin, 214, 1, 400);
PARAM(pc_improving_mult, 60, 1, 120);

PARAM(hist_div, 6069, 1, 16384);

PARAM(hp_depth_mult, -6080, -12500, -2500);

PARAM(fp_base, 56, 1, 300);
PARAM(fp_mult, 106, 5, 200);

PARAM(see_noisy_margin, -103, -200, -20);
PARAM(see_quiet_margin, -14, -100, -1);

PARAM(double_ext_margin, 12, 1, 30);
PARAM(triple_ext_margin, 89, 10, 250);

PARAM(lmr_base, 947, 1, 2048);
PARAM(lmr_mul, 387, 1, 512);
PARAM(lmr_improving, 902, 1, 2048);
PARAM(lmr_cut_node, 2094, 1, 4096);
PARAM(lmr_cut_node_no_tt_move, 1165, 1, 2048);
PARAM(lmr_tt_pv, 950, 1, 2048);
PARAM(lmr_tt_depth, 1090, 1, 2048);
PARAM(lmr_tt_score, 603, 1, 2048);
PARAM(lmr_tt_move_noisy, 883, 1, 2048);
PARAM(lmr_in_check, 1070, 1, 2048);
PARAM(lmr_quiet_hist_mul, 96, 1, 300);
PARAM(lmr_noisy_hist_mul, 78, 1, 300);

PARAM(dp_margin, 61, 10, 120);
PARAM(ds_margin, 9, 1, 20);

PARAM(sdr_min_depth, 2, 0, 6);
PARAM(sdr_max_depth, 14, 0, 20);

PARAM(qfp_margin, 101, 40, 280);
PARAM(qsee_margin, -24, -200, 50);

PARAM(hist_bonus_margin, 47, 10, 200);

PARAM(hist_bonus_mult, 292, 1, 1536);
PARAM(hist_bonus_bias, 30, -500, 500);
PARAM(hist_bonus_max, 2470, 1, 4096);
PARAM(hist_malus_mult, 406, 1, 1536);
PARAM(hist_malus_bias, 46, -500, 500);
PARAM(hist_malus_max, 1257, 1, 4096);

PARAM(noisy_hist_bonus_mul, 1164, 1, 2048);
PARAM(noisy_hist_malus_mul, 1188, 1, 2048);
PARAM(quiet_histories_bonus_mul, 905, 1, 2048);
PARAM(quiet_histories_malus_mul, 1166, 1, 2048);

PARAM(pawn_hist_mul, 1085, 1, 2048);
PARAM(cont_hist_mul, 1256, 1, 2048);

PARAM(quiet_checker_bonus, 72, 0, 150);

PARAM(p_corr_weight, 21, 1, 168);
PARAM(m_corr_weight, 17, 1, 168);
PARAM(np_corr_weight, 46, 1, 168);
PARAM(cont_corr_weight, 13, 1, 168);

PARAM(mp_threat_mul, 15, 1, 40);

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

#ifdef TUNE
inline int piece_values_see(PieceType pt) {
#else
constexpr int piece_values_see(PieceType pt) {
#endif
    assert(is_valid(pt) || pt == NO_PIECE_TYPE);

    switch (pt) {
    case PAWN:
        return pawn_value_see;
    case KNIGHT:
        return knight_value_see;
    case BISHOP:
        return bishop_value_see;
    case ROOK:
        return rook_value_see;
    case QUEEN:
        return queen_value_see;
    default:
        return 0;
    }
}

extern std::vector<Param*> params;

} // namespace astra::search
