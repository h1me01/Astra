#pragma once

#include <string>

 #define TUNE

namespace Astra
{

    struct Param
    {
        std::string name;
        int value, min, max;

        Param(std::string name, int value, int min, int max);

        operator int() const { return value; }
    };

    inline std::vector<Param *> params;

    void setParam(const std::string &name, int value);
    void paramsToSpsa();

#ifdef TUNE
#define PARAM(name, value, min, max) inline Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

    PARAM(stability_base, 130, 100, 200);
    PARAM(stability_mult, 68, 0, 250);

    PARAM(results_base, 64, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 27, 0, 45);
    PARAM(results_min, 69, 0, 150);
    PARAM(results_max, 140, 50, 220);

    PARAM(node_mult, 187, 120, 260);
    PARAM(node_base, 49, 10, 90);

    PARAM(lmr_base, 106, 40, 200);
    PARAM(lmr_div, 298, 150, 500);

    PARAM(static_h_mult, -58, -500, -1);
    PARAM(static_h_min, 47, 1, 1000);
    PARAM(static_h_max, 214, 1, 1000);

    PARAM(rfp_depth_mult, 100, 40, 200);
    PARAM(rfp_improving_mult, 83, 40, 200);

    PARAM(rzr_depth_mult, 245, 150, 350);

    PARAM(nmp_depth_mult, 28, 1, 58);
    PARAM(nmp_base, 139, 1, 400);
    PARAM(nmp_eval_div, 209, 50, 350);

    PARAM(hp_div, 8099, 1, 16384);

    PARAM(fp_base, 135, 1, 300);
    PARAM(fp_mult, 89, 5, 200);

    PARAM(see_cap_margin, 93, 5, 200);
    PARAM(see_quiet_margin, 80, -50, 100);

    PARAM(ext_margin, 90, 10, 250);
    PARAM(zws_margin, 51, 10, 160);

    PARAM(hp_depth_mult, 6377, 2500, 12500);
    PARAM(hp_qdiv, 6380, 1, 16384);
    PARAM(hp_cdiv, 3935, 1, 16384);
    PARAM(hbonus_margin, 54, 10, 200);

    PARAM(qfp_margin, 105, 40, 280);

    PARAM(history_bonus_mult, 428, 1, 1536);
    PARAM(history_bonus_minus, -36, -500, 500);
    PARAM(max_history_bonus, 2258, 1, 4096);

    PARAM(history_malus_mult, 395, 1, 1536);
    PARAM(history_malus_minus, 24, -500, 500);
    PARAM(max_history_malus, 1866, 1, 4096);

} // namespace Astra
