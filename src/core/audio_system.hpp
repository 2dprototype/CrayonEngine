#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

namespace crayon {

static constexpr int   CRAYON_MAX_VOICES  = 64;
static constexpr int   CRAYON_MIX_BLOCK   = 512;
static constexpr int   CRAYON_DEVICE_RATE = 44100;
static constexpr int   CRAYON_DEVICE_CH   = 2;

// ---------------- Generator ----------------
enum class WaveType : uint8_t {
    Sample = 0,
    Sine,
    Square,
    Saw,
    Triangle,
    Noise,
};

// ---------------- Envelope ----------------
enum class EnvStage : uint8_t {
    Idle = 0,
    Attack,
    Decay,
    Sustain,
    Release,
};

struct ADSR {
    float attack  = 0.001f;
    float decay   = 0.05f;
    float sustain = 0.7f;
    float release = 0.1f;
};

// ---------------- Filter (one-pole LP, HP via subtraction) ----------------
struct VoiceFilter {
    bool  enabled  = false;
    bool  highpass = false;
    float cutoff   = 8000.0f;
    float lp_state = 0.0f;
    float coeff    = 1.0f;
};

// ---------------- Sound template (procedural sounds) ----------------
struct SoundTemplate {
    WaveType wave = WaveType::Sine;
    float    freq = 440.0f;
    float    duration = 0.0f;  
    ADSR     adsr;
    bool     filter_enabled  = false;
    bool     filter_highpass = false;
    float    filter_cutoff   = 8000.0f;
    bool     pitch_sweep     = false;
    float    sweep_from      = 440.0f;
    float    sweep_to        = 440.0f;
    float    sweep_time      = 1.0f;
};

// ---------------- Sound (either file-backed samples or procedural template) ----
struct SoundData {
    uint32_t id = 0;
    std::string path;              // empty for procedural
    std::vector<float> samples;    // mono @ CRAYON_DEVICE_RATE
    bool is_procedural = false;
    SoundTemplate tmpl;
};

// ---------------- Voice (POD-ish; fixed pool) ----------------
struct Voice {
    bool     active = false;
    uint32_t voice_id = 0;
    uint32_t sound_id = 0;

    // Playback cursor
    double   sample_pos = 0.0;
    bool     loop = false;

    // Oscillator
    WaveType wave = WaveType::Sample;
    double   phase = 0.0;
    float    freq  = 440.0f;
    uint32_t noise_state = 0x12345678u;

    // Pitch sweep (oscillators only)
    bool     pitch_sweep   = false;
    float    sweep_from    = 440.0f;
    float    sweep_to      = 440.0f;
    float    sweep_time    = 1.0f;
    float    sweep_elapsed = 0.0f;

    // Envelope
    ADSR     adsr;
    EnvStage env_stage = EnvStage::Idle;
    float    env_level = 0.0f;
    float    env_attack_inc  = 1.0f;
    float    env_decay_dec   = 1.0f;
    float    env_release_dec = 1.0f;

    // Filter
    VoiceFilter filter;

    // Duration auto-release (0 = none)
    float    duration         = 0.0f;
    float    duration_elapsed = 0.0f;

    // Mix
    float    base_volume = 1.0f;
    float    gain_l = 0.707f;
    float    gain_r = 0.707f;
    float    pitch  = 1.0f;
    float    pan    = 0.0f;
    bool     manual_gain = false;

    // 3D
    bool     is_3d    = false;
    glm::vec3 pos{0.0f};
    float    min_dist = 1.0f;
    float    max_dist = 25.0f;

    int      priority = 0;

    // Custom Lua block generator
    bool     has_custom_gen   = false;
    int      custom_gen_ref   = -1;    // Lua registry reference
    int      custom_gen_block = 512;
    double   custom_gen_time  = 0.0;
    std::vector<float> custom_gen_buffer;
    size_t   custom_gen_pos   = 0;
};

// ---------------- AudioSystem ----------------
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool init();
    void shutdown();
    void update(float dt);

    // -------- Assets --------
    uint32_t load_sound(const std::string& filepath);
    bool     unload_sound(uint32_t sound_id);

    // -------- Procedural --------
    uint32_t create_sound(const SoundTemplate& tmpl);
    uint32_t create_buffer(int frames,
                           const std::function<float(float, int)>& generator);

