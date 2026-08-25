#include "TimelineDragDrop.hpp"

#include <cstdlib>

void TimelineDragDrop::press(Item item, int index, int groupIdx, int memberPos,
                              float x) {
    if (item == Item::None) return;
    m_item = item;
    m_index = index;
    m_groupIdx = groupIdx;
    m_memberPos = memberPos;
    m_startX = x;
    m_armed = true;
    m_active = false;
    m_plan = TlDndPlan();
}

void TimelineDragDrop::cancel() {
    m_item = Item::None;
    m_index = -1;
    m_groupIdx = -1;
    m_memberPos = -1;
    m_armed = false;
    m_active = false;
    m_plan = TlDndPlan();
}

void TimelineDragDrop::update(float x, const std::vector<TlDndItem>& items) {
    if (!m_armed && !m_active) return;
    if (m_armed) {
        if (x < m_startX - DRAG_START_PX || x > m_startX + DRAG_START_PX) {
            m_armed = false;   // motion threshold passed: now really dragging
            m_active = true;
        } else {
            return;            // still a plain press
        }
    }
    computePlan(x, items);
}

// The hovered item plus whether the pointer sits in its left half. Past the
// ends clamps to the first/last boundary so edge drops still work.
static const TlDndItem* itemAt(const std::vector<TlDndItem>& items, float x,
                                bool& leftHalf) {
    if (items.empty()) return nullptr;
    for (const TlDndItem& it : items) {
        if (x >= it.x && x < it.x + it.w) {
            leftHalf = x < it.x + it.w * 0.5f;
            return &it;
        }
    }
    if (x < items.front().x) { leftHalf = true; return &items.front(); }
    leftHalf = false;
    return &items.back();
}

// Is this item the dragged item itself?
static bool isSource(const TimelineDragDrop& dnd, const TlDndItem& it) {
    if (dnd.item() == TimelineDragDrop::Item::Frame)
        return !it.isHeader && it.index == dnd.frameIndex();
    if (dnd.item() == TimelineDragDrop::Item::Group)
        return it.isHeader && it.index == dnd.groupIndex();
    return false;
}

void TimelineDragDrop::computePlan(float x, const std::vector<TlDndItem>& items) {
    m_plan = TlDndPlan();
    bool leftHalf = false;
    const TlDndItem* hit = itemAt(items, x, leftHalf);
    if (!hit) return;
    if (isSource(*this, *hit)) return;   // drop on self: stay put

    if (m_item == Item::Group) {
        // Groups only reorder across the group list.
        if (!hit->isHeader) return;      // dropping a group onto a frame: no-op
        int target = hit->index;
        if (target == m_index) return;
        m_plan.action = TlDndAction::GroupReorder;
        m_plan.groupIndex = m_index;
        m_plan.targetIndex = target;
        m_plan.insertAfter = !leftHalf;
        m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
        return;
    }

    // ---- Dragging a frame ------------------------------------------------
    const bool iAmMember = (m_groupIdx >= 0);

    if (hit->isHeader) {
        int g = hit->index;
        if (iAmMember && g == m_groupIdx) {
            // Own header: nothing to do (already in this group).
            return;
        }
        // Any other header: join that group.
        m_plan.action = TlDndAction::FrameJoinGroup;
        m_plan.frameIndex = m_index;
        m_plan.groupIndex = g;
        m_plan.highlightGroup = g;
        m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
        return;
    }

    // Hovering a frame tile.
    if (hit->index == m_index) return;   // self

    int hitGroup = hit->groupIdx;

    if (iAmMember && hitGroup != m_groupIdx) {
        // Different group territory: join the target's group.
        // (If the target is ungrouped, we can't really "insert" there;
        //  just do a frame reorder in m_frames which moves us to the
        //  ungrouped section.)
        if (hitGroup >= 0) {
            m_plan.action = TlDndAction::FrameJoinGroup;
            m_plan.frameIndex = m_index;
            m_plan.groupIndex = hitGroup;
            m_plan.highlightGroup = hitGroup;
            m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
            return;
        }
        // Target is ungrouped: leave our group, then reorder.
        m_plan.action = TlDndAction::FrameLeaveGroup;
        m_plan.frameIndex = m_index;
        m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
        return;
    }

    if (!iAmMember && hitGroup >= 0) {
        // Ungrouped frame over grouped frame territory: join that group.
        m_plan.action = TlDndAction::FrameJoinGroup;
        m_plan.frameIndex = m_index;
        m_plan.groupIndex = hitGroup;
        m_plan.highlightGroup = hitGroup;
        m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
        return;
    }

    // Same section (both ungrouped, or both in the same group): reorder.
    m_plan.action = TlDndAction::FrameReorder;
    m_plan.frameIndex = m_index;
    m_plan.targetIndex = hit->index;
    m_plan.insertAfter = !leftHalf;
    m_plan.lineX = leftHalf ? hit->x : hit->x + hit->w;
}
