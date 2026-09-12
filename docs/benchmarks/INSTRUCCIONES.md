# Instrucciones para medir en una máquina nueva

Consultora HPC **Tres Punto Uno Cuatro** — Examen Parcial 1

Este documento es para quien vaya a tomar mediciones en un equipo distinto al
que ya está reportado. Sigue los pasos en orden; no hace falta entender el
código para ejecutarlo.

---

## 0. Qué necesitas

| Requisito | Cómo verificarlo |
|---|---|
| Linux, o Windows con WSL2 | `uname -a` |
| `g++` con C++17 y OpenMP | `g++ --version` |
| `make` | `make --version` |
| `python3` (solo para el análisis) | `python3 --version` |

Si falta algo en Ubuntu/Debian:

```bash
sudo apt update && sudo apt install -y build-essential python3
```

No se necesita instalar nada más. El analizador usa únicamente la librería
estándar de Python — no hay `pip install` en ningún paso.

---

## 1. Clonar el repositorio

```bash
git clone https://github.com/anthonylouschwank/Parcial1-Paralela.git && cd Parcial1-Paralela
```

Si ya lo tienes clonado, baja los últimos cambios:

```bash
git pull
```

---

## 2. Comprobar que compila y que el resultado es correcto

Antes de medir nada, verifica que las dos versiones producen **exactamente el
mismo resultado**. Si estos dos números no coinciden, hay un problema y no
tiene sentido seguir.

```bash
cd secuencial && make && ./bfs_secuencial --nodos 2000000 --destino todos --repeticiones 1 --solo-frontera | grep -E "usuarios visitados|amistades recorridas"
```

```bash
cd ../paralelo && make && ./bfs_paralelo --nodos 2000000 --destino todos --repeticiones 1 | grep -E "usuarios visitados|amistades recorridas|invariantes"
```

Ambos deben reportar **2 000 000 usuarios visitados** y **31 999 928 amistades
recorridas**, y la línea de invariantes debe decir `ESTABLES`.

> El grafo se genera con semilla fija, así que estos números son idénticos en
> cualquier máquina. Si difieren, avisa antes de continuar.

---

## 3. Correr el barrido de mediciones

Vuelve a la raíz del repositorio y ejecuta, **cambiando los dos valores**:

```bash
cd .. && bash docs/benchmarks/ejecutar.sh --integrante "Tu Nombre Completo" --maquina etiqueta-corta
```

- `--integrante` — tu nombre completo, tal como debe aparecer en el informe.
- `--maquina` — una etiqueta corta y sin espacios que identifique el equipo
  (por ejemplo `laptop-ryzen`, `pc-escritorio`, `thinkpad-t14`). Se usa como
  nombre de carpeta.

**Tarda entre 10 y 15 minutos.** Es normal: entre cada configuración hay una
pausa de 15 segundos.

### Reglas para que las mediciones sirvan

1. **No uses la computadora mientras corre.** Nada de navegador, video,
   compilaciones ni actualizaciones en segundo plano. Cualquier otro proceso
   compite por los mismos núcleos y ensucia los tiempos.
2. **Conéctala a la corriente** y, si puedes, ponla en modo de máximo
   rendimiento. Con batería, muchos equipos bajan la frecuencia del CPU.
3. **No canceles a medias.** Si se interrumpe, vuelve a correrlo completo; las
   mediciones anteriores se archivan solas, no se pierden.

### Si solo quieres probar que funciona

```bash
bash docs/benchmarks/ejecutar.sh --integrante "Tu Nombre" --maquina prueba --rapido
```

Tarda ~2 minutos, pero los números son **ruidosos y no sirven para el
informe** — es solo para comprobar que todo corre. Borra la carpeta
`docs/resultados/prueba/` cuando termines.

---

## 4. Capturar la evidencia

El examen pide screenshots o video de las ejecuciones. Lo mínimo:

1. Una captura de la salida del paso 2 (verificación de correctitud).
2. Una captura del **encabezado** del barrido, donde se ve tu nombre, el CPU y
   el número de núcleos.
3. Una captura del **final** del barrido, con la tabla de tiempos por número
   de hilos.
4. Opcional pero vale la pena: una captura de
   `docs/resultados/<tu-maquina>/salidas/detalle-dynamic.txt`, que muestra el
   reparto de carga nivel por nivel.

Guárdalas en `docs/resultados/<tu-maquina>/capturas/` con nombres descriptivos
(`01-verificacion.png`, `02-inicio-barrido.png`, ...).

```bash
mkdir -p docs/resultados/<tu-maquina>/capturas
```

---

## 5. Generar el análisis

```bash
python3 docs/benchmarks/analizar.py
```

Esto regenera `docs/resultados/RESUMEN.md` y las gráficas en `docs/graficas/`
combinando **todas** las máquinas que haya en el repositorio, no solo la tuya.

---

## 6. Subir los resultados

```bash
git add docs/resultados docs/graficas && git status
```

Revisa qué vas a subir y luego:

```bash
git commit -m "Mediciones en <tu-maquina> (<tu nombre>)" && git push
```

Si alguien más subió resultados mientras tanto, haz `git pull --rebase` antes
del push y vuelve a correr `analizar.py` para que el resumen incluya todo.

---

## Qué queda en el repositorio

```
docs/resultados/<tu-maquina>/
├── mediciones.csv    una fila por repetición cronometrada
├── entorno.txt       CPU, núcleos, RAM, compilador, fecha
├── salidas/          salida completa de cada corrida (28 archivos)
└── capturas/         tus screenshots
```

El `mediciones.csv` es el dato duro; `salidas/` es la evidencia de que cada
número salió de una ejecución real y no de una tabla escrita a mano.

---

## Problemas comunes

**`fatal error: omp.h: No such file or directory`**
Falta el soporte de OpenMP del compilador. En Ubuntu: `sudo apt install
build-essential`. En macOS el `clang` de Apple no trae OpenMP; instala
`brew install libomp gcc` y compila con `make CXX=g++-14`.

**Los tiempos de las últimas configuraciones son mucho peores que los de las
primeras**
Es sobrecalentamiento: el CPU baja su frecuencia. Sube la pausa entre
configuraciones y vuelve a correr:

```bash
bash docs/benchmarks/ejecutar.sh --integrante "Tu Nombre" --maquina etiqueta --enfriamiento 30
```

**`std::bad_alloc` o el sistema se queda sin memoria**
La red de 2 000 000 de usuarios necesita ~700 MB durante la construcción. En
un equipo con poca RAM, usa una red más chica **y anótalo en el informe**,
porque los tiempos ya no serán comparables con los de las otras máquinas:

```bash
bash docs/benchmarks/ejecutar.sh --integrante "Tu Nombre" --maquina etiqueta --nodos 1000000
```

**El script dice `ALERTA: las invariantes variaron entre repeticiones`**
Eso indicaría una condición de carrera real en la versión paralela. No lo
ignores: guarda la salida completa y repórtalo.
