// MASALA — moteur DSP, portage de app/js/dsp.js (voir commentaires là-bas).
#include "Engine.h"
#include <algorithm>
#include <complex>
#include <mutex>

namespace masala
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

template <typename T>
T clampv (T x, T a, T b) { return x < a ? a : (x > b ? b : x); }
double lerp (double a, double b, double t) { return a + (b - a) * t; }
double mtof (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); }
double gauss (double n, double c, double w) { return std::exp (-((n - c) * (n - c)) / (2 * w * w)); }

void ifftImag (std::vector<double>& re, std::vector<double>& im)
{
    const int n = (int) re.size();
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap (re[(size_t) i], re[(size_t) j]); std::swap (im[(size_t) i], im[(size_t) j]); }
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = kTwoPi / len;
        const double wr = std::cos (ang), wi = std::sin (ang);
        for (int i = 0; i < n; i += len)
        {
            double cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++)
            {
                const size_t a = (size_t) (i + k), b = a + (size_t) len / 2;
                const double xr = re[b] * cr - im[b] * ci;
                const double xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr; im[a] += xi;
                const double t = cr * wr - ci * wi;
                ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

double besselJ (int k, double x)
{
    const int N = 96;
    double s = 0;
    for (int i = 0; i <= N; i++)
    {
        const double t = kPi * i / N;
        const double w = (i == 0 || i == N) ? 0.5 : 1.0;
        s += w * std::cos (k * t - x * std::sin (t));
    }
    return s / N;
}

const double kVowels[5][2] = { { 450, 800 }, { 800, 1150 }, { 400, 1600 }, { 270, 2300 }, { 325, 700 } };

std::vector<double> spectrum (int t, double p, int H)
{
    std::vector<double> a ((size_t) H + 1, 0.0);
    switch (t)
    {
        case 0:
        {
            const int seg = std::min (2, (int) std::floor (p * 3));
            const double u = p * 3 - seg;
            for (int n = 1; n <= H; n++)
            {
                const bool odd = n & 1;
                const double sine = n == 1 ? 1 : 0;
                const double tri = odd ? (((((n - 1) / 2) & 1) ? -1.0 : 1.0) * 8.0 / (kPi * kPi * n * n)) : 0.0;
                const double saw = ((n & 1) ? 1.0 : -1.0) * 2.0 / (kPi * n);
                const double sq = odd ? 4.0 / (kPi * n) : 0.0;
                const double ks[4] = { sine, tri, saw, sq };
                a[(size_t) n] = lerp (ks[seg], ks[seg + 1], u);
            }
            break;
        }
        case 1:
        {
            const double F = 2 + p * 38, w = 1.5 + p * 4;
            for (int n = 1; n <= H; n++)
                a[(size_t) n] = ((n & 1) ? 1.0 : -1.0) * (1.0 / n) * (0.35 + 6 * gauss (n, F, w) + 2.5 * gauss (n, F * 2.1, w * 1.6));
            break;
        }
        case 2:
        {
            const double x = p * 4;
            const int i0 = std::min (3, (int) std::floor (x));
            const double u = x - i0;
            const double f1 = lerp (kVowels[i0][0], kVowels[i0 + 1][0], u) / 110.0;
            const double f2 = lerp (kVowels[i0][1], kVowels[i0 + 1][1], u) / 110.0;
            for (int n = 1; n <= H; n++)
                a[(size_t) n] = (1.0 / std::sqrt ((double) n)) * (gauss (n, f1, 1.2) + 0.7 * gauss (n, f2, 1.8) + 0.04);
            break;
        }
        case 3:
        {
            const double I = p * 9;
            const int r = 3;
            for (int k = -40; k <= 40; k++)
            {
                const int n = 1 + k * r;
                const double J = besselJ (k, I);
                if (n > 0 && n <= H) a[(size_t) n] += J;
                else if (n < 0 && -n <= H) a[(size_t) -n] -= J;
            }
            break;
        }
        case 4:
        {
            const double d = lerp (0.5, 0.04, p);
            for (int n = 1; n <= H; n++)
                a[(size_t) n] = (2.0 / (n * kPi)) * std::sin (n * kPi * d) * (1 + 0.6 * gauss (n, 5 + 10 * p, 2));
            break;
        }
        default:
        {
            const int m = 2 + (int) std::floor (p * 9);
            for (int n = 1; n <= H; n++)
                a[(size_t) n] = (1.0 / n) * (((n * m) % 7) < 3 ? 1.0 : 0.12) * ((n % m) == 1 ? 1.6 : 1.0);
            break;
        }
    }
    return a;
}

inline double readTable (const Wavetables& wt, int t, double frameF, int mip, double phase) noexcept
{
    const int fi = std::min (kFrames - 2, (int) std::floor (frameF));
    const double fu = frameF - fi;
    const float* ta = wt.table (t, fi, mip);
    const float* tb = wt.table (t, fi + 1, mip);
    const double x = phase * kTableSize;
    const int i = (int) x;
    const double u = x - i;
    const double a = ta[i] + (ta[i + 1] - ta[i]) * u;
    const double b = tb[i] + (tb[i + 1] - tb[i]) * u;
    return a + (b - a) * fu;
}

inline double warpPhase (double p, int mode, double w) noexcept
{
    if (w <= 0) return p;
    switch (mode)
    {
        case 0: return std::pow (p, 1 + w * 3);
        case 1: { const double q = p * (1 + w * 7); return q - std::floor (q); }
        default: { const double m = p < 0.5 ? 2 * p : 2 - 2 * p; return p + (m * 0.5 - p) * w; }
    }
}

inline double lfoValue (int shape, double ph, double held) noexcept
{
    switch (shape)
    {
        case 0: return 0.5 - 0.5 * std::cos (kTwoPi * ph);
        case 1: return ph < 0.5 ? ph * 2 : 2 - ph * 2;
        case 2: return ph;
        case 3: return 1 - ph;
        case 4: return ph < 0.5 ? 1 : 0;
        default: return held;
    }
}

const int kComb[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
const int kAllp[4] = { 556, 441, 341, 225 };
} // namespace

//==============================================================================
Wavetables::Wavetables()
{
    data.assign ((size_t) kNumTables * kFrames * kMips * (kTableSize + 1), 0.0f);
    std::vector<double> re (kTableSize), im (kTableSize);
    for (int t = 0; t < kNumTables; t++)
        for (int f = 0; f < kFrames; f++)
        {
            const auto amp = spectrum (t, f / double (kFrames - 1), 1024);
            double norm = 1;
            for (int m = 0; m < kMips; m++)
            {
                const int H = 1024 >> m;
                std::fill (re.begin(), re.end(), 0.0);
                std::fill (im.begin(), im.end(), 0.0);
                for (int n = 1; n <= H; n++) re[(size_t) n] = amp[(size_t) n];
                ifftImag (re, im);
                if (m == 0)
                {
                    double peak = 0;
                    for (int i = 0; i < kTableSize; i++) peak = std::max (peak, std::abs (im[(size_t) i]));
                    norm = peak > 1e-9 ? 1.0 / peak : 1.0;
                }
                float* tab = const_cast<float*> (table (t, f, m));
                for (int i = 0; i < kTableSize; i++) tab[i] = (float) (im[(size_t) i] * norm);
                tab[kTableSize] = tab[0];
            }
        }
}

const Wavetables& Wavetables::get()
{
    static Wavetables instance; // initialisation thread-safe (C++11)
    return instance;
}

std::vector<float> Wavetables::preview (int t, float pos, int warpMode, float warp, int points) const
{
    std::vector<float> out ((size_t) points);
    for (int i = 0; i < points; i++)
        out[(size_t) i] = (float) readTable (*this, t, pos * (kFrames - 1), 2, warpPhase ((double) i / points, warpMode, warp));
    return out;
}

//==============================================================================
double Env::tick (double a, double d, double s, double r, double sr, int n) noexcept
{
    switch (stage)
    {
        case 1:
            v += n / (a * sr);
            if (v >= 1) { v = 1; stage = 2; }
            break;
        case 2:
        {
            const double c = std::exp (-n / (d * sr * 0.25));
            v = s + (v - s) * c;
            if (v - s < 1e-4) { v = s; stage = 3; }
            break;
        }
        case 3: v = s; break;
        case 4:
        {
            const double c = std::exp (-n / (r * sr * 0.25));
            v *= c;
            if (v < 1e-4) { v = 0; stage = 0; }
            break;
        }
        default: break;
    }
    return v;
}

//==============================================================================
void Voice::start (int n, double velocity, bool retrigger, bool glide)
{
    Engine& e = *eng;
    note = n; vel = velocity; gate = true; active = true;
    targetPitch = n;
    if (! glide) pitch = n;
    if (retrigger)
    {
        env1.gate (true); env2.gate (true);
        for (int l = 0; l < 2; l++)
        {
            const bool rt = e.p[l == 0 ? PID::lfo1_retrig : PID::lfo2_retrig] > 0.5f;
            lfoPh[l] = rt ? 0.0 : e.lfoFree[l];
            lfoHeld[l] = e.rng.next();
        }
        if (! wasActive)
        {
            for (int u = 0; u < kMaxUni; u++)
            {
                phA[u] = u == 0 ? 0 : e.rng.next();
                phB[u] = u == 0 ? 0 : e.rng.next();
            }
            subPh = 0;
            fL[0].reset(); fL[1].reset(); fR[0].reset(); fR[1].reset();
        }
    }
}

void Voice::release() noexcept
{
    gate = false;
    env1.gate (false); env2.gate (false);
}

void Voice::control()
{
    Engine& e = *eng;
    const float* p = e.p;
    const double sr = e.sr;
    const int n = kControlRate;

    const double glide = p[PID::global_glide];
    if (glide > 0.0005)
    {
        const double c = std::exp (-n / (glide * sr * 0.25));
        pitch = targetPitch + (pitch - targetPitch) * c;
    }
    else pitch = targetPitch;

    const double env2v = env2.tick (p[PID::env2_a], p[PID::env2_d], p[PID::env2_s], p[PID::env2_r], sr, n);

    double lfo[2];
    for (int l = 0; l < 2; l++)
    {
        const int base = l == 0 ? PID::lfo1_shape : PID::lfo2_shape; // shape, sync, rate, div, retrig
        double rate = p[base + 1] > 0.5f ? (e.bpm / 60.0) / kDivBeats[(int) p[base + 3]] : (double) p[base + 2];
        rate *= std::pow (2.0, (double) mod[Dest::LFO1Rate + l]);
        double ph = lfoPh[l] + rate * n / sr;
        if (ph >= 1) { ph -= std::floor (ph); lfoHeld[l] = e.rng.next(); }
        lfoPh[l] = ph;
        lfo[l] = lfoValue ((int) p[base], ph, lfoHeld[l]);
    }

    std::fill (std::begin (mod), std::end (mod), 0.0f);
    double* sv = e.srcTmp;
    sv[0] = 0; sv[1] = lfo[0]; sv[2] = lfo[1]; sv[3] = env2v; sv[4] = vel;
    sv[5] = p[PID::macro1]; sv[6] = p[PID::macro2]; sv[7] = p[PID::macro3]; sv[8] = p[PID::macro4];
    sv[9] = e.modWheel; sv[10] = clampv ((note - 24) / 72.0, 0.0, 1.0);
    for (int s = 0; s < kNumModSlots; s++)
    {
        const int src = (int) p[PID::mod1_src + s * 3], dst = (int) p[PID::mod1_dst + s * 3];
        if (src <= 0 || dst <= 0 || src >= kNumModSources || dst >= kNumModDests) continue;
        mod[dst] += (float) (p[PID::mod1_amt + s * 3] * sv[src] * kModDestScale[dst]);
    }

    double cutSt = 12 * std::log2 (p[PID::filter_cutoff] / 440.0) + 69;
    cutSt += mod[Dest::Cutoff] + p[PID::filter_env] * env2v * 72 + p[PID::filter_keytrack] * (pitch - 60);
    const double fc = clampv (mtof (cutSt), 16.0, sr * 0.45);
    g = std::tan (kPi * fc / sr);
    const double res = clampv ((double) p[PID::filter_res] + mod[Dest::Resonance], 0.0, 1.0);
    k = 2 - 1.96 * res;
    c.drive = clampv ((double) p[PID::filter_drive] + mod[Dest::FilterDrive], 0.0, 1.0);

    const double bendSt = e.bend * p[PID::global_bend];
    for (int o = 0; o < 2; o++)
    {
        const int b = o == 0 ? PID::oscA_on : PID::oscB_on; // on, table, pos, semi, fine, level, unison, detune, spread, warpMode, warp
        const double pt = pitch + p[b + 3] + p[b + 4] / 100.0 + bendSt + mod[o == 0 ? Dest::OscAPitch : Dest::OscBPitch];
        const double f = mtof (pt);
        c.inc[o] = f / sr;
        const int hmax = std::max (1, (int) std::floor (0.5 * sr / f));
        int mip = 0;
        while (mip < kMips - 1 && (1024 >> mip) > hmax) mip++;
        c.mip[o] = mip;
        c.pos[o] = clampv ((double) p[b + 2] + mod[o == 0 ? Dest::OscAPos : Dest::OscBPos], 0.0, 1.0) * (kFrames - 1);
        c.warp[o] = clampv ((double) p[b + 10] + mod[o == 0 ? Dest::OscAWarp : Dest::OscBWarp], 0.0, 1.0);
        c.lvl[o] = clampv ((double) p[b + 5] + mod[o == 0 ? Dest::OscALevel : Dest::OscBLevel], 0.0, 1.0);
        c.det[o] = clampv ((double) p[b + 7] + mod[Dest::Detune], 0.0, 1.0);
    }
    c.fm = clampv ((double) p[PID::fm_amount] + mod[Dest::FMAmount], 0.0, 1.0);
    c.sub = clampv ((double) p[PID::sub_level] + mod[Dest::SubLevel], 0.0, 1.0);
    c.noise = clampv ((double) p[PID::noise_level] + mod[Dest::NoiseLevel], 0.0, 1.0);
    c.subInc = mtof (pitch + bendSt - 12 * (1 + (int) p[PID::sub_oct])) / sr;
    c.amp = clampv (1.0 + mod[Dest::Amp], 0.0, 2.0);

    ampPrev = ampCur;
    ampCur = env1.tick (p[PID::env1_a], p[PID::env1_d], p[PID::env1_s], p[PID::env1_r], sr, n);
    if (env1.stage == 0 && ampPrev < 1e-4) active = false;
}

void Voice::render (float* outL, float* outR, int len)
{
    Engine& e = *eng;
    const float* p = e.p;
    const Wavetables& wt = *e.tables;
    const bool onA = p[PID::oscA_on] > 0.5f, onB = p[PID::oscB_on] > 0.5f;
    const double fm = c.fm;
    const bool runB = onB || fm > 0;
    const int uniA = clampv ((int) p[PID::oscA_unison], 1, kMaxUni), uniB = clampv ((int) p[PID::oscB_unison], 1, kMaxUni);
    const int tA = clampv ((int) p[PID::oscA_table], 0, kNumTables - 1), tB = clampv ((int) p[PID::oscB_table], 0, kNumTables - 1);
    const int wmA = (int) p[PID::oscA_warpMode], wmB = (int) p[PID::oscB_warpMode];
    const double sprA = p[PID::oscA_spread], sprB = p[PID::oscB_spread];
    const bool subOn = p[PID::sub_on] > 0.5f;
    const int subShape = (int) p[PID::sub_shape];
    const bool fOn = p[PID::filter_on] > 0.5f;
    const int fType = (int) p[PID::filter_type];
    const double drive = c.drive, dGain = 1 + drive * 9, dNorm = 1.0 / std::tanh (dGain);
    const double gA = 1.0 / std::sqrt ((double) uniA), gB = 1.0 / std::sqrt ((double) uniB);

    double incA[kMaxUni], incB[kMaxUni], pLA[kMaxUni], pRA[kMaxUni], pLB[kMaxUni], pRB[kMaxUni];
    for (int u = 0; u < uniA; u++)
    {
        const double sp = uniA > 1 ? (2.0 * u) / (uniA - 1) - 1 : 0;
        incA[u] = c.inc[0] * std::pow (2.0, sp * c.det[0] * 50 / 1200);
        const double pan = sp * sprA;
        pLA[u] = std::sqrt (0.5 * (1 - pan)); pRA[u] = std::sqrt (0.5 * (1 + pan));
    }
    for (int u = 0; u < uniB; u++)
    {
        const double sp = uniB > 1 ? (2.0 * u) / (uniB - 1) - 1 : 0;
        incB[u] = c.inc[1] * std::pow (2.0, sp * c.det[1] * 50 / 1200);
        const double pan = sp * sprB;
        pLB[u] = std::sqrt (0.5 * (1 - pan)); pRB[u] = std::sqrt (0.5 * (1 + pan));
    }

    const double ampStep = (ampCur - ampPrev) / len;
    double amp = ampPrev;
    const double vgain = c.amp * 0.5;

    for (int i = 0; i < len; i++)
    {
        double L = 0, R = 0, modB = 0;
        if (runB)
        {
            for (int u = 0; u < uniB; u++)
            {
                const double s = readTable (wt, tB, c.pos[1], c.mip[1], warpPhase (phB[u], wmB, c.warp[1]));
                if (u == 0) modB = s;
                if (onB) { const double v = s * c.lvl[1] * gB; L += v * pLB[u]; R += v * pRB[u]; }
                const double ph = phB[u] + incB[u];
                phB[u] = ph - std::floor (ph);
            }
        }
        if (onA)
        {
            for (int u = 0; u < uniA; u++)
            {
                double ph = phA[u] + fm * 2 * modB;
                ph -= std::floor (ph);
                const double s = readTable (wt, tA, c.pos[0], c.mip[0], warpPhase (ph, wmA, c.warp[0]));
                const double v = s * c.lvl[0] * gA;
                L += v * pLA[u]; R += v * pRA[u];
                ph = phA[u] + incA[u];
                phA[u] = ph - std::floor (ph);
            }
        }
        if (subOn)
        {
            const double ph = subPh;
            double s;
            if (subShape == 0) s = std::sin (kTwoPi * ph);
            else if (subShape == 1) s = ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph;
            else s = ph < 0.5 ? 0.8 : -0.8;
            s *= c.sub * 0.7071;
            L += s; R += s;
            const double q = ph + c.subInc;
            subPh = q - std::floor (q);
        }
        if (c.noise > 0)
        {
            L += (e.rng.next() * 2 - 1) * c.noise * 0.5;
            R += (e.rng.next() * 2 - 1) * c.noise * 0.5;
        }
        if (fOn)
        {
            if (drive > 0) { L = std::tanh (L * dGain) * dNorm; R = std::tanh (R * dGain) * dNorm; }
            fL[0].run (L, g, k); fR[0].run (R, g, k);
            switch (fType)
            {
                case 0: fL[1].run (fL[0].lp, g, k); fR[1].run (fR[0].lp, g, k); L = fL[1].lp; R = fR[1].lp; break;
                case 1: L = fL[0].lp; R = fR[0].lp; break;
                case 2: L = fL[0].bp * k; R = fR[0].bp * k; break;
                case 3: L = fL[0].hp; R = fR[0].hp; break;
                default: L = fL[0].lp + fL[0].hp; R = fR[0].lp + fR[0].hp; break;
            }
        }
        amp += ampStep;
        const double a = amp * vgain;
        outL[i] += (float) (L * a);
        outR[i] += (float) (R * a);
    }
}

//==============================================================================
Engine::Engine()
{
    for (int i = 0; i < PID::COUNT; i++) p[i] = kParams[i].def;
    for (auto& v : voices) v.eng = this;
}

void Engine::prepare (double sampleRate)
{
    sr = sampleRate;
    tables = &Wavetables::get();
    delayL.init ((int) std::ceil (sr * 3));
    delayR.init ((int) std::ceil (sr * 3));
    const double sc = sr / 44100.0;
    for (int i = 0; i < 8; i++)
    {
        combL[i].b.assign ((size_t) std::lround (kComb[i] * sc), 0.0f);
        combR[i].b.assign ((size_t) std::lround ((kComb[i] + 23) * sc), 0.0f);
    }
    for (int i = 0; i < 4; i++)
    {
        apL[i].b.assign ((size_t) std::lround (kAllp[i] * sc), 0.0f);
        apR[i].b.assign ((size_t) std::lround ((kAllp[i] + 23) * sc), 0.0f);
    }
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) { v.active = false; v.gate = false; v.env1 = {}; v.env2 = {}; }
    held.clear();
    lastVoice = nullptr;
    dlyTime = sr * 0.25;
    dlyLP_L = dlyLP_R = 0;
    std::fill (delayL.buf.begin(), delayL.buf.end(), 0.0f);
    std::fill (delayR.buf.begin(), delayR.buf.end(), 0.0f);
    for (auto* arr : { combL, combR }) for (int i = 0; i < 8; i++) { std::fill (arr[i].b.begin(), arr[i].b.end(), 0.0f); arr[i].f = 0; arr[i].i = 0; }
    for (auto* arr : { apL, apR }) for (int i = 0; i < 4; i++) { std::fill (arr[i].b.begin(), arr[i].b.end(), 0.0f); arr[i].i = 0; }
    compEnv = 0; dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0;
}

