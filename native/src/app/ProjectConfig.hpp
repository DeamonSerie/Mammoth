#pragma once
#include <filesystem>
#include <string>

// User-configurable project paths and PSD format version.
// Defaults: ~/Mprojects/, PSD format version 1, 256×256 thumbnails.
namespace ProjectConfig {

constexpr const char* PROJECT_EXT = ".psd";
constexpr const char* THUMB_EXT = ".png";
constexpr int THUMB_SIZE = 256;
constexpr int CURRENT_FORMAT_VERSION = 1;
constexpr int MIN_READABLE_VERSION = 1;
constexpr int MAX_FORMAT_VERSION = 1;

const char* defaultProjectName();  // "Untitled Project"

// ~/Mprojects/ unless overridden by settings, env, or setProjectsDir().
std::filesystem::path getProjectsDir();
void setProjectsDir(const std::filesystem::path& dir);

std::filesystem::path getThumbsDir();
std::filesystem::path projectPath(const std::string& name);
std::filesystem::path thumbPath(const std::string& name);

int writeFormatVersion();
void setWriteFormatVersion(int version);

// Load ~/.config/mammoth/settings.ini (created on first save).
void loadSettings();
void saveSettings();

std::string sanitizeProjectName(const std::string& name);

} // namespace ProjectConfig
