# Define the compiler to use
CXX = g++ 
# Define compilation flags
CXXFLAGS = -std=c++11 -O3

# Define the source directory
SRC_DIR = src

# Define targets
all: BactCore BactCore-lm

BactCore: $(SRC_DIR)/BactCore.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore.cpp -o BactCore

BactCore-lm: $(SRC_DIR)/BactCore-lm.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore-lm.cpp -o BactCore-lm

clean:
	rm -f BactCore BactCore-lm
