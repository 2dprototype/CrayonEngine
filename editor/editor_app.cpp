#include "editor_app.hpp"
#include "editor_theme.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string_view>

namespace crayon::editor {

namespace {

// --- async native dialog bridge ---
struct DialogResult {
    std::mutex mtx;
    bool ready = false;
    int kind = 0;
    std::string path;
} g_dialog;

void SDLCALL dialog_cb(void* userdata, const char* const* list, int) {
    std::lock_guard<std::mutex> lk(g_dialog.mtx);
    g_dialog.kind = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
    g_dialog.path = (list && list[0]) ? list[0] : "";
    g_dialog.ready = !g_dialog.path.empty();
}

#if IMGUI_VERSION_NUM >= 19110
  #define CRAYON_CHILD_BORDER ImGuiChildFlags_Borders
#else
  #define CRAYON_CHILD_BORDER ImGuiChildFlags_Border
#endif

void open_folder(const std::string& dir) {
    std::string d = dir;
    std::replace(d.begin(), d.end(), '\\', '/');
    SDL_OpenURL(((d.size() && d[0] == '/') ? "file://" + d : "file:///" + d).c_str());
}

const ImVec4 kAccent   (0.26f, 0.54f, 0.95f, 1.0f);
const ImVec4 kMuted    (0.60f, 0.62f, 0.68f, 1.0f);
const ImVec4 kGood     (0.35f, 0.80f, 0.50f, 1.0f);
const ImVec4 kWarn     (0.95f, 0.75f, 0.30f, 1.0f);
const ImVec4 kBad      (0.95f, 0.40f, 0.38f, 1.0f);
const ImVec4 kDim      (0.48f, 0.50f, 0.56f, 1.0f);

// Fast case-insensitive substring search. No allocations, no O(n*m) inner loop.
bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    if (hay.size() < needle.size()) return false;
    auto it = std::search(
        hay.begin(), hay.end(),
        needle.begin(), needle.end(),
        [](char a, char b) noexcept {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
        });
    return it != hay.end();
}

bool read_text(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

void copy_to(char* dst, size_t cap, const std::string& s) { snprintf(dst, cap, "%s", s.c_str()); }

void section_label(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

void status_pill(const char* text, ImVec4 color) {
    ImVec2 pad(9.0f, 3.0f);
    ImVec2 tsz = ImGui::CalcTextSize(text);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(tsz.x + pad.x * 2.0f, tsz.y + pad.y * 2.0f);
    ImVec2 end(pos.x + size.x, pos.y + size.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, end, ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.18f)), size.y * 0.5f);
    dl->AddRect(pos, end,       ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.55f)), size.y * 0.5f);
    dl->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), ImGui::GetColorU32(color), text);
    ImGui::Dummy(size);
}

int count_lines(const std::string& s) {
    if (s.empty()) return 0;
    int n = 1;
    for (char c : s) if (c == '\n') ++n;
    return n;
}

std::string human_size(uintmax_t bytes) {
    char buf[48];
    if (bytes < 1024) snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    else if (bytes < 1024 * 1024) snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

// File extension → tag + category (called once per file during a directory scan).
void classify_file(const std::string& name, bool is_dir, const char*& tag, FileCat& cat) {
    if (is_dir) { tag = "DIR"; cat = FileCat::Dir; return; }

    size_t dot = name.rfind('.');
    if (dot == std::string::npos) { tag = "···"; cat = FileCat::Other; return; }

    size_t n = name.size() - dot - 1;
    const char* p = name.data() + dot + 1;
    auto eq = [&](const char* s) noexcept {
        for (size_t i = 0; i < n; ++i) {
            if (s[i] == '\0') return false;
            if (std::tolower((unsigned char)p[i]) != s[i]) return false;
        }
        return s[n] == '\0';
    };
    auto any = [&](std::initializer_list<const char*> xs) {
        for (auto* s : xs) if (eq(s)) return true;
        return false;
    };

    if (eq("lua"))                                { tag = "LUA"; cat = FileCat::Lua;      return; }
    if (eq("md"))                                 { tag = "MD "; cat = FileCat::Markdown; return; }
    if (eq("txt"))                                { tag = "TXT"; cat = FileCat::Text;     return; }
    if (eq("json"))                               { tag = "JSN"; cat = FileCat::Json;     return; }
    if (any({"png","jpg","jpeg","bmp","gif"}))    { tag = "IMG"; cat = FileCat::Image;    return; }
    if (any({"obj","gltf","glb","fbx"}))          { tag = "3D "; cat = FileCat::Model3D;  return; }
    if (any({"cpp","hpp","h","c","cc"}))          { tag = "CPP"; cat = FileCat::Code;     return; }

    tag = "···"; cat = FileCat::Other;
}

ImVec4 cat_color(FileCat c) {
    switch (c) {
        case FileCat::Dir:      return ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
        case FileCat::Lua:      return ImVec4(0.40f, 0.85f, 0.55f, 1.0f);
        case FileCat::Markdown: return ImVec4(0.60f, 0.75f, 1.00f, 1.0f);
        case FileCat::Text:     return ImVec4(0.70f, 0.70f, 0.75f, 1.0f);
        case FileCat::Json:     return ImVec4(0.95f, 0.85f, 0.45f, 1.0f);
        case FileCat::Image:    return ImVec4(0.85f, 0.55f, 0.95f, 1.0f);
        case FileCat::Model3D:  return ImVec4(0.95f, 0.60f, 0.40f, 1.0f);
        case FileCat::Code:     return ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
        default:                return ImVec4(0.55f, 0.55f, 0.60f, 1.0f);
    }
}

// Console log classification, computed once per rebuild, not per frame.
unsigned char classify_level(const std::string& l) {
    if (l.find("ERROR") != std::string::npos || l.find("error") != std::string::npos) return 3;
    if (l.find("WARN")  != std::string::npos || l.find("warn")  != std::string::npos) return 2;
    if (l.rfind("[editor]", 0) == 0) return 1;
    return 0;
}

bool level_passes(unsigned char lv, int min_level) {
    switch (min_level) {
        case 1: return lv >= 2;
        case 2: return lv >= 3;
        default: return true;
    }
}

} // namespace

