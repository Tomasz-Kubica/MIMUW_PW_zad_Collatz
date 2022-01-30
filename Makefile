main:
	g++ -pthread -o main main.cpp teams.cpp

new_process:
	g++ -pthread -o new_process new_process.cpp

all: main new_process

clean:
	rm -f main new_process