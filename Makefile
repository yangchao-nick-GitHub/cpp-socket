CXX = g++
CXXFLAGS = -Wall -std=c++11
TARGETS = server client
SRCS = server.cpp client.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = socket.h

all: $(TARGETS)

server: server.o
	$(CXX) $(CXXFLAGS) -o $@ $<

client: client.o
	$(CXX) $(CXXFLAGS) -o $@ $<

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) $(OBJS)

.PHONY: all clean