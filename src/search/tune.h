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
    PARAM(stability_base, 130, 100, 200);
    PARAM(stability_mult, 75, 0, 250);

    PARAM(results_base, 73, 0, 160);
    PARAM(results_mult1, 10, 0, 27);
    PARAM(results_mult2, 26, 0, 45);
    PARAM(results_min, 68, 0, 150);
    PARAM(results_max, 137, 50, 220);

    PARAM(node_mult, 193, 120, 260);
    PARAM(node_base, 54, 10, 90);

    PARAM(lmr_base, 103, 40, 200);
    PARAM(lmr_div, 257, 150, 500);

    PARAM(static_h_mult, -52, -500, -1);
    PARAM(static_h_min, 50, 1, 1000);
    PARAM(static_h_max, 196, 1, 1000);

    PARAM(rfp_depth_mult, 99, 40, 200);
    PARAM(rfp_improving_mult, 92, 40, 200);

    PARAM(rzr_depth_mult, 236, 150, 350);

    PARAM(nmp_depth_mult, 27, 1, 58);
    PARAM(nmp_base, 184, 1, 400);
    PARAM(nmp_eval_div, 215, 50, 350);

    PARAM(hp_div, 8342, 1, 16384);

    PARAM(fp_base, 155, 1, 300);
    PARAM(fp_mult, 95, 5, 200);

    PARAM(see_cap_margin, 94, 5, 200);
    PARAM(see_quiet_margin, 52, -50, 100);

    PARAM(ext_margin, 86, 10, 250);
    PARAM(zws_margin, 54, 10, 160);

    PARAM(hp_depth_mult, 5541, 2500, 12500);
    PARAM(hp_qdiv, 5769, 1, 16384);
    PARAM(hp_cdiv, 3624, 1, 16384);
    PARAM(hbonus_margin, 67, 10, 200);

    PARAM(qfp_margin, 100, 40, 280);

    // history parameters
    PARAM(history_mult, 364, 1, 1536);
    PARAM(history_minus, -66, -500, 500);
    PARAM(max_history_bonus, 1882, 1, 4096);

} // namespace Astra
