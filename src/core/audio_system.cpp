#include "audio_system.hpp"
#include "log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace crayon {

static constexpr float PI = 3.14159265358979f;
static constexpr float INV_SR = 1.0f / (float)CRAYON_DEVICE_RATE;

// ------------------------------------------------------------------
//  Convert an SDL_LoadWAV buffer into mono F32 @ CRAYON_DEVICE_RATE
// ------------------------------------------------------------------
static bool wav_to_mono_f32(const Uint8* src, int src_len,
                            const SDL_AudioSpec& src_spec,
                            std::vector<float>& out)
{
    SDL_AudioSpec dst{};
    dst.format   = SDL_AUDIO_F32;
    dst.channels = 1;
    dst.freq     = CRAYON_DEVICE_RATE;

    Uint8* dst_buf = nullptr;
    int    dst_len = 0;
    if (!SDL_ConvertAudioSamples(&src_spec, src, src_len, &dst, &dst_buf, &dst_len)) {
        CRAYON_LOG_WARN("SDL_ConvertAudioSamples failed: {}", SDL_GetError());
        return false;
    }
    int n = dst_len / (int)sizeof(float);
    out.resize(n);
    std::memcpy(out.data(), dst_buf, dst_len);
    SDL_free(dst_buf);
    return true;
}

static bool wav_to_stereo_f32(const Uint8* src, int src_len,
                              const SDL_AudioSpec& src_spec,
                              std::vector<float>& out)
{
    SDL_AudioSpec dst{};
    dst.format   = SDL_AUDIO_F32;
    dst.channels = 2;
    dst.freq     = CRAYON_DEVICE_RATE;

    Uint8* dst_buf = nullptr;
    int    dst_len = 0;
    if (!SDL_ConvertAudioSamples(&src_spec, src, src_len, &dst, &dst_buf, &dst_len)) {
        CRAYON_LOG_WARN("SDL_ConvertAudioSamples failed: {}", SDL_GetError());
        return false;
    }
    int n = dst_len / (int)sizeof(float);
    out.resize(n);
    std::memcpy(out.data(), dst_buf, dst_len);
    SDL_free(dst_buf);
    return true;
}

// ==================================================================
//  Lifecycle
// ==================================================================

AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem() {
    shutdown();
}

