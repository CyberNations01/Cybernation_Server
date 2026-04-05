CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./includes
SRCDIR   = src
OBJDIR   = obj
TARGET   = out/ep

 
SOURCES = $(SRCDIR)/core/Params.cpp \
          $(SRCDIR)/core/FeedbackPool.cpp \
          $(SRCDIR)/core/FeedbackTokenManager.cpp \
          $(SRCDIR)/core/Stack.cpp \
          $(SRCDIR)/core/Player.cpp \
          $(SRCDIR)/core/DisruptionCard.cpp \
          $(SRCDIR)/core/DataLoader.cpp \
          $(SRCDIR)/core/Goal.cpp \
          $(SRCDIR)/core/Tile.cpp \
          $(SRCDIR)/game/GameState.cpp \
          $(SRCDIR)/game/GameRoom.cpp \
          $(SRCDIR)/game/RoundController.cpp \
          $(SRCDIR)/phase/EnvisionPhaseHandler.cpp \
          $(SRCDIR)/phase/TraversePhaseHandler.cpp \
          $(SRCDIR)/phase/AdoptPhaseHandler.cpp \
          $(SRCDIR)/game/GameUtility.cpp \
          $(SRCDIR)/test/EnvisionPhaseTest.cpp

 
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))
 
all: $(TARGET)

COMMON_SOURCES = $(filter-out $(SRCDIR)/test/testLoadJson.cpp,$(SOURCES))
ADAPT_SOURCES = $(COMMON_SOURCES) $(SRCDIR)/test/testAdaptSimulation.cpp
ADAPT_OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(ADAPT_SOURCES))
 
$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^
 
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
 
clean:
	rm -rf $(OBJDIR) $(TARGET) $(ADAPT_TARGET)

adapt-sim: $(ADAPT_TARGET)

$(ADAPT_TARGET): $(ADAPT_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $^
 
.PHONY: all clean adapt-sim