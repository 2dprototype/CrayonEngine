#include "project_store.hpp"
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace crayon::editor {

namespace {

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': break;
            case '\t': o += "\\t";  break;
            default:   o += c;
        }
    }
    return o;
}

std::string json_unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            switch (n) {
                case 'n': o += '\n'; break;
                case 't': o += '\t'; break;
                default:  o += n;
            }
        } else o += s[i];
    }
    return o;
}

// Minimal "key": "value" extractor matching the engine's own manifest parser.
bool get_field(const std::string& json, const std::string& key, std::string& out) {
    std::string pat = "\"" + key + "\"";
    size_t k = json.find(pat);
    if (k == std::string::npos) return false;
    size_t colon = json.find(':', k + pat.size());
    if (colon == std::string::npos) return false;
    size_t q1 = json.find('"', colon + 1);
    if (q1 == std::string::npos) return false;
    size_t q2 = q1 + 1;
    while (q2 < json.size()) {
        if (json[q2] == '\\') { q2 += 2; continue; }
        if (json[q2] == '"') break;
        ++q2;
    }
    if (q2 >= json.size()) return false;
    out = json_unescape(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

std::string slugify(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') o += c;
        else if (c == ' ') o += '_';
    }
    return o;
}

bool write_text(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f << text;
    return true;
}

const char* kEmptyLua = R"(-- %NAME%
function crayon.init()
end

function crayon.update(dt)
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.13, 1.0)
end
)";

const char* k2DLua = R"(-- %NAME% (2D starter)
function crayon.config(t)
    t.window.title = "%NAME%"
    t.window.width = 640
    t.window.height = 480
    t.window.virtualWidth = 320
    t.window.virtualHeight = 240
    t.window.scaling = "aspect"
    t.window.resizable = true
end

local player = { x = 150, y = 110, size = 20, speed = 120 }

function crayon.update(dt)
    if crayon.input.isKeyDown("left")  or crayon.input.isKeyDown("a") then player.x = player.x - player.speed * dt end
    if crayon.input.isKeyDown("right") or crayon.input.isKeyDown("d") then player.x = player.x + player.speed * dt end
    if crayon.input.isKeyDown("up")    or crayon.input.isKeyDown("w") then player.y = player.y - player.speed * dt end
    if crayon.input.isKeyDown("down")  or crayon.input.isKeyDown("s") then player.y = player.y + player.speed * dt end
end

function crayon.draw()
    crayon.graphics.clear(0.08, 0.09, 0.13, 1.0)
    crayon.graphics.setColor(0.95, 0.45, 0.25, 1.0)
    crayon.graphics.drawRect("fill", player.x, player.y, player.size, player.size)
end
)";

const char* k3DLua = R"(-- %NAME% (3D starter)
function crayon.config(t)
    t.window.title = "%NAME%"
    t.window.width = 640
    t.window.height = 480
    t.modules.mesh3D = true
end

local angle = 0.0

function crayon.update(dt)
    angle = angle + 30.0 * dt
    local r = math.rad(angle)
    crayon.graphics.setCamera3d({
        position = { math.sin(r) * 8.0, 4.0, math.cos(r) * 8.0 },
        target   = { 0.0, 0.5, 0.0 },
        up       = { 0.0, 1.0, 0.0 },
        fov      = 50.0
    })
end

function crayon.draw()
    crayon.graphics.clear(0.06, 0.07, 0.11, 1.0)
end
)";

std::string fill_name(std::string s, const std::string& name) {
    const std::string key = "%NAME%";
    for (size_t p = 0; (p = s.find(key, p)) != std::string::npos; p += name.size())
        s.replace(p, key.size(), name);
    return s;
}

} // namespace

fs::path ProjectStore::config_file() const {
    char* pref = SDL_GetPrefPath("Crayon", "CrayonEditor");
    fs::path base = pref ? fs::path(pref) : fs::current_path();
    if (pref) SDL_free(pref);
    return base / "editor.cfg";
}

std::string ProjectStore::find_crayon() {
#ifdef _WIN32
    const char* exe = "crayon.exe";
#else
    const char* exe = "crayon";
#endif
    std::vector<fs::path> candidates;
    if (const char* base = SDL_GetBasePath()) {
        candidates.push_back(fs::path(base) / exe);
        candidates.push_back(fs::path(base) / ".." / exe);
        candidates.push_back(fs::path(base) / "build" / exe);
    }
    candidates.push_back(fs::current_path() / exe);
    candidates.push_back(fs::current_path() / "build" / exe);
    for (auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return fs::weakly_canonical(c, ec).string();
    }
    return exe; // rely on PATH
}

