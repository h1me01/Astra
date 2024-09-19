#ifndef OPTIONS_H
#define OPTIONS_H

#include <string>
#include <utility>

namespace UCI {

    class Option {
    public:
        Option() : min(0), max(0) {}

        Option(std::string type, std::string default_val, std::string val, int min, int max) :
        type(std::move(type)), default_val(std::move(val)), val(std::move(default_val)), min(min), max(max) {};

        Option &operator=(const std::string &v) {
            val = v;
            return *this;
        }

        const std::string &getValue() const {
            return val;
        }

        const std::string &getDefaultValue() const {
            return default_val;
        }

        const std::string &getType() const {
            return type;
        }

        int getMin() const {
            return min;
        }

        int getMax() const {
            return max;
        }

    private:
        std::string type, val, default_val;
        int min, max;
    };

} // namespace UCI

#endif //OPTIONS_H