void Engine::allNotesOff()
{
    held.clear();
    for (auto& v : voices) if (v.active) v.release();
}

void Engine::noteOn (int note, double vel)
{
    const int poly = clampv ((int) p[PID::global_poly], 1, kMaxVoices);
    const bool glide = p[PID::global_glide] > 0.0005f;
    if (poly <= 1)
    {
        Voice& v = voices[0];
        const bool legato = p[PID::global_legato] > 0.5f;
        const bool wasHeld = ! held.empty() && v.gate;
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        held.push_back (note);
        v.wasActive = v.active;
        if (wasHeld && legato) { v.targetPitch = note; v.note = note; if (! glide) v.pitch = note; }
        else v.start (note, vel, true, v.active && glide);
        lastVoice = &v;
        return;
    }
    Voice* pick = nullptr;
    for (int i = 0; i < poly; i++) if (! voices[(size_t) i].active) { pick = &voices[(size_t) i]; break; }
    if (! pick)
    {
        pick = &voices[0];
        for (int i = 1; i < poly; i++) if (voices[(size_t) i].age < pick->age) pick = &voices[(size_t) i];
    }
    pick->wasActive = pick->active && pick->note == note;
    pick->age = ++counter;
    pick->start (note, vel, true, false);
    lastVoice = pick;
}

