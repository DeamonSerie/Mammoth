#include "LayerDragDrop.hpp"

#include <cstdlib>

void LayerDragDrop::press(Item item, int index, int groupIdx, int memberPos,
                          int stackPos, float y) {
    if (item == Item::None) return;
    m_item = item;
    m_index = index;
    m_groupIdx = groupIdx;
    m_memberPos = memberPos;
    m_stackPos = stackPos;
    m_startY = y;
    m_armed = true;
    m_active = false;
    m_plan = DndPlan();
}

void LayerDragDrop::cancel() {
    m_item = Item::None;
    m_index = -1;
    m_groupIdx = -1;
    m_memberPos = -1;
    m_stackPos = -1;
    m_armed = false;
    m_active = false;
    m_plan = DndPlan();
}

void LayerDragDrop::update(float y, const std::vector<DndRow>& rows) {
    if (!m_armed && !m_active) return;
    if (m_armed) {
        if (y < m_startY - DRAG_START_PX || y > m_startY + DRAG_START_PX) {
            m_armed = false;   // motion threshold passed: now really dragging
            m_active = true;
        } else {
            return;            // still a plain press
        }
    }
    computePlan(y, rows);
}

// The hovered row plus whether the pointer sits in its top half. Past the
// ends clamps to the first/last boundary so edge drops still work.
static const DndRow* rowAt(const std::vector<DndRow>& rows, float y, bool& topHalf) {
    if (rows.empty()) return nullptr;
    for (const DndRow& r : rows) {
        if (y >= r.y && y < r.y + r.h) {
            topHalf = y < r.y + r.h * 0.5f;
            return &r;
        }
    }
    if (y < rows.front().y) { topHalf = true; return &rows.front(); }
    topHalf = false;
    return &rows.back();
}

// Is this row the dragged item itself?
static bool isSourceRow(const LayerDragDrop& dnd, const DndRow& r) {
    if (dnd.item() == LayerDragDrop::Item::Layer)
        return !r.isHeader && r.index == dnd.layerIndex();
    if (dnd.item() == LayerDragDrop::Item::Group)
        return r.isHeader && r.index == dnd.groupIndex();
    return false;
}

// Panel display order mirrors the paint stack reversed: a row's outer-stack
// anchor sits higher the closer it is to the top of the panel. Member rows
// anchor to their group's node; their run positions map inversely to display
// height (run[N-1] renders at the top of the group block).
void LayerDragDrop::computePlan(float y, const std::vector<DndRow>& rows) {
    m_plan = DndPlan();
    bool topHalf = false;
    const DndRow* hit = rowAt(rows, y, topHalf);
    if (!hit) return;
    if (isSourceRow(*this, *hit)) return;   // drop on self: stay put

    int anchor = hit->stackPos;
    if (anchor < 0) return;
    int T = topHalf ? anchor + 1 : anchor;

    if (m_item == Item::Group) {
        // Groups only reorder across the outer stack (no nesting).
        int F = (T <= m_stackPos) ? T : T - 1;   // permutation within S slots
        if (F == m_stackPos) return;             // net zero movement
        m_plan.action = DndAction::MoveOuter;
        m_plan.stackFrom = m_stackPos;
        m_plan.stackSteps = F - m_stackPos;
        m_plan.lineY = topHalf ? hit->y : hit->y + hit->h;
        return;
    }

    // ---- Dragging a layer ---------------------------------------------
    const bool iAmMember = (m_groupIdx >= 0);

    if (hit->isHeader) {
        int g = hit->index;
        if (iAmMember && g == m_groupIdx) {
            // Own header: hoist to the display-top of my own run.
            int n = hit->runCount;
            if (n < 1 || m_memberPos == n - 1) return;
            m_plan.action = DndAction::MemberReorder;
            m_plan.groupIndex = g;
            m_plan.memberFrom = m_memberPos;
            m_plan.memberTo = n - 1;
            return;
        }
        // Any other header: join that group at its display-top (the run's
        // end). MainWindow expands a collapsed target when applying.
        m_plan.action = DndAction::JoinGroup;
        m_plan.groupIndex = g;
        m_plan.memberInsert = hit->runCount;
        m_plan.highlightGroup = g;
        return;
    }

    int rc = hit->groupIdx;
    if (rc < 0) {
        // Ungrouped territory: reorder the outer stack. A dragged member is
        // spliced back at its group's slot first; the fresh node inserts one
        // wide slot, shifting targets that sat above the splice point.
        m_plan.action = DndAction::MoveOuter;
        m_plan.ungroupFirst = iAmMember;
        m_plan.spliceBase = m_stackPos;
        m_plan.stackFrom = m_stackPos;
        int F = iAmMember ? ((T <= m_stackPos) ? T : T + 1)
                          : ((T <= m_stackPos) ? T : T - 1);
        m_plan.stackSteps = F - m_stackPos;
        if (!iAmMember && m_plan.stackSteps == 0) { m_plan = DndPlan(); return; }
        m_plan.lineY = topHalf ? hit->y : hit->y + hit->h;
        return;
    }

    if (iAmMember && rc == m_groupIdx) {
        // Same run. Display-above r == right AFTER r in run order; the
        // erase-at-from step shifts targets depending on relative positions.
        int t = hit->memberPos;
        int n = hit->runCount;
        int to = topHalf ? ((m_memberPos < t) ? t : t + 1)
                         : ((m_memberPos < t) ? t - 1 : t);
        if (to < 0) to = 0;
        if (to > n - 1) to = n - 1;
        if (to == m_memberPos) return;
        m_plan.action = DndAction::MemberReorder;
        m_plan.groupIndex = rc;
        m_plan.memberFrom = m_memberPos;
        m_plan.memberTo = to;
        m_plan.lineY = topHalf ? hit->y : hit->y + hit->h;
        return;
    }

    // Another group's territory (or an ungrouped layer over a member row):
    // join that group at an exact run position. The dragged layer appends
    // at run index N first, so targets relate to the untouched run.
    int t = hit->memberPos;
    int N = hit->runCount;
    int to = topHalf ? (t + 1) : t;
    if (to < 0) to = 0;
    if (to > N) to = N;
    m_plan.action = DndAction::JoinGroup;
    m_plan.groupIndex = rc;
    m_plan.memberInsert = to;
    m_plan.highlightGroup = rc;
    m_plan.lineY = topHalf ? hit->y : hit->y + hit->h;
}
