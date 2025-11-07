#include "../chess/misc.h"
#include "../engine/threads.h"
#include "../third_party/fathom/src/tbprobe.h"

#include "uci_options.h"

namespace UCI {

// helper

std::string print_option_type(OptionType type) {
    switch(type) {
    case OptionType::SPIN:
        return "spin";
    case OptionType::STRING:
        return "string";
    default:
        return "unknown";
    }
}

std::string tolower(const std::string &str) {
    std::string result;
    for(const char c : str)
        result += std::tolower(c);
    return result;
}

// Options

void Options::print() const {
    for(const auto &elem : options) {
        Option option = elem.second;
        const std::string &value = option;

        std::cout << "option name " << elem.first                     //
                  << " type " << print_option_type(option.get_type()) //
                  << " default " << (value.empty() ? "<empty>" : value);

        if(option.get_min() != 0 && option.get_max() != 0) {
            std::cout << " min " << option.get_min() //
                      << " max " << option.get_max() //
                      << std::endl;
        } else {
            std::cout << std::endl;
        }
    }

    for(auto param : Engine::params) {
        std::cout << "option name " << param->name         //
                  << " type spin default " << param->value //
                  << " min " << param->min                 //
                  << " max " << param->max << std::endl;
    }
}

void Options::set(const std::string &info) {
    std::vector<std::string> tokens = split(info, ' ');
    std::string &name = tokens[2];
    std::string &value = tokens[4];

    if(tokens.size() < 5 || tokens[1] != "name" || tokens[3] != "value") {
        std::cout << "Invalid option " << name << std::endl;
        return;
    }

    if(value == "<empty>" || value.empty())
        return;

    bool found_tune_param = false;
#ifdef TUNE
    found_tune_param = Engine::set_param(name, std::stoi(value));
    Engine::init_reductions();
#endif

    if(options.count(name))
        options[name].set(value);
    else if(!found_tune_param)
        std::cout << "Unknown option " << name << std::endl;

    apply(name);
}

void Options::update_syzygy_path(const std::string &path) {
    if(!path.empty() && path != "<empty>") {
        bool success = tb_init(path.c_str());

        if(success && TB_LARGEST > 0)
            std::cout << "info string Successfully loaded syzygy path" << std::endl;
        else
            std::cout << "info string Failed to load syzygy path " << path << std::endl;
    }
}

void Options::apply(const std::string &name) {
    if(name == "SyzygyPath") {
        update_syzygy_path(options[name]);
    } else if(tolower(name) == "hash") {
        Engine::tt.init(std::stoi(options[name]));
    } else if(tolower(name) == "threads") {
        const int num_workers = std::stoi(get("Threads"));
        Engine::tt.set_worker_count(num_workers);
        Engine::threads.set_count(num_workers);
    }
}

} // namespace UCI
