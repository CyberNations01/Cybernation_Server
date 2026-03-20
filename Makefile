CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I./includes
SRCDIR   = src
OBJDIR   = obj
TARGET   = testLoadJson
 
SOURCES = $(SRCDIR)/core/Params.cpp \
          $(SRCDIR)/core/FeedbackPool.cpp \
          $(SRCDIR)/core/Stack.cpp \
          $(SRCDIR)/core/Player.cpp \
          $(SRCDIR)/core/DisruptionCard.cpp \
          $(SRCDIR)/core/DataLoader.cpp \
          $(SRCDIR)/core/Tile.cpp \
          $(SRCDIR)/game/GameState.cpp \
          $(SRCDIR)/game/GameRoom.cpp \
          $(SRCDIR)/game/RoundController.cpp \
          $(SRCDIR)/phase/EnvisionPhaseHandler.cpp \
          $(SRCDIR)/phase/TraversePhaseHandler.cpp \
          $(SRCDIR)/phase/AdoptPhaseHandler.cpp \
          $(SRCDIR)/test/testLoadJson.cpp

 
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SOURCES))
 
all: $(TARGET)
 
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^
 
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@
 
clean:
	rm -rf $(OBJDIR) $(TARGET)
 
.PHONY: all clean