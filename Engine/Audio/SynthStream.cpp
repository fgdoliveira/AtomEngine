#include "Audio/SynthStream.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iostream>

namespace Atom
{
    SynthStream::~SynthStream()
    {
        Stop();
    }

    bool SynthStream::Start(SynthSequencer& sequencer)
    {
        Stop();
        if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            std::cerr << "Synth: no audio (" << SDL_GetError() << "); running silent\n";
            return false;
        }
        m_sequencer = &sequencer;
        const SDL_AudioSpec spec{ SDL_AUDIO_F32, 1, SampleRate };
        m_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SynthStream::Callback, this);
        if (!m_stream)
        {
            std::cerr << "Synth: no playback device (" << SDL_GetError() << "); running silent\n";
            m_sequencer = nullptr;
            return false;
        }
        SDL_ResumeAudioStreamDevice(m_stream);
        return true;
    }

    void SynthStream::Stop()
    {
        if (m_stream)
        {
            SDL_DestroyAudioStream(m_stream); // stops the callback first
            m_stream = nullptr;
        }
        m_sequencer = nullptr;
    }

    void SynthStream::SetGain(float gain)
    {
        if (m_stream)
        {
            SDL_SetAudioStreamGain(m_stream, gain);
        }
    }

    bool SynthStream::Send(const SynthCommand& command)
    {
        const std::uint32_t head = m_head.load(std::memory_order_relaxed);
        const std::uint32_t tail = m_tail.load(std::memory_order_acquire);
        if (head - tail >= m_queue.size())
        {
            return false; // full
        }
        m_queue[head % m_queue.size()] = command;
        m_head.store(head + 1, std::memory_order_release);
        return true;
    }

    void SynthStream::Fill(SynthSequencer& sequencer, float* out, int frames)
    {
        // Commands first, then schedule and render, block by block.
        std::uint32_t tail = m_tail.load(std::memory_order_relaxed);
        const std::uint32_t head = m_head.load(std::memory_order_acquire);
        while (tail != head)
        {
            sequencer.OnCommand(m_synth, m_queue[tail % m_queue.size()]);
            ++tail;
        }
        m_tail.store(tail, std::memory_order_release);

        for (int done = 0; done < frames;)
        {
            const int count = std::min(Block, frames - done);
            sequencer.Advance(m_synth, m_synth.Now() + static_cast<double>(count) / SampleRate);
            m_synth.Render(out + done, count);
            done += count;
        }
    }

    void SynthStream::RenderOffline(SynthSequencer& sequencer, float* out, int frames)
    {
        Fill(sequencer, out, frames);
    }

    void SynthStream::Callback(void* user, SDL_AudioStream* stream, int additional, int /*total*/)
    {
        auto* self = static_cast<SynthStream*>(user);
        if (!self->m_sequencer || additional <= 0)
        {
            return;
        }
        // Stack buffer: nothing allocates on the audio thread.
        float buffer[1024];
        int frames = additional / static_cast<int>(sizeof(float));
        while (frames > 0)
        {
            const int count = std::min(frames, 1024);
            self->Fill(*self->m_sequencer, buffer, count);
            SDL_PutAudioStreamData(stream, buffer, count * static_cast<int>(sizeof(float)));
            frames -= count;
        }
    }
}
