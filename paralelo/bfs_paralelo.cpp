// ============================================================================
//  bfs_paralelo.cpp — BFS por frontera paralelizado con OpenMP
//
//  Consultora HPC "Tres Punto Uno Cuatro"
//  Problema 4: ruta minima de conexiones entre dos usuarios de una red social.
//
//  ---------------------------------------------------------------------------
//  LA IDEA
//
//  Una cola FIFO unica no se puede paralelizar: seria un cuello de botella con
//  todos los hilos peleando por el mismo pop(). La reestructuramos en niveles.
//
//      nivel k  ->  "frontera": todos los usuarios a distancia exactamente k
//
//  Dentro de un nivel, los usuarios son INDEPENDIENTES entre si: expandir los
//  amigos de uno no cambia lo que hay que hacer con otro. Ese es el paralelismo
//  que explotamos. La sincronizacion se reduce a unas barreras por nivel, y en
//  esta red hay solo 5-7 niveles.
//
//  ---------------------------------------------------------------------------
//  LOS TRES PROBLEMAS Y COMO SE RESUELVEN
//
//  (1) DESBALANCE DE CARGA
//      Un usuario tiene 8 amigos y otro tiene 6414. Con reparto estatico, el
//      hilo que agarre el bloque con los hubs trabaja mientras los demas
//      esperan en la barrera.
//      -> schedule(dynamic): los hilos toman bloques de la frontera conforme se
//         desocupan. El que agarra un hub tarda, pero los otros siguen tomando
//         trabajo en vez de esperarlo.
//
//  (2) CONDICION DE CARRERA AL MARCAR VISITADOS
//      Dos hilos pueden descubrir al mismo usuario w en el mismo instante. Si
//      ambos leen visitado[w] == 0 y ambos lo encolan, w entra DUPLICADO en la
//      frontera: trabajo repetido y, peor, padre[w] queda indefinido.
//      -> compare-and-swap atomico sobre visitado[w]. Exactamente un hilo logra
//         la transicion 0 -> 1; ese es el unico que encola y escribe padre.
//         Los demas ven que fallo y siguen.
//
//  (3) CONTENCION AL CONSTRUIR LA FRONTERA SIGUIENTE
//      Si todos los hilos hicieran push_back sobre un vector compartido (o peor,
//      dentro de un #pragma omp critical), la insercion se serializaria y
//      mataria el paralelismo: son millones de inserciones por nivel.
//      -> cada hilo acumula en su propio buffer local, sin sincronizacion
//         alguna. Al cerrar el nivel se calculan los desplazamientos y cada
//         hilo copia su bloque en paralelo a la region que le toca.
//
//  ---------------------------------------------------------------------------
//  DOS DECISIONES QUE SALIERON DE MEDIR, NO DE SUPONER
//
//  (a) UNA SOLA region paralela para todo el BFS, no una por nivel.
//      La primera version abria y cerraba un #pragma omp parallel en cada
//      nivel. Los niveles 0 a 2 mueven apenas unos miles de aristas, pero
//      costaban 20 ms con 8 hilos contra 0.45 ms con 1: era puro costo de
//      arrancar y dormir el equipo de hilos. Ahora el equipo se crea una vez y
//      los niveles se separan con barreras, que son ordenes de magnitud mas
//      baratas.
//
//  (b) La marca de visitado es uint8_t, no int32_t.
//      El acceso a ese arreglo es ALEATORIO: un salto por cada amistad
//      recorrida. Con int32_t son 8 MB para 2M de usuarios, que no caben en los
//      6 MB de L3 de esta maquina, asi que practicamente cada lectura iba a RAM
//      y cada escritura invalidaba una linea en los otros nucleos. Con uint8_t
//      son 2 MB y si caben. No se pierde nada: la distancia de cada usuario la
//      sabe el nivel actual, y para reconstruir el camino basta con padre[].
// ============================================================================
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <vector>

#include <omp.h>

#include "cronometro.hpp"
#include "grafo.hpp"
#include "opciones.hpp"
#include "resultado.hpp"