EditorApp::EditorApp() = default;
EditorApp::~EditorApp() { shutdown(); }

bool EditorApp::init(int width, int height, const std::string& title) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "[editor] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    m_window = SDL_CreateWindow(title.c_str(), width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_window) {
        std::fprintf(stderr, "[editor] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_gl = SDL_GL_CreateContext(m_window);
    if (!m_gl) {
        std::fprintf(stderr, "[editor] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(m_window, m_gl);
    SDL_GL_SetSwapInterval(1);
    SDL_SetWindowMinimumSize(m_window, 520, 360);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::fprintf(stderr, "[editor] gladLoadGLLoader failed\n");
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    apply_modern_dark_theme();

    float scale = SDL_GetWindowDisplayScale(m_window);
    if (scale > 1.05f) { io.FontGlobalScale = scale; ImGui::GetStyle().ScaleAllSizes(scale); }

    if (!ImGui_ImplSDL3_InitForOpenGL(m_window, m_gl)) return false;
    if (!ImGui_ImplOpenGL3_Init("#version 330")) return false;

    m_store.load();
    if (!m_store.projects().empty()) select_project(m_store.projects().front().dir);

    m_running = true;
    return true;
}

// ---------------------------------------------------------------- actions ---

ProjectEntry* EditorApp::current() {
    for (auto& p : m_store.projects()) if (p.dir == m_selected) return &p;
    return nullptr;
}

void EditorApp::toast(const std::string& msg, bool error) {
    m_status_msg   = msg;
    m_status_error = error;
    m_status_until = SDL_GetTicks() / 1000.0 + 4.0;
}

void EditorApp::select_project(const std::string& dir) {
    if (dir == m_selected) return;
    flush_dirty_file();
    m_selected = dir;
    m_open_rel.clear(); m_buffer.clear();
    m_file_dirty = false; m_file_editable = false; m_file_note.clear();
    m_file_line_count = 0; m_file_lines_valid = true;
    if (auto* p = current()) {
        m_store.refresh(*p);
        m_edit = p->manifest;
        m_manifest_dirty = false;
        refresh_files();
    }
}

void EditorApp::request_dialog(DialogKind kind) {
    void* ud = reinterpret_cast<void*>(static_cast<intptr_t>(kind));
    if (kind == DialogKind::CrayonExe) SDL_ShowOpenFileDialog(dialog_cb, ud, m_window, nullptr, 0, nullptr, false);
    else SDL_ShowOpenFolderDialog(dialog_cb, ud, m_window, nullptr, false);
}

void EditorApp::open_project_dialog() { request_dialog(DialogKind::OpenProject); }

void EditorApp::poll_dialog() {
    std::string path; int kind = 0;
    {
        std::lock_guard<std::mutex> lk(g_dialog.mtx);
        if (!g_dialog.ready) return;
        g_dialog.ready = false;
        path = g_dialog.path; kind = g_dialog.kind;
    }
    switch (static_cast<DialogKind>(kind)) {
        case DialogKind::OpenProject: {
            std::string err;
            if (m_store.add_project(path, err)) {
                m_selected.clear();
                select_project(m_store.projects().front().dir);
                toast("Opened project: " + m_edit.name);
            } else toast(err, true);
            break;
        }
        case DialogKind::NewLocation:     copy_to(m_np_location, sizeof(m_np_location), path); break;
        case DialogKind::DefaultLocation: m_store.default_location() = path; m_store.save(); break;
        case DialogKind::CrayonExe:       m_store.crayon_path() = path; m_store.save(); break;
        default: break;
    }
}

void EditorApp::save_manifest() {
    auto* p = current();
    if (!p) return;
    if (m_edit.name.empty()) { toast("Project name cannot be empty.", true); return; }
    if (ProjectStore::write_manifest(p->dir, m_edit)) {
        p->manifest = m_edit;
        m_manifest_dirty = false;
        toast("Saved .crayonproj");
    } else toast("Failed to write .crayonproj", true);
}

void EditorApp::refresh_files() {
    m_files.clear();
    auto* p = current();
    if (!p) return;
    std::error_code ec;
    fs::path root = p->dir;
    std::vector<std::pair<std::string, bool>> all;
    all.reserve(256);
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        std::string name = it->path().filename().string();
        if (!name.empty() && name[0] == '.') {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        all.emplace_back(fs::relative(it->path(), root, ec).generic_string(), it->is_directory(ec));
        if (all.size() > 4000) break;
    }
    std::sort(all.begin(), all.end(), [](auto& a, auto& b) {
        return std::lexicographical_compare(a.first.begin(), a.first.end(), b.first.begin(), b.first.end(),
            [](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
    });
    m_files.reserve(all.size());
    for (auto& [rel, dir] : all) {
        FileItem f;
        f.rel = std::move(rel); f.is_dir = dir;
        f.name = fs::path(f.rel).filename().string();
        f.depth = (int)std::count(f.rel.begin(), f.rel.end(), '/');
        classify_file(f.name, f.is_dir, f.tag, f.cat);   // ← cached here, not per frame
        m_files.push_back(std::move(f));
    }
}

void EditorApp::flush_dirty_file() { if (m_file_dirty) save_file(); }

void EditorApp::open_file(const std::string& rel) {
    auto* p = current();
    if (!p) return;
    flush_dirty_file();
    m_open_rel = rel; m_buffer.clear();
    m_file_dirty = false; m_file_editable = false; m_file_note.clear();
    m_file_line_count = 0; m_file_lines_valid = true;

    fs::path full = fs::path(p->dir) / rel;
    std::error_code ec;
    auto size = fs::file_size(full, ec);
    if (ec) { m_file_note = "Cannot read file."; return; }
    if (size > 2 * 1024 * 1024) { m_file_note = "File is larger than 2 MB and cannot be edited here."; return; }
    if (!read_text(full, m_buffer)) { m_file_note = "Cannot read file."; return; }
    if (m_buffer.find('\0') != std::string::npos) { m_buffer.clear(); m_file_note = "Binary file - preview not available."; return; }
    m_file_editable = true;
    m_file_line_count = count_lines(m_buffer);
    m_file_lines_valid = true;
}

void EditorApp::save_file() {
    auto* p = current();
    if (!p || m_open_rel.empty() || !m_file_editable) return;
    std::ofstream f(fs::path(p->dir) / m_open_rel, std::ios::binary);
    if (!f) { toast("Failed to save " + m_open_rel, true); return; }
    f << m_buffer;
    m_file_dirty = false;
    toast("Saved " + m_open_rel);
}

void EditorApp::run_project() {
    auto* p = current();
    if (!p) return;
    if (!p->exists) { toast("Project folder or .crayonproj is missing.", true); return; }
    flush_dirty_file();
    if (m_manifest_dirty) save_manifest();
    if (m_clear_console_on_run) m_runner.clear();
    m_runner.push_line("[editor] Running '" + p->manifest.name + "' with " + m_store.crayon_path());
    if (m_runner.start(m_store.crayon_path(), p->dir)) toast("Running " + p->manifest.name);
    else toast("Could not start crayon. Check Settings > crayon path.", true);
    m_request_tab = Tab::Console; m_force_tab = true;
}

void EditorApp::stop_project() {
    if (m_runner.is_running()) { m_runner.stop(); toast("Stopped"); }
}

void EditorApp::handle_shortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (io.KeyShift) stop_project(); else run_project();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (m_tab == Tab::Overview && m_manifest_dirty) save_manifest();
        else save_file();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) m_open_new_project = true;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) open_project_dialog();
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) m_open_shortcuts = true;
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) m_open_about = true;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B, false)) m_show_sidebar = !m_show_sidebar;
}

