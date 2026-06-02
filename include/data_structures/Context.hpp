#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <string>
#include <vector>

struct Context {
    double                   timestamp;
    std::vector<std::string> markers;
};

#endif  // CONTEXT_HPP