// ---------------------------------------------------------------------------
//  Contador alineado a linea de cache (64 B).
//
//  Sin el alignas, los contadores de los 8 hilos caerian en la misma linea de
//  cache y cada incremento invalidaria la copia de los otros 7 nucleos: falso
//  compartimiento. No cambia el resultado, pero si el tiempo -- y aqui estos
//  contadores son instrumentacion, no deben distorsionar lo que miden.
// ---------------------------------------------------------------------------
struct alignas(64) ContadorPad {
    int64_t v = 0;
};

// Radiografia de un nivel: sirve para demostrar con numeros que el scheduling
// elegido realmente reparte la carga.
struct MetricasNivel {
    int64_t nodos_frontera   = 0;
    int64_t aristas          = 0;
    int64_t aristas_max_hilo = 0;   // el hilo mas cargado del nivel
    int64_t aristas_min_hilo = 0;   // el menos cargado
    double  ms               = 0.0;
    double  desbalance       = 1.0; // max / promedio. 1.0 = reparto perfecto
};

// ---------------------------------------------------------------------------
//  BFS por frontera, paralelo.
//
//  El scheduling NO se fija aqui con schedule(static|dynamic|guided) sino con
//  schedule(runtime), y se elige desde main con omp_set_schedule(). Asi las
//  tres politicas recorren EXACTAMENTE el mismo codigo maquina y la comparacion
//  entre ellas mide la politica, no diferencias de compilacion.
// ---------------------------------------------------------------------------
static ResultadoBFS bfs_paralelo(const Grafo& g, int64_t origen, int64_t destino,
                                 bool completo,
                                 std::vector<MetricasNivel>* detalle,
                                 double* desbalance_global) {
    const int64_t n = g.n;

    ResultadoBFS r;
    r.padre.assign((size_t)n, -1);
    std::vector<uint8_t> visitado((size_t)n, 0);

    visitado[(size_t)origen] = 1;
    r.nodos_visitados = 1;
    if (!completo && origen == destino) { r.distancia = 0; return r; }

    // Las dos fronteras se reservan UNA vez con capacidad n y se maneja el
    // tamano a mano. Asi ningun nivel paga una reasignacion ni el borrado de
    // memoria que haria resize() en cada iteracion.
    std::vector<int32_t> frontera((size_t)n), siguiente((size_t)n);
    int64_t tam_f = 1, tam_s = 0;
    frontera[0] = (int32_t)origen;

    const int T = omp_get_max_threads();
    std::vector<std::vector<int32_t>> locales((size_t)T);
    for (auto& v : locales) v.reserve(4096);
    std::vector<ContadorPad> aristas_hilo((size_t)T);
    std::vector<int64_t> inicio((size_t)T + 1, 0);

    int nivel = 0;
    int64_t suma_max = 0, suma_total = 0;   // para el desbalance global
    bool continuar = true;
    Cronometro cn;

    // ====== UNA SOLA region paralela para todos los niveles (ver nota (a)) ======
    #pragma omp parallel
    {
        const int tid = omp_get_thread_num();
        std::vector<int32_t>& local = locales[(size_t)tid];

        // Todos los hilos evaluan `continuar` justo despues de una barrera, asi
        // que siempre coinciden: nunca uno sale del bucle y otro se queda
        // esperando en una barrera huerfana.
        while (continuar) {
            local.clear();
            int64_t mis_aristas = 0;

            // ---- EL REPARTO DEL TRABAJO ----
            // Cada iteracion expande un usuario de la frontera. El costo de una
            // iteracion es proporcional a su numero de amigos, y ese numero
            // varia en 3 ordenes de magnitud: por eso el scheduling importa.
            #pragma omp for schedule(runtime)
            for (int64_t i = 0; i < tam_f; ++i) {
                const int32_t  u   = frontera[(size_t)i];
                const int32_t* ady = g.vecinos(u);
                const int64_t  gr  = g.grado(u);
                mis_aristas += gr;

                for (int64_t k = 0; k < gr; ++k) {
                    const int32_t w = ady[k];

                    // Filtro barato: la enorme mayoria de los vecinos ya fueron
                    // visitados. Una lectura relajada los descarta sin pagar el
                    // CAS, que es mucho mas caro. Es una lectura atomica (no una
                    // lectura normal) porque otros hilos escriben esa posicion.
                    if (__atomic_load_n(&visitado[(size_t)w], __ATOMIC_RELAXED))
                        continue;

                    // ---- AQUI SE RESUELVE LA CONDICION DE CARRERA ----
                    // Solo el hilo que consiga la transicion 0 -> 1 se queda con
                    // w. Es indivisible: no hay ventana entre "leo que esta
                    // libre" y "lo marco como mio".
                    uint8_t esperado = 0;
                    if (__atomic_compare_exchange_n(&visitado[(size_t)w], &esperado,
                                                    (uint8_t)1, false,
                                                    __ATOMIC_RELAXED,
                                                    __ATOMIC_RELAXED)) {
                        // Ganador exclusivo: nadie mas escribe padre[w] ni
                        // encola w. Sin seccion critica.
                        r.padre[(size_t)w] = u;
                        local.push_back(w);
                    }
                }
            }
            // Barrera implicita del omp for: al llegar aqui, todos los buffers
            // locales del nivel estan completos.

            aristas_hilo[(size_t)tid].v = mis_aristas;

            // ---- FUSION DE LOS BUFFERS LOCALES ----
            // Un solo hilo calcula donde arranca el bloque de cada uno; el
            // resto espera en la barrera implicita del single.
            #pragma omp single
            {
                int64_t acc = 0;
                for (int t = 0; t < T; ++t) {
                    inicio[(size_t)t] = acc;
                    acc += (int64_t)locales[(size_t)t].size();
                }
                inicio[(size_t)T] = acc;
                tam_s = acc;
            }

            // Copia en paralelo: cada hilo escribe en un rango disjunto, asi que
            // no hace falta ninguna exclusion mutua.
            if (!local.empty())
                std::copy(local.begin(), local.end(),
                          siguiente.begin() + inicio[(size_t)tid]);

            // Las copias deben estar completas antes de intercambiar fronteras.
            #pragma omp barrier

            // ---- CIERRE DEL NIVEL: contabilidad y decision ----
            #pragma omp single
            {
                const double ms_nivel = cn.ms();
                cn.reiniciar();

                int64_t ar_total = 0, ar_max = 0, ar_min = INT64_MAX;
                for (int t = 0; t < T; ++t) {
                    const int64_t v = aristas_hilo[(size_t)t].v;
                    ar_total += v;
                    ar_max = std::max(ar_max, v);
                    ar_min = std::min(ar_min, v);
                }
                if (ar_min == INT64_MAX) ar_min = 0;

                ++nivel;
                r.niveles             = nivel;
                r.nodos_visitados    += tam_s;
                r.aristas_recorridas += ar_total;
                suma_max   += ar_max;     // cota inferior del tiempo: camino critico
                suma_total += ar_total;

                if (detalle) {
                    MetricasNivel mn;
                    mn.nodos_frontera   = tam_f;
                    mn.aristas          = ar_total;
                    mn.aristas_max_hilo = ar_max;
                    mn.aristas_min_hilo = ar_min;
                    mn.ms               = ms_nivel;
                    mn.desbalance = (ar_total > 0)
                                  ? (double)ar_max / ((double)ar_total / (double)T)
                                  : 1.0;
                    detalle->push_back(mn);
                }

                // Corte anticipado al CERRAR el nivel, igual que en la version
                // secuencial por frontera: asi ambas ejecutan el mismo trabajo y
                // el speedup compara algoritmos equivalentes, no atajos distintos.
                if (!completo && visitado[(size_t)destino]) {
                    r.distancia = nivel;   // todo lo hallado en este barrido
                    continuar   = false;
                } else if (tam_s == 0) {
                    continuar = false;     // se agoto la componente conexa
                } else {
                    frontera.swap(siguiente);   // O(1): intercambia punteros
                    tam_f = tam_s;
                    tam_s = 0;
                }
            }   // barrera implicita: todos ven el nuevo `continuar` y `tam_f`
        }
    }

    // Desbalance global = (suma de los maximos por nivel) / (trabajo ideal por
    // hilo). Es el factor por el que el reparto desigual alarga la corrida
    // respecto del reparto perfecto. 1.00 = ideal.
    if (desbalance_global)
        *desbalance_global = (suma_total > 0)
            ? (double)suma_max / ((double)suma_total / (double)T) : 1.0;

    return r;
}

