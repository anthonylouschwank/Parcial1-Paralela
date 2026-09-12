# Resultados consolidados

*Generado por `docs/benchmarks/analizar.py`. No editar a mano: se regenera.*

Todos los tiempos son **medianas** de las repeticiones. Se usa la mediana y no el promedio porque en una laptop cualquier tarea del sistema operativo produce valores atipicos que arrastran el promedio.

## Equipos de medicion

| Maquina | Integrante | CPU | Núcleos físicos | Hilos lógicos | RAM | Compilador |
|---|---|---|---:|---:|---|---|
| `laptop-i5-1035g1` | Anthony Lou Schwank | Intel(R) Core(TM) i5-1035G1 CPU @ 1.00GHz | 4 | 8 | 5.7Gi | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| `laptop-i7-11370h` | Anthony Lou Schwank | 11th Gen Intel(R) Core(TM) i7-11370H @ 3.30GHz | 4 | 8 | 7.6Gi | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |

---

## Anthony Lou Schwank — maquina `laptop-i5-1035g1`

Red de **2 000 000 usuarios** y **15 999 964 amistades**, semilla fija.

### Escenario B — recorrido completo de la red

Metrica principal de escalabilidad: la version secuencial y la paralela recorren exactamente el mismo numero de amistades, asi que la comparacion es limpia.

Baseline secuencial (BFS por frontera, binario sin OpenMP): **342.54 ms**.
BFS de cola clasico, misma maquina: 454.25 ms.

| Hilos | Mediana (ms) | Speedup vs. secuencial | Eficiencia | Speedup vs. 1 hilo | Eficiencia |
|---:|---:|---:|---:|---:|---:|
| 1 | 400.91 | 0.85x | 85% | 1.00x | 100% |
| 2 | 224.33 | 1.53x | 76% | 1.79x | 89% |
| 3 | 184.66 | 1.85x | 62% | 2.17x | 72% |
| 4 | 158.79 | 2.16x | 54% | 2.52x | 63% |
| 6 | 137.04 | 2.50x | 42% | 2.93x | 49% |
| 8 | 144.89 | 2.36x | 30% | 2.77x | 35% |

> Esta maquina tiene **4 nucleos fisicos**. Mas alla de ese numero los hilos comparten unidades de ejecucion (Hyper-Threading/SMT), asi que la eficiencia baja por definicion: no hay mas nucleos entre los que repartir.

### Escenario A — ruta minima usuario X -> usuario Y

La consulta real del enunciado. El corte anticipado hace que el trabajo dependa del nivel en que aparece el destino.

Baseline secuencial (BFS por frontera, binario sin OpenMP): **181.56 ms**.
BFS de cola clasico, misma maquina: 49.66 ms.

| Hilos | Mediana (ms) | Speedup vs. secuencial | Eficiencia | Speedup vs. 1 hilo | Eficiencia |
|---:|---:|---:|---:|---:|---:|
| 1 | 176.68 | 1.03x | 103% | 1.00x | 100% |
| 2 | 98.98 | 1.83x | 92% | 1.78x | 89% |
| 3 | 96.36 | 1.88x | 63% | 1.83x | 61% |
| 4 | 81.86 | 2.22x | 55% | 2.16x | 54% |
| 6 | 72.71 | 2.50x | 42% | 2.43x | 40% |
| 8 | 59.97 | 3.03x | 38% | 2.95x | 37% |

> El mejor secuencial en este escenario no es el BFS por frontera (181.56 ms) sino el BFS de cola clasico (49.66 ms), porque puede abandonar a media capa al descubrir el destino. Contra ese, el speedup real es **0.83x**, no el de la tabla.

> Esta maquina tiene **4 nucleos fisicos**. Mas alla de ese numero los hilos comparten unidades de ejecucion (Hyper-Threading/SMT), asi que la eficiencia baja por definicion: no hay mas nucleos entre los que repartir.

### Politicas de reparto de carga

Esta es la respuesta directa a la pregunta del enunciado: como repartir la exploracion sin que un hilo quede solo con las amistades de un hub.

| Scheduling | Chunk | Mediana (ms) | Frente a la mejor |
|---|---:|---:|---:|
| static | default | 140.44 | +7.9% |
| static | 16 | 141.02 | +8.3% |
| static | 64 | 150.87 | +15.9% |
| static | 256 | 162.74 | +25.0% |
| dynamic | default | 152.76 | +17.3% |
| dynamic | 16 | 130.18 | +0.0% |
| dynamic | 64 | 139.03 | +6.8% |
| dynamic | 256 | 172.32 | +32.4% |
| guided | default | 143.61 | +10.3% |
| guided | 16 | 143.53 | +10.3% |
| guided | 64 | 148.30 | +13.9% |
| guided | 256 | 144.76 | +11.2% |

