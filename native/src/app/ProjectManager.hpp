#pragma once
#include <ctime>
#include <string>
#include <vector>

class DrawingDocument;

class ProjectManager {
public:
    struct ProjectInfo {
        std::string name;
        std::string path;
        std::string thumbPath;
        std::time_t modifiedTime = 0;
        int width = 0, height = 0;
        int frameCount = 0;
        int layerCount = 0;
    };

    static ProjectManager& instance();
    void init();
    std::vector<ProjectInfo> listProjects() const;
    bool createProject(const std::string& name, int width, int height);
    bool saveProject(const std::string& name, const DrawingDocument& doc);
    bool loadProject(const std::string& name, DrawingDocument& out);
    bool deleteProject(const std::string& name);
    bool renameProject(const std::string& oldName, const std::string& newName);
    bool duplicateProject(const std::string& name, const std::string& newName);
private:
    bool m_initialized = false;
};
