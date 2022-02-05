#include <utility>
#include <deque>
#include <future>

#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"
#include "sharedresults.hpp"

uint64_t
collatzWithShared(InfInt input, std::shared_ptr<SharedResults> results) {
    //std::cout << "start " << std::this_thread::get_id() << std::endl;
    assert(input > 0);
    uint64_t count = 0;
    std::vector<std::pair<InfInt, uint64_t>> reserved;
    while (input > 1) {
        //std::cout << input << std::endl;
        SharedResults::getResult getRes = results->getOrReserve(input);
        //std::cout << "get" << std::endl;
        if (getRes.first) {
            count += getRes.second;
            //std::cout << "break" << std::endl;
            break;
        } else {
            reserved.push_back({input, count});
            //std::cout << "reserved" << std::endl;
        }
        if (input % 2 == 1) {
            input *= 3;
            input += 1;
        } else {
            input /= 2;
        }
        count++;
    }
    for (auto res: reserved) {
        results->setReserved(res.first, count - res.second);
    }
    //std::cout << "koniec " << std::this_thread::get_id() << std::endl;
    return count;
}

inline void
teamNewThreadsThread(InfInt input, uint64_t *result, std::mutex *mut,
                     std::condition_variable *condActive,
                     uint64_t *activeThreads,
                     std::shared_ptr<SharedResults> shared) {
    if (shared)
        *result = collatzWithShared(input, shared);
    else
        *result = calcCollatz(input);
    mut->lock();
    (*activeThreads)--;
    condActive->notify_one();
    mut->unlock();
}

inline void
teamConstThreadsThread(ContestInput::const_iterator input,
                       ContestResult::iterator result,
                       size_t to_proces,
                       std::shared_ptr<SharedResults> shared
//                       std::mutex *mut,
                       //std::condition_variable *cv,
                       //int *active
                       ) {
    //std::cout << "start: " << std::this_thread::get_id() << std::endl;
    for (int i = 0; i < to_proces; i++) {
        if (shared)
            *result = collatzWithShared(*input, shared);
        else
            *result = calcCollatz(*input);
        input++;
        result++;
    }
//    mut->lock();
//    (*active)--;
//    if (*active == 0)
//        cv->notify_all();
//    mut->unlock();
    //std::cout << "koniec: " << std::this_thread::get_id() << std::endl;
}

ContestResult TeamNewThreads::runContestImpl(ContestInput const &contestInput) {
    ContestResult r;
    r.resize(contestInput.size());
    uint64_t activeThreads = 0;
    std::mutex mut;
    std::condition_variable condActive;

    size_t i = 0;
    for (const InfInt &singleInput: contestInput) {
        {
            std::unique_lock<std::mutex> lock(mut);
            //std::cout << "czeka na start wątku " << activeThreads << std::endl;
            if (activeThreads >= getSize()) {
                condActive.wait(lock, [&activeThreads, this] {
                    return activeThreads < this->getSize();
                });
//                while (activeThreads >= this->getSize()) {
//                    std::cout << "kontynuuje czekanie " << activeThreads << std::endl;
//                    condActive.wait(lock);
//                }
            }
            activeThreads++;
        }
        std::thread t;
        t = createThread(teamNewThreadsThread, singleInput, &r[i], &mut,
                         &condActive, &activeThreads, this->getSharedResults());
        t.detach();
        i++;
    }

    // oczekiwanie na zakończenie pracy przez wszystkie wątki
    {
        std::unique_lock<std::mutex> lock(mut);
        if (activeThreads > 0) {
            condActive.wait(lock, [&activeThreads] {
                return activeThreads == 0;
            });
        }
    }
    //for (size_t j = 0; j < contestInput.size(); j++) {
    //    std::cout << contestInput[j] << ' ' << r[j] << '\n';
    //}

    return r;
}

ContestResult
TeamConstThreads::runContestImpl(ContestInput const &contestInput) {
    ContestResult r;
    r.resize(contestInput.size());
    std::mutex mut;
    std::condition_variable cv;
    int active_threads = this->getSize();
    size_t forEach = contestInput.size() / this->getSize();
    size_t additional = contestInput.size() - (forEach * this->getSize());
    ContestInput::const_iterator inputIt = contestInput.begin();
    ContestResult::iterator resIt = r.begin();
    std::vector<std::thread> threads;
    //std::cout << "startowanie wątków" << std::endl;
    for (size_t i = 0; i < this->getSize(); i++) {
        size_t s = forEach;
        if (additional > 0) {
            s++;
            additional--;
        }
        //std::cout << "przed startem wątku" << std::endl;
        std::thread t = createThread(teamConstThreadsThread,
                                     inputIt,
                                     resIt,
                                     s,
                                     this->getSharedResults()
//                                     &mut,
//                                     &cv,
//                                     &active_threads
                                     );
        threads.push_back(std::move(t));
        //std::cout << "po starcie" << std::endl;
        inputIt = inputIt + s;
        resIt = resIt + s;
        //std::cout << "po inkrementacji" << std::endl;
    }
//    {
//        std::unique_lock<std::mutex> lock(mut);
//        std::cout << "main lock" << std::endl;
//        cv.wait(lock, [&active_threads]{return active_threads == 0;});
//    }
    for (auto &i : threads)
        i.join();
//    for (auto a : contestInput)
//        std::cout << a << ' ';
//    std::cout << std::endl;
//    for (auto a : r)
//        std::cout << a << ' ';
    //std::cout << std::endl;
    //std::cout << "main end" << std::endl;
    return r;
}

ContestResult TeamPool::runContest(ContestInput const &contestInput) {
    using f_t = std::future<uint64_t>;
    ContestResult r;
    std::vector<f_t> results;
    auto shared = this->getSharedResults();
    for (auto i : contestInput) {
        if (shared)
            results.push_back(this->pool.push(collatzWithShared, i, shared));
        else
            results.push_back(this->pool.push(calcCollatz, i));
    }
    for (auto &i : results)
        r.push_back(i.get());
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
