#include <utility>
#include <deque>
#include <future>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#include "teams.hpp"
#include "contest.hpp"
#include "collatz.hpp"
#include "sharedresults.hpp"
#include "err.h"
#include "shared_mem_info.h"

uint64_t
collatzWithShared(InfInt input, std::shared_ptr<SharedResults> results) {
    assert(input > 0);
    uint64_t count = 0;
    std::vector<std::pair<InfInt, uint64_t>> reserved;
    while (input > 1) {
        SharedResults::getResult getRes = results->getOrReserve(input);
        if (getRes.first) {
            count += getRes.second;
            break;
        } else {
            reserved.push_back({input, count});
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
                       ) {
    for (int i = 0; i < to_proces; i++) {
        if (shared)
            *result = collatzWithShared(*input, shared);
        else
            *result = calcCollatz(*input);
        input++;
        result++;
    }
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
            if (activeThreads >= getSize()) {
                condActive.wait(lock, [&activeThreads, this] {
                    return activeThreads < this->getSize();
                });
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
    for (size_t i = 0; i < this->getSize(); i++) {
        size_t s = forEach;
        if (additional > 0) {
            s++;
            additional--;
        }
        std::thread t = createThread(teamConstThreadsThread,
                                     inputIt,
                                     resIt,
                                     s,
                                     this->getSharedResults()
                                     );
        threads.push_back(std::move(t));
        inputIt = inputIt + s;
        resIt = resIt + s;
    }

    for (auto &i : threads)
        i.join();

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

void saveFromPipe(int desc, ContestResult &result) {
    uint64_t singleResult;
    int readOut = 1;
    while (readOut) {
        readOut = read(desc, &singleResult, sizeof(uint64_t));
        if (readOut == -1)
            assert(false);
        else if (readOut != 0) {
            result.push_back(singleResult);
        }
    }
}

ContestResult TeamNewProcesses::runContest(ContestInput const &contestInput) {
    ContestResult r;

    std::vector<int> readDesc;
    size_t activeProcesses = 0;
    size_t savedResults = 0;

    int fd_memory = -1;
    sem_t *sem;
    if (getSharedResults()) {
        fd_memory = shm_open(SHM_NAME, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd_memory == -1) syserr("error in shm_open");
        ftruncate(fd_memory, SHM_SIZE);

        bool *mapped_mem;
        mapped_mem = (bool*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_memory, 0);
        if(mapped_mem == MAP_FAILED)
            syserr("error in mmap in teams");
        *mapped_mem = false;
        munmap(mapped_mem, SHM_SIZE);

        sem = sem_open(SEM_NAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0);
        if (sem == SEM_FAILED)
            syserr("sem_open teams");
        if (sem_post(sem) == -1)
            syserr("sem_post teams");
        if (sem_close(sem))
            syserr("sem_close teams");
    }

    for (auto singleInput : contestInput) {
        if (activeProcesses == getSize()) {
            auto desc = readDesc[savedResults];
            savedResults++;
            saveFromPipe(desc, r);
            if (close(desc) == -1)
                assert(false);
            if (wait(0) == -1) syserr("err in wait"); // assert(false);
            activeProcesses--;
        }
        activeProcesses++;
        int pipe1_desc[2];
        int pipe2_desc[2];
        if (pipe(pipe1_desc) == -1)
            syserr("err in making pipe1");
        if (pipe(pipe2_desc) == -1)
            syserr("err in making pipe2");
        switch (fork()) {
            case -1:
                syserr("error in fork");
            case 0:
                if (dup2(pipe1_desc[PIPE_READ], 0) == -1) // input dziecka
                    syserr("error in dup2 input");
                if (dup2(pipe2_desc[PIPE_WRITE], 1) == -1) // output dziecka
                    syserr("error in dup2 output");

                // zamykanie niepotrzebnych deskryptorów
                if (close(pipe1_desc[PIPE_READ]) == -1)
                    syserr("error in close(pipe1_desc[0])");
                if (close(pipe1_desc[PIPE_WRITE]) == -1)
                    syserr("error in close(pipe1_desc[1])");
                if (close(pipe2_desc[PIPE_READ]) == -1)
                    syserr("error in close(pipe2_desc[0])");
                if (close(pipe2_desc[PIPE_WRITE]) == -1)
                    syserr("error in close(pipe2_desc[1])");

                execlp("./new_process", "new_process", (char*)NULL);
                syserr("error in execlp");
                break;
            default:
                if (close(pipe1_desc[PIPE_READ]) == -1)
                    syserr("error in close(pipe1_desc[0])");
                if (close(pipe2_desc[PIPE_WRITE]) == -1)
                    syserr("error in close(pipe2_desc[1])");

                readDesc.push_back(pipe2_desc[PIPE_READ]);
                std::string asString = singleInput.toString();
                char is_shared = (bool)getSharedResults() ? 't' : 'f';
                write(pipe1_desc[PIPE_WRITE], &is_shared, sizeof(char));
                if (write(pipe1_desc[PIPE_WRITE], asString.c_str(), sizeof(char) * asString.size()) == -1)
                    syserr("error in writing input to pipe");
                if (close(pipe1_desc[PIPE_WRITE]) == -1)
                    assert(false);
                break;
        }
    }

    while (activeProcesses > 0) {
        wait(0);
        activeProcesses--;
    }

    while (savedResults < readDesc.size()) {
        auto desc = readDesc[savedResults];
        savedResults++;
        saveFromPipe(desc, r);
        if (close(desc) == -1)
            assert(false);
    }

    if (getSharedResults()) {
        close(fd_memory);
        shm_unlink(SHM_NAME);
        if (sem_unlink(SEM_NAME) == -1)
            syserr("error in sem_unlink");
    }

    return r;
}

ContestResult TeamConstProcesses::runContest(ContestInput const &contestInput) {
    ContestResult r;
    //TODO
    return r;
}

ContestResult TeamAsync::runContest(ContestInput const &contestInput) {
    using f_t = std::future<uint64_t>;
    ContestResult r;
    std::vector<f_t> results;
    auto shared = this->getSharedResults();
    for (auto i : contestInput) {
        if (shared)
            results.push_back(std::async(std::launch::async, collatzWithShared, i, shared));
        else
            results.push_back(std::async(std::launch::async, calcCollatz, i));
    }
    for (auto &i : results)
        r.push_back(i.get());
    return r;
}
