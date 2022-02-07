#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "lib/infint/InfInt.h"

#include "collatz.hpp"

int main() {
//    int x = 0;
//    while (true) {
//        x++;
//        std::cerr << x << std::endl;
//    }
    std::vector<InfInt> inputs;
    std::string s;
    InfInt a = 1;
    while (std::cin >> a) {
        inputs.push_back(a);
//        std::cerr << a << std::endl;
    }

    for (auto i : inputs) {
        uint64_t r = calcCollatz(i);
        write(1, &r, sizeof(uint64_t));
        //std::cerr << i << " " << r << std::endl;
    }
    return 0;
}