// ------------------------------------------------------------------- UI -----

void EditorApp::render_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;
    auto* p = current();

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project...", "Ctrl+N")) m_open_new_project = true;
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) open_project_dialog();
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, p != nullptr)) {
            if (m_tab == Tab::Overview && m_manifest_dirty) save_manifest();
            else save_file();
        }
        if (ImGui::MenuItem("Refresh Project", nullptr, false, p != nullptr)) refresh_files();
        ImGui::Separator();
        if (ImGui::MenuItem("Settings...")) m_open_settings = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) m_running = false;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Project")) {
        bool can_run = p && p->exists;
        if (ImGui::MenuItem("Run", "F5", false, can_run && !m_runner.is_running())) run_project();
        if (ImGui::MenuItem("Stop", "Shift+F5", false, m_runner.is_running())) stop_project();
        ImGui::Separator();
        if (ImGui::MenuItem("Open Folder", nullptr, false, p != nullptr)) open_folder(p->dir);
        if (ImGui::MenuItem("Copy Path", nullptr, false, p != nullptr)) SDL_SetClipboardText(p->dir.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Remove From List", nullptr, false, p != nullptr)) {
            std::string d = m_selected;
            m_selected.clear();
            m_store.remove_project(d);
            if (!m_store.projects().empty()) select_project(m_store.projects().front().dir);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Sidebar", "Ctrl+B", &m_show_sidebar);
        ImGui::MenuItem("Status Bar", nullptr, &m_show_status_bar);
        ImGui::Separator();
        if (ImGui::MenuItem("Overview Tab", nullptr, false, p != nullptr)) { m_tab = Tab::Overview; m_request_tab = m_tab; m_force_tab = true; }
        if (ImGui::MenuItem("Files Tab",    nullptr, false, p != nullptr)) { m_tab = Tab::Files;    m_request_tab = m_tab; m_force_tab = true; }
        if (ImGui::MenuItem("Console Tab",  nullptr, false, p != nullptr)) { m_tab = Tab::Console;  m_request_tab = m_tab; m_force_tab = true; }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Keyboard Shortcuts", "F1")) m_open_shortcuts = true;
        if (ImGui::MenuItem("About Crayon")) m_open_about = true;
        ImGui::EndMenu();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::Text("%.0f FPS  |  %.2f ms",
                ImGui::GetIO().Framerate,
                1000.0f / std::max(1.0f, ImGui::GetIO().Framerate));
    ImGui::PopStyleColor();

    ImGui::EndMainMenuBar();
}

void EditorApp::render_root() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##root", nullptr, flags);
    ImGui::PopStyleVar(2);

    float status_h = m_show_status_bar ? ImGui::GetFrameHeight() + 4.0f : 0.0f;
    float avail_w  = ImGui::GetContentRegionAvail().x;
    float body_h   = ImGui::GetContentRegionAvail().y - status_h;

    const float min_main_w = 360.0f;
    bool can_show_sidebar = m_show_sidebar && (avail_w >= min_main_w + 200.0f);
    float side_w = 0.0f;
    if (can_show_sidebar) {
        m_sidebar_width = std::clamp(m_sidebar_width, 200.0f, std::max(200.0f, avail_w - min_main_w));
        side_w = m_sidebar_width;
    }
    bool narrow = avail_w < 520.0f;

    // -------- Sidebar + splitter --------
    if (can_show_sidebar) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::BeginChild("##sidebar", ImVec2(side_w, body_h), ImGuiChildFlags_None);
        ImGui::PopStyleVar();
        render_sidebar(side_w);
        ImGui::EndChild();

        ImGui::SameLine(0, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::InvisibleButton("##splitter", ImVec2(4.0f, body_h));
        ImGui::PopStyleVar();
        bool active = ImGui::IsItemActive();
        bool hover  = ImGui::IsItemHovered();
        if (active) {
            m_sidebar_width += ImGui::GetIO().MouseDelta.x;
            m_sidebar_width = std::clamp(m_sidebar_width, 200.0f, std::max(200.0f, avail_w - min_main_w));
        }
        if (active || hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImVec2 hmin = ImGui::GetItemRectMin();
        ImVec2 hmax = ImGui::GetItemRectMax();
        ImU32 col = (active || hover) ? IM_COL32(80, 130, 220, 230) : IM_COL32(60, 62, 70, 120);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(hmin.x + 1.0f, hmin.y), ImVec2(hmax.x - 1.0f, hmax.y), col);
        ImGui::SameLine(0, 0);
    }

    // -------- Main area --------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 12));
    ImGui::BeginChild("##main", ImVec2(0, body_h), ImGuiChildFlags_AlwaysUseWindowPadding);

    if (narrow) {
        auto* cur = current();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##proj", cur ? cur->manifest.name.c_str() : "Select a project...")) {
            for (auto& pr : m_store.projects()) {
                bool sel = pr.dir == m_selected;
                if (ImGui::Selectable((pr.manifest.name + "##" + pr.dir).c_str(), sel)) select_project(pr.dir);
            }
            ImGui::EndCombo();
        }
        ImGui::Spacing();
    }

    if (current()) render_project_view(); else render_welcome();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (m_show_status_bar) render_status_bar();
    ImGui::End();
}

