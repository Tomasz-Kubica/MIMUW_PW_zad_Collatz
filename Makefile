main: main.cpp teams.cpp sharedresults.hpp collatz.hpp err.cpp err.h new_process
	g++ -pthread -o main main.cpp teams.cpp err.cpp

new_process: new_process.cpp collatz.hpp err.cpp err.h
	g++ -pthread -o new_process new_process.cpp err.cpp

all: main new_process

clean:
	rm -f main new_process