#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include <iostream>

class Runtime {
   public:
    Runtime()  = default;
    ~Runtime() = default;
    static void start();
};

#endif  // RUNTIME_HPP