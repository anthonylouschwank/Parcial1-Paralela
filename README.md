# Tres Punto Uno Cuatro

**Consultoría HPC** — Examen Parcial 1, Computación Paralela

## Integrantes

| Integrante |
|---|
| Anthony Lou Schwank |
| Christa Isabella Obando |

---

## Problema 4 — Búsqueda de Ruta Mínima en Grafos

Dada una red social de millones de usuarios (nodos) y sus amistades (aristas),
encontrar el **camino más corto de conexiones** entre un Usuario X y un Usuario Y,
explorando los vecinos paso a paso mediante una cola de tareas pendientes.

El reto no es el algoritmo en sí, sino el **desbalance de carga**: la distribución de
grados en una red social sigue una ley de potencias. La mayoría de usuarios tiene
un puñado de amigos, pero unos pocos *hubs* tienen decenas de miles. Si la
exploración se reparte ingenuamente, un hilo termina en microsegundos mientras otro
se queda solo procesando las 10 000 amistades de un influencer.

## Estructura del repositorio

```
.
├── secuencial/   Implementación base (BFS de un solo hilo)
├── paralelo/     Implementación optimizada con OpenMP
├── docs/         Reporte técnico, métricas, gráficas y evidencia de corridas
└── README.md
```

## Entorno de desarrollo

| | |
|---|---|
| Sistema | Ubuntu 24.04 LTS sobre WSL2 (Windows 11) |
| Compilador | g++ 13.3.0 |
| OpenMP | 4.5 (`_OPENMP=201511`) |
| CPU | Intel Core i5-1035G1 — 4 núcleos físicos / 8 hilos lógicos |

## Cómo compilar y ejecutar

Pendiente — se documenta al completar las implementaciones.

## Documentación

El análisis completo (contexto y datos, estrategia de paralelización, y resultados
de speedup y eficiencia por integrante) se encuentra en [`docs/`](docs/).
