CXX       := g++
CXXFLAGS  := -g -std=c++11 -Os -Wall -Wpedantic
INCLUDES  := 
LIBRARIES := -lGLEW -lglfw -lm -lGL -lfftw3 -lpulse-simple -lpulse

all: main

SRC := $(wildcard *.cpp)
OBJ := $(patsubst %.cpp, %.o, $(SRC))

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $< $(INCLUDES)

main: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LIBRARIES) $(INCLUDES) -o $@ $^

clean:
	# add ; echo to silence errors from missing files in rm
	rm *.o main ; echo
