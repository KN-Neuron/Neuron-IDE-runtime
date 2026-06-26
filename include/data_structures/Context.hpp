#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <string>
#include <vector>

struct Context {
    double                    timestamp = 0.0;
    std::vector<std::string>* markers   = nullptr;
};

#endif  // CONTEXT_HPP