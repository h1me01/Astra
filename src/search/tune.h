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

    // time management
    PARAM(stability_base, 131, 100, 200);
    PARAM(stability_mult, 85, 0, 250);

    PARAM(results_base, 75, 0, 160);
    PARAM(results_mult1, 11, 0, 27);
    PARAM(results_mult2, 27, 0, 45);
    PARAM(results_min, 73, 0, 150);
    PARAM(results_max, 142, 50, 220);

    PARAM(node_mult, 198, 120, 260);
    PARAM(node_base, 55, 10, 90);

    // search parameters
    PARAM(lmr_base, 104, 40, 200);
    PARAM(lmr_div, 199, 150, 500);

    PARAM(rfp_depth_mult, 98, 40, 200);
    PARAM(rfp_improving_mult, 101, 40, 200);

    PARAM(rzr_depth_mult, 228, 150, 350);

    PARAM(nmp_depth_mult, 27, 27, 34);
    PARAM(nmp_base, 185, 160, 220);
    PARAM(nmp_eval_div, 227, 100, 400);

    PARAM(see_cap_margin, 96, 80, 110);
    PARAM(see_quiet_margin, 50, 30, 60);

    PARAM(fp_base, 150, 40, 300);
    PARAM(fp_mult, 105, 40, 200);

    PARAM(ext_margin, 108, 10, 250);
    PARAM(zws_margin, 62, 10, 160);

    PARAM(hp_depth_mult, 5633, 2500, 12500);
    PARAM(hp_qdiv, 6599, 1, 16384);
    PARAM(hp_cdiv, 5274, 1, 16384);
    PARAM(hbonus_margin, 71, 10, 200);

    PARAM(qfp_margin, 101, 40, 280);

    // history parameters
    PARAM(history_mult, 296, 1, 1536);
    PARAM(history_minus, -13, -500, 500);
    PARAM(max_history_bonus, 1633, 1, 4096);

} // namespace Astra
