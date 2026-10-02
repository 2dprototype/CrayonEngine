#include "editor_app.hpp"
#include "editor_theme.hpp"
#include "../src/core/log.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <sstream>

namespace crayon::editor {

namespace {

// --- async native dialog bridge (SDL callbacks may fire on another thread) ---
struct DialogResult {
    std::mutex mtx;
    bool ready = false;
    int kind = 0;
    std::string path;
} g_dialog;

void SDLCALL dialog_cb(void* userdata, const char* const* list, int /*filter*/) {
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

bool read_text(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

bool icontains(const std::string& hay, const char* needle) {
    if (!*needle) return true;
    std::string h = hay, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

void copy_to(char* dst, size_t cap, const std::string& s) { snprintf(dst, cap, "%s", s.c_str()); }

void section_label(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

} // namespace

EditorApp::EditorApp() = default;
EditorApp::~EditorApp() { shutdown(); }

bool EditorApp::init(int width, int height, const std::string& title) {
    if (!m_engine.init(width, height, 480, 360, title)) {
        CRAYON_LOG_ERROR("EditorApp failed to initialize Engine");
        return false;
    }
    auto& win = m_engine.get_window();
    win.set_vsync(true);
    win.set_window_min_size(420, 320);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    apply_modern_dark_theme();

    SDL_Window* sdl_win = win.get_sdl_window();
    float scale = SDL_GetWindowDisplayScale(sdl_win);
    if (scale > 1.05f) { io.FontGlobalScale = scale; ImGui::GetStyle().ScaleAllSizes(scale); }

    if (!ImGui_ImplSDL3_InitForOpenGL(sdl_win, win.get_gl_context())) return false;
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
    m_toast = msg;
    m_toast_error = error;
    m_toast_until = SDL_GetTicks() / 1000.0 + 4.0;
}

void EditorApp::select_project(const std::string& dir) {
    if (dir == m_selected) return;
    flush_dirty_file();
    m_selected = dir;
    m_open_rel.clear(); m_buffer.clear(); m_file_dirty = false; m_file_editable = false; m_file_note.clear();
    if (auto* p = current()) {
        m_store.refresh(*p);
        m_edit = p->manifest;
        m_manifest_dirty = false;
        refresh_files();
    }
}

void EditorApp::request_dialog(DialogKind kind) {
    SDL_Window* w = m_engine.get_window().get_sdl_window();
    void* ud = reinterpret_cast<void*>(static_cast<intptr_t>(kind));
    if (kind == DialogKind::CrayonExe) SDL_ShowOpenFileDialog(dialog_cb, ud, w, nullptr, 0, nullptr, false);
    else SDL_ShowOpenFolderDialog(dialog_cb, ud, w, nullptr, false);
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
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
        std::string name = it->path().filename().string();
        if (!name.empty() && name[0] == '.' ) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        all.emplace_back(fs::relative(it->path(), root, ec).generic_string(), it->is_directory(ec));
        if (all.size() > 4000) break;
    }
    // Directories-first stable ordering by path
    std::sort(all.begin(), all.end(), [](auto& a, auto& b) {
        return std::lexicographical_compare(a.first.begin(), a.first.end(), b.first.begin(), b.first.end(),
            [](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
    });
    for (auto& [rel, dir] : all) {
        FileItem f;
        f.rel = rel; f.is_dir = dir;
        f.name = fs::path(rel).filename().string();
        f.depth = (int)std::count(rel.begin(), rel.end(), '/');
        m_files.push_back(f);
    }
}

void EditorApp::flush_dirty_file() {
    if (m_file_dirty) save_file();
}

void EditorApp::open_file(const std::string& rel) {
    auto* p = current();
    if (!p) return;
    flush_dirty_file();
    m_open_rel = rel; m_buffer.clear(); m_file_dirty = false; m_file_editable = false; m_file_note.clear();
    fs::path full = fs::path(p->dir) / rel;
    std::error_code ec;
    auto size = fs::file_size(full, ec);
    if (ec) { m_file_note = "Cannot read file."; return; }
    if (size > 2 * 1024 * 1024) { m_file_note = "File is larger than 2 MB and cannot be edited here."; return; }
    if (!read_text(full, m_buffer)) { m_file_note = "Cannot read file."; return; }
    if (m_buffer.find('\0') != std::string::npos) { m_buffer.clear(); m_file_note = "Binary file - preview not available."; return; }
    m_file_editable = true;
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
    m_runner.clear();
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
}

// ------------------------------------------------------------------- UI -----

void EditorApp::render_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;
    auto* p = current();
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project...", "Ctrl+N")) m_open_new_project = true;
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) open_project_dialog();
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
        if (ImGui::MenuItem("Remove From List", nullptr, false, p != nullptr)) {
            std::string d = m_selected;
            m_selected.clear();
            m_store.remove_project(d);
            if (!m_store.projects().empty()) select_project(m_store.projects().front().dir);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("F5 - Run    Shift+F5 - Stop", nullptr, false, false);
        ImGui::MenuItem("Ctrl+S - Save    Ctrl+N - New", nullptr, false, false);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void EditorApp::render_root() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##root", nullptr, flags);
    ImGui::PopStyleVar(2);

    float status_h = ImGui::GetFrameHeight() + 4.0f;
    float avail_w = ImGui::GetContentRegionAvail().x;
    float body_h = ImGui::GetContentRegionAvail().y - status_h;
    bool narrow = avail_w < 520.0f;
    float side_w = narrow ? 0.0f : std::clamp(avail_w * 0.27f, 160.0f, 300.0f);

    if (!narrow) {
        ImGui::BeginChild("##sidebar", ImVec2(side_w, body_h), ImGuiChildFlags_None);
        render_sidebar(side_w);
        ImGui::EndChild();
        ImGui::SameLine(0, 0);
    }
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 10));
    ImGui::BeginChild("##main", ImVec2(0, body_h), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (narrow) {
        // Compact project switcher replaces the sidebar
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

    render_status_bar();
    ImGui::End();
}

void EditorApp::render_sidebar(float width) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Indent(10);
    section_label("PROJECTS");
    float bw = (width - 20 - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("+ New", ImVec2(bw, 0))) m_open_new_project = true;
    ImGui::SameLine();
    if (ImGui::Button("Open...", ImVec2(bw, 0))) open_project_dialog();
    ImGui::SetNextItemWidth(width - 20);
    ImGui::InputTextWithHint("##filter", "Search projects", m_filter, sizeof(m_filter));
    ImGui::Unindent(10);
    ImGui::Spacing();

    ImGui::BeginChild("##projlist", ImVec2(0, 0), ImGuiChildFlags_None);
    std::string to_remove;
    for (auto& pr : m_store.projects()) {
        if (!icontains(pr.manifest.name, m_filter) && !icontains(pr.dir, m_filter)) continue;
        ImGui::PushID(pr.dir.c_str());
        bool sel = pr.dir == m_selected;
        float h = ImGui::GetTextLineHeight() * 2 + 8;
        if (ImGui::Selectable("##item", sel, ImGuiSelectableFlags_None, ImVec2(0, h))) select_project(pr.dir);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) { select_project(pr.dir); run_project(); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Select")) select_project(pr.dir);
            if (ImGui::MenuItem("Remove from list")) to_remove = pr.dir;
            ImGui::EndPopup();
        }
        ImVec2 mn = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 tc = ImGui::GetColorU32(pr.exists ? ImGuiCol_Text : ImGuiCol_TextDisabled);
        dl->PushClipRect(mn, ImGui::GetItemRectMax(), true);
        dl->AddText(ImVec2(mn.x + 12, mn.y + 4), tc, pr.manifest.name.c_str());
        std::string sub = pr.exists ? pr.dir : "(missing) " + pr.dir;
        dl->AddText(ImVec2(mn.x + 12, mn.y + 4 + ImGui::GetTextLineHeight()),
                    pr.exists ? ImGui::GetColorU32(kMuted) : ImGui::GetColorU32(kBad), sub.c_str());
        if (sel) dl->AddRectFilled(mn, ImVec2(mn.x + 3, ImGui::GetItemRectMax().y), ImGui::GetColorU32(kAccent));
        dl->PopClipRect();
        ImGui::PopID();
    }
    if (!to_remove.empty()) {
        bool was = to_remove == m_selected;
        m_store.remove_project(to_remove);
        if (was) { m_selected.clear(); if (!m_store.projects().empty()) select_project(m_store.projects().front().dir); }
    }
    if (m_store.projects().empty()) {
        ImGui::Indent(10);
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("No projects yet.");
        ImGui::PopStyleColor();
        ImGui::Unindent(10);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

void EditorApp::render_welcome() {
    float w = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0, std::max(8.0f, ImGui::GetContentRegionAvail().y * 0.18f)));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::TextUnformatted("Crayon Project Manager");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextWrapped("Create, organize and run your Crayon games. Projects are described by a .crayonproj manifest and run with the standalone crayon player.");
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();
    float bw = std::min(180.0f, (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    if (ImGui::Button("New Project", ImVec2(bw, 32))) m_open_new_project = true;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Open Project", ImVec2(bw, 32))) open_project_dialog();
}

void EditorApp::render_toolbar() {
    auto* p = current();
    bool running = m_runner.is_running();
    float avail = ImGui::GetContentRegionAvail().x;

    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextUnformatted(p->manifest.name.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, running ? kGood : kMuted);
    ImGui::TextUnformatted(running ? "  * running" : (p->exists ? "  v" : "  (missing)"));
    ImGui::PopStyleColor();
    if (!running && p->exists) { ImGui::SameLine(0, 0); ImGui::TextDisabled("%s", p->manifest.version.c_str()); }

    bool compact = avail < 360.0f;
    float bw = compact ? 56.0f : 74.0f;
    float total = bw * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
    if (avail > total + 200.0f) ImGui::SameLine(avail - total + ImGui::GetStyle().WindowPadding.x * 0.0f);
    else ImGui::NewLine();

    ImGui::BeginDisabled(running || !p->exists);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.32f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.65f, 0.38f, 1.0f));
    if (ImGui::Button(compact ? "Run" : "> Run", ImVec2(bw, 0))) run_project();
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.27f, 0.27f, 1.0f));
    if (ImGui::Button(compact ? "Stop" : "[] Stop", ImVec2(bw, 0))) stop_project();
    ImGui::PopStyleColor(2);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Folder", ImVec2(bw, 0))) {
        open_folder(p->dir);
    }
    ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
    ImGui::TextUnformatted(p->dir.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void EditorApp::render_project_view() {
    render_toolbar();
    if (ImGui::BeginTabBar("##tabs")) {
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
    bool narrow = ImGui::GetContentRegionAvail().x < 380.0f;
    auto field = [&](const char* label, std::string& v, const char* hint = "") {
        if (narrow) { section_label(label); ImGui::SetNextItemWidth(-FLT_MIN); }
        else { ImGui::AlignTextToFramePadding(); section_label(label); ImGui::SameLine(110); ImGui::SetNextItemWidth(-FLT_MIN); }
        std::string id = std::string("##") + label;
        if (ImGui::InputTextWithHint(id.c_str(), hint, &v)) m_manifest_dirty = true;
    };
    field("Name", m_edit.name);
    field("Version", m_edit.version, "1.0.0");
    field("Author", m_edit.author);
    field("License", m_edit.license, "MIT");
    field("Entry script", m_edit.main, "src/main.lua");
    field("Target override", m_edit.targetSrc, "optional");
    if (narrow) { section_label("Description"); ImGui::SetNextItemWidth(-FLT_MIN); }
    else { section_label("Description"); ImGui::SameLine(110); ImGui::SetNextItemWidth(-FLT_MIN); }
    if (ImGui::InputTextMultiline("##Description", &m_edit.description, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3.5f))) m_manifest_dirty = true;

    ImGui::Spacing();
    std::string entry = !m_edit.targetSrc.empty() ? m_edit.targetSrc : m_edit.main;
    std::error_code ec;
    bool entry_ok = fs::exists(fs::path(p->dir) / entry, ec);
    ImGui::PushStyleColor(ImGuiCol_Text, entry_ok ? kGood : kBad);
    ImGui::TextWrapped(entry_ok ? "Entry script found." : "Entry script not found: %s", entry.c_str());
    ImGui::PopStyleColor();
    bool exe_ok = fs::exists(m_store.crayon_path(), ec) || m_store.crayon_path().find_first_of("/\\") == std::string::npos;
    if (!exe_ok) {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
        ImGui::TextWrapped("Player not found at '%s'. Set it in File > Settings.", m_store.crayon_path().c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(!m_manifest_dirty);
    if (ImGui::Button("Save Changes")) save_manifest();
    ImGui::SameLine();
    if (ImGui::Button("Revert")) { m_edit = p->manifest; m_manifest_dirty = false; }
    ImGui::EndDisabled();
    if (m_manifest_dirty) { ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Text, kWarn); ImGui::TextUnformatted("unsaved"); ImGui::PopStyleColor(); }
    ImGui::EndChild();
}

void EditorApp::render_files() {
    auto* p = current();
    float avail_w = ImGui::GetContentRegionAvail().x;
    float list_w = std::clamp(avail_w * 0.34f, 120.0f, 260.0f);

    if (ImGui::Button("New File")) { m_open_new_file = true; m_modal_error.clear(); }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_open_rel.empty());
    if (ImGui::Button("Delete")) { m_delete_rel = m_open_rel; m_open_delete = true; }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) refresh_files();
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_file_dirty);
    if (ImGui::Button("Save")) save_file();
    ImGui::EndDisabled();
    if (m_file_dirty) { ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Text, kWarn); ImGui::TextUnformatted("unsaved"); ImGui::PopStyleColor(); }

    ImGui::BeginChild("##filelist", ImVec2(list_w, 0), CRAYON_CHILD_BORDER);
    for (auto& f : m_files) {
        ImGui::PushID(f.rel.c_str());
        ImGui::Indent(f.depth * 12.0f);
        if (f.is_dir) {
            ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
            ImGui::Selectable((f.name + "/").c_str(), false, ImGuiSelectableFlags_Disabled);
            ImGui::PopStyleColor();
        } else if (ImGui::Selectable(f.name.c_str(), f.rel == m_open_rel)) {
            open_file(f.rel);
        }
        ImGui::Unindent(f.depth * 12.0f);
        ImGui::PopID();
    }
    if (m_files.empty()) ImGui::TextDisabled("Empty project");
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##editor", ImVec2(0, 0), ImGuiChildFlags_None);
    if (m_open_rel.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("Select a file to view or edit it.");
        ImGui::PopStyleColor();
    } else {
        ImGui::TextUnformatted(m_open_rel.c_str());
        if (m_file_editable) {
            if (ImGui::InputTextMultiline("##code", &m_buffer, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_AllowTabInput))
                m_file_dirty = true;
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
            ImGui::TextWrapped("%s", m_file_note.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndChild();
    (void)p;
}

void EditorApp::render_console() {
    if (m_console_version != m_runner.version()) m_runner.snapshot(m_console, m_console_version);

    if (ImGui::Button("Clear")) { m_runner.clear(); }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        std::string all;
        for (auto& l : m_console) all += l + "\n";
        SDL_SetClipboardText(all.c_str());
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoscroll);

    ImGui::BeginChild("##console", ImVec2(0, 0), CRAYON_CHILD_BORDER, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clip;
    clip.Begin((int)m_console.size());
    while (clip.Step()) {
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
            const std::string& l = m_console[i];
            ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_Text];
            if (l.find("ERROR") != std::string::npos || l.find("error") != std::string::npos) c = kBad;
            else if (l.find("WARN") != std::string::npos) c = kWarn;
            else if (l.rfind("[editor]", 0) == 0) c = kMuted;
            ImGui::PushStyleColor(ImGuiCol_Text, c);
            ImGui::TextUnformatted(l.c_str());
            ImGui::PopStyleColor();
        }
    }
    if (m_console.empty()) ImGui::TextDisabled("Press Run (F5) to launch the project. Output appears here.");
    if (m_autoscroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
}

void EditorApp::render_status_bar() {
    ImGui::SetCursorPosX(10);
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeight() - 2.0f);
    double now = SDL_GetTicks() / 1000.0;
    if (now < m_toast_until) {
        ImGui::PushStyleColor(ImGuiCol_Text, m_toast_error ? kBad : kGood);
        ImGui::TextUnformatted(m_toast.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        if (m_runner.is_running()) ImGui::TextUnformatted("Game running...");
        else ImGui::Text("%zu project%s", m_store.projects().size(), m_store.projects().size() == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

// -------------------------------------------------------------- modals ------

void EditorApp::render_modals() {
    if (m_open_new_project) {
        copy_to(m_np_location, sizeof(m_np_location), m_store.default_location());
        m_np_name[0] = 0; m_modal_error.clear();
        ImGui::OpenPopup("New Project");
        m_open_new_project = false;
    }
    if (m_open_settings)  { ImGui::OpenPopup("Settings");   m_open_settings = false; }
    if (m_open_new_file)  { ImGui::OpenPopup("New File");   m_open_new_file = false; }
    if (m_open_delete)    { ImGui::OpenPopup("Delete File"); m_open_delete = false; }
    render_new_project_modal();
    render_settings_modal();
    render_new_file_modal();
    render_delete_modal();
}

static void center_modal(float w) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float width = std::min(w, vp->Size.x - 20.0f);
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0));
}

void EditorApp::render_new_project_modal() {
    center_modal(520);
    if (!ImGui::BeginPopupModal("New Project", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    section_label("Name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::InputText("##np_name", m_np_name, sizeof(m_np_name));
    section_label("Author");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##np_author", m_np_author, sizeof(m_np_author));
    section_label("Description");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##np_desc", m_np_desc, sizeof(m_np_desc));
    section_label("Location");
    float bw = 76.0f;
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##np_loc", m_np_location, sizeof(m_np_location));
    ImGui::SameLine();
    if (ImGui::Button("Browse", ImVec2(bw, 0))) request_dialog(DialogKind::NewLocation);
    section_label("Template");
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* templates[] = { "Empty", "2D Starter", "3D Starter" };
    ImGui::Combo("##np_tpl", &m_np_template, templates, 3);

    if (m_np_name[0]) {
        ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
        ImGui::TextWrapped("Will be created in: %s", (fs::path(m_np_location) / m_np_name).string().c_str());
        ImGui::PopStyleColor();
    }
    if (!m_modal_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kBad);
        ImGui::TextWrapped("%s", m_modal_error.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    bool create = false;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.85f, 1.0f));
    if (ImGui::Button("Create", ImVec2(100, 0))) create = true;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();

    if (create) {
        Manifest m;
        m.name = m_np_name; m.author = m_np_author; m.description = m_np_desc;
        std::string dir, err;
        Template t = m_np_template == 1 ? Template::Starter2D : m_np_template == 2 ? Template::Starter3D : Template::Empty;
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
    center_modal(520);
    if (!ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    float bw = 76.0f;
    section_label("Crayon player (crayon.exe)");
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##crayon", &m_store.crayon_path());
    ImGui::SameLine();
    if (ImGui::Button("Browse##c", ImVec2(bw, 0))) request_dialog(DialogKind::CrayonExe);
    if (ImGui::Button("Auto-detect")) m_store.crayon_path() = ProjectStore::find_crayon();

    ImGui::Spacing();
    section_label("Default project location");
    ImGui::SetNextItemWidth(-bw - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##loc", &m_store.default_location());
    ImGui::SameLine();
    if (ImGui::Button("Browse##l", ImVec2(bw, 0))) request_dialog(DialogKind::DefaultLocation);

    ImGui::Spacing();
    if (ImGui::Button("Done", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_store.save();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void EditorApp::render_new_file_modal() {
    center_modal(420);
    if (!ImGui::BeginPopupModal("New File", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    section_label("Path relative to the project (folders are created as needed)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool enter = ImGui::InputText("##nf", m_nf_name, sizeof(m_nf_name), ImGuiInputTextFlags_EnterReturnsTrue);
    if (!m_modal_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kBad);
        ImGui::TextWrapped("%s", m_modal_error.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    if (ImGui::Button("Create", ImVec2(100, 0)) || enter) {
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
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorApp::render_delete_modal() {
    center_modal(380);
    if (!ImGui::BeginPopupModal("Delete File", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) return;
    ImGui::TextWrapped("Permanently delete '%s'?", m_delete_rel.c_str());
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
    if (ImGui::Button("Delete", ImVec2(100, 0))) {
        if (auto* p = current()) {
            std::error_code ec;
            fs::remove(fs::path(p->dir) / m_delete_rel, ec);
            if (m_delete_rel == m_open_rel) { m_open_rel.clear(); m_buffer.clear(); m_file_dirty = false; }
            refresh_files();
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ---------------------------------------------------------------- loop ------

void EditorApp::run() {
    using clock = std::chrono::high_resolution_clock;
    const double target_frame_time = 1.0 / 60.0;

    while (m_running && !m_engine.get_window().should_close()) {
        auto frame_start = clock::now();

        m_engine.get_input().begin_frame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            m_engine.get_window().handle_event(event);
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
        m_engine.get_window().get_window_size(w, h);
        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        m_engine.get_window().swap_buffers();

        std::chrono::duration<double> work = clock::now() - frame_start;
        if (work.count() < target_frame_time)
            SDL_Delay(static_cast<Uint32>((target_frame_time - work.count()) * 1000.0));
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
    m_engine.shutdown();
}

} // namespace crayon::editor
