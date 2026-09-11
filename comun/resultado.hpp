#pragma once
// ============================================================================
//  resultado.hpp — Resultado de un BFS, reconstruccion del camino e informes.
// ============================================================================
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>
#include "grafo.hpp"

// Resultado de una corrida de BFS. `padre` permite reconstruir el camino.
struct ResultadoBFS {
    int64_t distancia = -1;        // -1 => destino inalcanzable
    int64_t nodos_visitados = 0;
    int64_t aristas_recorridas = 0;  // trabajo real hecho (metrica de carga)
    int niveles = 0;
    std::vector<int32_t> padre;    // padre[v] = quien descubrio a v, -1 si ninguno
};

// Camino minimo origen -> destino, deshaciendo la cadena de padres.
inline std::vector<int32_t> reconstruir_camino(const std::vector<int32_t>& padre,
                                               int64_t origen, int64_t destino) {
    std::vector<int32_t> camino;
    if (destino < 0 || (size_t)destino >= padre.size()) return camino;
    if (padre[(size_t)destino] < 0 && destino != origen) return camino;  // inalcanzable

    int64_t v = destino;
    while (v != origen) {
        camino.push_back((int32_t)v);
        v = padre[(size_t)v];
        if (v < 0) return {};                       // cadena rota
        if (camino.size() > padre.size()) return {};  // salvaguarda anti-ciclo
    }
    camino.push_back((int32_t)origen);
    std::reverse(camino.begin(), camino.end());
    return camino;
}

// Verifica que el camino sea real: cada salto consecutivo debe ser una amistad
// existente. Es la red de seguridad contra un bug de sincronizacion en la
// version paralela, donde un CAS mal hecho produciria padres inconsistentes.
inline bool camino_valido(const Grafo& g, const std::vector<int32_t>& camino) {
    if (camino.size() < 2) return camino.size() == 1;
    for (size_t i = 0; i + 1 < camino.size(); ++i) {
        const int32_t u = camino[i], w = camino[i + 1];
        const int32_t* ini = g.vecinos(u);
        const int32_t* fin = ini + g.grado(u);
        if (!std::binary_search(ini, fin, w)) return false;  // ady esta ordenada
    }
    return true;
}

inline void imprimir_camino(const std::vector<int32_t>& camino, size_t maximo = 12) {
    if (camino.empty()) { printf("    (sin camino)\n"); return; }
    printf("    ");
    if (camino.size() <= maximo) {
        for (size_t i = 0; i < camino.size(); ++i)
            printf("%d%s", camino[i], i + 1 < camino.size() ? " -> " : "\n");
        return;
    }
    for (size_t i = 0; i < maximo / 2; ++i) printf("%d -> ", camino[i]);
    printf("... (%zu mas) ... ", camino.size() - maximo);
    for (size_t i = camino.size() - maximo / 2; i < camino.size(); ++i)
        printf("%d%s", camino[i], i + 1 < camino.size() ? " -> " : "\n");
}

inline std::string nombre_maquina() {
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf) - 1) != 0) return "desconocida";
    return std::string(buf);
}

inline void imprimir_estadisticas_grado(const EstadisticasGrado& e) {
    printf("  Distribucion de amistades por usuario:\n");
    printf("    minimo %ld | mediana %ld | promedio %.1f | p99 %ld | MAXIMO %ld\n",
           (long)e.minimo, (long)e.mediana, e.promedio, (long)e.p99, (long)e.maximo);
    printf("    el 1%% de usuarios mas conectados concentra el %.1f%% de las amistades\n",
           e.pct_aristas_top1);
    printf("    -> razon max/mediana = %.0fx  (este es el desbalance a vencer)\n",
           e.mediana > 0 ? (double)e.maximo / (double)e.mediana : 0.0);
}

// Una fila por repeticion. Alimenta las graficas de speedup y eficiencia.
inline void escribir_csv(const std::string& ruta, const std::string& implementacion,
                         const std::string& etiqueta, const Grafo& g, int hilos,
                         const std::string& scheduling, int chunk, int repeticion,
                         double tiempo_ms, const ResultadoBFS& r) {
    if (ruta.empty()) return;
    const bool nuevo = (access(ruta.c_str(), F_OK) != 0);
    FILE* f = fopen(ruta.c_str(), "a");
    if (!f) { fprintf(stderr, "AVISO: no se pudo abrir %s\n", ruta.c_str()); return; }
    if (nuevo)
        fprintf(f, "implementacion,etiqueta,maquina,nodos,aristas,hilos,scheduling,"
                   "chunk,repeticion,tiempo_ms,distancia,nodos_visitados,aristas_recorridas\n");
    fprintf(f, "%s,%s,%s,%ld,%ld,%d,%s,%d,%d,%.4f,%ld,%ld,%ld\n",
            implementacion.c_str(), etiqueta.c_str(), nombre_maquina().c_str(),
            (long)g.n, (long)g.m, hilos, scheduling.c_str(), chunk, repeticion,
            tiempo_ms, (long)r.distancia, (long)r.nodos_visitados,
            (long)r.aristas_recorridas);
    fclose(f);
}
