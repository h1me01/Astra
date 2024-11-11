#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <string>

 #define TUNE

namespace Astra
{
    struct Param;

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
#define PARAM(name, value, min, max, step) Param name(#name, value, min, max, step)
#else
#define PARAM(name, value, min, max, step) constexpr int name = value
#endif

} // namespace Astra

#endif // SEARCHPARAMS_H
