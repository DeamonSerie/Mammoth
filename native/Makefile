CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread
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
          $(SRCDIR)/DebugLog.cpp

OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR)
