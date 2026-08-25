#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Frame.hpp"

class DrawingDocument {
public:
    DrawingDocument();
    DrawingDocument(int width, int height, const char* name = nullptr);

    int width() const { return m_width; }
    int height() const { return m_height; }
    const char* name() const { return m_name.c_str(); }
    void setName(const char* n) { m_name = n; }

    void resize(int w, int h);

    Frame* addFrame(int index = -1);
    void removeFrame(int index);
    void duplicateFrame(int index);
    int frameCount() const { return (int)m_frames.size(); }
    Frame* getFrame(int index);
    const Frame* getFrame(int index) const;
    Frame* activeFrame() const { return m_activeFrame; }
    void setActiveFrame(int index);
    int activeFrameIndex() const;

    // Frame groups are purely organizational: they never change playback
    // order, and there is deliberately no API to reorder frames or groups.
    // A frame belongs to at most one group.
    struct FrameGroup {
        std::string name;
        uint32_t color = 0;
        std::vector<int> frameIndices;
        bool collapsed = false;
    };
    void addFrameGroup(const char* name, uint32_t color);
    // Removes the group; its frames survive ungrouped.
    void removeFrameGroup(int groupIndex);
    int frameGroupCount() const { return (int)m_frameGroups.size(); }
    const FrameGroup& getFrameGroup(int index) const;
    int findGroupForFrame(int frameIndex) const;
    void addFrameToGroup(int frameIndex, int groupIndex);
    void removeFrameFromGroup(int frameIndex);
    void setFrameGroupCollapsed(int groupIndex, bool collapsed);
    bool isFrameGroupCollapsed(int groupIndex) const;
    void renameFrameGroup(int groupIndex, const char* name);
    void setFrameGroupColor(int groupIndex, uint32_t color);

    void clear();

private:
    int m_width = 0;
    int m_height = 0;
    std::string m_name;
    std::vector<std::unique_ptr<Frame>> m_frames;
    Frame* m_activeFrame = nullptr;
    std::vector<FrameGroup> m_frameGroups;
};
