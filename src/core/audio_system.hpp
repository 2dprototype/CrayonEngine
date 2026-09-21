#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

namespace crayon {

struct SoundData {
    uint32_t id = 0;
    std::string path;
    SDL_AudioSpec spec{};
    Uint8* buffer = nullptr;
    Uint32 length = 0;
    ~SoundData() {
        if (buffer) {
            SDL_free(buffer);
            buffer = nullptr;
        }
    }
};

struct AudioVoice {
    uint32_t voice_id = 0;
    uint32_t sound_id = 0;
    SDL_AudioStream* stream = nullptr;
    float base_volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool loop = false;
    bool is_3d = false;
    glm::vec3 pos{0.0f};
    float min_dist = 1.0f;
    float max_dist = 25.0f;
};

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init();
    void shutdown();
    void update(float dt);

    // Asset Management
    uint32_t load_sound(const std::string& filepath);
    bool unload_sound(uint32_t sound_id);

    // Playback
    uint32_t play_sound(uint32_t sound_id, float volume = 1.0f, float pitch = 1.0f, float pan = 0.0f, bool loop = false);
    uint32_t play_sound_3d(uint32_t sound_id, const glm::vec3& pos, float volume = 1.0f, float pitch = 1.0f, float min_dist = 1.0f, float max_dist = 25.0f);
    void stop_sound(uint32_t voice_id);
    void stop_all_sounds();
    bool is_sound_playing(uint32_t voice_id) const;

    // Music
    bool play_music(const std::string& filepath, bool loop = true, float fade_in = 0.0f);
    void stop_music(float fade_out = 0.0f);
    void pause_music();
    void resume_music();
    bool is_music_playing() const;

    // Volumes
    void set_master_volume(float vol);
    float get_master_volume() const { return m_master_volume; }
    void set_sfx_volume(float vol);
    float get_sfx_volume() const { return m_sfx_volume; }
    void set_music_volume(float vol);
    float get_music_volume() const { return m_music_volume; }

    // Listener for 3D Audio
    void set_listener_position(const glm::vec3& pos) { m_listener_pos = pos; }
    const glm::vec3& get_listener_position() const { return m_listener_pos; }
    void set_listener_orientation(const glm::vec3& forward, const glm::vec3& up);

private:
    void update_3d_voice(AudioVoice& voice);

    SDL_AudioDeviceID m_device_id = 0;
    SDL_AudioSpec m_device_spec{};
    bool m_initialized = false;

    float m_master_volume = 1.0f;
    float m_sfx_volume = 1.0f;
    float m_music_volume = 1.0f;

    glm::vec3 m_listener_pos{0.0f};
    glm::vec3 m_listener_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_listener_up{0.0f, 1.0f, 0.0f};

    uint32_t m_next_sound_id = 1;
    uint32_t m_next_voice_id = 1;

    std::unordered_map<std::string, uint32_t> m_path_to_sound_id;
    std::unordered_map<uint32_t, std::shared_ptr<SoundData>> m_sounds;
    std::vector<AudioVoice> m_active_voices;

    // Music State
    std::shared_ptr<SoundData> m_music_sound;
    SDL_AudioStream* m_music_stream = nullptr;
    bool m_music_loop = true;
    float m_music_fade_timer = 0.0f;
    float m_music_fade_duration = 0.0f;
    bool m_music_fading_out = false;
};

} // namespace crayon
