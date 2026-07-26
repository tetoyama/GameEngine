# =======================================================================
#
# AgentOS Core テスト用 Makefile（Linux / g++）
#
# =======================================================================

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Werror -O1
INCLUDES  = -I Source/GameApplication -I Source/GameApplication/Backends/llama/vendor

# ヘッダ依存の自動追跡。
# これが無いとヘッダだけを変更した際にオブジェクトが再ビルドされず、
# 古いレイアウトのまま混ざってクラッシュする（実際に踏んだ）。
DEPFLAGS  = -MMD -MP

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
        AgentOSCodeIndexSmokeTest \
        AgentOSCodeSearchSmokeTest \
        AgentOSCodeIndexServiceSmokeTest \
        AgentOSCodeInvestigationRoutingSmokeTest \
        AgentOSTaskRetirementSmokeTest \
        AgentOSRepairHygieneSmokeTest \
        AgentOSVerticalSliceSmokeTest

# ---------------------------------
# コード索引CLI（オフライン索引構築ツール）
# allには含めない。CI時間を増やさず、必要なときだけ `make codeindex` で建てる。
# ---------------------------------
CODEINDEX_SRC = Source/GameApplication/AgentOS/Tools/CodeIndexCli.cpp
CODEINDEX_OBJ = $(OBJ_DIR)/$(CODEINDEX_SRC:.cpp=.o)
CODEINDEX_BIN = $(BUILD_DIR)/codeindex

.PHONY: all build run clean codeindex $(addprefix run-,$(TESTS))

all: build run
	@echo "==== AgentOS: ALL TESTS PASSED ===="

build: $(addprefix $(BUILD_DIR)/,$(TESTS))
run: $(addprefix run-,$(TESTS))

$(SQLITE_OBJ): $(SQLITE_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) -c -O1 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/Tests/%.o: Tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# 生成された .d を取り込む（初回は存在しないので - を付ける）
-include $(CORE_OBJS:.o=.d)
-include $(addprefix $(OBJ_DIR)/Tests/,$(addsuffix .d,$(TESTS)))
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
