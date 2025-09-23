CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# If you have FBX SDK installed, uncomment and adjust these lines:
# FBX_SDK_ROOT = /path/to/fbx/sdk
# CXXFLAGS += -DHAVE_FBX_SDK -I$(FBX_SDK_ROOT)/include
# LDFLAGS += -L$(FBX_SDK_ROOT)/lib -lfbxsdk

TARGETS = FBXLoaderDemo FBXIntegrationDemo
SOURCES = FBXLoaderDemo.cpp FBXIntegrationDemo.cpp

all: $(TARGETS)

FBXLoaderDemo: FBXLoaderDemo.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

FBXIntegrationDemo: FBXIntegrationDemo.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

test: $(TARGETS)
	@echo "Running FBX Loader Demo:"
	./FBXLoaderDemo sample_model.fbx
	@echo ""
	@echo "Running FBX Integration Demo:"
	./FBXIntegrationDemo

.PHONY: all clean test