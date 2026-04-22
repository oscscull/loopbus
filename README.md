# LoopBus

A minimal VST3 plugin that creates feedback loops within your DAW's effect chain. Built for DAWs like Renoise that don't natively support send track loops.

## What it does

LoopBus is two plugins in one: a **Send** and a **Return**. Place the Return at the top of your effect chain and the Send at the bottom. Audio from the Send feeds back into the Return on the next buffer cycle, creating a feedback loop through whatever effects sit between them.

```
Signal flow on a single track:

Input audio
    ↓
┌─ LoopBus (Return) ← reads from shared bus, mixes in feedback
│   ↓
│  Delay
│   ↓
│  Chorus
│   ↓
│  (any other effects)
│   ↓
└─ LoopBus (Send) → writes to shared bus
    ↓
Output audio
```

## Parameters

- **Mode** — Send or Return
- **Channel** (1–16) — Send and Return must be on the same channel to communicate. Supports up to 16 independent loops.
- **Feedback (dB)** — How much the signal is attenuated each time around the loop. -6 dB = half volume per lap. Only active in Return mode.
- **Limit (dB)** — Hard clip ceiling as a safety net. Default 0 dB. Only active in Return mode.

## Download

Pre-built VST3 binaries are available on the [Releases](../../releases) page.

## Building from source

### Requirements

- C++17 compiler (Clang, GCC, or MSVC)
- CMake 3.22+
- macOS: Xcode Command Line Tools (`xcode-select --install`)
- Windows: Visual Studio 2019+
- Linux: ALSA and X11 dev packages

### Steps

```bash
git clone --recursive https://github.com/YOURNAME/LoopBus.git
cd LoopBus
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 will be in `build/LoopBus_artefacts/Release/VST3/`.

Copy it to your VST3 folder:
- **macOS:** `~/Library/Audio/Plug-Ins/VST3/`
- **Windows:** `C:\Program Files\Common Files\VST3\`
- **Linux:** `~/.vst3/`

## License

MIT — see [LICENSE](LICENSE).