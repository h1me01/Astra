#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <string>

// #define TUNE

namespace Astra
{
    struct Param;
    
    inline std::vector<Param *> params;

    struct Param
    {
        std::string name;
        int value;
        int min, max;
        int step;

        Param(std::string name, int value, int min, int max, int step);

        operator int() const;
    };

    void paramsToUCI();

    void setParam(const std::string &name, int value);

    void paramsToSpsa();

    void paramsToJSON();

#ifdef TUNE
#define PARAM(name, value, min, max, step) inline Param name(#name, value, min, max, step)
#else
#define PARAM(name, value, min, max, step) constexpr int name = value
#endif

    // search parameters
    PARAM(lmr_base, 89, 80, 120, 8);
    PARAM(lmr_div, 176, 150, 200, 8);

    PARAM(asp_depth, 9, 5, 9, 1);
    PARAM(asp_window, 10, 5, 30, 5);

    PARAM(rzr_depth_mult, 196, 150, 250, 15);
    PARAM(rfp_depth_mult, 67, 60, 110, 8);

    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_depth_div, 7, 3, 15, 1);
    PARAM(nmp_div, 222, 190, 235, 8);

    PARAM(probcut_margin, 153, 130, 180, 15);

    PARAM(see_cap_margin, 96, 70, 120, 8);
    PARAM(see_quiet_margin, 99, 70, 120, 8);

    PARAM(fp_base, 155, 120, 180, 10);
    PARAM(fp_mult, 97, 70, 150, 10);

    PARAM(ext_margin, 138, 45, 150, 12);

    PARAM(zws_margin, 83, 60, 90, 8);

    PARAM(hp_margin, 4499, 2500, 5000, 400);
    PARAM(hp_div, 8016, 7000, 8500, 400);
    PARAM(hbonus_margin, 75, 65, 80, 5);

    PARAM(qfp_margin, 98, 60, 150, 15);
    
    // history parameters
    PARAM(history_mult, 148, 130, 180, 8);
    PARAM(history_minus, 50, 10, 50, 10);
    PARAM(max_history_bonus, 1607, 1100, 1800, 50);

} // namespace Astra

#endif // SEARCHPARAMS_H
