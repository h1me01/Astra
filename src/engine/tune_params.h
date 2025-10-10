#pragma once

#include <string>
#include <vector>

#define TUNE

namespace Engine {

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

PARAM(pawn_value_see, 104, 50, 150);
PARAM(knight_value_see, 312, 200, 400);
PARAM(bishop_value_see, 336, 200, 400);
PARAM(rook_value_see, 495, 400, 675);
PARAM(queen_value_see, 995, 700, 1500);

PARAM(tm_stability_base, 153, 100, 200);
PARAM(tm_stability_mult, 38, 0, 250);
PARAM(tm_stability_max, 5, 1, 20);

PARAM(tm_results_base, 70, 0, 160);
PARAM(tm_results_mult1, 16, 0, 27);
PARAM(tm_results_mult2, 28, 0, 45);
PARAM(tm_results_min, 90, 0, 150);
PARAM(tm_results_max, 139, 50, 220);

PARAM(tm_node_mult, 210, 120, 260);
PARAM(tm_node_base, 60, 10, 90);

PARAM(lmr_base, 115, 40, 200);
PARAM(lmr_div, 299, 150, 500);

PARAM(static_h_mult, -52, -500, -1);
PARAM(static_h_min, -81, -1000, -1);
PARAM(static_h_max, 271, 1, 1000);

PARAM(rzr_depth_mult, 268, 150, 350);

PARAM(rfp_depth, 11, 2, 20);
PARAM(rfp_depth_mult, 106, 40, 200);
PARAM(rfp_improving_mult, 93, 40, 200);

PARAM(nmp_depth_mult, 26, 1, 58);
PARAM(nmp_base, 180, 1, 400);
PARAM(nmp_eval_div, 195, 50, 350);
PARAM(nmp_rbase, 4, 1, 5);
PARAM(nmp_rdepth_div, 3, 1, 6);
PARAM(nmp_rmin, 4, 1, 10);

PARAM(probcut_margin, 253, 1, 500);

PARAM(history_div, 8306, 1, 16384);

PARAM(hp_depth, 6, 1, 15);
PARAM(hp_depth_mult, -5862, -12500, -2500);

PARAM(fp_depth, 9, 2, 20);
PARAM(fp_base, 100, 1, 300);
PARAM(fp_mult, 109, 5, 200);

PARAM(see_cap_margin, -101, -200, -20);
PARAM(see_quiet_margin, -18, -100, -1);

PARAM(double_ext_margin, 14, 1, 30);
PARAM(tripple_ext_margin, 90, 10, 250);
PARAM(zws_margin, 44, 10, 160);

PARAM(quiet_history_div, 7746, 1, 16384);
PARAM(noisy_history_div, 4792, 1, 16384);

PARAM(qfp_margin, 112, 40, 280);
PARAM(qsee_margin, -19, -200, 50);

PARAM(asp_delta, 11, 1, 30);
PARAM(asp_depth, 4, 2, 6);

PARAM(history_bonus_margin, 52, 10, 200);
PARAM(history_bonus_mult, 323, 1, 1536);
PARAM(history_bonus_minus, -31, -500, 500);
PARAM(max_history_bonus, 2262, 1, 4096);

PARAM(history_malus_mult, 324, 1, 1536);
PARAM(history_malus_minus, 121, -500, 500);
PARAM(max_history_malus, 1762, 1, 4096);

} // namespace Engine
