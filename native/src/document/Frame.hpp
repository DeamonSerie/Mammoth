#pragma once
#include <vector>
#include <memory>
#include "Layer.hpp"

class Frame {
public:
    Frame();
    Frame(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    float opacity() const { return m_opacity; }
    float duration() const { return m_duration; }
    bool visible() const { return m_visible; }

    void setOpacity(float o) { m_opacity = o; }
    void setDuration(float d) { m_duration = d; }
    void setVisible(bool v) { m_visible = v; }

    void resize(int w, int h);

    Layer* addLayer(const char* name = nullptr);
    void removeLayer(int index);
    int layerCount() const { return (int)m_layers.size(); }
    Layer* getLayer(int index);
    const Layer* getLayer(int index) const;
    Layer* activeLayer() const { return m_activeLayer; }
    void setActiveLayer(int index);

    // Group management
    struct Group {
        std::string name;
        uint32_t color;
        std::vector<int> layerIndices;
        bool collapsed = false;
    };
    void addGroup(const char* name, uint32_t color);
    void removeGroup(int groupIndex);
    int groupCount() const { return (int)m_groups.size(); }
    const Group& getGroup(int index) const;
    int findGroupForLayer(int layerIndex) const;
    int groupIdForLayer(int layerIndex) const;
    void setGroupCollapsed(int groupIndex, bool collapsed);
    bool isGroupCollapsed(int groupIndex) const;

    // Layer operations
    void reorderLayer(int from, int to);
    void setLayerVisible(int index, bool v);
    void hideGroup(int groupId, bool hide);
    void setActiveLayerPreserveOrder(int index);

    void clear();

    bool isDirty() const { return m_dirty; }
    void clearDirty();
    void setDirty() { m_dirty = true; }

    void compositeToBuffer(std::vector<uint8_t>& out, int& outW, int& outH) const;

private:
    int m_width = 0;
    int m_height = 0;
    float m_opacity = 1.0f;
    float m_duration = 1.0f;
    bool m_visible = true;
    bool m_dirty = true;
    std::vector<std::unique_ptr<Layer>> m_layers;
    Layer* m_activeLayer = nullptr;
    std::vector<Group> m_groups;
};
