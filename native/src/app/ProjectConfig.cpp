#include "ProjectConfig.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace ProjectConfig {

static std::filesystem::path g_projectsDir;
static int g_writeVersion = CURRENT_FORMAT_VERSION;
static bool g_loaded = false;

static std::filesystem::path homeDir() {
    const char* h = std::getenv("HOME");
    if (h && h[0]) return std::filesystem::path(h);
    return std::filesystem::current_path();
}

static std::filesystem::path configDir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) return std::filesystem::path(xdg) / "mammoth";
    return homeDir() / ".config" / "mammoth";
}

static std::filesystem::path settingsPath() {
    return configDir() / "settings.ini";
}

const char* defaultProjectName() { return "Untitled Project"; }

void loadSettings() {
    g_loaded = true;
    g_projectsDir.clear();
    g_writeVersion = CURRENT_FORMAT_VERSION;

    std::ifstream in(settingsPath());
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (key == "projects_dir" && !val.empty())
                g_projectsDir = val;
            else if (key == "write_version") {
                int v = std::atoi(val.c_str());
                if (v >= MIN_READABLE_VERSION && v <= MAX_FORMAT_VERSION)
                    g_writeVersion = v;
            }
        }
    }

    if (g_projectsDir.empty()) {
        const char* env = std::getenv("MAMMOTH_PROJECTS_DIR");
        if (env && env[0])
            g_projectsDir = env;
        else
            g_projectsDir = homeDir() / "Mprojects";
    }
}

void saveSettings() {
    if (!g_loaded) loadSettings();
    std::error_code ec;
    std::filesystem::create_directories(configDir(), ec);
    std::ofstream out(settingsPath());
    if (!out) return;
    out << "projects_dir=" << getProjectsDir().string() << "\n";
    out << "write_version=" << g_writeVersion << "\n";
}

std::filesystem::path getProjectsDir() {
    if (!g_loaded) loadSettings();
    if (g_projectsDir.empty())
        g_projectsDir = homeDir() / "Mprojects";
    return g_projectsDir;
}

void setProjectsDir(const std::filesystem::path& dir) {
    if (!g_loaded) loadSettings();
    g_projectsDir = dir;
    std::error_code ec;
    std::filesystem::create_directories(g_projectsDir, ec);
    std::filesystem::create_directories(g_projectsDir / ".thumbs", ec);
    saveSettings();
}

std::filesystem::path getThumbsDir() {
    return getProjectsDir() / ".thumbs";
}

std::filesystem::path projectPath(const std::string& name) {
    return getProjectsDir() / (sanitizeProjectName(name) + PROJECT_EXT);
}

std::filesystem::path thumbPath(const std::string& name) {
    return getThumbsDir() / (sanitizeProjectName(name) + THUMB_EXT);
}

int writeFormatVersion() {
    if (!g_loaded) loadSettings();
    return g_writeVersion;
}

void setWriteFormatVersion(int version) {
    if (!g_loaded) loadSettings();
    if (version < MIN_READABLE_VERSION) version = MIN_READABLE_VERSION;
    if (version > MAX_FORMAT_VERSION) version = MAX_FORMAT_VERSION;
    g_writeVersion = version;
    saveSettings();
}

std::string sanitizeProjectName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c == '/' || c == '\\' || c == '\0' || c == ':' || c < 32)
            continue;
        out.push_back((char)c);
    }
    // Trim
    size_t b = out.find_first_not_of(" \t.");
    if (b == std::string::npos) return {};
    size_t e = out.find_last_not_of(" \t.");
    out = out.substr(b, e - b + 1);
    if (out == "." || out == "..") return {};
    return out;
}

} // namespace ProjectConfig