void Engine::noteOff (int note)
{
    const int poly = clampv ((int) p[PID::global_poly], 1, kMaxVoices);
    if (poly <= 1)
    {
        Voice& v = voices[0];
        const bool wasTop = ! held.empty() && held.back() == note;
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (! wasTop) return;
        if (! held.empty())
        {
            const int n = held.back();
            const bool legato = p[PID::global_legato] > 0.5f;
            const bool glide = p[PID::global_glide] > 0.0005f;
            if (legato) { v.targetPitch = n; v.note = n; if (! glide) v.pitch = n; }
            else { v.wasActive = true; v.start (n, v.vel, true, glide); }
        }
        else v.release();
        return;
    }
    for (auto& v : voices) if (v.active && v.gate && v.note == note) v.release();
}

void Engine::process (float* outL, float* outR, int n)
{
    int off = 0;
    while (off < n)
    {
        const int len = std::min (kControlRate, n - off);
        std::fill (bufL, bufL + kControlRate, 0.0f);
        std::fill (bufR, bufR + kControlRate, 0.0f);
        for (int l = 0; l < 2; l++)
        {
            const int base = l == 0 ? PID::lfo1_shape : PID::lfo2_shape;
            const double rate = p[base + 1] > 0.5f ? (bpm / 60.0) / kDivBeats[(int) p[base + 3]] : (double) p[base + 2];
            const double ph = lfoFree[l] + rate * len / sr;
            lfoFree[l] = ph - std::floor (ph);
        }
        for (auto& v : voices)
        {
            if (! v.active) continue;
            v.control();
            v.render (bufL, bufR, len);
        }
        const float* lv = lastMod();
        processFx (bufL, bufR, len, lv ? lv[Dest::DistDrive] : 0.0, lv ? lv[Dest::DelayMix] : 0.0, lv ? lv[Dest::ReverbMix] : 0.0);
        const float vol = p[PID::global_volume];
        for (int i = 0; i < len; i++)
        {
            outL[off + i] = clampv (bufL[i] * vol, -1.0f, 1.0f);
            outR[off + i] = clampv (bufR[i] * vol, -1.0f, 1.0f);
        }
        off += len;
    }
}

