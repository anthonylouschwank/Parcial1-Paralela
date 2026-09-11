#pragma once
// ============================================================================
//  opciones.hpp — Argumentos de linea de comandos, compartidos por ambas
//  implementaciones para que los experimentos se lancen igual en las dos.
// ============================================================================
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct Opciones {
    int64_t     nodos        = 2000000;   // usuarios de la red
    int         grado        = 8;         // amistades que trae cada usuario nuevo
    uint64_t    semilla      = 314;       // reproducibilidad (guino a la consultora)
    int64_t     origen       = -1;        // -1 => n-1 (usuario "normal", poco conectado)
    int64_t     destino      = -1;        // -1 => n-2
    bool        bfs_completo = false;     // --destino todos : recorrer la red entera
    int         repeticiones = 5;
    int         hilos        = 0;         // 0 => lo que decida OpenMP (solo paralelo)
    std::string scheduling   = "dynamic"; // static | dynamic | guided (solo paralelo)
    int         chunk        = 64;
    std::string csv;                      // ruta donde acumular mediciones
    std::string etiqueta     = "base";    // nombre del experimento en el CSV
    bool        solo_frontera = false;    // secuencial: omitir el BFS de cola clasico
};

inline void imprimir_ayuda(const char* prog, bool paralelo) {
    printf("Uso: %s [opciones]\n\n", prog);
    printf("  --nodos N          usuarios en la red        (def. 2000000)\n");
    printf("  --grado M          amistades por usuario nuevo (def. 8)\n");
    printf("  --semilla S        semilla del generador     (def. 314)\n");
    printf("  --origen X         usuario de partida        (def. n-1)\n");
    printf("  --destino Y        usuario a alcanzar        (def. n-2)\n");
    printf("  --destino todos    recorrer la red completa (sin corte anticipado)\n");
    printf("  --repeticiones R   corridas cronometradas    (def. 5)\n");
    if (paralelo) {
        printf("  --hilos T          hilos OpenMP            (def. omp_get_max_threads)\n");
        printf("  --scheduling S     static | dynamic | guided (def. dynamic)\n");
        printf("  --chunk C          tamano de bloque        (def. 64)\n");
    } else {
        printf("  --solo-frontera    omitir el BFS de cola clasico\n");
    }
    printf("  --csv ARCHIVO      anexar las mediciones a ARCHIVO\n");
    printf("  --etiqueta NOMBRE  nombre del experimento en el CSV (def. base)\n");
    printf("  --ayuda            mostrar esta ayuda\n");
}

inline Opciones parsear_opciones(int argc, char** argv, bool paralelo) {
    Opciones o;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto siguiente = [&](const char* nombre) -> const char* {
            if (i + 1 >= argc) {
                fprintf(stderr, "ERROR: %s necesita un valor\n", nombre);
                exit(1);
            }
            return argv[++i];
        };
        if      (!strcmp(a, "--nodos"))        o.nodos = atoll(siguiente(a));
        else if (!strcmp(a, "--grado"))        o.grado = atoi(siguiente(a));
        else if (!strcmp(a, "--semilla"))      o.semilla = strtoull(siguiente(a), nullptr, 10);
        else if (!strcmp(a, "--origen"))       o.origen = atoll(siguiente(a));
        else if (!strcmp(a, "--destino")) {
            const char* v = siguiente(a);
            if (!strcmp(v, "todos")) o.bfs_completo = true;
            else                     o.destino = atoll(v);
        }
        else if (!strcmp(a, "--repeticiones")) o.repeticiones = atoi(siguiente(a));
        else if (!strcmp(a, "--hilos"))        o.hilos = atoi(siguiente(a));
        else if (!strcmp(a, "--scheduling"))   o.scheduling = siguiente(a);
        else if (!strcmp(a, "--chunk"))        o.chunk = atoi(siguiente(a));
        else if (!strcmp(a, "--csv"))          o.csv = siguiente(a);
        else if (!strcmp(a, "--etiqueta"))     o.etiqueta = siguiente(a);
        else if (!strcmp(a, "--solo-frontera")) o.solo_frontera = true;
        else if (!strcmp(a, "--ayuda") || !strcmp(a, "-h")) {
            imprimir_ayuda(argv[0], paralelo);
            exit(0);
        }
        else {
            fprintf(stderr, "ERROR: opcion desconocida '%s'\n\n", a);
            imprimir_ayuda(argv[0], paralelo);
            exit(1);
        }
    }
    if (o.repeticiones < 1) o.repeticiones = 1;
    return o;
}

// Los nodos por defecto dependen de n, asi que se fijan tras generar el grafo.
// Se eligen los DOS ULTIMOS usuarios en llegar: en un modelo de conexion
// preferente son los menos conectados, o sea el caso realista de "dos usuarios
// cualesquiera", no dos celebridades que se alcanzan en un salto.
inline void resolver_extremos(Opciones& o, int64_t n) {
    if (o.origen  < 0) o.origen  = n - 1;
    if (o.destino < 0) o.destino = n - 2;
    if (o.origen  >= n) o.origen  = n - 1;
    if (o.destino >= n) o.destino = n - 2;
}
