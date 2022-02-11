main: main.cpp teams.cpp sharedresults.hpp collatz.hpp err.cpp err.h shared_mem_info.h new_process
	g++ -pthread -o main main.cpp teams.cpp err.cpp -lrt

new_process: new_process.cpp collatz.hpp err.cpp err.h shared_mem_info.h
	g++ -pthread -o new_process new_process.cpp err.cpp -lrt

all: main new_process

clean:
	rm -f main new_process