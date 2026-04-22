#pragma once
#include <array>
#include <atomic>
#include <cstring>

class LoopBus
{
public:
    static constexpr int kMaxBlockSize = 8192;
    static constexpr int kNumChannels = 2;
    static constexpr int kNumBuses = 16;

    static LoopBus& getInstance(int busIndex)
    {
        static std::array<LoopBus, kNumBuses> instances;
        return instances[(size_t)std::clamp(busIndex, 0, kNumBuses - 1)];
    }

    void write(const float* const* data, int numSamples, int numChannels)
    {
        int ch = std::min(numChannels, kNumChannels);
        for (int c = 0; c < ch; ++c)
            std::memcpy(buffer[(size_t)c].data(), data[c], sizeof(float) * (size_t)numSamples);

        samplesReady.store(numSamples);
    }

    int read(float* const* dest, int maxSamples, int numChannels)
    {
        int n = samplesReady.exchange(0);
        if (n == 0) return 0;

        n = std::min(n, maxSamples);
        int ch = std::min(numChannels, kNumChannels);
        for (int c = 0; c < ch; ++c)
        {
            std::memcpy(dest[c], buffer[(size_t)c].data(), sizeof(float) * (size_t)n);
            std::memset(buffer[(size_t)c].data(), 0, sizeof(float) * (size_t)n);
        }
        return n;
    }

private:
    LoopBus() { clear(); }
    friend struct std::array<LoopBus, kNumBuses>;

    void clear()
    {
        for (auto& ch : buffer)
            ch.fill(0.0f);
        samplesReady.store(0);
    }

    std::array<std::array<float, kMaxBlockSize>, kNumChannels> buffer;
    std::atomic<int> samplesReady{0};
};