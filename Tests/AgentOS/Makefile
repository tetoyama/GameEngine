# =======================================================================
#
# AgentOS Core テスト用 Makefile（Linux / g++）
#
# =======================================================================

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -O1
INCLUDES  = -I Source/GameApplication -I Source/GameApplication/Backends/llama/vendor

BUILD_DIR = build/agentos_tests
OBJ_DIR   = $(BUILD_DIR)/obj

CORE_SRCS := $(shell find Source/GameApplication/AgentOS/Core -name '*.cpp')
CORE_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CORE_SRCS))

SQLITE_SRC = Source/GameApplication/Backends/sqlite/sqlite3.c
SQLITE_OBJ = $(BUILD_DIR)/sqlite3.o

TESTS = AgentOSCommandPipelineSmokeTest \
        AgentOSAuditSinkDedupSmokeTest \
        AgentOSTaskStoreSmokeTest \
        AgentOSEvidenceLogicSmokeTest \
        AgentOSExecutionControlSmokeTest \
        AgentOSConversationMemorySmokeTest \
        AgentOSContextIsolationSmokeTest \
        AgentOSRepairExecutionSmokeTest \
        AgentOSSceneSnapshotFastPathSmokeTest \
        AgentOSComponentQueryRoutingSmokeTest \
        AgentOSGoalGateSmokeTest \
        AgentOSMemoryAndRetrySmokeTest \
        AgentOSFuzzyMatchSmokeTest \
        AgentOSAmbiguityAndSynthesisSafetySmokeTest \
        AgentOSStructuredConversationContextSmokeTest \
        AgentOSVerticalSliceSmokeTest

.PHONY: all build run clean $(addprefix run-,$(TESTS))

all: build run
	@echo "==== AgentOS: ALL TESTS PASSED ===="

build: $(addprefix $(BUILD_DIR)/,$(TESTS))
run: $(addprefix run-,$(TESTS))

$(SQLITE_OBJ): $(SQLITE_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) -c -O1 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/Tests/%.o: Tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%: $(OBJ_DIR)/Tests/%.o $(CORE_OBJS) $(SQLITE_OBJ)
	$(CXX) $^ -lpthread -ldl -o $@

run-%: $(BUILD_DIR)/%
	$<

clean:
	rm -rf $(BUILD_DIR)
