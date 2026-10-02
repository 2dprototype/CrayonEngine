#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace crayon::editor {

namespace fs = std::filesystem;

struct Manifest {
    std::string name;
    std::string version = "1.0.0";
    std::string author;
    std::string description;
    std::string main = "src/main.lua";
    std::string targetSrc;
    std::string license = "MIT";
};

struct ProjectEntry {
    std::string dir;       // absolute project directory
    Manifest manifest;
    bool exists = true;    // .crayonproj still present on disk
};

enum class Template { Empty, Starter2D, Starter3D };

class ProjectStore {
public:
    void load();
    void save() const;

    std::vector<ProjectEntry>& projects() { return m_projects; }
    std::string& crayon_path() { return m_crayon_path; }
    std::string& default_location() { return m_default_location; }

    // Adds (or moves to front) the project in `path` (a folder or a .crayonproj file).
    bool add_project(const std::string& path, std::string& error);
    void remove_project(const std::string& dir);
    void refresh(ProjectEntry& e) const;

    static bool read_manifest(const fs::path& dir, Manifest& out);
    static bool write_manifest(const fs::path& dir, const Manifest& m);
    static bool create_project(const std::string& parent, const Manifest& m, Template t, std::string& out_dir, std::string& error);

    // Locates crayon.exe next to the editor, in the cwd, or falls back to PATH.
    static std::string find_crayon();

private:
    fs::path config_file() const;

    std::vector<ProjectEntry> m_projects;
    std::string m_crayon_path;
    std::string m_default_location;
};

} // namespace crayon::editor
