#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <iostream>
#include <string>

// #define TUNE

namespace Astra
{
    struct Param;

    struct Param
    {
        std::string name;
        int value;
        int min, max;

        Param(std::string name, int value, int min, int max);

        operator int() const;
    };

    void paramsToUCI();

    void setParam(const std::string &name, int value);

    void paramsToSpsa();

#ifdef TUNE
#define PARAM(name, value, min, max) Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

    // search parameters
    PARAM(lmr_base, 95, 50, 150);
    PARAM(lmr_div, 152, 150, 250);
    PARAM(lmr_depth, 2, 2, 5);
    PARAM(lmr_min_moves, 3, 1, 5);

    PARAM(delta_margin, 588, 400, 900);

    PARAM(iir_depth, 2, 2, 4);

    PARAM(razor_margin, 134, 60, 200);
    PARAM(rzr_depth, 5, 3, 7);

    PARAM(rfp_depth_mult, 47, 30, 80);
    PARAM(rfp_impr_bonus, 40, 30, 100);
    PARAM(rfp_depth, 5, 3, 9);

    PARAM(snmp_depth_mult, 65, 50, 90);
    PARAM(snmp_depth, 3, 2, 7);

    PARAM(nmp_depth, 3, 2, 5);
    PARAM(nmp_base, 5, 3, 5);
    PARAM(nmp_depth_div, 5, 3, 7);
    PARAM(nmp_min, 4, 2, 6);
    PARAM(nmp_div, 214, 205, 215);

    PARAM(prob_cut_margin, 136, 100, 190);

    PARAM(pv_see_cap_margin, 97, 70, 110);
    PARAM(pv_see_cap_depth, 4, 4, 8);

    PARAM(pv_see_quiet_margin, 79, 30, 95);
    PARAM(pv_see_quiet_depth, 6, 6, 9);

    PARAM(lmp_depth, 6, 4, 7);
    PARAM(lmp_count_base, 3, 3, 6);

    PARAM(fp_depth, 9, 3, 13);
    PARAM(fp_base, 139, 100, 200);
    PARAM(fp_mult, 96, 80, 120);

    PARAM(hh_bonus_mult, 155, 100, 200);
    PARAM(max_hh_bonus, 2047, 1900, 2200);

    PARAM(ch_bonus_mult, 9, 5, 15);
    PARAM(max_ch_bonus, 1582, 1400, 1800);

    PARAM(asp_window, 33, 10, 50);

} // namespace Astra

#endif // SEARCHPARAMS_H
