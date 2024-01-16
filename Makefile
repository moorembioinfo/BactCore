# Define the compiler to use
CXX = g++ 
# Define compilation flags
CXXFLAGS = -std=c++11 -O3

# Define the source directory
SRC_DIR = src

# Define targets
all: BactCore BactCore-large

BactCore: $(SRC_DIR)/BactCore.cpp
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC_DIR)/BactCore.cpp -o BactCore

BactCore-large: $(SRC_DIR)/BactCore-large.cpp
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/BactCore-large.cpp -o BactCore-large

clean:
	rm -f BactCore BactCore-large
