#ifndef SEARCHPARAMS_H
#define SEARCHPARAMS_H

#include <iostream>
#include <string>

//#define TUNE

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

    void setParam(const std::string& name, int value);

    std::string paramsToSpsa();

#ifdef TUNE
#define PARAM(name, value, min, max) Param name(#name, value, min, max)
#else
#define PARAM(name, value, min, max) constexpr int name = value
#endif

} // namespace Astra

#endif //SEARCHPARAMS_H
