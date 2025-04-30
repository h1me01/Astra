#pragma once

#include <string>

// #define TUNE

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

    PARAM(stability_base, 131, 100, 200);
    PARAM(stability_mult, 68, 0, 250);

    PARAM(results_base, 65, 0, 160);
    PARAM(results_mult1, 9, 0, 27);
    PARAM(results_mult2, 28, 0, 45);
    PARAM(results_min, 67, 0, 150);
    PARAM(results_max, 140, 50, 220);

    PARAM(node_mult, 185, 120, 260);
    PARAM(node_base, 47, 10, 90);

    PARAM(lmr_base, 105, 40, 200);
    PARAM(lmr_div, 322, 150, 500);

    PARAM(static_h_mult, -42, -500, -1);
    PARAM(static_h_min, 38, 1, 1000);
    PARAM(static_h_max, 229, 1, 1000);

    PARAM(rfp_depth_mult, 94, 40, 200);
    PARAM(rfp_improving_mult, 81, 40, 200);

    PARAM(rzr_depth_mult, 244, 150, 350);

    PARAM(nmp_depth_mult, 28, 1, 58);
    PARAM(nmp_base, 150, 1, 400);
    PARAM(nmp_eval_div, 214, 50, 350);

    PARAM(hp_div, 7948, 1, 16384);

    PARAM(fp_base, 133, 1, 300);
    PARAM(fp_mult, 88, 5, 200);

    PARAM(see_cap_margin, 97, 5, 200);
    PARAM(see_quiet_margin, 83, -50, 100);

    PARAM(ext_margin, 91, 10, 250);
    PARAM(zws_margin, 52, 10, 160);

    PARAM(hp_depth_mult, 6057, 2500, 12500);
    PARAM(hp_qdiv, 6632, 1, 16384);
    PARAM(hp_cdiv, 3573, 1, 16384);
    PARAM(hbonus_margin, 42, 10, 200);

    PARAM(qfp_margin, 112, 40, 280);

    PARAM(history_bonus_mult, 411, 1, 1536);
    PARAM(history_bonus_minus, -33, -500, 500);
    PARAM(max_history_bonus, 2362, 1, 4096);

    PARAM(history_malus_mult, 390, 1, 1536);
    PARAM(history_malus_minus, 42, -500, 500);
    PARAM(max_history_malus, 1857, 1, 4096);

} // namespace Astra
