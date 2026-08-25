CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
# -MMD -MP: emit header dependency files so editing a .hpp recompiles its dependents.
CXXFLAGS += -MMD -MP
CXXFLAGS += -I$(SOKOL_DIR)

LDFLAGS = -lGL -lX11 -lXi -lXcursor -lpthread -lm -ldl

SOKOL_DIR = /home/knw102/.dub/packages/sokol-d/0.4.7/sokol-d/src/sokol/c

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
               $(SRCDIR)/DebugLog.cpp

TEST_TARGET = $(BUILDDIR)/MammothTests

OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

.PHONY: all clean test

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
	rm -rf $(BUILDDIR)

-include $(OBJECTS:.o=.d)