void EditorApp::render_sidebar(float width) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::BeginChild("##sidebar_inner", ImVec2(0, 0), ImGuiChildFlags_None);
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted("PROJECTS");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 22.0f);
    if (ImGui::SmallButton("+")) m_open_new_project = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("New project (Ctrl+N)");

    ImGui::Spacing();
    float bw = (width - 24 - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("+ New", ImVec2(bw, 0))) m_open_new_project = true;
    ImGui::SameLine();
    if (ImGui::Button("Open...", ImVec2(bw, 0))) open_project_dialog();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(width - 24);
    ImGui::InputTextWithHint("##filter", "Search projects", m_filter, sizeof(m_filter));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("##projlist", ImVec2(0, 0), ImGuiChildFlags_None);
    std::string to_remove;
    int shown = 0;
    std::string_view filter_view(m_filter);
    for (auto& pr : m_store.projects()) {
        if (!icontains(pr.manifest.name, filter_view) && !icontains(pr.dir, filter_view)) continue;
        ++shown;
        ImGui::PushID(pr.dir.c_str());
        bool sel = pr.dir == m_selected;
        float h = ImGui::GetTextLineHeight() * 2 + 12;

        if (ImGui::Selectable("##item", sel, ImGuiSelectableFlags_None, ImVec2(0, h))) select_project(pr.dir);
        bool hovered = ImGui::IsItemHovered();
        if (hovered && ImGui::IsMouseDoubleClicked(0)) { select_project(pr.dir); run_project(); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Select")) select_project(pr.dir);
            if (ImGui::MenuItem("Run")) { select_project(pr.dir); run_project(); }
            if (ImGui::MenuItem("Open Folder")) open_folder(pr.dir);
            if (ImGui::MenuItem("Copy Path")) SDL_SetClipboardText(pr.dir.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Remove from list")) to_remove = pr.dir;
            ImGui::EndPopup();
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 mn = ImGui::GetItemRectMin();
        ImVec2 mx = ImGui::GetItemRectMax();

        if (hovered && !sel)
            dl->AddRectFilled(mn, mx, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.03f)), 5.0f);
        if (sel) {
            dl->AddRectFilled(mn, mx, ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.14f)), 5.0f);
            dl->AddRect(mn, mx, ImGui::GetColorU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f)), 5.0f);
            dl->AddRectFilled(mn, ImVec2(mn.x + 3.0f, mx.y), ImGui::GetColorU32(kAccent), 5.0f);
        }
        ImU32 dot = pr.exists ? ImGui::GetColorU32(kGood) : ImGui::GetColorU32(kBad);
        dl->AddCircleFilled(ImVec2(mn.x + 14.0f, mn.y + h * 0.5f), 3.5f, dot);
        ImU32 tc = ImGui::GetColorU32(pr.exists ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        dl->AddText(ImVec2(mn.x + 26.0f, mn.y + 6.0f), tc, pr.manifest.name.c_str());
        std::string sub = pr.exists ? pr.dir : "(missing) " + pr.dir;
        ImU32 sc = pr.exists ? ImGui::GetColorU32(kMuted) : ImGui::GetColorU32(kBad);
        dl->PushClipRect(ImVec2(mn.x + 26.0f, mn.y), ImVec2(mx.x - 8.0f, mx.y), true);
        dl->AddText(ImVec2(mn.x + 26.0f, mn.y + 6.0f + ImGui::GetTextLineHeight()), sc, sub.c_str());
        dl->PopClipRect();
        ImGui::PopID();
    }
    if (shown == 0 && !m_store.projects().empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("No projects match your search.");
        ImGui::PopStyleColor();
    }
    if (m_store.projects().empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("No projects yet. Click New or Open to get started.");
        ImGui::PopStyleColor();
    }
    if (!to_remove.empty()) {
        bool was = to_remove == m_selected;
        m_store.remove_project(to_remove);
        if (was) { m_selected.clear(); if (!m_store.projects().empty()) select_project(m_store.projects().front().dir); }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}

