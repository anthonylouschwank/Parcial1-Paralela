#pragma once
// ============================================================================
//  grafo.hpp — Red social en formato CSR + generador sintético libre de escala
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <vector>
#include <random>

struct Grafo {
    int64_t n = 0;                 // usuarios (nodos)
    int64_t m = 0;                 // amistades (aristas no dirigidas)
    std::vector<int64_t> offsets;  // tamano n+1
    std::vector<int32_t> ady;      // tamano 2*m (cada amistad aparece 2 veces)

    inline const int32_t* vecinos(int64_t u) const { return ady.data() + offsets[u]; }
    inline int64_t grado(int64_t u) const { return offsets[u + 1] - offsets[u]; }
};

// ---------------------------------------------------------------------------
//  Generador: modelo de conexion preferente (Barabasi-Albert).
//
//  Cada usuario nuevo elige `grado_m` amigos con probabilidad proporcional al
//  numero de amigos que ya tienen ("el rico se hace mas rico"). El resultado es
//  una distribucion de grados de ley de potencias: es justo el fenomeno que
//  describe el enunciado -- casi todos con un punado de amigos y unos pocos
//  hubs con decenas de miles.
//
// ---------------------------------------------------------------------------
inline Grafo generar_red_social(int64_t n, int grado_m, uint64_t semilla) {
    if (grado_m < 1) grado_m = 1;
    const int64_t n0 = grado_m + 1;   // clique inicial que arranca el proceso
    if (n < n0) n = n0;

    std::mt19937_64 rng(semilla);

    std::vector<int32_t> pool;
    const int64_t aristas_est = n0 * (n0 - 1) / 2 + (n - n0) * grado_m;
    pool.reserve(2 * (size_t)aristas_est);

    // Nucleo inicial: clique completo sobre los primeros n0 usuarios.
    for (int64_t u = 0; u < n0; ++u)
        for (int64_t v = u + 1; v < n0; ++v) {
            pool.push_back((int32_t)u);
            pool.push_back((int32_t)v);
        }

    // Llegada de los usuarios restantes, uno a uno.
    std::vector<int32_t> destinos(grado_m);
    for (int64_t v = n0; v < n; ++v) {
        // Foto del pool ANTES de agregar a v: si no, las aristas que v va
        // creando sesgarian su propia eleccion de amigos.
        const size_t tam = pool.size();
        int elegidos = 0;
        while (elegidos < grado_m) {
            int32_t w = pool[rng() % tam];
            bool repetido = false;
            for (int i = 0; i < elegidos; ++i)
                if (destinos[i] == w) { repetido = true; break; }
            if (!repetido) destinos[elegidos++] = w;   // sin amistades duplicadas
        }
        for (int i = 0; i < grado_m; ++i) {
            pool.push_back((int32_t)v);
            pool.push_back(destinos[i]);
        }
    }

    // ---- Construccion del CSR a partir de la lista de aristas ----
    Grafo g;
    g.n = n;
    g.m = (int64_t)pool.size() / 2;

    // Contar grados: cada nodo aparece en `pool` una vez por cada arista suya.
    g.offsets.assign(n + 1, 0);
    for (int32_t x : pool) g.offsets[x + 1]++;
    for (int64_t u = 0; u < n; ++u) g.offsets[u + 1] += g.offsets[u];

    // Volcar cada arista en las dos direcciones.
    g.ady.resize(pool.size());
    std::vector<int64_t> cursor(g.offsets.begin(), g.offsets.end() - 1);
    for (size_t k = 0; k < pool.size(); k += 2) {
        int32_t u = pool[k], v = pool[k + 1];
        g.ady[cursor[u]++] = v;
        g.ady[cursor[v]++] = u;
    }
    std::vector<int32_t>().swap(pool);   // liberar: era el pico de memoria

    // Ordenar cada lista de amigos. No lo exige el algoritmo, pero mejora la
    // localidad al recorrerla y hace que el recorrido sea reproducible.
    for (int64_t u = 0; u < n; ++u)
        std::sort(g.ady.begin() + g.offsets[u], g.ady.begin() + g.offsets[u + 1]);

    return g;
}

// ---------------------------------------------------------------------------
//  Estadisticas de grado: son la EVIDENCIA del desbalance de carga que
//  motiva todo el trabajo de scheduling en la version paralela.
// ---------------------------------------------------------------------------
struct EstadisticasGrado {
    int64_t minimo = 0, maximo = 0, mediana = 0, p99 = 0;
    double promedio = 0.0;
    double pct_aristas_top1 = 0.0;   // % de aristas concentradas en el 1% mas conectado
};

inline EstadisticasGrado estadisticas_grado(const Grafo& g) {
    std::vector<int64_t> d((size_t)g.n);
    for (int64_t u = 0; u < g.n; ++u) d[(size_t)u] = g.grado(u);
    std::sort(d.begin(), d.end());

    EstadisticasGrado e;
    e.minimo   = d.front();
    e.maximo   = d.back();
    e.mediana  = d[(size_t)(g.n / 2)];
    e.p99      = d[(size_t)((double)g.n * 0.99)];
    e.promedio = (double)(2 * g.m) / (double)g.n;

    const int64_t top = std::max<int64_t>(1, g.n / 100);
    int64_t suma = 0;
    for (int64_t i = g.n - top; i < g.n; ++i) suma += d[(size_t)i];
    e.pct_aristas_top1 = 100.0 * (double)suma / (double)(2 * g.m);
    return e;
}
