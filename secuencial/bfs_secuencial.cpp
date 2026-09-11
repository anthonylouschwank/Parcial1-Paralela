#include <algorithm>
#include <cstdio>
#include <numeric>
#include <queue>
#include <vector>

#include "cronometro.hpp"
#include "grafo.hpp"
#include "opciones.hpp"
#include "resultado.hpp"

// ---------------------------------------------------------------------------
//  Variante 1: BFS clasico con una cola FIFO.
//  Corta en cuanto DESCUBRE el destino: en un
//  grafo sin pesos, la primera vez que se llega a un nodo ya es por el camino
//  mas corto.
// ---------------------------------------------------------------------------
static ResultadoBFS bfs_cola(const Grafo& g, int64_t origen, int64_t destino,
                             bool completo) {
    ResultadoBFS r;
    r.padre.assign((size_t)g.n, -1);
    std::vector<int32_t> dist((size_t)g.n, -1);

    dist[(size_t)origen] = 0;
    r.nodos_visitados = 1;
    if (!completo && origen == destino) { r.distancia = 0; return r; }

    std::queue<int32_t> cola;
    cola.push((int32_t)origen);

    while (!cola.empty()) {
        const int32_t u = cola.front();
        cola.pop();
        const int32_t du = dist[(size_t)u];
        if (du > r.niveles) r.niveles = du;

        const int32_t* ady = g.vecinos(u);
        const int64_t  gr  = g.grado(u);
        r.aristas_recorridas += gr;

        for (int64_t i = 0; i < gr; ++i) {
            const int32_t w = ady[i];
            if (dist[(size_t)w] < 0) {
                dist[(size_t)w]    = du + 1;
                r.padre[(size_t)w] = u;
                ++r.nodos_visitados;
                if (!completo && w == destino) {   // corte anticipado
                    r.distancia = du + 1;
                    r.niveles   = du + 1;
                    return r;
                }
                cola.push(w);
            }
        }
    }
    if (!completo) r.distancia = dist[(size_t)destino];
    return r;
}

// ---------------------------------------------------------------------------
//  Variante 2: BFS sincronico por niveles.
//
//  La cola unica se parte en dos vectores: la frontera del nivel actual y la
//  que se esta construyendo. Es una reescritura equivalente -- procesa los
//  mismos nodos en el mismo orden de distancia -- pero expone el paralelismo:
//  todos los nodos de un mismo nivel son independientes entre si.
//
//  El corte anticipado se evalua AL CERRAR el nivel, no al descubrir el nodo.
//  Es deliberado: es exactamente lo que puede hacer la version paralela, y
//  mantenerlo aqui hace que ambas ejecuten la misma cantidad de trabajo.
//
//  DETALLE DE MEMORIA (importante, y medido):
//  La marca de visitado es un uint8_t por usuario, no un int32_t. Para 2M de
//  usuarios eso son 2 MB en vez de 8 MB. El acceso a este arreglo es ALEATORIO
//  -- un salto por cada amistad recorrida -- asi que lo que decide el tiempo es
//  si cabe o no en la cache L3 (6 MB en esta maquina). Con 8 MB no cabia y cada
//  lectura iba a RAM. No hace falta guardar la distancia de cada usuario: el
//  nivel actual ya la conoce, y para el camino basta con padre[].
// ---------------------------------------------------------------------------
static ResultadoBFS bfs_frontera(const Grafo& g, int64_t origen, int64_t destino,
                                 bool completo) {
    ResultadoBFS r;
    r.padre.assign((size_t)g.n, -1);
    std::vector<uint8_t> visitado((size_t)g.n, 0);

    visitado[(size_t)origen] = 1;
    r.nodos_visitados = 1;
    if (!completo && origen == destino) { r.distancia = 0; return r; }

    std::vector<int32_t> frontera, siguiente;
    frontera.push_back((int32_t)origen);

    int nivel = 0;
    while (!frontera.empty()) {
        siguiente.clear();

        for (const int32_t u : frontera) {
            const int32_t* ady = g.vecinos(u);
            const int64_t  gr  = g.grado(u);
            r.aristas_recorridas += gr;
            for (int64_t i = 0; i < gr; ++i) {
                const int32_t w = ady[i];
                if (!visitado[(size_t)w]) {
                    visitado[(size_t)w] = 1;
                    r.padre[(size_t)w]  = u;
                    siguiente.push_back(w);
                }
            }
        }

        ++nivel;
        r.nodos_visitados += (int64_t)siguiente.size();
        r.niveles = nivel;

        // Todo lo descubierto en este barrido esta a distancia `nivel`.
        if (!completo && visitado[(size_t)destino]) {   // corte al cerrar el nivel
            r.distancia = nivel;
            return r;
        }
        frontera.swap(siguiente);
    }
    return r;   // destino inalcanzable: distancia queda en -1
}

