#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include <iostream>

class Runtime {
   public:
    Runtime()  = default;
    ~Runtime() = default;

    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&)                 = delete;
    Runtime&    operator=(Runtime&&)   = delete;
    static void start();
};

#endif  // RUNTIME_HPP