void EditorApp::render_welcome() {
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0, std::max(8.0f, ImGui::GetContentRegionAvail().y * 0.12f)));

    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextUnformatted("Crayon Project Manager");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped(
        "Create, organize and run your Crayon games. "
        "Projects are described by a .crayonproj manifest and run with the standalone crayon player.");
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    float bw = std::min(180.0f, (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("New Project", ImVec2(bw, 34))) m_open_new_project = true;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Open Project", ImVec2(bw, 34))) open_project_dialog();

    if (!m_store.projects().empty()) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        section_label("RECENT PROJECTS");
        ImGui::Spacing();

        int shown = 0;
        for (auto& pr : m_store.projects()) {
            if (shown >= 5) break;
            ++shown;
            ImGui::PushID(pr.dir.c_str());
            float card_w = std::min(w, 520.0f);
            float card_h = ImGui::GetTextLineHeight() * 2 + 16.0f;
            ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##card", false, ImGuiSelectableFlags_None, ImVec2(card_w, card_h)))
                select_project(pr.dir);
            bool hovered = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 mn = pos;
            ImVec2 mx(pos.x + card_w, pos.y + card_h);
            dl->AddRectFilled(mn, mx,
                ImGui::GetColorU32(hovered ? ImVec4(1,1,1,0.05f) : ImVec4(1,1,1,0.02f)), 6.0f);
            dl->AddRect(mn, mx, ImGui::GetColorU32(ImVec4(1,1,1,0.06f)), 6.0f);
            ImU32 dot = pr.exists ? ImGui::GetColorU32(kGood) : ImGui::GetColorU32(kBad);
            dl->AddCircleFilled(ImVec2(mn.x + 16.0f, mn.y + card_h * 0.5f), 4.0f, dot);
            dl->AddText(ImVec2(mn.x + 32.0f, mn.y + 8.0f),
                        ImGui::GetColorU32(ImGuiCol_Text), pr.manifest.name.c_str());
            std::string sub = pr.exists ? pr.dir : "(missing) " + pr.dir;
            dl->AddText(ImVec2(mn.x + 32.0f, mn.y + 8.0f + ImGui::GetTextLineHeight()),
                        pr.exists ? ImGui::GetColorU32(kMuted) : ImGui::GetColorU32(kBad), sub.c_str());
            ImGui::PopID();
            ImGui::Spacing();
        }
    }
}

void EditorApp::render_toolbar() {
    auto* p = current();
    bool running = m_runner.is_running();
    float avail = ImGui::GetContentRegionAvail().x;

    ImGui::SetWindowFontScale(1.30f);
    ImGui::TextUnformatted(p->manifest.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::Text("v%s", p->manifest.version.c_str());
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 12.0f);
    if (running)               status_pill("running", kGood);
    else if (!p->exists)       status_pill("missing", kBad);
    else if (m_manifest_dirty) status_pill("unsaved", kWarn);
    else                       status_pill("ready", kAccent);

    bool compact = avail < 480.0f;
    float bw = compact ? 62.0f : 84.0f;
    float total = bw * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
    if (avail > total + 240.0f) ImGui::SameLine(avail - total + ImGui::GetStyle().WindowPadding.x);
    else ImGui::NewLine();

    ImGui::BeginDisabled(running || !p->exists);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.18f, 0.55f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.14f, 0.45f, 0.26f, 1.0f));
    if (ImGui::Button(compact ? "Run" : "Run (F5)", ImVec2(bw, 0))) run_project();
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.27f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.52f, 0.18f, 0.18f, 1.0f));
    if (ImGui::Button(compact ? "Stop" : "Stop (Shift+F5)", ImVec2(bw, 0))) stop_project();
    ImGui::PopStyleColor(3);
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(compact ? "Folder" : "Open Folder", ImVec2(bw, 0))) open_folder(p->dir);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", p->dir.c_str());

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    ImGui::TextUnformatted(p->dir.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Right-click to copy path");
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) SDL_SetClipboardText(p->dir.c_str());
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void EditorApp::render_project_view() {
    render_toolbar();
    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        auto tab = [&](const char* label, Tab t, void (EditorApp::*fn)()) {
            ImGuiTabItemFlags f = (m_force_tab && m_request_tab == t) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label, nullptr, f)) {
                m_tab = t;
                if (m_force_tab && m_request_tab == t) m_force_tab = false;
                ImGui::Spacing();
                (this->*fn)();
                ImGui::EndTabItem();
            }
        };
        tab("Overview", Tab::Overview, &EditorApp::render_overview);
        tab("Files",    Tab::Files,    &EditorApp::render_files);
        tab("Console",  Tab::Console,  &EditorApp::render_console);
        ImGui::EndTabBar();
    }
}

void EditorApp::render_overview() {
    auto* p = current();
    ImGui::BeginChild("##ov", ImVec2(0, 0), ImGuiChildFlags_None);
    bool narrow = ImGui::GetContentRegionAvail().x < 420.0f;

    auto field = [&](const char* label, std::string& v, const char* hint = "") {
        if (narrow) {
            section_label(label);
            ImGui::SetNextItemWidth(-FLT_MIN);
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine(130.0f);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }
        std::string id = std::string("##") + label;
        if (ImGui::InputTextWithHint(id.c_str(), hint, &v)) m_manifest_dirty = true;
    };

    section_label("IDENTITY");
    ImGui::Spacing();
    field("Name",    m_edit.name);
    field("Version", m_edit.version, "1.0.0");
    field("Author",  m_edit.author,  "Your name or studio");
    field("License", m_edit.license, "MIT");

    if (narrow) { section_label("Description"); ImGui::SetNextItemWidth(-FLT_MIN); }
    else {
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextUnformatted("Description");
        ImGui::PopStyleColor();
        ImGui::SameLine(130.0f);
    }
    if (ImGui::InputTextMultiline("##Description", &m_edit.description,
                                  ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4.0f)))
        m_manifest_dirty = true;

    ImGui::Spacing(); ImGui::Spacing();
    section_label("ENTRY POINT");
    ImGui::Spacing();
    field("Entry script",    m_edit.main,      "src/main.lua");
    field("Target override", m_edit.targetSrc, "optional");

    std::string entry = !m_edit.targetSrc.empty() ? m_edit.targetSrc : m_edit.main;
    std::error_code ec;
    bool entry_ok = fs::exists(fs::path(p->dir) / entry, ec);
    ImGui::PushStyleColor(ImGuiCol_Text, entry_ok ? kGood : kBad);
    ImGui::TextWrapped(entry_ok ? "  Entry script found."
                                : "  Entry script not found: %s", entry.c_str());
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();
    section_label("RUNTIME");
    ImGui::Spacing();
    bool exe_ok = fs::exists(m_store.crayon_path(), ec) ||
                  m_store.crayon_path().find_first_of("/\\") == std::string::npos;
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted("Player:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(m_store.crayon_path().c_str());
    ImGui::PushStyleColor(ImGuiCol_Text, exe_ok ? kGood : kWarn);
    ImGui::TextWrapped(exe_ok ? "  Player found."
                              : "  Player not found. Set it in File > Settings.");
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Spacing();
    section_label("ACTIONS");
    ImGui::Spacing();
    ImGui::BeginDisabled(!m_manifest_dirty);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("Save Changes", ImVec2(140, 0))) save_manifest();
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_manifest_dirty);
    if (ImGui::Button("Revert", ImVec2(110, 0))) { m_edit = p->manifest; m_manifest_dirty = false; }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Open Folder", ImVec2(140, 0))) open_folder(p->dir);
    ImGui::SameLine();
    ImGui::BeginDisabled(!p->exists || m_runner.is_running());
    if (ImGui::Button("Run Project", ImVec2(140, 0))) run_project();
    ImGui::EndDisabled();
    if (m_manifest_dirty) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
        ImGui::TextUnformatted("unsaved changes");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}