double Engine::DelayLine::read (double d) const noexcept
{
    const int n = (int) buf.size();
    double r = w - d;
    while (r < 0) r += n;
    const int i = (int) r;
    const double u = r - i;
    const double a = buf[(size_t) i], b = buf[(size_t) ((i + 1) % n)];
    return a + (b - a) * u;
}

void Engine::reverbRun (double l, double r, double size, double damp) noexcept
{
    const double fb = 0.7 + size * 0.28, dm = damp * 0.4;
    const double x = (l + r) * 0.015;
    auto comb = [&] (Comb& c, double in) {
        const double y = c.b[(size_t) c.i];
        c.f = y * (1 - dm) + c.f * dm;
        c.b[(size_t) c.i] = (float) (in + c.f * fb);
        if (++c.i >= (int) c.b.size()) c.i = 0;
        return y;
    };
    auto ap = [] (AllP& a, double in) {
        const double y = a.b[(size_t) a.i];
        a.b[(size_t) a.i] = (float) (in + y * 0.5);
        if (++a.i >= (int) a.b.size()) a.i = 0;
        return y - in;
    };
    double ol = 0, orr = 0;
    for (int i = 0; i < 8; i++) { ol += comb (combL[i], x); orr += comb (combR[i], x); }
    for (int i = 0; i < 4; i++) { ol = ap (apL[i], ol); orr = ap (apR[i], orr); }
    revL = ol; revR = orr;
}

