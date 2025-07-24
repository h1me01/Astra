#pragma once

#include <string>
#include <vector>

 #define TUNE

namespace Astra {

struct Param {
    std::string name;
    int value, min, max;

    Param(std::string name, int value, int min, int max);

    operator int() const {
        return value;
    }
};

inline std::vector<Param *> params;

void set_param(const std::string &name, int value);
void params_to_spsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

PARAM(pawn_value, 103, 50, 150);
PARAM(knight_value, 327, 200, 400);
PARAM(bishop_value, 326, 200, 400);
PARAM(rook_value, 493, 400, 600);
PARAM(queen_value, 994, 700, 1100);

PARAM(pawn_value_see, 99, 50, 150);
PARAM(knight_value_see, 332, 200, 400);
PARAM(bishop_value_see, 318, 200, 400);
PARAM(rook_value_see, 507, 400, 600);
PARAM(queen_value_see, 1006, 700, 1100);

PARAM(tm_stability_base, 141, 100, 200);
PARAM(tm_stability_mult, 48, 0, 250);
PARAM(tm_stability_max, 8, 1, 20);

PARAM(tm_results_base, 59, 0, 160);
PARAM(tm_results_mult1, 13, 0, 27);
PARAM(tm_results_mult2, 27, 0, 45);
PARAM(tm_results_min, 64, 0, 150);
PARAM(tm_results_max, 132, 50, 220);

PARAM(tm_node_mult, 199, 120, 260);
PARAM(tm_node_base, 51, 10, 90);

PARAM(lmr_base, 112, 40, 200);
PARAM(lmr_div, 304, 150, 500);
PARAM(lmr_min_moves, 3, 1, 3);

PARAM(static_h_mult, -46, -500, -1);
PARAM(static_h_min, 34, 1, 1000);
PARAM(static_h_max, 247, 1, 1000);

PARAM(rzr_depth, 6, 2, 20);
PARAM(rzr_depth_mult, 260, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 105, 40, 200);
PARAM(rfp_improving_mult, 93, 40, 200);

PARAM(nmp_depth_mult, 25, 1, 58);
PARAM(nmp_base, 159, 1, 400);
PARAM(nmp_eval_div, 227, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(prob_cut_margin, 218, 1, 500);

PARAM(hp_depth, 4, 1, 15);
PARAM(hp_div, 8156, 1, 16384);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 94, 1, 300);
PARAM(fp_mult, 111, 5, 200);

PARAM(see_cap_margin, 93, 5, 200);
PARAM(see_quiet_margin, 22, -100, 100);

PARAM(double_ext_margin, 14, 1, 30);
PARAM(tripple_ext_margin, 74, 10, 250);
PARAM(zws_margin, 65, 10, 160);

PARAM(hp_depth_mult, 6337, 2500, 12500);
PARAM(hp_qdiv, 7640, 1, 16384);
PARAM(hp_cdiv, 4681, 1, 16384);
PARAM(hbonus_margin, 67, 10, 200);

PARAM(qfp_margin, 104, 40, 280);
PARAM(qsee_margin, -4, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 5, 2, 6);

PARAM(history_bonus_mult, 286, 1, 1536);
PARAM(history_bonus_minus, -28, -500, 500);
PARAM(max_history_bonus, 2456, 1, 4096);

PARAM(history_malus_mult, 412, 1, 1536);
PARAM(history_malus_minus, 98, -500, 500);
PARAM(max_history_malus, 1790, 1, 4096);

} // namespace Astra
