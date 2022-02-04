main: main.cpp teams.cpp sharedresults.hpp collatz.hpp
	g++ -pthread -o main main.cpp teams.cpp

new_process: new_process.cpp collatz.hpp
	g++ -pthread -o new_process new_process.cpp

all: main new_process

clean:
	rm -f main new_process