struct Resumen { double minimo, mediana, promedio, maximo; };

static Resumen resumir(std::vector<double> t) {
    std::sort(t.begin(), t.end());
    Resumen s;
    s.minimo   = t.front();
    s.maximo   = t.back();
    s.mediana  = t[t.size() / 2];
    s.promedio = std::accumulate(t.begin(), t.end(), 0.0) / (double)t.size();
    return s;
}

static void imprimir_tiempos(const char* nombre, const Resumen& s) {
    printf("  %-24s mediana %8.2f ms   (min %8.2f | prom %8.2f | max %8.2f)\n",
           nombre, s.mediana, s.minimo, s.promedio, s.maximo);
}

int main(int argc, char** argv) {
    Opciones o = parsear_opciones(argc, argv, /*paralelo=*/false);

    printf("\n=====================================================================\n");
    printf(" Tres Punto Uno Cuatro  |  BFS SECUENCIAL (1 hilo, sin OpenMP)\n");
    printf("=====================================================================\n");

    // --- Construccion de la red (queda FUERA del cronometro del BFS) ---
    printf("\n[1] Generando la red social...\n");
    Cronometro c;
    Grafo g = generar_red_social(o.nodos, o.grado, o.semilla);
    const double t_gen = c.ms();

    resolver_extremos(o, g.n);

    printf("  usuarios (nodos)   : %ld\n", (long)g.n);
    printf("  amistades (aristas): %ld\n", (long)g.m);
    printf("  memoria del CSR    : %.1f MB\n",
           (double)(g.ady.size() * sizeof(int32_t) +
                    g.offsets.size() * sizeof(int64_t)) / (1024.0 * 1024.0));
    printf("  semilla            : %llu   (grafo reproducible)\n",
           (unsigned long long)o.semilla);
    printf("  tiempo de armado   : %.0f ms\n", t_gen);
    imprimir_estadisticas_grado(estadisticas_grado(g));

    // --- Busqueda ---
    printf("\n[2] Buscando la ruta minima  usuario %ld  ->  usuario %ld%s\n",
           (long)o.origen, (long)o.destino,
           o.bfs_completo ? "   [modo: red completa]" : "");
    printf("  amigos del origen: %ld | amigos del destino: %ld\n",
           (long)g.grado(o.origen), (long)g.grado(o.destino));

    // Corrida de calentamiento: la primera toca por primera vez dist[] y
    // padre[], y pagaria fallos de pagina que no son culpa del algoritmo.
    bfs_frontera(g, o.origen, o.destino, o.bfs_completo);

    printf("\n[3] Cronometrando %d repeticiones...\n", o.repeticiones);
    std::vector<double> t_frontera;
    ResultadoBFS r_frontera;
    for (int i = 0; i < o.repeticiones; ++i) {
        c.reiniciar();
        r_frontera = bfs_frontera(g, o.origen, o.destino, o.bfs_completo);
        const double ms = c.ms();
        t_frontera.push_back(ms);
        escribir_csv(o.csv, "secuencial", o.etiqueta, g, 1, "n/a", 0, i + 1, ms, r_frontera);
        printf("    rep %d/%d : %8.2f ms\n", i + 1, o.repeticiones, ms);
    }

    std::vector<double> t_cola;
    ResultadoBFS r_cola;
    if (!o.solo_frontera) {
        bfs_cola(g, o.origen, o.destino, o.bfs_completo);   // calentamiento
        for (int i = 0; i < o.repeticiones; ++i) {
            c.reiniciar();
            r_cola = bfs_cola(g, o.origen, o.destino, o.bfs_completo);
            const double ms = c.ms();
            t_cola.push_back(ms);
            // Tambien va al CSV: es el "mejor secuencial" honesto contra el que
            // se calcula el speedup real, no solo el algoritmico.
            escribir_csv(o.csv, "secuencial_cola", o.etiqueta, g, 1, "n/a", 0,
                         i + 1, ms, r_cola);
        }
    }

    // --- Resultados ---
    printf("\n[4] Resultado de la busqueda\n");
    if (r_frontera.distancia >= 0 || o.bfs_completo) {
        if (!o.bfs_completo) {
            printf("  grados de separacion : %ld\n", (long)r_frontera.distancia);
            std::vector<int32_t> camino =
                reconstruir_camino(r_frontera.padre, o.origen, o.destino);
            printf("  camino:\n");
            imprimir_camino(camino);
            printf("  verificacion: %s\n",
                   camino_valido(g, camino)
                       ? "VALIDO (cada salto es una amistad real del grafo)"
                       : "*** INVALIDO ***");
        }
        printf("  niveles explorados   : %d\n", r_frontera.niveles);
        printf("  usuarios visitados   : %ld  (%.1f%% de la red)\n",
               (long)r_frontera.nodos_visitados,
               100.0 * (double)r_frontera.nodos_visitados / (double)g.n);
        printf("  amistades recorridas : %ld   <- este es el trabajo a repartir\n",
               (long)r_frontera.aristas_recorridas);
    } else {
        printf("  los usuarios NO estan conectados\n");
    }

    printf("\n[5] Tiempos\n");
    const Resumen s_frontera = resumir(t_frontera);
    imprimir_tiempos("BFS por frontera", s_frontera);
    if (!o.solo_frontera) {
        const Resumen s_cola = resumir(t_cola);
        imprimir_tiempos("BFS de cola (clasico)", s_cola);

        printf("\n  Coherencia entre variantes: ");
        if (o.bfs_completo)
            printf("%ld vs %ld usuarios visitados  ->  %s\n",
                   (long)r_frontera.nodos_visitados, (long)r_cola.nodos_visitados,
                   r_frontera.nodos_visitados == r_cola.nodos_visitados
                       ? "COINCIDEN" : "*** DIFIEREN ***");
        else
            printf("distancia %ld vs %ld  ->  %s\n",
                   (long)r_frontera.distancia, (long)r_cola.distancia,
                   r_frontera.distancia == r_cola.distancia ? "COINCIDEN"
                                                            : "*** DIFIEREN ***");

        // Si la cola clasica gana por mucho es porque puede abandonar a media
        // capa al descubrir el destino, mientras que la version por frontera
        // debe cerrar el nivel. Se deja explicito para no inflar el speedup.
        const double brecha = s_frontera.mediana / s_cola.mediana;
        if (brecha > 1.15)
            printf("  AVISO: la cola clasica es %.1fx mas rapida aqui (corta a media\n"
                   "         capa). El speedup contra la frontera es ALGORITMICO;\n"
                   "         el speedup REAL debe medirse contra %.2f ms.\n",
                   brecha, s_cola.mediana);
    }
    printf("\n  >> Baseline algoritmica (frontera, 1 hilo): %.2f ms\n",
           s_frontera.mediana);
    if (!o.solo_frontera)
        printf("  >> Mejor secuencial observado            : %.2f ms\n",
               std::min(s_frontera.mediana, resumir(t_cola).mediana));
    if (!o.csv.empty()) printf("  >> Mediciones anexadas a: %s\n", o.csv.c_str());
    printf("\n");

    return 0;
}