void EditorApp::render_files() {
    auto* p = current();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float list_w = std::clamp(avail_w * 0.32f, 150.0f, 300.0f);

    if (ImGui::Button("New File")) { m_open_new_file = true; m_modal_error.clear(); }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_open_rel.empty());
    if (ImGui::Button("Delete")) { m_delete_rel = m_open_rel; m_open_delete = true; }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refresh_files();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_file_dirty);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("Save")) save_file();
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();

    if (m_file_dirty) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
        ImGui::TextUnformatted("unsaved");
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    // ----- File list -----
    ImGui::BeginChild("##filelist", ImVec2(list_w, 0), CRAYON_CHILD_BORDER);
    for (auto& f : m_files) {
        ImGui::PushID(f.rel.c_str());
        ImGui::Indent(f.depth * 12.0f);

        if (f.is_dir) {
            ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
            ImGui::Selectable((f.name + "/").c_str(), false, ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, cat_color(f.cat));
            ImGui::TextUnformatted(f.tag);
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 8.0f);
            if (ImGui::Selectable(f.name.c_str(), f.rel == m_open_rel)) open_file(f.rel);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.rel.c_str());
        }
        ImGui::Unindent(f.depth * 12.0f);
        ImGui::PopID();
    }
    if (m_files.empty()) ImGui::TextDisabled("Empty project");
    ImGui::EndChild();
    ImGui::SameLine();

    // ----- Editor -----
    ImGui::BeginChild("##editor", ImVec2(0, 0), ImGuiChildFlags_None);
    if (m_open_rel.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("Select a file from the list to view or edit it.");
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::TextUnformatted(m_open_rel.c_str());
        ImGui::PopStyleColor();

        float footer_h = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        float body_h   = ImGui::GetContentRegionAvail().y - footer_h;
        if (body_h < 40.0f) body_h = 40.0f;

        if (m_file_editable) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.09f, 0.09f, 0.11f, 1.0f));
            if (ImGui::InputTextMultiline("##code", &m_buffer, ImVec2(-FLT_MIN, body_h),
                                          ImGuiInputTextFlags_AllowTabInput)) {
                m_file_dirty = true;
                m_file_lines_valid = false;   // invalidate cached line count
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
            ImGui::TextWrapped("%s", m_file_note.c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, body_h - 30.0f));
        }

        ImGui::Separator();

        // Footer: line count is cached; recompute only when buffer was edited.
        if (!m_file_lines_valid && m_file_editable) {
            m_file_line_count = count_lines(m_buffer);
            m_file_lines_valid = true;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::Text("%d lines  |  %s  |  %s",
                    m_file_line_count,
                    human_size(m_buffer.size()).c_str(),
                    m_file_dirty ? "modified" : "saved");
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    (void)p;
}

void EditorApp::rebuild_console_visible() {
    m_console_visible.clear();
    m_console_visible.reserve(m_console.size());
    const bool have_filter = (m_console_filter[0] != 0);
    const std::string_view filter_v(m_console_filter);
    for (size_t i = 0; i < m_console.size(); ++i) {
        const std::string& l = m_console[i];
        unsigned char lv = classify_level(l);
        if (!level_passes(lv, m_console_level)) continue;
        if (have_filter && !icontains(l, filter_v)) continue;
        m_console_visible.push_back({ (unsigned)i, lv });
    }
}