void Engine::processFx (float* L, float* R, int n, double mDist, double mDelay, double mRev)
{
    if (p[PID::fx_dist_on] > 0.5f)
    {
        const int type = (int) p[PID::fx_dist_type];
        const double drive = clampv (p[PID::fx_dist_drive] + mDist, 0.0, 1.0);
        const double mix = p[PID::fx_dist_mix];
        const double gain = std::pow (10.0, drive * 36 / 20);
        const double bits = 16 - drive * 13, q = std::pow (2.0, bits - 1);
        const uint64_t hold = 1 + (uint64_t) std::floor (drive * 15);
        for (int i = 0; i < n; i++)
        {
            for (int ch = 0; ch < 2; ch++)
            {
                float* buf = ch ? R : L;
                const double x = buf[i];
                double y;
                switch (type)
                {
                    case 0: y = std::tanh (x * gain); break;
                    case 1: y = clampv (x * gain, -1.0, 1.0); break;
                    case 2: y = std::sin (x * gain * 1.5707963); break;
                    case 3:
                        if (ch == 0 && crushCnt++ % hold == 0) { crushHold[0] = L[i]; crushHold[1] = R[i]; }
                        y = std::round (clampv (crushHold[ch], -1.0, 1.0) * q) / q;
                        break;
                    default: y = std::tanh (std::abs (x) * gain) * 2 - std::tanh (gain * 0.5); break;
                }
                const double dy = y - dcX[ch] + 0.995 * dcY[ch];
                dcX[ch] = y; dcY[ch] = dy;
                buf[i] = (float) (x + (dy - x) * mix);
            }
        }
    }
    if (p[PID::fx_comp_on] > 0.5f)
    {
        const double amt = p[PID::fx_comp_amount], mix = p[PID::fx_comp_mix];
        const double target = 0.25, att = std::exp (-1 / (0.001 * sr)), rel = std::exp (-1 / (0.08 * sr));
        const double maxUp = 1 + amt * 7, expo = amt * 0.85;
        for (int i = 0; i < n; i++)
        {
            const double lvl = std::max (std::abs (L[i]), std::abs (R[i]));
            const double c = lvl > compEnv ? att : rel;
            compEnv = lvl + (compEnv - lvl) * c;
            const double env = std::max (compEnv, 1e-5);
            const double g = clampv (std::pow (target / env, expo), 0.05, maxUp) * (1 + amt * 0.5);
            L[i] += (float) ((L[i] * g - L[i]) * mix);
            R[i] += (float) ((R[i] * g - R[i]) * mix);
        }
    }
    if (p[PID::fx_delay_on] > 0.5f)
    {
        const double target = std::min (sr * 2.9, (60.0 / bpm) * kDivBeats[(int) p[PID::fx_delay_div]] * sr);
        const double fb = p[PID::fx_delay_fb], mix = clampv (p[PID::fx_delay_mix] + mDelay, 0.0, 1.0);
        const bool pp = p[PID::fx_delay_pingpong] > 0.5f;
        const double lpc = std::exp (-2 * kPi * 6000 / sr);
        for (int i = 0; i < n; i++)
        {
            dlyTime += (target - dlyTime) * 0.0005;
            double dl = delayL.read (dlyTime), dr = delayR.read (dlyTime);
            dlyLP_L = dl + (dlyLP_L - dl) * lpc; dl = dlyLP_L;
            dlyLP_R = dr + (dlyLP_R - dr) * lpc; dr = dlyLP_R;
            if (pp) { delayL.write ((L[i] + R[i]) * 0.5 + dr * fb); delayR.write (dl * fb); }
            else { delayL.write (L[i] + dl * fb); delayR.write (R[i] + dr * fb); }
            L[i] += (float) (dl * mix); R[i] += (float) (dr * mix);
        }
    }
    if (p[PID::fx_reverb_on] > 0.5f)
    {
        const double size = p[PID::fx_reverb_size], damp = p[PID::fx_reverb_damp];
        const double mix = clampv (p[PID::fx_reverb_mix] + mRev, 0.0, 1.0);
        for (int i = 0; i < n; i++)
        {
            reverbRun (L[i], R[i], size, damp);
            L[i] = (float) (L[i] * (1 - mix * 0.5) + revL * mix);
            R[i] = (float) (R[i] * (1 - mix * 0.5) + revR * mix);
        }
    }
}
} // namespace masala
