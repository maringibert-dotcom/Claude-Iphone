// Rend un preset avec le moteur C++ du plugin. Entrée (stdin) :
//   ligne 1 : sampleRate nSamples
//   ligne 2 : PID::COUNT valeurs de paramètres
//   puis    : événements "t on|off note vel" (t en échantillons, triés)
// Sortie : échantillons gauche/droite en binaire float32 sur stdout.
#include "../plugin/Source/dsp/Engine.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
int main()
{
    double sr; long n;
    std::cin >> sr >> n;
    masala::Engine e;
    for (int i = 0; i < masala::PID::COUNT; i++) std::cin >> e.p[i];
    e.prepare (sr);
    e.seed (1234);
    struct Ev { long t; bool on; int note; double vel; };
    std::vector<Ev> evs;
    long t; std::string w; int note; double vel;
    while (std::cin >> t >> w >> note >> vel) evs.push_back ({ t, w == "on", note, vel });
    std::vector<float> L ((size_t) n), R ((size_t) n);
    size_t k = 0;
    for (long off = 0; off < n; off += 128)
    {
        while (k < evs.size() && evs[k].t <= off) { evs[k].on ? e.noteOn (evs[k].note, evs[k].vel) : e.noteOff (evs[k].note); k++; }
        const int len = (int) std::min<long> (128, n - off);
        e.process (L.data() + off, R.data() + off, len);
    }
    for (long i = 0; i < n; i++) { fwrite (&L[(size_t) i], 4, 1, stdout); fwrite (&R[(size_t) i], 4, 1, stdout); }
}