void EditorApp::render_console() {
    // --- Toolbar first, so the Clear button can bump the version before we refetch.
    if (ImGui::Button("Clear")) m_runner.clear();
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        std::string all;
        for (auto& l : m_console) { all += l; all += '\n'; }
        SDL_SetClipboardText(all.c_str());
        toast("Copied console to clipboard");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoscroll);
    ImGui::SameLine();
    ImGui::Checkbox("Wrap", &m_console_wrap);
    ImGui::SameLine();
    ImGui::Checkbox("Clear on run", &m_clear_console_on_run);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    const char* levels[] = { "All", "Warnings", "Errors" };
    if (ImGui::Combo("##lvl", &m_console_level, levels, 3))
        m_console_visible_dirty = true;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputTextWithHint("##cfilter", "Filter console...",
                                 m_console_filter, sizeof(m_console_filter)))
        m_console_visible_dirty = true;

    ImGui::Spacing();

    // --- Refetch content only if the runner's version changed.
    if (m_console_version != m_runner.version()) {
        m_runner.snapshot(m_console, m_console_version);
        m_console_visible_dirty = true;
    }

    // --- Rebuild the filtered index list only when something actually changed.
    if (m_console_visible_dirty) {
        rebuild_console_visible();
        m_console_visible_dirty = false;
    }

    // --- Render body ---
    ImGuiWindowFlags cflags = ImGuiWindowFlags_HorizontalScrollbar;
    if (m_console_wrap) cflags &= ~ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("##console", ImVec2(0, 0), CRAYON_CHILD_BORDER, cflags);

    ImGuiListClipper clip;
    clip.Begin((int)m_console_visible.size());
    while (clip.Step()) {
        for (int vi = clip.DisplayStart; vi < clip.DisplayEnd; ++vi) {
            const ConsoleEntry& e = m_console_visible[vi];
            ImVec4 c;
            switch (e.level) {
                case 3:  c = kBad;   break;
                case 2:  c = kWarn;  break;
                case 1:  c = kMuted; break;
                default: c = ImGui::GetStyle().Colors[ImGuiCol_Text]; break;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, c);
            ImGui::TextUnformatted(m_console[e.idx].c_str());
            ImGui::PopStyleColor();
        }
    }

    if (m_console.empty())
        ImGui::TextDisabled("Press Run (F5) to launch the project. Output appears here.");
    else if (m_console_visible.empty())
        ImGui::TextDisabled("No lines match the current filter.");

    if (m_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

void EditorApp::render_status_bar() {
    float h = ImGui::GetFrameHeight();
    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = h;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(20, 21, 25, 255));
    dl->AddLine(ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y), IM_COL32(50, 52, 60, 200));

    ImGui::SetCursorScreenPos(ImVec2(pos.x + 12.0f, pos.y + 3.0f));

    double now = SDL_GetTicks() / 1000.0;
    if (now < m_status_until && !m_status_msg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, m_status_error ? kBad : kGood);
        ImGui::TextUnformatted(m_status_msg.c_str());
        ImGui::PopStyleColor();
    } else if (m_runner.is_running()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kGood);
        ImGui::TextUnformatted("● Game running");
        ImGui::PopStyleColor();
    } else {
        auto n = m_store.projects().size();
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::Text("%zu project%s", n, n == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }

    if (auto* p = current()) {
        std::string center = p->manifest.name + "  ·  " + p->dir;
        float cw = ImGui::CalcTextSize(center.c_str()).x;
        float cx = pos.x + (size.x - cw) * 0.5f;
        if (cx < pos.x + 200.0f) cx = pos.x + 200.0f;
        dl->PushClipRect(ImVec2(pos.x + 180.0f, pos.y),
                         ImVec2(pos.x + size.x - 180.0f, pos.y + size.y), true);
        dl->AddText(ImVec2(cx, pos.y + 3.0f), ImGui::GetColorU32(kDim), center.c_str());
        dl->PopClipRect();
    }

    char fps[32];
    snprintf(fps, sizeof(fps), "%.0f FPS", ImGui::GetIO().Framerate);
    float fw = ImGui::CalcTextSize(fps).x;
    dl->AddText(ImVec2(pos.x + size.x - fw - 12.0f, pos.y + 3.0f),
                ImGui::GetColorU32(kMuted), fps);

    ImGui::Dummy(size);
}

// -------------------------------------------------------------- modals ------

void EditorApp::render_modals() {
    if (m_open_new_project) {
        copy_to(m_np_location, sizeof(m_np_location), m_store.default_location());
        m_np_name[0] = 0; m_np_author[0] = 0; m_np_desc[0] = 0;
        m_modal_error.clear();
        ImGui::OpenPopup("New Project");
        m_open_new_project = false;
    }
    if (m_open_settings)  { ImGui::OpenPopup("Settings");   m_open_settings  = false; }
    if (m_open_new_file)  { ImGui::OpenPopup("New File");   m_open_new_file  = false; }
    if (m_open_delete)    { ImGui::OpenPopup("Delete File"); m_open_delete   = false; }
    if (m_open_about)     { ImGui::OpenPopup("About");      m_open_about     = false; }
    if (m_open_shortcuts) { ImGui::OpenPopup("Shortcuts");  m_open_shortcuts = false; }

    render_new_project_modal();
    render_settings_modal();
    render_new_file_modal();
    render_delete_modal();
    render_about_modal();
    render_shortcuts_modal();
}

static void center_modal(float w) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float width = std::min(w, vp->Size.x - 40.0f);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0));
}

void EditorApp::render_new_project_modal() {
    center_modal(540);
    if (!ImGui::BeginPopupModal("New Project", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    section_label("NAME"); ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputText("##np_name", m_np_name, sizeof(m_np_name));

    section_label("AUTHOR"); ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##np_author", m_np_author, sizeof(m_np_author));

    section_label("DESCRIPTION"); ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##np_desc", m_np_desc, sizeof(m_np_desc));

    section_label("LOCATION");
    float bw = 84.0f;
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##np_loc", m_np_location, sizeof(m_np_location));
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(bw, 0))) request_dialog(DialogKind::NewLocation);

    section_label("TEMPLATE"); ImGui::SetNextItemWidth(-FLT_MIN);
    const char* templates[] = { "Empty", "2D Starter", "3D Starter" };
    ImGui::Combo("##np_tpl", &m_np_template, templates, 3);

    if (m_np_name[0]) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("Will be created in: %s",
                           (fs::path(m_np_location) / m_np_name).string().c_str());
        ImGui::PopStyleColor();
    }
    if (!m_modal_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kBad);
        ImGui::TextWrapped("%s", m_modal_error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    bool create = false;
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("Create", ImVec2(110, 0))) create = true;
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();

    if (create) {
        Manifest m;
        m.name = m_np_name; m.author = m_np_author; m.description = m_np_desc;
        std::string dir, err;
        Template t = m_np_template == 1 ? Template::Starter2D
                   : m_np_template == 2 ? Template::Starter3D
                                        : Template::Empty;
        if (ProjectStore::create_project(m_np_location, m, t, dir, err)) {
            m_store.default_location() = m_np_location;
            std::string e2;
            m_store.add_project(dir, e2);
            m_selected.clear();
            select_project(dir);
            m_tab = Tab::Overview; m_request_tab = Tab::Overview; m_force_tab = true;
            toast("Created project: " + m.name);
            ImGui::CloseCurrentPopup();
        } else m_modal_error = err;
    }
    ImGui::EndPopup();
}

