#pragma once

#include <string>

namespace crayon::editor {

class EditorPanel {
public:
    EditorPanel(std::string title, bool open = true)
        : m_title(std::move(title)), m_is_open(open) {}

    virtual ~EditorPanel() = default;

    virtual void on_render() = 0;
    virtual void render_content() {}

    const std::string& get_title() const { return m_title; }
    bool& is_open() { return m_is_open; }
    bool is_open() const { return m_is_open; }
    void set_open(bool open) { m_is_open = open; }

protected:
    std::string m_title;
    bool m_is_open = true;
};

} // namespace crayon::editor
