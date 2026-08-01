CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread
TARGET = reactor
OBJS = main.o Buffer.o Channel.o EventLoop.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)