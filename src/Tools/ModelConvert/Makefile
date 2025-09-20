CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# If you have FBX SDK installed, uncomment and adjust these lines:
# FBX_SDK_ROOT = /path/to/fbx/sdk
# CXXFLAGS += -DHAVE_FBX_SDK -I$(FBX_SDK_ROOT)/include
# LDFLAGS += -L$(FBX_SDK_ROOT)/lib -lfbxsdk

TARGET = FBXLoaderDemo
SOURCE = FBXLoaderDemo.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) sample_model.fbx

.PHONY: all clean test