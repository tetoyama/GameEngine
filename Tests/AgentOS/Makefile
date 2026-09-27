# =======================================================================
#
# AgentOS tests
#
# Automatic CI runs CORE_TESTS. FULL_TESTS remain available manually.
#
# =======================================================================

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -O1
INCLUDES  = -I Source/GameApplication -I Source/GameApplication/Backends/llama/vendor
DEPFLAGS  = -MMD -MP

BUILD_DIR = build/agentos_tests
OBJ_DIR   = $(BUILD_DIR)/obj

CORE_SRCS := $(shell find Source/GameApplication/AgentOS/Core -name '*.cpp')
CORE_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CORE_SRCS))

SQLITE_SRC = Source/GameApplication/Backends/sqlite/sqlite3.c
SQLITE_OBJ = $(BUILD_DIR)/sqlite3.o

CORE_TESTS = AgentOSCommandPipelineSmokeTest \
             AgentOSEvidenceLogicSmokeTest \
             AgentOSConversationMemorySmokeTest \
             AgentOSContextIsolationSmokeTest \
             AgentOSRepairExecutionSmokeTest \
             AgentOSCodeInvestigationRoutingSmokeTest \
             AgentOSRecursiveFlowEndToEndSmokeTest \
             AgentOSVerticalSliceSmokeTest

FULL_TESTS = AgentOSCommandPipelineSmokeTest \
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
             AgentOSConversationHistoryToolSmokeTest \
             AgentOSRespondToolSmokeTest \
             AgentOSMemoryAndRetrySmokeTest \
             AgentOSFuzzyMatchSmokeTest \
             AgentOSAmbiguityAndSynthesisSafetySmokeTest \
             AgentOSStructuredConversationContextSmokeTest \
             AgentOSCodeIndexSmokeTest \
             AgentOSCodeSearchSmokeTest \
             AgentOSCodeIndexServiceSmokeTest \
             AgentOSCodeInvestigationRoutingSmokeTest \
             AgentOSToolCatalogPromptSmokeTest \
             AgentOSEvidencePromptCompressionSmokeTest \
             AgentOSTaskRetirementSmokeTest \
             AgentOSRepairHygieneSmokeTest \
             AgentOSUtf8SafetySmokeTest \
             AgentOSRecursiveFlowControlSmokeTest \
             AgentOSRecursiveFlowEndToEndSmokeTest \
             AgentOSVerticalSliceSmokeTest

ALL_TESTS = $(sort $(FULL_TESTS))

CODEINDEX_SRC = Source/GameApplication/AgentOS/Tools/CodeIndexCli.cpp
CODEINDEX_OBJ = $(OBJ_DIR)/$(CODEINDEX_SRC:.cpp=.o)
CODEINDEX_BIN = $(BUILD_DIR)/codeindex

.PHONY: all core full build-core build-full clean codeindex \
        $(addprefix run-,$(ALL_TESTS))

all: core

core: build-core $(addprefix run-,$(CORE_TESTS))
	@echo "==== AgentOS: CORE TESTS PASSED ===="

full: build-full $(addprefix run-,$(FULL_TESTS))
	@echo "==== AgentOS: FULL TESTS PASSED ===="

build-core: $(addprefix $(BUILD_DIR)/,$(CORE_TESTS))
build-full: $(addprefix $(BUILD_DIR)/,$(FULL_TESTS))

$(SQLITE_OBJ): $(SQLITE_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) -c -O1 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/Tests/%.o: Tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

-include $(CORE_OBJS:.o=.d)
-include $(addprefix $(OBJ_DIR)/Tests/,$(addsuffix .d,$(ALL_TESTS)))
-include $(CODEINDEX_OBJ:.o=.d)

$(BUILD_DIR)/%: $(OBJ_DIR)/Tests/%.o $(CORE_OBJS) $(SQLITE_OBJ)
	$(CXX) $^ -lpthread -ldl -o $@

run-%: $(BUILD_DIR)/%
	$<

codeindex: $(CODEINDEX_BIN)

$(CODEINDEX_BIN): $(CODEINDEX_OBJ) $(CORE_OBJS) $(SQLITE_OBJ)
	$(CXX) $^ -lpthread -ldl -o $@

clean:
	rm -rf $(BUILD_DIR)
