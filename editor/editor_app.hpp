#pragma once

#include <string>
#include <vector>
#include <SDL3/SDL.h>
#include "project_store.hpp"
#include "process_runner.hpp"

namespace crayon::editor {

enum class FileCat : unsigned char {
    Dir, Lua, Markdown, Text, Json, Image, Model3D, Code, Other
};

class EditorApp {
public:
    EditorApp();
    ~EditorApp();

    bool init(int width = 630, int height = 450, const std::string& title = "Crayon Project Manager");
    void run();
    void shutdown();

private:
    enum class Tab { Overview, Files, Console };
    enum class DialogKind { None, OpenProject, NewLocation, CrayonExe, DefaultLocation };

    struct FileItem {
        std::string rel;
        std::string name;
        int  depth  = 0;
        bool is_dir = false;
        const char* tag = "···";
        FileCat     cat = FileCat::Other;
    };

    struct ConsoleEntry {
        unsigned       idx;   // index into m_console
        unsigned char  level; // 0=info 1=editor 2=warn 3=error
    };

    // UI
    void render_menu_bar();
    void render_root();
    void render_sidebar(float width);
    void render_project_view();
    void render_welcome();
    void render_toolbar();
    void render_overview();
    void render_files();
    void render_console();
    void render_status_bar();
    void render_modals();

    void render_new_project_modal();
    void render_settings_modal();
    void render_new_file_modal();
    void render_delete_modal();
    void render_about_modal();
    void render_shortcuts_modal();

    // Helpers
    void rebuild_console_visible();

    // Actions
    ProjectEntry* current();
    void select_project(const std::string& dir);
    void open_project_dialog();
    void request_dialog(DialogKind kind);
    void poll_dialog();
    void run_project();
    void stop_project();
    void save_manifest();
    void refresh_files();
    void open_file(const std::string& rel);
    void save_file();
    void flush_dirty_file();
    void toast(const std::string& msg, bool error = false);
    void handle_shortcuts();

    SDL_Window*   m_window       = nullptr;
    SDL_GLContext m_gl           = nullptr;
    bool          m_should_close = false;

    ProjectStore  m_store;
    ProcessRunner m_runner;

    // layout
    float m_sidebar_width   = 268.0f;
    bool  m_show_sidebar    = true;
    bool  m_show_status_bar = true;

    // selection / manifest editing
    std::string m_selected;
    Manifest    m_edit;
    bool        m_manifest_dirty = false;

    // files tab
    std::vector<FileItem> m_files;
    std::string m_open_rel;
    std::string m_buffer;
    bool        m_file_dirty       = false;
    bool        m_file_editable    = false;
    std::string m_file_note;
    std::string m_delete_rel;

    // file footer cache (avoid O(n) every frame)
    int  m_file_line_count  = 0;
    bool m_file_lines_valid = true;

    // console + cache
    std::vector<std::string>  m_console;
    unsigned                  m_console_version = ~0u;
    std::vector<ConsoleEntry> m_console_visible;
    bool                      m_console_visible_dirty = true;
    bool m_autoscroll           = true;
    bool m_console_wrap         = false;
    bool m_clear_console_on_run = true;
    char m_console_filter[128]  = "";
    int  m_console_level        = 0; // 0=All, 1=Warn+, 2=Errors

    // transient status message (replaces toast UI)
    std::string m_status_msg;
    bool        m_status_error = false;
    double      m_status_until = 0.0;

    // modals
    bool m_open_new_project = false;
    bool m_open_settings    = false;
    bool m_open_new_file    = false;
    bool m_open_delete      = false;
    bool m_open_about       = false;
    bool m_open_shortcuts   = false;
    char m_np_name[96]      = "";
    char m_np_author[96]    = "";
    char m_np_desc[256]     = "";
    char m_np_location[512] = "";
    int  m_np_template      = 1;
    char m_nf_name[128]     = "src/new_script.lua";
    char m_filter[64]       = "";
    std::string m_modal_error;

    // misc
    Tab  m_tab         = Tab::Overview;
    Tab  m_request_tab = Tab::Overview;
    bool m_force_tab   = false;
    bool m_running     = false;
    bool m_shut        = false;
};

} // namespace crayon::editor