    // -------- Playback --------
    uint32_t play_sound(uint32_t sound_id, float volume = 1.0f, float pitch = 1.0f,
                        float pan = 0.0f, bool loop = false);
    uint32_t play_sound_3d(uint32_t sound_id, const glm::vec3& pos,
                           float volume = 1.0f, float pitch = 1.0f,
                           float min_dist = 1.0f, float max_dist = 25.0f);
    void stop_sound(uint32_t voice_id);
    void stop_all_sounds();
    bool is_sound_playing(uint32_t voice_id) const;

    // -------- Live voice control --------
    void set_voice_gain(uint32_t voice_id, float left, float right);
    void set_voice_volume(uint32_t voice_id, float vol);
    void set_voice_pitch(uint32_t voice_id, float pitch);
    void set_voice_pan(uint32_t voice_id, float pan);
    void set_voice_position(uint32_t voice_id, const glm::vec3& pos);
    void set_voice_priority(uint32_t voice_id, int priority);
    void set_voice_loop(uint32_t voice_id, bool loop);

    // -------- Custom Lua generator --------
    void set_custom_gen(uint32_t voice_id, int lua_ref, int block_size);
    void clear_custom_gen(uint32_t voice_id);

    using CustomGenCallback =
        std::function<void(int lua_ref, float* out, int frames,
                           double t_start, int sample_rate)>;
    void set_custom_gen_callback(CustomGenCallback cb) {
        m_custom_gen_cb = std::move(cb);
    }

    // -------- Music --------
    bool play_music(const std::string& filepath, bool loop = true, float fade_in = 0.0f);
    void stop_music(float fade_out = 0.0f);
    void pause_music();
    void resume_music();
    bool is_music_playing() const { return m_music_active && !m_music_paused; }

    // -------- Volume --------
    void  set_master_volume(float vol);
    float get_master_volume() const { return m_master_volume; }
    void  set_sfx_volume(float vol);
    float get_sfx_volume() const { return m_sfx_volume; }
    void  set_music_volume(float vol);
    float get_music_volume() const { return m_music_volume; }

    // -------- Listener --------
    void set_listener_position(const glm::vec3& pos) { m_listener_pos = pos; }
    const glm::vec3& get_listener_position() const { return m_listener_pos; }
    void set_listener_orientation(const glm::vec3& forward, const glm::vec3& up);

    // -------- Stats --------
    int get_active_voice_count() const;
    int get_max_voices() const { return CRAYON_MAX_VOICES; }

private:
    // Internal helpers
    Voice* acquire_voice(int priority);
    void   reset_voice(Voice& v);
    Voice* find_voice(uint32_t voice_id);

    void   mix(int frames);
    float  generate_sample(Voice& v);
    void   enter_release(Voice& v);
    void   update_3d_gain(Voice& v);
    void   refill_custom_gen(Voice& v);

    void   apply_template(Voice& v, const SoundTemplate& tmpl);
    void   apply_from_sound(Voice& v, const SoundData& snd,
                            float volume, float pitch, bool loop);

    // State
    SDL_AudioStream* m_output_stream = nullptr;
    SDL_AudioSpec    m_stream_spec{};
    bool m_initialized = false;

    float m_master_volume = 1.0f;
    float m_sfx_volume    = 1.0f;
    float m_music_volume  = 1.0f;

    glm::vec3 m_listener_pos{0.0f};
    glm::vec3 m_listener_forward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_listener_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_listener_right{1.0f, 0.0f, 0.0f};

    uint32_t m_next_sound_id = 1;
    uint32_t m_next_voice_id = 1;

    std::unordered_map<std::string, uint32_t>                 m_path_to_sound_id;
    std::unordered_map<uint32_t, std::shared_ptr<SoundData>>  m_sounds;

    Voice m_voices[CRAYON_MAX_VOICES];
    std::vector<float> m_mix_buffer;

    CustomGenCallback m_custom_gen_cb;

    // Music (mixed inside mix(), no separate SDL stream)
    bool m_music_active = false;
    bool m_music_paused = false;
    bool m_music_loop   = true;
    std::vector<float> m_music_samples;   // stereo interleaved F32 @ CRAYON_DEVICE_RATE
    size_t m_music_frames = 0;
    double m_music_pos    = 0.0;
    float  m_music_fade_timer    = 0.0f;
    float  m_music_fade_duration = 0.0f;
    bool   m_music_fading_in     = false;
    bool   m_music_fading_out    = false;
    float  m_music_current_gain  = 1.0f;
};

} // namespace crayon