void EditorApp::render_settings_modal() {
    center_modal(540);
    if (!ImGui::BeginPopupModal("Settings", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    float bw = 84.0f;

    section_label("CRAYON PLAYER (crayon.exe)");
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##crayon", &m_store.crayon_path());
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(bw, 0))) request_dialog(DialogKind::CrayonExe);
    if (ImGui::Button("Auto-detect")) m_store.crayon_path() = ProjectStore::find_crayon();

    ImGui::Spacing();
    section_label("DEFAULT PROJECT LOCATION");
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##loc", &m_store.default_location());
    ImGui::SameLine();
    if (ImGui::Button("Browse##l", ImVec2(bw, 0))) request_dialog(DialogKind::DefaultLocation);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("Settings are saved to a per-user config file and reloaded on startup.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("Save & Close", ImVec2(140, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_store.save();
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::EndPopup();
}

void EditorApp::render_new_file_modal() {
    center_modal(440);
    if (!ImGui::BeginPopupModal("New File", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    section_label("PATH (RELATIVE TO PROJECT)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool enter = ImGui::InputText("##nf", m_nf_name, sizeof(m_nf_name),
                                  ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("Folders are created as needed. Example: src/enemies/boss.lua");
    ImGui::PopStyleColor();

    if (!m_modal_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kBad);
        ImGui::TextWrapped("%s", m_modal_error.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.54f, 0.95f, 1.0f));
    if (ImGui::Button("Create", ImVec2(110, 0)) || enter) {
        auto* p = current();
        std::string rel = m_nf_name;
        std::replace(rel.begin(), rel.end(), '\\', '/');
        if (!p || rel.empty() || rel.find("..") != std::string::npos || rel[0] == '/') {
            m_modal_error = "Enter a valid relative path.";
        } else {
            fs::path full = fs::path(p->dir) / rel;
            std::error_code ec;
            if (fs::exists(full, ec)) m_modal_error = "File already exists.";
            else {
                fs::create_directories(full.parent_path(), ec);
                std::ofstream(full).put('\n');
                refresh_files();
                open_file(rel);
                m_modal_error.clear();
                ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorApp::render_delete_modal() {
    center_modal(400);
    if (!ImGui::BeginPopupModal("Delete File", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    ImGui::TextWrapped("Permanently delete this file?");
    ImGui::PushStyleColor(ImGuiCol_Text, kBad);
    ImGui::TextWrapped("  %s", m_delete_rel.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.27f, 0.27f, 1.0f));
    if (ImGui::Button("Delete", ImVec2(110, 0))) {
        if (auto* p = current()) {
            std::error_code ec;
            fs::remove(fs::path(p->dir) / m_delete_rel, ec);
            if (m_delete_rel == m_open_rel) {
                m_open_rel.clear(); m_buffer.clear(); m_file_dirty = false;
                m_file_line_count = 0; m_file_lines_valid = true;
            }
            refresh_files();
            toast("Deleted " + m_delete_rel);
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(110, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorApp::render_about_modal() {
    center_modal(460);
    if (!ImGui::BeginPopupModal("About", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    ImGui::SetWindowFontScale(1.4f);
    ImGui::TextUnformatted("Crayon Project Manager");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("A lightweight, standalone project manager for the Crayon engine.");
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

    ImGui::Text("Dear ImGui version: %s", IMGUI_VERSION);
    ImGui::Text("SDL version:       %d.%d.%d",
                SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    ImGui::Text("OpenGL:            3.3 Core");

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorApp::render_shortcuts_modal() {
    center_modal(460);
    if (!ImGui::BeginPopupModal("Shortcuts", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    struct Row { const char* keys; const char* what; };
    Row rows[] = {
        { "F5",       "Run project" },
        { "Shift+F5", "Stop project" },
        { "Ctrl+N",   "New project" },
        { "Ctrl+O",   "Open project" },
        { "Ctrl+S",   "Save (manifest / file)" },
        { "Ctrl+B",   "Toggle sidebar" },
        { "F1",       "Show this dialog" },
        { "F2",       "About" },
    };
    if (ImGui::BeginTable("##sc", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Action");
        for (auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
            ImGui::TextUnformatted(r.keys);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.what);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::Button("Close", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- loop ------

void EditorApp::run() {
    // Safety cap only. With vsync enabled (SDL_GL_SetSwapInterval(1)),
    // SDL_GL_SwapWindow already paces us to the monitor refresh rate.
    constexpr double kMinFrameTime = 1.0 / 250.0;

    while (m_running && !m_should_close) {
        // Skip the entire frame while minimized — saves CPU and lets the OS
        // give the editor's thread back to the user.
        if (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(100);
            continue;
        }

        using clock = std::chrono::high_resolution_clock;
        auto frame_start = clock::now();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) m_should_close = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(m_window)) m_should_close = true;
        }
        poll_dialog();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        handle_shortcuts();
        render_menu_bar();
        render_root();
        render_modals();

        ImGui::Render();
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(m_window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.09f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(m_window);   // blocks until vblank

        auto elapsed = std::chrono::duration<double>(clock::now() - frame_start).count();
        if (elapsed < kMinFrameTime)
            SDL_Delay(static_cast<Uint32>((kMinFrameTime - elapsed) * 1000.0));
    }
    m_runner.stop();
    m_store.save();
}

void EditorApp::shutdown() {
    if (m_shut) return;
    m_shut = true;
    m_runner.stop();
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (m_gl)     { SDL_GL_DestroyContext(m_gl); m_gl = nullptr; }
    if (m_window) { SDL_DestroyWindow(m_window); m_window = nullptr; }
    SDL_Quit();
}

} // namespace crayon::editor