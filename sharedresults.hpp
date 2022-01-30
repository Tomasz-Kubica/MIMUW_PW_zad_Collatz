#ifndef SHAREDRESULTS_HPP
#define SHAREDRESULTS_HPP

#include <utility>
#include <deque>
#include <future>
#include <map>

#include "collatz.hpp"
#include "lib/infint/InfInt.h"

class SharedResults
{
public:
    using getResult = std::pair<bool, uint64_t>;

     getResult get(InfInt key) {
         getResult r;
         mut.lock();
         auto it = results.find(key);
         if (it != results.end()) {
             r = {false, 0};
         } else {
             r = {true, it->second};
         }
         mut.unlock();
         return r;
    }

    void set(InfInt key, uint64_t value) {
         mut.lock();
         auto it = results.find(key);
         if (it == results.end()) {
             results.insert({key, value});
         }
         mut.unlock();
     }
private:
    using mapValue = std::pair<uint64_t, std::mutex>;

    std::mutex mut;
    std::map<InfInt, uint64_t> results;
};

#endif