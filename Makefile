CXX = g++
CXXFLAGS = -Wall -std=c++17 -O3
TARGET = estimate.exe

all: $(TARGET)

$(TARGET): $*.cpp
	$(CXX) $(CXXFLAGS) -o $@ $**

clean:
	del $(TARGET)