![Scheduling](../graficas/scheduling-laptop-i5-1035g1.svg)

---

## Anthony Lou Schwank — maquina `laptop-i7-11370h`

Red de **2 000 000 usuarios** y **15 999 964 amistades**, semilla fija.

### Escenario B — recorrido completo de la red

Metrica principal de escalabilidad: la version secuencial y la paralela recorren exactamente el mismo numero de amistades, asi que la comparacion es limpia.

Baseline secuencial (BFS por frontera, binario sin OpenMP): **369.19 ms**.
BFS de cola clasico, misma maquina: 503.73 ms.

| Hilos | Mediana (ms) | Speedup vs. secuencial | Eficiencia | Speedup vs. 1 hilo | Eficiencia |
|---:|---:|---:|---:|---:|---:|
| 1 | 449.46 | 0.82x | 82% | 1.00x | 100% |
| 2 | 240.40 | 1.54x | 77% | 1.87x | 93% |
| 3 | 185.71 | 1.99x | 66% | 2.42x | 81% |
| 4 | 156.34 | 2.36x | 59% | 2.87x | 72% |
| 6 | 122.22 | 3.02x | 50% | 3.68x | 61% |
| 8 | 114.86 | 3.21x | 40% | 3.91x | 49% |

> Esta maquina tiene **4 nucleos fisicos**. Mas alla de ese numero los hilos comparten unidades de ejecucion (Hyper-Threading/SMT), asi que la eficiencia baja por definicion: no hay mas nucleos entre los que repartir.

### Escenario A — ruta minima usuario X -> usuario Y

La consulta real del enunciado. El corte anticipado hace que el trabajo dependa del nivel en que aparece el destino.

Baseline secuencial (BFS por frontera, binario sin OpenMP): **136.27 ms**.
BFS de cola clasico, misma maquina: 37.73 ms.

| Hilos | Mediana (ms) | Speedup vs. secuencial | Eficiencia | Speedup vs. 1 hilo | Eficiencia |
|---:|---:|---:|---:|---:|---:|
| 1 | 156.16 | 0.87x | 87% | 1.00x | 100% |
| 2 | 98.95 | 1.38x | 69% | 1.58x | 79% |
| 3 | 76.92 | 1.77x | 59% | 2.03x | 68% |
| 4 | 67.59 | 2.02x | 50% | 2.31x | 58% |
| 6 | 58.98 | 2.31x | 39% | 2.65x | 44% |
| 8 | 52.91 | 2.58x | 32% | 2.95x | 37% |

> El mejor secuencial en este escenario no es el BFS por frontera (136.27 ms) sino el BFS de cola clasico (37.73 ms), porque puede abandonar a media capa al descubrir el destino. Contra ese, el speedup real es **0.71x**, no el de la tabla.

> Esta maquina tiene **4 nucleos fisicos**. Mas alla de ese numero los hilos comparten unidades de ejecucion (Hyper-Threading/SMT), asi que la eficiencia baja por definicion: no hay mas nucleos entre los que repartir.

### Politicas de reparto de carga

Esta es la respuesta directa a la pregunta del enunciado: como repartir la exploracion sin que un hilo quede solo con las amistades de un hub.

| Scheduling | Chunk | Mediana (ms) | Frente a la mejor |
|---|---:|---:|---:|
| static | default | 136.06 | +24.1% |
| static | 16 | 121.31 | +10.6% |
| static | 64 | 130.05 | +18.6% |
| static | 256 | 180.34 | +64.5% |
| dynamic | default | 140.17 | +27.8% |
| dynamic | 16 | 109.64 | +0.0% |
| dynamic | 64 | 112.04 | +2.2% |
| dynamic | 256 | 112.41 | +2.5% |
| guided | default | 119.12 | +8.6% |
| guided | 16 | 124.33 | +13.4% |
| guided | 64 | 117.52 | +7.2% |
| guided | 256 | 128.78 | +17.5% |

![Scheduling](../graficas/scheduling-laptop-i7-11370h.svg)

---

## Graficas

![Speedup escenario B](../graficas/speedup-escenarioB.svg)

![Eficiencia escenario B](../graficas/eficiencia-escenarioB.svg)

![Speedup escenario A](../graficas/speedup-escenarioA.svg)