bool AudioSystem::init() {
    if (m_initialized) return true;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        CRAYON_LOG_WARN("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: {}", SDL_GetError());
        return false;
    }

    m_stream_spec.format   = SDL_AUDIO_F32;
    m_stream_spec.channels = CRAYON_DEVICE_CH;
    m_stream_spec.freq     = CRAYON_DEVICE_RATE;

    m_output_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &m_stream_spec,
        nullptr, nullptr);

    if (!m_output_stream) {
        CRAYON_LOG_WARN("Failed to open audio stream: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioStreamDevice(m_output_stream);

    // Pre-allocate mix buffer
    m_mix_buffer.assign((size_t)CRAYON_MIX_BLOCK * CRAYON_DEVICE_CH, 0.0f);

    // Reset all voices
    for (auto& v : m_voices) reset_voice(v);

    m_initialized = true;
    CRAYON_LOG_INFO("AudioSystem initialized (F32, {}Hz, {} voices max)",
                    CRAYON_DEVICE_RATE, CRAYON_MAX_VOICES);
    return true;
}

void AudioSystem::shutdown() {
    if (!m_initialized) {
        // Still make sure SDL audio subsystem is torn down if init failed midway
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    if (m_output_stream) {
        SDL_DestroyAudioStream(m_output_stream);
        m_output_stream = nullptr;
    }

    for (auto& v : m_voices) reset_voice(v);
    m_sounds.clear();
    m_path_to_sound_id.clear();
    m_music_samples.clear();
    m_music_active = false;

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    m_initialized = false;
}

// ==================================================================
//  Voice management
// ==================================================================

void AudioSystem::reset_voice(Voice& v) {
    v.active            = false;
    v.voice_id          = 0;
    v.sound_id          = 0;
    v.sample_pos        = 0.0;
    v.loop              = false;
    v.wave              = WaveType::Sample;
    v.phase             = 0.0;
    v.freq              = 440.0f;
    v.noise_state       = 0x12345678u;
    v.pitch_sweep       = false;
    v.sweep_elapsed     = 0.0f;
    v.env_stage         = EnvStage::Idle;
    v.env_level         = 0.0f;
    v.filter.enabled    = false;
    v.filter.lp_state   = 0.0f;
    v.duration          = 0.0f;
    v.duration_elapsed  = 0.0f;
    v.base_volume       = 1.0f;
    v.gain_l            = 0.707f;
    v.gain_r            = 0.707f;
    v.pitch             = 1.0f;
    v.pan               = 0.0f;
    v.manual_gain       = false;
    v.is_3d             = false;
    v.priority          = 0;
    v.has_custom_gen    = false;
    v.custom_gen_ref    = -1;
    v.custom_gen_pos    = 0;
    v.custom_gen_buffer.clear();
}

Voice* AudioSystem::acquire_voice(int priority) {
    // 1) Free slot?
    for (auto& v : m_voices) {
        if (!v.active) return &v;
    }
    // 2) Steal lowest-priority
    Voice* victim = nullptr;
    for (auto& v : m_voices) {
        if (!victim || v.priority < victim->priority) victim = &v;
    }
    if (victim && victim->priority <= priority) {
        // Hard-steal (fast; a short fade could be added later)
        reset_voice(*victim);
        return victim;
    }
    return nullptr; // all voices higher priority — drop
}

Voice* AudioSystem::find_voice(uint32_t voice_id) {
    for (auto& v : m_voices) {
        if (v.active && v.voice_id == voice_id) return &v;
    }
    return nullptr;
}

int AudioSystem::get_active_voice_count() const {
    int n = 0;
    for (const auto& v : m_voices) if (v.active) ++n;
    return n;
}

// ==================================================================
//  Asset management
// ==================================================================

uint32_t AudioSystem::load_sound(const std::string& filepath) {
    if (!m_initialized) return 0;

    auto it = m_path_to_sound_id.find(filepath);
    if (it != m_path_to_sound_id.end()) return it->second;

    SDL_AudioSpec spec{};
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (!SDL_LoadWAV(filepath.c_str(), &spec, &buf, &len)) {
        CRAYON_LOG_WARN("Failed to load WAV '{}': {}", filepath, SDL_GetError());
        return 0;
    }

    auto data = std::make_shared<SoundData>();
    data->path = filepath;
    bool ok = wav_to_mono_f32(buf, (int)len, spec, data->samples);
    SDL_free(buf);

    if (!ok || data->samples.empty()) return 0;

    uint32_t id = m_next_sound_id++;
    data->id = id;
    m_sounds[id] = data;
    m_path_to_sound_id[filepath] = id;
    CRAYON_LOG_INFO("Loaded sound '{}' (ID {}, {} samples)", filepath, id, data->samples.size());
    return id;
}

bool AudioSystem::unload_sound(uint32_t sound_id) {
    auto it = m_sounds.find(sound_id);
    if (it == m_sounds.end()) return false;

    // Kill any active voices using this sound
    for (auto& v : m_voices) {
        if (v.active && v.sound_id == sound_id) reset_voice(v);
    }

    m_path_to_sound_id.erase(it->second->path);
    m_sounds.erase(it);
    return true;
}

// ==================================================================
//  Procedural sounds
// ==================================================================

uint32_t AudioSystem::create_sound(const SoundTemplate& tmpl) {
    if (!m_initialized) return 0;

    auto data = std::make_shared<SoundData>();
    data->is_procedural = true;
    data->tmpl = tmpl;

    uint32_t id = m_next_sound_id++;
    data->id = id;
    m_sounds[id] = data;
    return id;
}

uint32_t AudioSystem::create_buffer(int frames,
                                    const std::function<float(float, int)>& generator)
{
    if (!m_initialized || frames <= 0) return 0;

    auto data = std::make_shared<SoundData>();
    data->samples.resize((size_t)frames);
    for (int i = 0; i < frames; ++i) {
        float t = (float)i * INV_SR;
        data->samples[(size_t)i] = generator(t, i);
    }

    uint32_t id = m_next_sound_id++;
    data->id = id;
    m_sounds[id] = data;
    return id;
}

// ==================================================================
//  Voice setup from sound / template
// ==================================================================

void AudioSystem::apply_template(Voice& v, const SoundTemplate& t) {
    v.adsr = t.adsr;
    v.env_attack_inc  = 1.0f / std::max(0.0001f, t.adsr.attack  * (float)CRAYON_DEVICE_RATE);
    v.env_decay_dec   = (1.0f - t.adsr.sustain) /
                        std::max(0.0001f, t.adsr.decay * (float)CRAYON_DEVICE_RATE);
    v.env_release_dec = 1.0f; // recomputed when entering release
    v.env_stage = EnvStage::Attack;
    v.env_level = 0.0f;

    v.filter.enabled  = t.filter_enabled;
    v.filter.highpass = t.filter_highpass;
    v.filter.cutoff   = t.filter_cutoff;
    v.filter.lp_state = 0.0f;

    v.pitch_sweep   = t.pitch_sweep;
    v.sweep_from    = t.sweep_from;
    v.sweep_to      = t.sweep_to;
    v.sweep_time    = t.sweep_time;
    v.sweep_elapsed = 0.0f;

    v.wave = t.wave;
    v.freq = t.freq;
    
    v.duration         = t.duration;
    v.duration_elapsed = 0.0f;
}

void AudioSystem::apply_from_sound(Voice& v, const SoundData& snd,
                                   float volume, float pitch, bool loop)
{
    v.sound_id     = snd.id;
    v.base_volume  = volume;
    v.pitch        = std::clamp(pitch, 0.05f, 10.0f);
    v.loop         = loop;
    v.duration     = 0.0f;
    v.duration_elapsed = 0.0f;
    v.sample_pos   = 0.0;
    v.phase        = 0.0;

    if (snd.is_procedural) {
        apply_template(v, snd.tmpl);
    } else {
        // Sample-based: default ADSR = instant attack, no release (until stop)
        v.adsr.attack  = 0.0f;
        v.adsr.decay   = 0.0f;
        v.adsr.sustain = 1.0f;
        v.adsr.release = 0.02f;
        v.env_attack_inc = 1.0f;
        v.env_level = 1.0f;
        v.env_stage = EnvStage::Sustain;
        v.wave = WaveType::Sample;
        v.filter.enabled = false;
    }
}

// ==================================================================
//  Playback API
// ==================================================================

uint32_t AudioSystem::play_sound(uint32_t sound_id, float volume, float pitch,
                                 float pan, bool loop)
{
    if (!m_initialized) return 0;
    auto it = m_sounds.find(sound_id);
    if (it == m_sounds.end()) return 0;

    Voice* v = acquire_voice(0);
    if (!v) return 0;

    reset_voice(*v);
    apply_from_sound(*v, *it->second, volume, pitch, loop);
    v->pan         = std::clamp(pan, -1.0f, 1.0f);
    v->is_3d       = false;
    v->active      = true;
    v->voice_id    = m_next_voice_id++;
    v->priority    = 0;

    return v->voice_id;
}

uint32_t AudioSystem::play_sound_3d(uint32_t sound_id, const glm::vec3& pos,
                                    float volume, float pitch,
                                    float min_dist, float max_dist)
{
    if (!m_initialized) return 0;
    auto it = m_sounds.find(sound_id);
    if (it == m_sounds.end()) return 0;

    Voice* v = acquire_voice(0);
    if (!v) return 0;

    reset_voice(*v);
    apply_from_sound(*v, *it->second, volume, pitch, false);
    v->is_3d    = true;
    v->pos      = pos;
    v->min_dist = std::max(0.1f, min_dist);
    v->max_dist = std::max(v->min_dist + 0.1f, max_dist);
    v->active   = true;
    v->voice_id = m_next_voice_id++;
    v->priority = 0;

    return v->voice_id;
}

void AudioSystem::stop_sound(uint32_t voice_id) {
    Voice* v = find_voice(voice_id);
    if (!v) return;
    // Graceful: force release. If release is tiny, voice dies next block.
    enter_release(*v);
}

void AudioSystem::stop_all_sounds() {
    for (auto& v : m_voices) {
        if (v.active) enter_release(v);
    }
}

bool AudioSystem::is_sound_playing(uint32_t voice_id) const {
    for (const auto& v : m_voices) {
        if (v.active && v.voice_id == voice_id) return true;
    }
    return false;
}

// ==================================================================
//  Live voice control
// ==================================================================

void AudioSystem::set_voice_gain(uint32_t voice_id, float left, float right) {
    if (Voice* v = find_voice(voice_id)) {
        v->gain_l = left;
        v->gain_r = right;
        v->manual_gain = true;
    }
}

void AudioSystem::set_voice_volume(uint32_t voice_id, float vol) {
    if (Voice* v = find_voice(voice_id)) {
        v->base_volume = vol;
        v->manual_gain = false;
    }
}

void AudioSystem::set_voice_pitch(uint32_t voice_id, float pitch) {
    if (Voice* v = find_voice(voice_id)) v->pitch = std::clamp(pitch, 0.05f, 10.0f);
}

void AudioSystem::set_voice_pan(uint32_t voice_id, float pan) {
    if (Voice* v = find_voice(voice_id)) {
        v->pan = std::clamp(pan, -1.0f, 1.0f);
        v->manual_gain = false;
    }
}

void AudioSystem::set_voice_position(uint32_t voice_id, const glm::vec3& pos) {
    if (Voice* v = find_voice(voice_id)) {
        if (v->is_3d) v->pos = pos;
    }
}

void AudioSystem::set_voice_priority(uint32_t voice_id, int priority) {
    if (Voice* v = find_voice(voice_id)) v->priority = priority;
}

void AudioSystem::set_voice_loop(uint32_t voice_id, bool loop) {
    if (Voice* v = find_voice(voice_id)) v->loop = loop;
}

// ==================================================================
//  Custom Lua generator
// ==================================================================

void AudioSystem::set_custom_gen(uint32_t voice_id, int lua_ref, int block_size) {
    Voice* v = find_voice(voice_id);
    if (!v) return;
    v->has_custom_gen   = true;
    v->custom_gen_ref   = lua_ref;
    v->custom_gen_block = std::clamp(block_size, 32, 4096);
    v->custom_gen_buffer.assign((size_t)v->custom_gen_block, 0.0f);
    v->custom_gen_pos   = v->custom_gen_buffer.size(); // force refill on next sample
    v->custom_gen_time  = 0.0;
}

void AudioSystem::clear_custom_gen(uint32_t voice_id) {
    Voice* v = find_voice(voice_id);
    if (!v) return;
    v->has_custom_gen = false;
    v->custom_gen_ref = -1;
    v->custom_gen_buffer.clear();
    v->custom_gen_pos = 0;
}

void AudioSystem::refill_custom_gen(Voice& v) {
    if (!m_custom_gen_cb || v.custom_gen_ref < 0) {
        v.custom_gen_buffer.assign(v.custom_gen_buffer.size(), 0.0f);
        v.custom_gen_pos = 0;
        return;
    }
    m_custom_gen_cb(v.custom_gen_ref,
                    v.custom_gen_buffer.data(),
                    (int)v.custom_gen_buffer.size(),
                    v.custom_gen_time,
                    CRAYON_DEVICE_RATE);
    v.custom_gen_pos  = 0;
    v.custom_gen_time += (double)v.custom_gen_buffer.size() * INV_SR;
}

// ==================================================================
//  Listener
// ==================================================================

void AudioSystem::set_listener_orientation(const glm::vec3& forward, const glm::vec3& up) {
    if (glm::length(forward) > 0.0001f) m_listener_forward = glm::normalize(forward);
    if (glm::length(up) > 0.0001f)      m_listener_up      = glm::normalize(up);
    m_listener_right = glm::normalize(glm::cross(m_listener_forward, m_listener_up));
}

// ==================================================================
//  Music
// ==================================================================

bool AudioSystem::play_music(const std::string& filepath, bool loop, float fade_in) {
    if (!m_initialized) return false;

    stop_music(0.0f);

    SDL_AudioSpec spec{};
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (!SDL_LoadWAV(filepath.c_str(), &spec, &buf, &len)) {
        CRAYON_LOG_WARN("Failed to load music '{}': {}", filepath, SDL_GetError());
        return false;
    }

    bool ok = wav_to_stereo_f32(buf, (int)len, spec, m_music_samples);
    SDL_free(buf);
    if (!ok || m_music_samples.empty()) return false;

    m_music_frames = m_music_samples.size() / 2;
    m_music_pos    = 0.0;
    m_music_loop   = loop;
    m_music_active = true;
    m_music_paused = false;
    m_music_current_gain = m_music_volume * m_master_volume;
    m_music_fading_out   = false;
    m_music_fading_in    = false;
    m_music_fade_duration = 0.0f;

    if (fade_in > 0.0f) {
        m_music_fade_timer    = 0.0f;
        m_music_fade_duration = fade_in;
        m_music_fading_in     = true;
        m_music_current_gain  = 0.0f;
    }

    return true;
}

void AudioSystem::stop_music(float fade_out) {
    if (!m_music_active) return;
    if (fade_out > 0.05f) {
        m_music_fade_timer    = 0.0f;
        m_music_fade_duration = fade_out;
        m_music_fading_out    = true;
        m_music_fading_in     = false;
    } else {
        m_music_active = false;
        m_music_samples.clear();
        m_music_frames = 0;
        m_music_pos    = 0.0;
    }
}

void AudioSystem::pause_music() {
    m_music_paused = true;
}

void AudioSystem::resume_music() {
    m_music_paused = false;
}

// ==================================================================
//  Volume
// ==================================================================

void AudioSystem::set_master_volume(float vol) {
    m_master_volume = std::clamp(vol, 0.0f, 2.0f);
}

void AudioSystem::set_sfx_volume(float vol) {
    m_sfx_volume = std::clamp(vol, 0.0f, 2.0f);
}

void AudioSystem::set_music_volume(float vol) {
    m_music_volume = std::clamp(vol, 0.0f, 2.0f);
}

// ==================================================================
//  Envelope helpers
// ==================================================================

void AudioSystem::enter_release(Voice& v) {
    if (v.env_stage == EnvStage::Release || v.env_stage == EnvStage::Idle) return;
    v.env_stage = EnvStage::Release;
    float rel = std::max(0.0005f, v.adsr.release);
    // Decay linearly from current level to 0 over `release` seconds
    v.env_release_dec = v.env_level / (rel * (float)CRAYON_DEVICE_RATE);
    if (v.env_release_dec <= 0.0f) v.env_release_dec = 1.0f;
}

// ==================================================================
//  3D gain (constant-power pan using listener right-vector)
// ==================================================================

void AudioSystem::update_3d_gain(Voice& v) {
    glm::vec3 to_emitter = v.pos - m_listener_pos;
    float dist = glm::length(to_emitter);

    float atten = 1.0f;
    if (dist > v.min_dist) {
        if (dist >= v.max_dist) {
            atten = 0.0f;
        } else {
            float t = (dist - v.min_dist) / (v.max_dist - v.min_dist);
            atten = 1.0f - t;
            atten *= atten; // inverse-quadratic-ish
        }
    }

    // Pan from right-vector dot product
    float pan = 0.0f;
    if (dist > 0.0001f) {
        glm::vec3 dir = to_emitter / dist;
        pan = glm::clamp(glm::dot(dir, m_listener_right), -1.0f, 1.0f);
    }

    float angle = (pan + 1.0f) * 0.25f * PI; // 0 .. PI/2
    float gl = std::cos(angle);
    float gr = std::sin(angle);
    float master = v.base_volume * atten * m_sfx_volume * m_master_volume;
    v.gain_l = gl * master;
    v.gain_r = gr * master;
}

// ==================================================================
//  Generator (per-sample)
// ==================================================================

float AudioSystem::generate_sample(Voice& v) {
    // ---- Custom Lua generator ----
    if (v.has_custom_gen) {
        if (v.custom_gen_pos >= v.custom_gen_buffer.size()) {
            refill_custom_gen(v);
        }
        if (v.custom_gen_pos < v.custom_gen_buffer.size()) {
            return v.custom_gen_buffer[v.custom_gen_pos++];
        }
        return 0.0f;
    }

    // ---- Sample playback ----
    if (v.wave == WaveType::Sample) {
        auto it = m_sounds.find(v.sound_id);
        if (it == m_sounds.end() || it->second->samples.empty()) {
            v.active = false;
            return 0.0f;
        }
        const auto& s = it->second->samples;
        size_t n = s.size();

        if (v.sample_pos >= (double)n) {
            if (v.loop) {
                v.sample_pos = std::fmod(v.sample_pos, (double)n);
            } else {
                enter_release(v);
                return 0.0f;
            }
        }

        size_t i0 = (size_t)v.sample_pos;
        size_t i1 = (i0 + 1 < n) ? (i0 + 1) : (v.loop ? 0 : i0);
        float frac = (float)(v.sample_pos - (double)i0);
        float out = s[i0] + (s[i1] - s[i0]) * frac;

        v.sample_pos += (double)v.pitch;
        return out;
    }

    // ---- Oscillator ----
    float freq = v.freq;
    if (v.pitch_sweep) {
        float t = (v.sweep_time > 0.0f) ? (v.sweep_elapsed / v.sweep_time) : 1.0f;
        if (t > 1.0f) t = 1.0f;
        freq = v.sweep_from + (v.sweep_to - v.sweep_from) * t;
        v.sweep_elapsed += INV_SR;
    }
    freq *= v.pitch;

    double phase_inc = (double)freq * (double)INV_SR;
    v.phase += phase_inc;
    if (v.phase >= 1.0) v.phase -= std::floor(v.phase);

    float out = 0.0f;
    switch (v.wave) {
        case WaveType::Sine:
            out = std::sin((float)(v.phase * 2.0 * PI));
            break;
        case WaveType::Square:
            out = (v.phase < 0.5) ? 1.0f : -1.0f;
            break;
        case WaveType::Saw:
            out = (float)(v.phase * 2.0 - 1.0);
            break;
        case WaveType::Triangle:
            out = (float)(4.0 * std::abs(v.phase - 0.5) - 1.0);
            break;
        case WaveType::Noise:
            v.noise_state = v.noise_state * 1664525u + 1013904223u;
            out = ((float)(v.noise_state >> 8) / 8388608.0f) - 1.0f;
            break;
        default:
            out = 0.0f;
            break;
    }
    return out;
}

// ==================================================================
//  Mixer
// ==================================================================

void AudioSystem::mix(int frames) {
    float* buf = m_mix_buffer.data();

    // Reset output block
    std::memset(buf, 0, (size_t)frames * CRAYON_DEVICE_CH * sizeof(float));

    // --- Per-block voice prep ---
    for (auto& v : m_voices) {
        if (!v.active) continue;

        // Filter coefficient
        if (v.filter.enabled) {
            float fc = std::clamp(v.filter.cutoff, 20.0f,
                                  (float)CRAYON_DEVICE_RATE * 0.45f);
            float dt = INV_SR;
            float rc = 1.0f / (2.0f * PI * fc);
            v.filter.coeff = dt / (rc + dt);
        }

        // Gain (skip manual gain override)
        if (!v.manual_gain) {
            if (v.is_3d) {
                update_3d_gain(v);
            } else {
                float angle = (v.pan + 1.0f) * 0.25f * PI;
                float master = v.base_volume * m_sfx_volume * m_master_volume;
                v.gain_l = std::cos(angle) * master;
                v.gain_r = std::sin(angle) * master;
            }
        }
    }

    // Music fade (per-block is plenty smooth)
    if (m_music_active && m_music_fade_duration > 0.0f) {
        float target = m_music_volume * m_master_volume;
        if (m_music_fading_out) {
            m_music_current_gain = target * (1.0f - m_music_fade_timer / m_music_fade_duration);
            if (m_music_current_gain < 0.0f) m_music_current_gain = 0.0f;
        } else if (m_music_fading_in) {
            m_music_current_gain = target * (m_music_fade_timer / m_music_fade_duration);
        }
    } else if (m_music_active) {
        m_music_current_gain = m_music_volume * m_master_volume;
    }

    const float music_gain = m_music_active && !m_music_paused
                             ? m_music_current_gain : 0.0f;

    // --- Per-sample loop ---
    for (int i = 0; i < frames; ++i) {
        float L = 0.0f, R = 0.0f;

        for (int vi = 0; vi < CRAYON_MAX_VOICES; ++vi) {
            Voice& v = m_voices[vi];
            if (!v.active) continue;

            // --- Sample / oscillator ---
            float s = generate_sample(v);

            // --- Envelope (per-sample) ---
            switch (v.env_stage) {
                case EnvStage::Attack:
                    v.env_level += v.env_attack_inc;
                    if (v.env_level >= 1.0f) {
                        v.env_level = 1.0f;
                        v.env_stage = EnvStage::Decay;
                    }
                    break;
                case EnvStage::Decay:
                    v.env_level -= v.env_decay_dec;
                    if (v.env_level <= v.adsr.sustain) {
                        v.env_level = v.adsr.sustain;
                        v.env_stage = EnvStage::Sustain;
                    }
                    break;
                case EnvStage::Sustain:
                    v.env_level = v.adsr.sustain;
                    break;
                case EnvStage::Release:
                    v.env_level -= v.env_release_dec;
                    if (v.env_level <= 0.0f) {
                        v.env_level = 0.0f;
                        v.env_stage = EnvStage::Idle;
                        v.active    = false;
                    }
                    break;
                default:
                    v.active = false;
                    break;
            }
            if (!v.active) continue;

            s *= v.env_level;

            // --- Filter ---
            if (v.filter.enabled) {
                v.filter.lp_state += v.filter.coeff * (s - v.filter.lp_state);
                s = v.filter.highpass ? (s - v.filter.lp_state)
                                      : v.filter.lp_state;
            }

            // --- Mix ---
            L += s * v.gain_l;
            R += s * v.gain_r;

            // --- Duration auto-release ---
            if (v.duration > 0.0f && v.env_stage != EnvStage::Release) {
                v.duration_elapsed += INV_SR;
                if (v.duration_elapsed >= v.duration) enter_release(v);
            }
        }

        // --- Music ---
        if (music_gain > 0.0f && m_music_frames > 0) {
            size_t idx = (size_t)m_music_pos;
            if (idx < m_music_frames) {
                L += m_music_samples[idx * 2 + 0] * music_gain;
                R += m_music_samples[idx * 2 + 1] * music_gain;
            }
            m_music_pos += 1.0;
            if (m_music_pos >= (double)m_music_frames) {
                if (m_music_loop) m_music_pos = 0.0;
                else              m_music_active = false;
            }
        }

        // --- Soft clip (once) ---
        L = L / (1.0f + std::abs(L));
        R = R / (1.0f + std::abs(R));

        buf[i * 2 + 0] = L;
        buf[i * 2 + 1] = R;
    }

    // Push to SDL
    SDL_PutAudioStreamData(m_output_stream, buf,
                           frames * CRAYON_DEVICE_CH * (int)sizeof(float));
}

// ==================================================================
//  Update (called every frame from Engine)
// ==================================================================

void AudioSystem::update(float dt) {
    if (!m_initialized || !m_output_stream) return;

    // Advance music fade timers
    if (m_music_active && m_music_fade_duration > 0.0f) {
        m_music_fade_timer += dt;
        if (m_music_fade_timer >= m_music_fade_duration) {
            if (m_music_fading_out) {
                stop_music(0.0f);
            } else if (m_music_fading_in) {
                m_music_fading_in     = false;
                m_music_fade_duration = 0.0f;
                m_music_current_gain  = m_music_volume * m_master_volume;
            }
        }
    }

    // How many frames are queued (in the stream)?
    int bytes_queued = SDL_GetAudioStreamQueued(m_output_stream);
    if (bytes_queued < 0) return;
    int frames_queued = bytes_queued / (CRAYON_DEVICE_CH * (int)sizeof(float));

    // Target buffer: ~33 ms of audio
    int target = CRAYON_DEVICE_RATE / 30;
    int to_mix = target - frames_queued;
    if (to_mix <= 0) return;

    while (to_mix > 0) {
        int n = std::min(to_mix, CRAYON_MIX_BLOCK);
        mix(n);
        to_mix -= n;
    }
}

} // namespace crayon