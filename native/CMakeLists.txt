cmake_minimum_required(VERSION 3.16)
project(DrawingApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOKOL_DIR "$ENV{HOME}/.dub/packages/sokol-d/0.4.7/sokol-d/src/sokol/c")

add_executable(Mammoth
    src/sokol_impl.cpp
    src/main.cpp
    src/app/Application.cpp
    src/document/Frame.cpp
    src/document/Layer.cpp
    src/document/DrawingDocument.cpp
    src/canvas/Canvas.cpp
    src/canvas/CanvasManager.cpp
    src/drawing/Brush.cpp
    src/drawing/BrushEngine.cpp
    src/drawing/CustomBrushConfig.cpp
    src/drawing/CustomBrushGeometry.cpp
    src/drawing/GradualEraser.cpp
    src/drawing/MoveTool.cpp
    src/drawing/RectSelectTool.cpp
    src/selection/Selection.cpp
    src/clipboard/Clipboard.cpp
    src/input/Mouse.cpp
    src/rendering/Renderer.cpp
    src/ui/MainWindow.cpp
    src/ui/LayerDragDrop.cpp
    src/ui/TimelineDragDrop.cpp
    src/DebugLog.cpp
)

target_include_directories(Mammoth PRIVATE ${SOKOL_DIR})
target_compile_definitions(Mammoth PRIVATE SOKOL_GLCORE)
target_link_libraries(Mammoth PRIVATE GL X11 Xi Xcursor pthread m dl)
