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
    PARAM(stability_base, 141, 100, 200);
    PARAM(stability_mult, 50, 0, 250);

    PARAM(results_base, 84, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 81, 0, 150);
    PARAM(results_max, 146, 50, 220);

    PARAM(node_mult, 200, 120, 260);
    PARAM(node_base, 60, 10, 90);

    // search parameters
    PARAM(lmr_base, 120, 40, 200);
    PARAM(lmr_div, 186, 150, 500);

    PARAM(rfp_depth_mult, 93, 40, 200);
    PARAM(rfp_improving_mult, 93, 40, 200);

    PARAM(rzr_depth_mult, 237, 150, 350);

    PARAM(nmp_depth_mult, 33, 27, 34);
    PARAM(nmp_base, 202, 160, 220);
    PARAM(nmp_eval_div, 219, 100, 400);

    PARAM(see_cap_margin, 94, 80, 110);
    PARAM(see_quiet_margin, 50, 30, 60);

    PARAM(fp_base, 159, 40, 300);
    PARAM(fp_mult, 110, 40, 200);

    PARAM(ext_margin, 86, 10, 250);
    PARAM(zws_margin, 74, 10, 160);

    PARAM(hp_depth_mult, 4901, 2500, 12500);
    PARAM(hp_qdiv, 7885, 1, 16384);
    PARAM(hp_cdiv, 6156, 1, 16384);
    PARAM(hbonus_margin, 80, 10, 200);

    PARAM(qfp_margin, 98, 40, 280);

    // history parameters
    PARAM(history_mult, 131, 1, 1536);
    PARAM(history_minus, 26, -500, 500);
    PARAM(max_history_bonus, 1483, 1, 4096);

} // namespace Astra
