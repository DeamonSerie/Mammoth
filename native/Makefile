CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
# -MMD -MP: emit header dependency files so editing a .hpp recompiles its dependents.
CXXFLAGS += -MMD -MP
CXXFLAGS += -I$(SOKOL_DIR)

LDFLAGS = -lGL -lX11 -lXi -lXcursor -lpthread -lm -ldl

SOKOL_DIR = $(CURDIR)/thirdparty/sokol

SRCDIR = src
BUILDDIR = build
TARGET = $(BUILDDIR)/Mammoth

SOURCES = $(SRCDIR)/sokol_impl.cpp \
          $(SRCDIR)/main.cpp \
          $(SRCDIR)/app/Application.cpp \
          $(SRCDIR)/document/Frame.cpp \
          $(SRCDIR)/document/Layer.cpp \
          $(SRCDIR)/document/DrawingDocument.cpp \
          $(SRCDIR)/canvas/Canvas.cpp \
          $(SRCDIR)/canvas/CanvasManager.cpp \
          $(SRCDIR)/drawing/CustomBrushConfig.cpp \
          $(SRCDIR)/drawing/CustomBrushGeometry.cpp \
          $(SRCDIR)/drawing/Brush.cpp \
          $(SRCDIR)/drawing/BrushEngine.cpp \
          $(SRCDIR)/drawing/GradualEraser.cpp \
          $(SRCDIR)/drawing/MoveTool.cpp \
          $(SRCDIR)/drawing/RectSelectTool.cpp \
          $(SRCDIR)/selection/Selection.cpp \
          $(SRCDIR)/clipboard/Clipboard.cpp \
          $(SRCDIR)/input/Mouse.cpp \
          $(SRCDIR)/rendering/Renderer.cpp \
          $(SRCDIR)/ui/MainWindow.cpp \
          $(SRCDIR)/ui/LayerDragDrop.cpp \
          $(SRCDIR)/ui/TimelineDragDrop.cpp \
          $(SRCDIR)/DebugLog.cpp

# Headless unit tests: drawing stack + document types only (no sokol/GL).
TEST_SOURCES = tests/test_main.cpp \
               $(SRCDIR)/drawing/CustomBrushConfig.cpp \
               $(SRCDIR)/drawing/CustomBrushGeometry.cpp \
               $(SRCDIR)/drawing/Brush.cpp \
               $(SRCDIR)/drawing/BrushEngine.cpp \
               $(SRCDIR)/drawing/GradualEraser.cpp \
               $(SRCDIR)/drawing/RectSelectTool.cpp \
               $(SRCDIR)/drawing/MoveTool.cpp \
               $(SRCDIR)/canvas/Canvas.cpp \
               $(SRCDIR)/selection/Selection.cpp \
               $(SRCDIR)/clipboard/Clipboard.cpp \
               $(SRCDIR)/document/DrawingDocument.cpp \
               tests/RendererStub.cpp \
               $(SRCDIR)/document/Layer.cpp \
               $(SRCDIR)/document/Frame.cpp \
               $(SRCDIR)/ui/LayerDragDrop.cpp \
               $(SRCDIR)/ui/TimelineDragDrop.cpp \
               $(SRCDIR)/DebugLog.cpp

TEST_TARGET = $(BUILDDIR)/MammothTests

OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

# --- SDL standalone backend: sokol_gfx renders into an SDL-owned GL context ---
SDL_DIR = $(CURDIR)/../SDL/SDL-main
SDL_INCLUDE = -I$(SDL_DIR)/include
SDL_LIB = $(SDL_DIR)/build/libSDL3.a
SDL_BUILDDIR = build-sdl
SDL_TARGET = $(SDL_BUILDDIR)/MammothSDL
SDL_SOURCES = $(filter-out $(SRCDIR)/main.cpp,$(SOURCES)) \
              $(SRCDIR)/app/main_sdl.cpp \
              $(SRCDIR)/app/SdlTranslator.cpp
SDL_OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(SDL_BUILDDIR)/%.o,$(SDL_SOURCES))

.PHONY: all clean test sdl

sdl: $(SDL_TARGET)

$(SDL_TARGET): $(SDL_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(SDL_LIB) -ldl -lpthread

$(SDL_BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DUSE_SDL $(SDL_INCLUDE) -c -o $@ $<

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_SOURCES)

clean:
	rm -rf $(BUILDDIR) $(SDL_BUILDDIR)

-include $(OBJECTS:.o=.d) $(SDL_OBJECTS:.o=.d)

# Headless SDL translator unit tests (no display needed: exercises the pure
# SdlTranslator::Translate() mapping with synthetic SDL_Event structs).
SDL_TEST_TARGET = $(BUILDDIR)/SdlTranslatorTests
SDL_TEST_SOURCES = tests/sdl_translator_test.cpp \
                   $(SRCDIR)/app/SdlTranslator.cpp \
                   $(SRCDIR)/DebugLog.cpp

.PHONY: test-sdl
test-sdl: $(SDL_TEST_TARGET)
	./$(SDL_TEST_TARGET)

$(SDL_TEST_TARGET): $(SDL_TEST_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL $(SDL_INCLUDE) -o $@ $^ $(SDL_LIB) -ldl -lpthread
