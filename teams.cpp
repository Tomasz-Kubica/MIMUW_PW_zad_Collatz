#include <utility>
#include <deque>
#include <future>

#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"
#include "sharedresults.hpp"

uint64_t collatzWithShared(InfInt input, SharedResults *results) {

}

inline void
teamNewThreadsThread(InfInt input, uint64_t *result, std::mutex *mut,
                     std::condition_variable *condActive,
                     uint64_t *activeThreads) {
    *result = calcCollatz(input);
    mut->lock();
    activeThreads--;
    condActive->notify_one();
    mut->unlock();
}

ContestResult TeamNewThreads::runContestImpl(ContestInput const &contestInput) {
    ContestResult r;
    r.resize(contestInput.size());
    uint64_t activeThreads = 0;
    std::mutex mut;
    std::condition_variable condActive;

    std::vector<std::thread> threads;
    size_t i = 0;
    for (const InfInt &singleInput: contestInput) {
        {
            std::unique_lock<std::mutex> lock(mut);
            if (activeThreads >= getSize()) {
                condActive.wait(lock, [&activeThreads, this] {
                    return activeThreads >= this->getSize();
                });
            }
            activeThreads++;
        }
        auto t = createThread(teamNewThreadsThread, singleInput, &r[i], &mut,
                              &condActive, &activeThreads);
        t.detach();
        i++;
    }

    // oczekiwanie na zakończenie pracy przez wszystkie wątki
    {
        std::unique_lock<std::mutex> lock(mut);
        if (activeThreads > 0) {
            condActive.wait(lock, [&activeThreads] {
                return activeThreads > 0;
            });
        }
    }

    return r;
}

ContestResult
TeamConstThreads::runContestImpl(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamPool::runContest(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamNewProcesses::runContest(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamAsync::runContest(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}