// ---------------------------------------------------------------------------
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

static omp_sched_t traducir_scheduling(const std::string& s) {
    if (s == "static")  return omp_sched_static;
    if (s == "dynamic") return omp_sched_dynamic;
    if (s == "guided")  return omp_sched_guided;
    fprintf(stderr, "ERROR: scheduling '%s' desconocido "
                    "(use static | dynamic | guided)\n", s.c_str());
    exit(1);
}

int main(int argc, char** argv) {
    Opciones o = parsear_opciones(argc, argv, /*paralelo=*/true);

    if (o.hilos > 0) omp_set_num_threads(o.hilos);
    omp_set_dynamic(0);   // que el runtime no ajuste el numero de hilos por su
                          // cuenta: arruinaria la reproducibilidad del estudio
    omp_set_schedule(traducir_scheduling(o.scheduling), o.chunk);

    const int hilos = omp_get_max_threads();
    const std::string txt_chunk =
        o.chunk > 0 ? std::to_string(o.chunk) : std::string("default del runtime");

    printf("\n=====================================================================\n");
    printf(" Tres Punto Uno Cuatro  |  BFS PARALELO (OpenMP)\n");
    printf("=====================================================================\n");
    printf("  hilos          : %d   (nucleos logicos disponibles: %d)\n",
           hilos, omp_get_num_procs());
    printf("  scheduling     : %s, chunk %s\n", o.scheduling.c_str(),
           txt_chunk.c_str());

    // --- Construccion de la red (misma semilla que la version secuencial) ---
    printf("\n[1] Generando la red social...\n");
    Cronometro c;
    Grafo g = generar_red_social(o.nodos, o.grado, o.semilla);
    const double t_gen = c.ms();

    resolver_extremos(o, g.n);

    printf("  usuarios (nodos)   : %ld\n", (long)g.n);
    printf("  amistades (aristas): %ld\n", (long)g.m);
    printf("  semilla            : %llu   (identica a la corrida secuencial)\n",
           (unsigned long long)o.semilla);
    printf("  tiempo de armado   : %.0f ms\n", t_gen);
    imprimir_estadisticas_grado(estadisticas_grado(g));

    printf("\n[2] Buscando la ruta minima  usuario %ld  ->  usuario %ld%s\n",
           (long)o.origen, (long)o.destino,
           o.bfs_completo ? "   [modo: red completa]" : "");

    // Calentamiento: la primera corrida paga los fallos de pagina de visitado[]
    // y padre[], y ademas levanta el pool de hilos de OpenMP. Ninguna de las dos
    // cosas es parte del algoritmo.
    bfs_paralelo(g, o.origen, o.destino, o.bfs_completo, nullptr, nullptr);

    printf("\n[3] Cronometrando %d repeticiones con %d hilos...\n",
           o.repeticiones, hilos);

    std::vector<double> tiempos;
    ResultadoBFS r;
    double desbalance = 1.0;
    std::vector<MetricasNivel> detalle;
    bool invariantes_estables = true;
    int64_t ref_dist = -2, ref_vis = -1, ref_ar = -1;

    for (int i = 0; i < o.repeticiones; ++i) {
        detalle.clear();
        c.reiniciar();
        r = bfs_paralelo(g, o.origen, o.destino, o.bfs_completo,
                         o.detalle ? &detalle : nullptr, &desbalance);
        const double ms = c.ms();
        tiempos.push_back(ms);
        escribir_csv(o.csv, "paralelo", o.etiqueta, g, hilos, o.scheduling,
                     o.chunk, i + 1, ms, r);
        printf("    rep %d/%d : %8.2f ms\n", i + 1, o.repeticiones, ms);

        // El corte por nivel hace el trabajo DETERMINISTA: sin importar cuantos
        // hilos ni en que orden, se deben visitar los mismos usuarios y recorrer
        // las mismas amistades. Si esto variara, habria una carrera sin resolver.
        if (i == 0) {
            ref_dist = r.distancia; ref_vis = r.nodos_visitados;
            ref_ar   = r.aristas_recorridas;
        } else if (r.distancia != ref_dist || r.nodos_visitados != ref_vis ||
                   r.aristas_recorridas != ref_ar) {
            invariantes_estables = false;
        }
    }

    // --- Resultados ---
    printf("\n[4] Resultado de la busqueda\n");
    if (!o.bfs_completo) {
        printf("  grados de separacion : %ld\n", (long)r.distancia);
        std::vector<int32_t> camino = reconstruir_camino(r.padre, o.origen, o.destino);
        printf("  camino:\n");
        imprimir_camino(camino);
        printf("  verificacion: %s\n",
               camino_valido(g, camino)
                   ? "VALIDO (cada salto es una amistad real del grafo)"
                   : "*** INVALIDO ***");
    }
    printf("  niveles explorados   : %d\n", r.niveles);
    printf("  usuarios visitados   : %ld\n", (long)r.nodos_visitados);
    printf("  amistades recorridas : %ld\n", (long)r.aristas_recorridas);
    printf("  invariantes en las %d repeticiones: %s\n", o.repeticiones,
           invariantes_estables
               ? "ESTABLES (mismo trabajo en todas -> sin carrera)"
               : "*** VARIAN -> HAY UNA CONDICION DE CARRERA ***");

    if (o.detalle && !detalle.empty()) {
        printf("\n[5] Reparto de carga, nivel por nivel  (ultima repeticion)\n");
        printf("  %-6s %12s %14s %14s %14s %9s %10s\n", "nivel", "frontera",
               "amistades", "max/hilo", "min/hilo", "desbal.", "ms");
        for (size_t i = 0; i < detalle.size(); ++i) {
            const MetricasNivel& d = detalle[i];
            printf("  %-6zu %12ld %14ld %14ld %14ld %8.2fx %10.2f\n",
                   i, (long)d.nodos_frontera, (long)d.aristas,
                   (long)d.aristas_max_hilo, (long)d.aristas_min_hilo,
                   d.desbalance, d.ms);
        }
        printf("\n  Lectura: desbalance = amistades del hilo mas cargado /"
               " promedio por hilo.\n");
        printf("           1.00x seria un reparto perfecto; cuanto mas alto,"
               " mas hilos esperando\n           en la barrera al cierre del"
               " nivel.\n");
    }

    const Resumen s = resumir(tiempos);
    printf("\n[%d] Tiempos con %d hilos\n", o.detalle ? 6 : 5, hilos);
    printf("  mediana %8.2f ms   (min %8.2f | prom %8.2f | max %8.2f)\n",
           s.mediana, s.minimo, s.promedio, s.maximo);
    printf("  desbalance global de carga: %.3fx", desbalance);
    if (desbalance < 1.05)      printf("   <- reparto practicamente perfecto\n");
    else if (desbalance < 1.30) printf("   <- reparto aceptable\n");
    else                        printf("   <- REPARTO DESIGUAL: hilos ociosos en la barrera\n");

    printf("\n  >> Para el speedup, compare contra la mediana del binario"
           " secuencial\n     y contra esta misma corrida con --hilos 1.\n");
    if (!o.csv.empty()) printf("  >> Mediciones anexadas a: %s\n", o.csv.c_str());
    printf("\n");

    return 0;
}
