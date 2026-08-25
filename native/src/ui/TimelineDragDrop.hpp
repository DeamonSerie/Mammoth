#pragma once
#include <vector>

// Mouse drag-and-drop for the timeline strip: frame tiles to reorder
// (within a group section or across sections), frames to join/leave groups,
// and group chips to reorder.  Pure state machine + drop planning - no
// sokol/GL/document dependencies - so the planning math is covered by
// headless unit tests.
//
// MainWindow drives it: feed presses/motion, render plan().feedback, and
// apply plan() to the DrawingDocument on release.

// One timeline-item snapshot, rebuilt whenever the pointer moves during a drag.
struct TlDndItem {
    bool isHeader = false;  // true = group chip, false = frame tile
    int index = -1;         // group index (header) or frame index (tile)
    int groupIdx = -1;      // owning group for member frames, else -1
    int memberPos = -1;     // position inside the group's frameIndices, else -1
    float x = 0.0f;         // left edge in framebuffer coords
    float w = 0.0f;         // width (chipW for headers, TL_TILE for frames)
};

enum class TlDndAction {
    None,
    FrameReorder,   // move frame in m_frames (reorder within or across sections)
    FrameJoinGroup, // join an ungrouped frame into a group
    FrameLeaveGroup,// remove a frame from its group (becomes ungrouped)
    GroupReorder    // reorder groups in m_frameGroups
};

struct TlDndPlan {
    TlDndAction action = TlDndAction::None;
    int frameIndex = -1;     // frame being dragged (FrameReorder/Join/Leave)
    int groupIndex = -1;     // group being dragged (GroupReorder) or target group (FrameJoinGroup)
    int targetIndex = -1;    // target frame (FrameReorder) or group (GroupReorder)
    bool insertAfter = false;// insert after target (true) or before (false)

    // Rendering feedback
    int highlightGroup = -1; // group chip shown as join target (-1 none)
    float lineX = -1.0f;     // insertion-line x in framebuffer coords (-1 none)
};

class TimelineDragDrop {
public:
    enum class Item { None, Frame, Group };

    static constexpr float DRAG_START_PX = 4.0f;

    // Capture a press on a timeline item body.
    void press(Item item, int index, int groupIdx, int memberPos, float x);
    // Feed pointer motion; builds/rebuilds the plan once dragging started.
    void update(float x, const std::vector<TlDndItem>& items);
    void cancel();

    bool armed() const { return m_armed; }   // pressed, not yet moving
    bool active() const { return m_active; } // threshold passed, dragging
    Item item() const { return m_item; }
    int frameIndex() const { return m_item == Item::Frame ? m_index : -1; }
    int groupIndex() const { return m_item == Item::Group ? m_index : -1; }
    int groupContext() const { return m_groupIdx; }

    const TlDndPlan& plan() const { return m_plan; }

private:
    void computePlan(float x, const std::vector<TlDndItem>& items);

    Item m_item = Item::None;
    int m_index = -1;
    int m_groupIdx = -1;    // group membership of the dragged frame
    int m_memberPos = -1;
    float m_startX = 0.0f;
    bool m_armed = false;
    bool m_active = false;
    TlDndPlan m_plan;
};
