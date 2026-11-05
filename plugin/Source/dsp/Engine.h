// MASALA — moteur DSP (portage C++ fidèle de app/js/dsp.js).
// Toute modification de l'algorithme doit être faite des deux côtés pour que
// les presets sonnent pareil sur téléphone et dans le DAW.
#pragma once

#include "../ParamDefs.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace masala
{
constexpr int kTableSize = 2048;
constexpr int kFrames = 8;
constexpr int kMips = 10;
constexpr int kControlRate = 32;
constexpr int kMaxUni = 7;
constexpr int kMaxVoices = 8;

//==============================================================================
class Wavetables
{
public:
    static const Wavetables& get(); // construit une seule fois (thread-safe)
    const float* table (int t, int frame, int mip) const noexcept
    {
        return data.data() + ((size_t) ((t * kFrames + frame) * kMips + mip)) * (kTableSize + 1);
    }
    std::vector<float> preview (int table, float pos, int warpMode, float warp, int points) const;

private:
    Wavetables();
    std::vector<float> data;
};

//==============================================================================
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    double next() noexcept
    {
        uint32_t x = s;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        s = x;
        return x / 4294967296.0;
    }
};

struct Env
{
    int stage = 0; // 0 idle 1 att 2 dec 3 sus 4 rel
    double v = 0;
    void gate (bool on) noexcept { stage = on ? 1 : (stage ? 4 : 0); }
    double tick (double a, double d, double s, double r, double sr, int n) noexcept;
};

struct Svf
{
    double ic1 = 0, ic2 = 0, lp = 0, bp = 0, hp = 0;
    void reset() noexcept { ic1 = ic2 = 0; }
    void run (double x, double g, double k) noexcept
    {
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + g * a1 * v3;
        const double v2 = ic2 + g * v1;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

class Engine;

struct Voice
{
    Engine* eng = nullptr;
    int note = -1;
    double vel = 0;
    bool active = false, gate = false, wasActive = false;
    uint64_t age = 0;
    double pitch = 60, targetPitch = 60;
    double phA[kMaxUni] {}, phB[kMaxUni] {};
    double subPh = 0;
    Env env1, env2;
    double lfoPh[2] {}, lfoHeld[2] {};
    Svf fL[2], fR[2];
    float mod[kNumModDests] {};
    double g = 0.5, k = 1.4;
    double ampPrev = 0, ampCur = 0;

    struct Ctl
    {
        double drive = 0, fm = 0, sub = 0, noise = 0, subInc = 0, amp = 1;
        double inc[2] {}, pos[2] {}, warp[2] {}, lvl[2] {}, det[2] {};
        int mip[2] {};
    } c;

    void start (int note, double vel, bool retrigger, bool glide);
    void release() noexcept;
    void control();
    void render (float* outL, float* outR, int len);
};

//==============================================================================
class Engine
{
public:
    Engine();
    void prepare (double sampleRate);
    void reset();

    float p[PID::COUNT];
    double bpm = 140.0;
    double bend = 0.0;     // -1..1
    double modWheel = 0.0; // 0..1

    void seed (uint32_t s) noexcept { rng.s = s ? s : 1u; }
    void noteOn (int note, double vel);
    void noteOff (int note);
    void allNotesOff();
    void process (float* outL, float* outR, int n);

    // valeurs de modulation de la voix la plus récente (pour l'interface)
    const float* lastMod() const noexcept { return lastVoice && lastVoice->active ? lastVoice->mod : nullptr; }

private:
    friend struct Voice;
    double sr = 44100.0;
    const Wavetables* tables = nullptr;
    Rng rng;
    std::array<Voice, kMaxVoices> voices;
    std::vector<int> held;
    double lfoFree[2] {};
    uint64_t counter = 0;
    Voice* lastVoice = nullptr;
    float bufL[kControlRate] {}, bufR[kControlRate] {};
    double srcTmp[kNumModSources] {};

    // FX
    struct DelayLine
    {
        std::vector<float> buf;
        int w = 0;
        void init (int n) { buf.assign ((size_t) n, 0.0f); w = 0; }
        double read (double d) const noexcept;
        void write (double x) noexcept { buf[(size_t) w] = (float) x; w = (w + 1) % (int) buf.size(); }
    } delayL, delayR;
    double dlyLP_L = 0, dlyLP_R = 0, dlyTime = 0;

    struct Comb { std::vector<float> b; int i = 0; double f = 0; };
    struct AllP { std::vector<float> b; int i = 0; };
    Comb combL[8], combR[8];
    AllP apL[4], apR[4];
    double revL = 0, revR = 0;
    void reverbRun (double l, double r, double size, double damp) noexcept;

    double crushHold[2] {};
    uint64_t crushCnt = 0;
    double dcX[2] {}, dcY[2] {};
    double compEnv = 0;

    void processFx (float* L, float* R, int n, double mDist, double mDelay, double mRev);
};
} // namespace masala
