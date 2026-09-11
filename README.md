# Tres Punto Uno Cuatro

Consultoría HPC — Examen Parcial 1, Computación Paralela

## Integrantes

| Integrante |
|---|
| Anthony Lou Schwank |
| Christa Isabella Obando |

---

## Problema 4 — Búsqueda de Ruta Mínima en Grafos

Dada una red social de millones de usuarios y sus amistades,
encontrar el camino más corto de conexiones entre un Usuario X y un Usuario Y,
explorando los vecinos paso a paso mediante una cola de tareas pendientes.

El reto no es el algoritmo en sí, sino el desbalance de carga: la distribución de
grados en una red social sigue una ley de potencias. La mayoría de usuarios tiene
un puñado de amigos, pero unos pocos hubs tienen decenas de miles. Si la
exploración se reparte ingenuamente, un hilo termina en microsegundos mientras otro
se queda solo procesando las 10 000 amistades de un influencer.

## Estructura del repositorio

```
.
├── comun/        Cabeceras compartidas: grafo CSR, generador, cronómetro, CLI
├── secuencial/   Implementación base
├── paralelo/     Implementación optimizada con OpenMP
├── docs/         Reporte técnico, métricas, gráficas y evidencia de corridas
└── README.md
```

`comun/` si cada implementación generara su
propio grafo, los tiempos no serían comparables. Ambas incluyen el mismo
generador y la misma semilla, así que miden sobre la red idéntica.

## Entorno de desarrollo

| | |
|---|---|
| Sistema | Ubuntu 24.04 LTS sobre WSL2 (Windows 11) |
| Compilador | g++ 13.3.0 |
| OpenMP | 4.5 (`_OPENMP=201511`) |
| CPU | Intel Core i5-1035G1 — 4 núcleos físicos / 8 hilos lógicos |

## Cómo compilar y ejecutar

Requisitos: `g++` con soporte C++17 y OpenMP (`sudo apt install build-essential`).

### Versión secuencial

```bash
cd secuencial && make
```

```bash
./bfs_secuencial --nodos 2000000 --repeticiones 5
```

Opciones principales (`--ayuda` lista todas):

| Opción | Significado | Def. |
|---|---|---|
| `--nodos N` | usuarios en la red | 2000000 |
| `--grado M` | amistades que trae cada usuario nuevo | 8 |
| `--semilla S` | semilla del generador (reproducibilidad) | 314 |
| `--origen X` / `--destino Y` | extremos de la búsqueda | `n-1` / `n-2` |
| `--destino todos` | recorrer la red completa, sin corte anticipado | — |
| `--repeticiones R` | corridas cronometradas | 5 |
| `--csv ARCHIVO` | anexar mediciones para las gráficas | — |

## Documentación

El análisis completo (contexto y datos, estrategia de paralelización, y resultados
de speedup y eficiencia por integrante) se encuentra en [`docs/`](docs/).
