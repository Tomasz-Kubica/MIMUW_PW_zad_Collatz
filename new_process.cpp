#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "lib/infint/InfInt.h"
#include "cstring"

#include "collatz.hpp"
#include "err.h"
#include "shared_mem_info.h"

struct SharedList {
    uint64_t result;
    bool is_next;
    size_t number_length;
};

SharedList* listGetNext(SharedList *l) {
    assert(l->is_next);
    return (SharedList*)((char*)(l + 1) + l->number_length);
}

char* listGetNumber(SharedList *l) {
    return (char*)(l + 1);
}

sem_t *sem;
SharedList *list;

std::pair<bool, uint64_t> searchList(InfInt x) {
    if (sem_wait(sem) == -1)
        syserr("error in wait");

    SharedList *l;
    l = list;
    std::string str = x.toString();

    while (l != nullptr) {
        if (strcmp(listGetNumber(l), str.c_str()) == 0) {
            //std::cerr << listGetNumber(l) << " ^ " << str.c_str() << std::endl;
            uint64_t r = l->result;
            if (sem_post(sem) == -1)
                syserr("error in post");
            return {true, r};
        }
        if (l->is_next)
            l = listGetNext(l);
        else
            l = nullptr;
    }

    if (sem_post(sem) == -1)
        syserr("error in post");

    return {false, 0};
}

void addToList(InfInt x, uint64_t r) {
    if (sem_wait(sem) == -1)
        syserr("error in wait");

    SharedList *listEnd = list;
    while (listEnd->is_next)
        listEnd = listGetNext(listEnd);

    listEnd->is_next = true;
    listEnd = listGetNext(listEnd);
    std::string toString = x.toString();
    strcpy(listGetNumber(listEnd), toString.c_str());
    *listEnd = {r, false, toString.size() + 1};

    if (sem_post(sem) == -1)
        syserr("error in post");
}

uint64_t collatzWithShared(InfInt x) {
    auto searchResult = searchList(x);
    uint64_t r;
    if (searchResult.first)
        r = searchResult.second;
    else {
        r = calcCollatz(x);
        addToList(x, r);
    }
    return r;
}

int main() {
    int fd_memory = -1;
    void *mapped_mem;

    char c;
    std::cin >> c;
    bool is_shared = c == 't';
    if (is_shared) {
        fd_memory = shm_open(SHM_NAME, O_RDWR, S_IRUSR | S_IWUSR);
        if (fd_memory == -1) syserr("error in shm_open in new_process");
        mapped_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_memory, 0);
        if(mapped_mem == MAP_FAILED)
            syserr("error in mmap in new_process");

        sem = sem_open(SEM_NAME, O_RDWR, S_IRUSR | S_IWUSR, 0);
        if (sem == SEM_FAILED)
            syserr("sem_open");

        bool *initialized = (bool*)mapped_mem;
        list = (SharedList*)(initialized + 1);

        if (sem_wait(sem) == -1)
            syserr("error in wait init");



        if (!*initialized) {

            *initialized = true;
            char *str = (char*)(list + 1);
            std::string dummyStr = "$";
            strcpy(str, dummyStr.c_str());
            SharedList *next = (SharedList*)(str + dummyStr.size());
            //std::cerr << next << " next in init" << std::endl;
            *list = {2137, false, dummyStr.size() + 1};

        }

        if (sem_post(sem) == -1)
            syserr("error in post init");
    }
    std::vector<InfInt> inputs;
    std::string s;
    InfInt a = 1;
    while (std::cin >> a) {
        inputs.push_back(a);
    }


    size_t  count = 0;
    for (auto i: inputs) {
        count++;
        uint64_t r;
        if (is_shared)
            r = collatzWithShared(i);
        else
            r = calcCollatz(i);
        if (write(1, &r, sizeof(uint64_t)) == -1)
            syserr("error in write in new_proces");
    }

    if (is_shared) {
        close(fd_memory);
        munmap(mapped_mem, SHM_SIZE);
        if (sem_close(sem))
            syserr("sem_close");
    }

    return 0;
}