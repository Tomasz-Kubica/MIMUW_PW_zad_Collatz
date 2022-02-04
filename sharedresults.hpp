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

     getResult getOrReserve(InfInt key) {
         getResult r;
         //std::cout << "pre lock " << std::this_thread::get_id() << std::endl;
         std::unique_lock<std::mutex> lock(mut);
         //std::cout << "lock" << std::endl;
         auto it = results.find(key);
         //std::cout << "pozyskano iterator" << std::endl;
         if (it == results.end()) {
             //std::cout << "nie ma" << std::endl;
             r = {false, 0};
             std::shared_ptr<std::condition_variable> cv(new std::condition_variable);
             results.insert({key, {{0, false}, cv}});
         } else {
             //std::cout << "jest" << std::endl;
             auto cv = it->second.second;
             cv->wait(lock , [&key, this]{return this->results[key].first.second;});
             r = {true, results[key].first.first};
         }
         return r;
    }

    void setReserved(InfInt key, uint64_t value) {
        //std::cout << "setting" << std::endl;
        std::unique_lock<std::mutex> lock(mut);
        auto it = results.find(key);
        assert(it != results.end());
        mapValue v = it->second;
        v.first = {value, true};
        results[key] = v;
        v.second->notify_all();
    }
private:
    using mapValue = std::pair<std::pair<uint64_t, bool>, std::shared_ptr<std::condition_variable>>;

    std::mutex mut;
    std::map<InfInt, mapValue> results;
};

#endif