# CHECKMATE - build with GCC/MinGW (Windows) or GCC/Clang (Linux/macOS)
CC  ?= gcc
CXX ?= g++
CFLAGS   = -std=c99   -O2 -Wall -Wextra
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra

OBJS = undo_stack.o move_history.o event_queue.o board_hash.o \
       chess_rules.o minimax.o main.o

checkmate: $(OBJS)
	$(CXX) $(CXXFLAGS) -static $(OBJS) -o $@

%.o: %.c dsa.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp dsa.h chess_rules.h minimax.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) checkmate checkmate.exe