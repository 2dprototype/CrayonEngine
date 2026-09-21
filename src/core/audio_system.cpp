#include "audio_system.hpp"
#include "log.hpp"
#include <algorithm>
#include <cmath>

namespace crayon {

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

    m_device_spec.format = SDL_AUDIO_S16;
    m_device_spec.channels = 2;
    m_device_spec.freq = 44100;

    m_device_id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &m_device_spec);
    if (!m_device_id) {
        CRAYON_LOG_WARN("Failed to open default audio playback device: {}", SDL_GetError());
        return false;
    }

    SDL_ResumeAudioDevice(m_device_id);
    m_initialized = true;
    CRAYON_LOG_INFO("AudioSystem initialized (Device ID: {}, 44.1kHz Stereo)", m_device_id);
    return true;
}

void AudioSystem::shutdown() {
    if (!m_initialized) return;

    stop_all_sounds();
    stop_music();

    m_sounds.clear();
    m_path_to_sound_id.clear();

    if (m_device_id) {
        SDL_CloseAudioDevice(m_device_id);
        m_device_id = 0;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    m_initialized = false;
}

void AudioSystem::set_listener_orientation(const glm::vec3& forward, const glm::vec3& up) {
    if (glm::length(forward) > 0.0001f) {
        m_listener_forward = glm::normalize(forward);
    }
    if (glm::length(up) > 0.0001f) {
        m_listener_up = glm::normalize(up);
    }
}

uint32_t AudioSystem::load_sound(const std::string& filepath) {
    if (!m_initialized) return 0;

    auto it = m_path_to_sound_id.find(filepath);
    if (it != m_path_to_sound_id.end()) {
        return it->second;
    }

    auto data = std::make_shared<SoundData>();
    data->path = filepath;
    if (!SDL_LoadWAV(filepath.c_str(), &data->spec, &data->buffer, &data->length)) {
        CRAYON_LOG_WARN("Failed to load WAV audio '{}': {}", filepath, SDL_GetError());
        return 0;
    }

    uint32_t id = m_next_sound_id++;
    data->id = id;
    m_sounds[id] = data;
    m_path_to_sound_id[filepath] = id;
    CRAYON_LOG_INFO("Loaded audio '{}' (ID: {}, {} bytes)", filepath, id, data->length);
    return id;
}

bool AudioSystem::unload_sound(uint32_t sound_id) {
    auto it = m_sounds.find(sound_id);
    if (it == m_sounds.end()) return false;

    for (auto& voice : m_active_voices) {
        if (voice.sound_id == sound_id && voice.stream) {
            SDL_DestroyAudioStream(voice.stream);
            voice.stream = nullptr;
        }
    }

    m_path_to_sound_id.erase(it->second->path);
    m_sounds.erase(it);
    return true;
}

uint32_t AudioSystem::play_sound(uint32_t sound_id, float volume, float pitch, float pan, bool loop) {
    if (!m_initialized || !m_device_id) return 0;

    auto it = m_sounds.find(sound_id);
    if (it == m_sounds.end() || !it->second->buffer) return 0;

    auto data = it->second;
    SDL_AudioStream* stream = SDL_CreateAudioStream(&data->spec, &m_device_spec);
    if (!stream) {
        CRAYON_LOG_WARN("Failed to create audio stream: {}", SDL_GetError());
        return 0;
    }

    if (!SDL_BindAudioStream(m_device_id, stream)) {
        CRAYON_LOG_WARN("Failed to bind audio stream: {}", SDL_GetError());
        SDL_DestroyAudioStream(stream);
        return 0;
    }

    float final_gain = std::clamp(volume * m_sfx_volume * m_master_volume, 0.0f, 4.0f);
    SDL_SetAudioStreamGain(stream, final_gain);
    if (pitch > 0.01f && pitch < 10.0f) {
        SDL_SetAudioStreamFrequencyRatio(stream, pitch);
    }

    SDL_PutAudioStreamData(stream, data->buffer, data->length);

    AudioVoice voice;
    voice.voice_id = m_next_voice_id++;
    voice.sound_id = sound_id;
    voice.stream = stream;
    voice.base_volume = volume;
    voice.pitch = pitch;
    voice.pan = pan;
    voice.loop = loop;
    voice.is_3d = false;

    m_active_voices.push_back(voice);
    return voice.voice_id;
}

uint32_t AudioSystem::play_sound_3d(uint32_t sound_id, const glm::vec3& pos, float volume, float pitch, float min_dist, float max_dist) {
    uint32_t v_id = play_sound(sound_id, volume, pitch, 0.0f, false);
    if (v_id == 0) return 0;

    for (auto& v : m_active_voices) {
        if (v.voice_id == v_id) {
            v.is_3d = true;
            v.pos = pos;
            v.min_dist = std::max(0.1f, min_dist);
            v.max_dist = std::max(v.min_dist + 0.1f, max_dist);
            update_3d_voice(v);
            break;
        }
    }
    return v_id;
}

void AudioSystem::update_3d_voice(AudioVoice& voice) {
    if (!voice.stream) return;

    glm::vec3 to_emitter = voice.pos - m_listener_pos;
    float dist = glm::length(to_emitter);

    float atten = 1.0f;
    if (dist > voice.min_dist) {
        if (dist >= voice.max_dist) {
            atten = 0.0f;
        } else {
            atten = 1.0f - ((dist - voice.min_dist) / (voice.max_dist - voice.min_dist));
            atten = atten * atten; // Inverse quadratic-like rolloff
        }
    }

    float final_gain = std::clamp(voice.base_volume * atten * m_sfx_volume * m_master_volume, 0.0f, 4.0f);
    SDL_SetAudioStreamGain(voice.stream, final_gain);
}

void AudioSystem::stop_sound(uint32_t voice_id) {
    for (auto it = m_active_voices.begin(); it != m_active_voices.end(); ++it) {
        if (it->voice_id == voice_id) {
            if (it->stream) {
                SDL_DestroyAudioStream(it->stream);
                it->stream = nullptr;
            }
            m_active_voices.erase(it);
            return;
        }
    }
}

void AudioSystem::stop_all_sounds() {
    for (auto& v : m_active_voices) {
        if (v.stream) {
            SDL_DestroyAudioStream(v.stream);
            v.stream = nullptr;
        }
    }
    m_active_voices.clear();
}

bool AudioSystem::is_sound_playing(uint32_t voice_id) const {
    for (const auto& v : m_active_voices) {
        if (v.voice_id == voice_id) return true;
    }
    return false;
}

bool AudioSystem::play_music(const std::string& filepath, bool loop, float fade_in) {
    if (!m_initialized || !m_device_id) return false;

    stop_music(0.0f);

    auto data = std::make_shared<SoundData>();
    data->path = filepath;
    if (!SDL_LoadWAV(filepath.c_str(), &data->spec, &data->buffer, &data->length)) {
        CRAYON_LOG_WARN("Failed to load music WAV '{}': {}", filepath, SDL_GetError());
        return false;
    }

    m_music_stream = SDL_CreateAudioStream(&data->spec, &m_device_spec);
    if (!m_music_stream) return false;

    if (!SDL_BindAudioStream(m_device_id, m_music_stream)) {
        SDL_DestroyAudioStream(m_music_stream);
        m_music_stream = nullptr;
        return false;
    }

    m_music_sound = data;
    m_music_loop = loop;
    SDL_PutAudioStreamData(m_music_stream, data->buffer, data->length);

    if (fade_in > 0.0f) {
        m_music_fade_timer = 0.0f;
        m_music_fade_duration = fade_in;
        m_music_fading_out = false;
        SDL_SetAudioStreamGain(m_music_stream, 0.0f);
    } else {
        SDL_SetAudioStreamGain(m_music_stream, m_music_volume * m_master_volume);
    }

    return true;
}

void AudioSystem::stop_music(float fade_out) {
    if (!m_music_stream) return;

    if (fade_out > 0.05f) {
        m_music_fade_timer = 0.0f;
        m_music_fade_duration = fade_out;
        m_music_fading_out = true;
    } else {
        SDL_DestroyAudioStream(m_music_stream);
        m_music_stream = nullptr;
        m_music_sound.reset();
    }
}

void AudioSystem::pause_music() {
    if (m_music_stream) {
        SDL_PauseAudioStreamDevice(m_music_stream);
    }
}

void AudioSystem::resume_music() {
    if (m_music_stream) {
        SDL_ResumeAudioStreamDevice(m_music_stream);
    }
}

bool AudioSystem::is_music_playing() const {
    return m_music_stream != nullptr;
}

void AudioSystem::set_master_volume(float vol) {
    m_master_volume = std::clamp(vol, 0.0f, 2.0f);
    if (m_music_stream) {
        SDL_SetAudioStreamGain(m_music_stream, m_music_volume * m_master_volume);
    }
}

void AudioSystem::set_sfx_volume(float vol) {
    m_sfx_volume = std::clamp(vol, 0.0f, 2.0f);
}

void AudioSystem::set_music_volume(float vol) {
    m_music_volume = std::clamp(vol, 0.0f, 2.0f);
    if (m_music_stream) {
        SDL_SetAudioStreamGain(m_music_stream, m_music_volume * m_master_volume);
    }
}

void AudioSystem::update(float dt) {
    if (!m_initialized) return;

    // 1. Update SFX Voices
    for (auto it = m_active_voices.begin(); it != m_active_voices.end();) {
        if (!it->stream) {
            it = m_active_voices.erase(it);
            continue;
        }

        if (it->is_3d) {
            update_3d_voice(*it);
        }

        int available = SDL_GetAudioStreamAvailable(it->stream);
        if (available <= 0) {
            if (it->loop) {
                auto sound_it = m_sounds.find(it->sound_id);
                if (sound_it != m_sounds.end() && sound_it->second->buffer) {
                    SDL_PutAudioStreamData(it->stream, sound_it->second->buffer, sound_it->second->length);
                }
                ++it;
            } else {
                SDL_DestroyAudioStream(it->stream);
                it->stream = nullptr;
                it = m_active_voices.erase(it);
            }
        } else {
            ++it;
        }
    }

    // 2. Update Music Stream
    if (m_music_stream && m_music_sound) {
        int avail = SDL_GetAudioStreamAvailable(m_music_stream);
        if (avail <= 4096 && m_music_loop) {
            SDL_PutAudioStreamData(m_music_stream, m_music_sound->buffer, m_music_sound->length);
        }

        if (m_music_fade_duration > 0.0f) {
            m_music_fade_timer += dt;
            float progress = std::clamp(m_music_fade_timer / m_music_fade_duration, 0.0f, 1.0f);
            float target_gain = m_music_volume * m_master_volume;
            if (m_music_fading_out) {
                float gain = target_gain * (1.0f - progress);
                SDL_SetAudioStreamGain(m_music_stream, gain);
                if (progress >= 1.0f) {
                    stop_music(0.0f);
                }
            } else {
                float gain = target_gain * progress;
                SDL_SetAudioStreamGain(m_music_stream, gain);
                if (progress >= 1.0f) {
                    m_music_fade_duration = 0.0f;
                }
            }
        }
    }
}

} // namespace crayon
