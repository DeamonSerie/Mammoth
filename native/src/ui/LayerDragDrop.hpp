#pragma once
#include <vector>

// Mouse drag-and-drop for the layer panel: layers onto groups, layers to
// reorder (inside their group or across the outer paint stack), and groups
// to reorder. Pure state machine + drop planning - no sokol/GL/document
// dependencies - so the planning math is covered by headless unit tests.
//
// MainWindow drives it: feed presses/motion, render plan().feedback, and
// apply plan() to the Frame on release (the module never mutates documents).

// One panel row snapshot, rebuilt whenever the pointer moves during a drag.
struct DndRow {
    bool isHeader = false;
    int index = -1;      // layer index, or group index for headers
    int groupIdx = -1;   // owning group for member rows, else -1
    int memberPos = -1;  // position inside the owning group's run, else -1
    int stackPos = -1;   // outer-stack slot; member rows report their group's
    int runCount = 0;    // member count of the related group (else 0)
    float y = 0.0f;      // row top in framebuffer coords
    float h = 24.0f;     // row height
};

enum class DndAction {
    None,
    JoinGroup,      // nest the dragged layer into a group at memberInsert
    MoveOuter,      // step a header/ungrouped-layer node through the stack
    MemberReorder,  // permute members inside one group
};

struct DndPlan {
    DndAction action = DndAction::None;

    int groupIndex = -1;   // JoinGroup target / MemberReorder container
    int memberInsert = -1; // JoinGroup: run position to land at
    int memberFrom = -1;   // MemberReorder source run position
    int memberTo = -1;     // MemberReorder destination run position

    int stackFrom = -1;    // MoveOuter: starting outer-stack slot
    int stackSteps = 0;    // MoveOuter: signed swap count (+1 raises Z)
    bool ungroupFirst = false; // MoveOuter: splice member out of its group
    int spliceBase = -1;   // MoveOuter: group slot the spliced node starts at

    // Rendering feedback
    int highlightGroup = -1; // header shown as the nest target (-1 none)
    float lineY = -1.0f;     // insertion-line y in framebuffer coords (-1 none)
};

class LayerDragDrop {
public:
    enum class Item { None, Layer, Group };

    static constexpr float DRAG_START_PX = 4.0f;

    // Capture a press on a row body. The caller decides which presses are
    // draggable (plain row regions only, not embedded controls).
    void press(Item item, int index, int groupIdx, int memberPos, int stackPos,
               float y);
    // Feed pointer motion; builds/rebuilds the plan once dragging started.
    void update(float y, const std::vector<DndRow>& rows);
    void cancel();

    bool armed() const { return m_armed; }   // pressed, not yet moving
    bool active() const { return m_active; } // threshold passed, dragging
    Item item() const { return m_item; }
    int layerIndex() const { return m_item == Item::Layer ? m_index : -1; }
    int groupIndex() const { return m_item == Item::Group ? m_index : -1; }

    const DndPlan& plan() const { return m_plan; }

private:
    void computePlan(float y, const std::vector<DndRow>& rows);

    Item m_item = Item::None;
    int m_index = -1;
    int m_groupIdx = -1;
    int m_memberPos = -1;
    int m_stackPos = -1;
    float m_startY = 0.0f;
    bool m_armed = false;
    bool m_active = false;
    DndPlan m_plan;
};
