#include "ProjectManager.hpp"
#include "ProjectConfig.hpp"
#include "../document/DrawingDocument.hpp"
#include "../io/PsdWriter.hpp"
#include "../io/PsdReader.hpp"
#include "../io/PsdCodec.hpp"
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <chrono>

ProjectManager& ProjectManager::instance() { static ProjectManager manager; return manager; }

void ProjectManager::init() {
    if (m_initialized) return;
    ProjectConfig::loadSettings();
    std::error_code ec;
    std::filesystem::create_directories(ProjectConfig::getProjectsDir(), ec);
    std::filesystem::create_directories(ProjectConfig::getThumbsDir(), ec);
    m_initialized = true;
    bool hasProject = false;
    for (const auto& entry : std::filesystem::directory_iterator(ProjectConfig::getProjectsDir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ProjectConfig::PROJECT_EXT) { hasProject = true; break; }
    }
    if (!hasProject) createProject(ProjectConfig::defaultProjectName(), 512, 512);
}

std::vector<ProjectManager::ProjectInfo> ProjectManager::listProjects() const {
    std::vector<ProjectInfo> results;
    std::error_code ec;
    const auto dir = ProjectConfig::getProjectsDir();
    if (!std::filesystem::is_directory(dir, ec)) return results;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file() || entry.path().extension() != ProjectConfig::PROJECT_EXT) continue;
        ProjectInfo p;
        p.name = entry.path().stem().string(); p.path = entry.path().string();
        p.thumbPath = ProjectConfig::thumbPath(p.name).string();
        auto stamp = entry.last_write_time(ec);
        if (!ec) p.modifiedTime = (std::time_t)std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
        DrawingDocument loaded;
        Psd::MammothMeta meta;
        if (PsdReader::read(p.path, loaded, &meta)) {
            p.width = loaded.width(); p.height = loaded.height(); p.frameCount = loaded.frameCount();
            for (int i=0;i<loaded.frameCount();++i) if (const Frame* f=loaded.getFrame(i)) p.layerCount += f->layerCount();
        }
        results.push_back(std::move(p));
    }
    std::sort(results.begin(), results.end(), [](const ProjectInfo& a, const ProjectInfo& b) { return a.modifiedTime > b.modifiedTime; });
    return results;
}

bool ProjectManager::createProject(const std::string& name, int width, int height) {
    std::string clean = ProjectConfig::sanitizeProjectName(name);
    if (clean.empty() || width <= 0 || height <= 0) return false;
    if (std::filesystem::exists(ProjectConfig::projectPath(clean))) return false;
    DrawingDocument doc(width, height, clean.c_str());
    return saveProject(clean, doc);
}

bool ProjectManager::saveProject(const std::string& name, const DrawingDocument& doc) {
    init();
    std::string clean = ProjectConfig::sanitizeProjectName(name);
    if (clean.empty()) return false;
    Psd::MammothMeta meta;
    meta.formatVersion = ProjectConfig::writeFormatVersion();
    meta.docName = clean; meta.width=doc.width(); meta.height=doc.height();
    for (int i=0;i<doc.frameCount();++i) {
        const Frame* f=doc.getFrame(i); if (!f) continue;
        Psd::MammothMeta::FrameExtra e; e.name=f->name(); e.duration=f->duration(); e.opacity=f->opacity(); e.visible=f->visible();
        for (int j=0;j<f->layerCount();++j) { const Layer* l=f->getLayer(j); if (!l) continue; Psd::MammothMeta::FrameExtra::LayerExtra x; x.isAttribute=l->isAttributeLayer(); x.attrSource=l->attributeSourceIndex(); x.attrOpacity=l->attrOpacity(); x.attrTint=l->attrTint(); x.color=l->color(); e.layers.push_back(x); }
        meta.frames.push_back(std::move(e));
    }
    for (int i=0;i<doc.frameGroupCount();++i) { const auto& g=doc.getFrameGroup(i); Psd::MammothMeta::FrameGroupExtra e; e.name=g.name; e.color=g.color; e.collapsed=g.collapsed; e.frameIndices=g.frameIndices; meta.frameGroups.push_back(std::move(e)); }
    if (!PsdWriter::write(doc, ProjectConfig::projectPath(clean).string(), meta)) return false;
    const Frame* f=doc.getFrame(0);
    if (f) { std::vector<uint8_t> pixels; int w,h; f->compositeToBuffer(pixels,w,h); std::vector<uint8_t> thumb((size_t)ProjectConfig::THUMB_SIZE*ProjectConfig::THUMB_SIZE*4); Psd::resizeNearest(pixels.data(),w,h,thumb.data(),ProjectConfig::THUMB_SIZE,ProjectConfig::THUMB_SIZE); Psd::writePngRGBA(ProjectConfig::thumbPath(clean).string(),thumb.data(),ProjectConfig::THUMB_SIZE,ProjectConfig::THUMB_SIZE); }
    return true;
}

bool ProjectManager::loadProject(const std::string& name, DrawingDocument& out) { init(); return PsdReader::read(ProjectConfig::projectPath(name).string(), out); }

bool ProjectManager::deleteProject(const std::string& name) { init(); std::error_code ec; bool ok=std::filesystem::remove(ProjectConfig::projectPath(name),ec); std::filesystem::remove(ProjectConfig::thumbPath(name),ec); return ok && !ec; }

bool ProjectManager::renameProject(const std::string& oldName, const std::string& newName) {
    init();
    std::string cleanNew = ProjectConfig::sanitizeProjectName(newName);
    if (cleanNew.empty()) return false;
    auto from = ProjectConfig::projectPath(oldName), to = ProjectConfig::projectPath(cleanNew);
    if (std::filesystem::exists(to)) return false;
    std::error_code ec;
    std::filesystem::rename(from, to, ec);
    if (ec) return false;
    auto tf = ProjectConfig::thumbPath(oldName), tt = ProjectConfig::thumbPath(cleanNew);
    if (std::filesystem::exists(tf)) std::filesystem::rename(tf, tt, ec);
    return !ec;
}

bool ProjectManager::duplicateProject(const std::string& name, const std::string& newName) {
    init();
    if (ProjectConfig::sanitizeProjectName(newName).empty() || std::filesystem::exists(ProjectConfig::projectPath(newName))) return false;
    DrawingDocument document;
    if (!loadProject(name, document)) return false;
    document.setName(newName.c_str());
    return saveProject(newName, document);
}
