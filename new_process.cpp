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
//    std::cerr << "search start" << std::endl;
    if (sem_wait(sem) == -1)
        syserr("error in wait");

    SharedList *l;
    l = list;
    std::string str = x.toString();

    while (l != nullptr) {
//        if (x.toString() == "1586") {
//            std::cerr << "." << std::endl;
//            std::cerr << "searched str: " << str.c_str() << std::endl;
//            std::cerr << "is next: " << l->is_next << std::endl;
//            std::cerr << "result: " << l->result << std::endl;
//            std::cerr << "number: " << listGetNumber(l) << std::endl;
//        }

        if (strcmp(listGetNumber(l), str.c_str()) == 0) {
            //std::cerr << listGetNumber(l) << " ^ " << str.c_str() << std::endl;
            uint64_t r = l->result;
            if (sem_post(sem) == -1)
                syserr("error in post");
//            std::cerr << "search end true" << std::endl;
            return {true, r};
        }
        if (l->is_next)
            l = listGetNext(l);
        else
            l = nullptr;
    }

    if (sem_post(sem) == -1)
        syserr("error in post");

//    std::cerr << "search end false" << std::endl;
    return {false, 0};
}

void addToList(InfInt x, uint64_t r) {
//    if (x.toString() == "1586" || r == 78 || r == 36)
//        std::cerr << "### wstawione: " << x << " -> " << r << std::endl;
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

//    std::cerr << "add to list end list: " << list << std::endl;
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
    if (r != calcCollatz(x))
        std::cerr << "!!! " << x << ": " << calcCollatz(x) << " =/= " << r << std::endl;
    return r;
}

int main() {
//    int x = 0;
//    while (true) {
//        x++;
//        std::cerr << x << std::endl;
//    }

    int fd_memory = -1;
    void *mapped_mem;

    char c;
    std::cin >> c;
    //std::cerr << c << std::endl;
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

//        std::cerr << "waiting for init check" << std::endl;

        if (sem_wait(sem) == -1)
            syserr("error in wait init");

//        std::cerr << "xxx" << std::endl;


        if (!*initialized) {
//            std::cerr << "initialized start" << std::endl;

            *initialized = true;
            char *str = (char*)(list + 1);
            std::string dummyStr = "$";
            strcpy(str, dummyStr.c_str());
            SharedList *next = (SharedList*)(str + dummyStr.size());
            //std::cerr << next << " next in init" << std::endl;
            *list = {2137, false, dummyStr.size() + 1};

//            std::cerr << "initialized finished" << std::endl;
        }

        if (sem_post(sem) == -1)
            syserr("error in post init");
    }
    std::vector<InfInt> inputs;
    std::string s;
    InfInt a = 1;
    while (std::cin >> a) {
        inputs.push_back(a);
//        std::cerr << a << std::endl;
    }

    for (auto i: inputs) {
        uint64_t r;
        if (is_shared)
            r = collatzWithShared(i);
        else
            r = calcCollatz(i);
        if (write(1, &r, sizeof(uint64_t)) == -1)
            syserr("error in write in new_proces");
        //std::cerr << i << " " << r << std::endl;
    }

    if (is_shared) {
        close(fd_memory);
        munmap(mapped_mem, SHM_SIZE);
        if (sem_close(sem))
            syserr("sem_close");
    }
    return 0;
}