CC = g++
CFLAGS = -std=c++11 -pthread
TARGETS = pipegrep
all: $(TARGETS)

pipegrep: pipegrep.cpp
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)