bool ProjectStore::read_manifest(const fs::path& dir, Manifest& m) {
    std::ifstream f(dir / ".crayonproj");
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    std::string j = ss.str();
    get_field(j, "name", m.name);
    get_field(j, "version", m.version);
    get_field(j, "author", m.author);
    get_field(j, "description", m.description);
    get_field(j, "main", m.main);
    get_field(j, "targetSrc", m.targetSrc);
    get_field(j, "license", m.license);
    if (m.name.empty()) m.name = dir.filename().string();
    return true;
}

bool ProjectStore::write_manifest(const fs::path& dir, const Manifest& m) {
    std::ostringstream o;
    o << "{\n"
      << "  \"name\": \""        << json_escape(m.name)        << "\",\n"
      << "  \"version\": \""     << json_escape(m.version)     << "\",\n"
      << "  \"author\": \""      << json_escape(m.author)      << "\",\n"
      << "  \"description\": \"" << json_escape(m.description) << "\",\n"
      << "  \"main\": \""        << json_escape(m.main)        << "\",\n";
    if (!m.targetSrc.empty()) o << "  \"targetSrc\": \"" << json_escape(m.targetSrc) << "\",\n";
    o << "  \"license\": \""     << json_escape(m.license)     << "\"\n}\n";
    return write_text(dir / ".crayonproj", o.str());
}

bool ProjectStore::create_project(const std::string& parent, const Manifest& m, Template t,
                                  std::string& out_dir, std::string& error) {
    std::string slug = slugify(m.name);
    if (slug.empty()) { error = "Project name must contain letters or digits."; return false; }
    if (parent.empty()) { error = "Choose a location for the project."; return false; }

    std::error_code ec;
    fs::path dir = fs::absolute(fs::path(parent) / slug, ec).lexically_normal();
    if (fs::exists(dir, ec) && !fs::is_empty(dir, ec)) { error = "Folder already exists and is not empty."; return false; }

    fs::create_directories(dir / "src", ec);
    if (ec) { error = "Cannot create folder: " + ec.message(); return false; }
    fs::create_directories(dir / "assets", ec);

    Manifest mm = m;
    if (mm.main.empty()) mm.main = "src/main.lua";
    if (!write_manifest(dir, mm)) { error = "Could not write .crayonproj."; return false; }

    const char* src = kEmptyLua;
    if (t == Template::Starter2D) src = k2DLua;
    else if (t == Template::Starter3D) src = k3DLua;

    fs::path entry = dir / mm.main;
    fs::create_directories(entry.parent_path(), ec);
    if (!write_text(entry, fill_name(src, mm.name))) { error = "Could not write entry script."; return false; }

    out_dir = dir.string();
    return true;
}

void ProjectStore::refresh(ProjectEntry& e) const {
    std::error_code ec;
    e.exists = fs::exists(fs::path(e.dir) / ".crayonproj", ec);
    if (e.exists) { Manifest m; if (read_manifest(e.dir, m)) e.manifest = m; }
}

bool ProjectStore::add_project(const std::string& path, std::string& error) {
    std::error_code ec;
    fs::path p = fs::absolute(path, ec).lexically_normal();
    if (fs::is_regular_file(p, ec)) p = p.parent_path();
    if (!fs::exists(p / ".crayonproj", ec)) { error = "No .crayonproj found in: " + p.string(); return false; }

    std::string dir = p.string();
    m_projects.erase(std::remove_if(m_projects.begin(), m_projects.end(),
                     [&](const ProjectEntry& e) { return e.dir == dir; }), m_projects.end());
    ProjectEntry e; e.dir = dir;
    refresh(e);
    m_projects.insert(m_projects.begin(), e);
    save();
    return true;
}

void ProjectStore::remove_project(const std::string& dir) {
    m_projects.erase(std::remove_if(m_projects.begin(), m_projects.end(),
                     [&](const ProjectEntry& e) { return e.dir == dir; }), m_projects.end());
    save();
}

void ProjectStore::load() {
    m_projects.clear();
    std::ifstream f(config_file());
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "crayon") m_crayon_path = v;
        else if (k == "location") m_default_location = v;
        else if (k == "project") {
            ProjectEntry e; e.dir = v;
            refresh(e);
            m_projects.push_back(e);
        }
    }
    if (m_crayon_path.empty()) m_crayon_path = find_crayon();
    if (m_default_location.empty()) {
        const char* home = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
        m_default_location = home ? (fs::path(home) / "CrayonProjects").string() : "CrayonProjects";
    }
}

void ProjectStore::save() const {
    std::error_code ec;
    fs::create_directories(config_file().parent_path(), ec);
    std::ofstream f(config_file());
    if (!f) return;
    f << "crayon=" << m_crayon_path << "\n";
    f << "location=" << m_default_location << "\n";
    for (auto& e : m_projects) f << "project=" << e.dir << "\n";
}

} // namespace crayon::editor
