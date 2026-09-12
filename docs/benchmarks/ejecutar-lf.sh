#!/usr/bin/env bash
# ============================================================================
#  ejecutar.sh — Barrido completo de mediciones para UNA maquina.
#
#  Consultora HPC "Tres Punto Uno Cuatro"
#
#  Cada integrante corre este script en su equipo. Todo queda en
#  docs/resultados/<etiqueta-de-maquina>/ :
#
#     mediciones.csv   una fila por repeticion cronometrada
#     entorno.txt      CPU, nucleos, RAM, compilador, fecha
#     salidas/*.txt    la salida completa de cada corrida (evidencia)
#
#  Uso:
#     ./ejecutar.sh --integrante "Nombre Completo" --maquina etiqueta-corta
#
#  Opciones:
#     --nodos N          tamano de la red            (def. 2000000)
#     --repeticiones R   corridas por configuracion  (def. 7)
#     --enfriamiento S   pausa entre configuraciones (def. 15 s)
#     --rapido           barrido de prueba, red chica y sin pausas
#
#  POR QUE LA PAUSA ENTRE CONFIGURACIONES:
#  medirlo nos costo una tarde. En una laptop, correr los barridos uno tras
#  otro calienta el CPU y baja la frecuencia: la misma configuracion daba
#  132 ms en frio y 316 ms despues de diez minutos de carga. Sin la pausa, la
#  curva de escalabilidad mide la disipacion termica del equipo, no el
#  algoritmo. Las configuraciones tardias parecerian peores solo por ir al
#  final de la lista.
# ============================================================================
set -uo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

INTEGRANTE=""
MAQUINA=""
NODOS=2000000
REPS=7
ENFRIAMIENTO=15

uso() {
    sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --integrante)    INTEGRANTE="${2:-}"; shift 2 ;;
        --maquina)       MAQUINA="${2:-}";    shift 2 ;;
        --nodos)         NODOS="${2:-}";      shift 2 ;;
        --repeticiones)  REPS="${2:-}";       shift 2 ;;
        --enfriamiento)  ENFRIAMIENTO="${2:-}"; shift 2 ;;
        --rapido)        NODOS=400000; REPS=3; ENFRIAMIENTO=2; shift ;;
        --ayuda|-h)      uso; exit 0 ;;
        *) echo "ERROR: opcion desconocida '$1'"; echo; uso; exit 1 ;;
    esac
done

if [ -z "$INTEGRANTE" ] || [ -z "$MAQUINA" ]; then
    echo "ERROR: hacen falta --integrante y --maquina"
    echo
    uso
    exit 1
fi

# La etiqueta de maquina se usa como nombre de carpeta: sin espacios ni rarezas.
case "$MAQUINA" in
    *[!A-Za-z0-9._-]*) echo "ERROR: --maquina solo admite letras, numeros, punto, guion y guion bajo"; exit 1 ;;
esac

DEST="$RAIZ/docs/resultados/$MAQUINA"
LOGS="$DEST/salidas"
CSV="$DEST/mediciones.csv"
mkdir -p "$LOGS"

# Nunca se pisan mediciones anteriores en silencio: se archivan con fecha.
if [ -f "$CSV" ]; then
    ANTERIOR="$DEST/mediciones-$(date +%Y%m%d-%H%M%S).csv.anterior"
    mv "$CSV" "$ANTERIOR"
    echo "AVISO: habia mediciones previas, se guardaron en $(basename "$ANTERIOR")"
fi

# ---------------------------------------------------------------------------
#  Entorno: sin esto los numeros no son interpretables ni reproducibles.
# ---------------------------------------------------------------------------
NPROC=$(nproc)
{
    echo "integrante: $INTEGRANTE"
    echo "maquina: $MAQUINA"
    echo "fecha: $(date -Is)"
    echo "hostname: $(hostname)"
    echo "kernel: $(uname -srmo)"
    echo "distro: $(lsb_release -ds 2>/dev/null || cat /etc/os-release 2>/dev/null | grep -m1 PRETTY_NAME | cut -d= -f2- | tr -d '\"')"
    echo "cpu:$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2-)"
    echo "nucleos_fisicos: $(lscpu 2>/dev/null | awk -F: '/^Core\(s\) per socket/{gsub(/ /,"",$2); print $2}')"
    echo "hilos_por_nucleo: $(lscpu 2>/dev/null | awk -F: '/^Thread\(s\) per core/{gsub(/ /,"",$2); print $2}')"
    echo "hilos_logicos: $NPROC"
    echo "ram: $(free -h 2>/dev/null | awk '/^Mem:/{print $2}')"
    echo "compilador: $(g++ --version | head -1)"
    echo "nodos: $NODOS"
    echo "repeticiones: $REPS"
    echo "enfriamiento_s: $ENFRIAMIENTO"
} > "$DEST/entorno.txt"

echo "====================================================================="
echo " Tres Punto Uno Cuatro  |  barrido de mediciones"
echo "====================================================================="
cat "$DEST/entorno.txt" | sed 's/^/  /'
echo

# ---------------------------------------------------------------------------
#  Compilacion. Siempre desde cero, para que nadie mida un binario viejo.
# ---------------------------------------------------------------------------
echo "[0] Compilando..."
make -C "$RAIZ/secuencial" clean >/dev/null 2>&1
make -C "$RAIZ/paralelo"   clean >/dev/null 2>&1
if ! make -C "$RAIZ/secuencial" 2>&1 | sed 's/^/    /'; then
    echo "ERROR: fallo la compilacion de la version secuencial"; exit 1
fi
if ! make -C "$RAIZ/paralelo" 2>&1 | sed 's/^/    /'; then
    echo "ERROR: fallo la compilacion de la version paralela"; exit 1
fi
echo

SEC="$RAIZ/secuencial/bfs_secuencial"
PAR="$RAIZ/paralelo/bfs_paralelo"

# Lista de hilos a probar: potencias y divisores razonables, sin pasarse del
# numero de nucleos logicos que tenga ESTE equipo.
HILOS=""
for t in 1 2 3 4 6 8 12 16 24 32; do
    [ "$t" -le "$NPROC" ] && HILOS="$HILOS $t"
done
case " $HILOS " in *" $NPROC "*) ;; *) HILOS="$HILOS $NPROC" ;; esac

N_CONFIGS=$(( 2 + 2 * $(echo $HILOS | wc -w) + 12 + 2 ))
echo "  configuraciones a medir : $N_CONFIGS"
echo "  hilos a probar          :$HILOS"
echo "  tiempo estimado         : ~$(( (N_CONFIGS * (ENFRIAMIENTO + 6)) / 60 + 1 )) min"
echo

enfriar() {
    [ "$ENFRIAMIENTO" -gt 0 ] && sleep "$ENFRIAMIENTO"
    return 0
}

# Ejecuta una configuracion, guarda la salida completa y muestra la mediana.
corre() {
    local desc="$1"; shift
    local log="$LOGS/${desc}.txt"
    printf '  %-34s ' "$desc"
    if ! "$@" > "$log" 2>&1; then
        echo "FALLO  (ver salidas/${desc}.txt)"
        enfriar
        return 0
    fi
    local med
    med=$(grep -E '^  mediana' "$log" | head -1 | awk '{print $2}')
    if [ -z "$med" ]; then
        med=$(grep 'BFS por frontera' "$log" | awk '{print $5}')
    fi
    echo "${med:-?} ms"
    # Una carrera sin resolver se delataria aqui: el aviso lo imprime el binario.
    if grep -q 'VARIAN' "$log"; then
        echo "     *** ALERTA: las invariantes variaron entre repeticiones ***"
    fi
    if grep -q 'INVALIDO' "$log"; then
        echo "     *** ALERTA: el camino encontrado no es valido ***"
    fi
    enfriar
}

# ---------------------------------------------------------------------------
echo "[1] Baseline secuencial (binario sin OpenMP)"
corre "secuencial-A" "$SEC" --nodos "$NODOS" --repeticiones "$REPS" \
      --csv "$CSV" --etiqueta escenarioA
corre "secuencial-B" "$SEC" --nodos "$NODOS" --destino todos --repeticiones "$REPS" \
      --csv "$CSV" --etiqueta escenarioB
echo

# ---------------------------------------------------------------------------
# Escenario B = recorrer la red completa. Es la metrica PRINCIPAL de
# escalabilidad: el trabajo es identico en la version secuencial y en la
# paralela, y no depende del orden en que los hilos descubran el destino.
echo "[2] Escalabilidad — escenario B (red completa)"
for T in $HILOS; do
    corre "escalaB-hilos$T" "$PAR" --nodos "$NODOS" --destino todos \
          --repeticiones "$REPS" --hilos "$T" --scheduling dynamic --chunk 16 \
          --csv "$CSV" --etiqueta escenarioB
done
echo

# ---------------------------------------------------------------------------
# Escenario A = la consulta real del enunciado, usuario X -> usuario Y.
echo "[3] Escalabilidad — escenario A (ruta X -> Y)"
for T in $HILOS; do
    corre "escalaA-hilos$T" "$PAR" --nodos "$NODOS" \
          --repeticiones "$REPS" --hilos "$T" --scheduling dynamic --chunk 16 \
          --csv "$CSV" --etiqueta escenarioA
done
echo

# ---------------------------------------------------------------------------
# Aqui se responde la pregunta del enunciado: como repartir la exploracion sin
# que un hilo quede solo con las 10 000 amistades de un hub.
echo "[4] Politicas de reparto de carga — $NPROC hilos, escenario B"
for S in static dynamic guided; do
    for C in 0 16 64 256; do
        corre "sched-$S-chunk$C" "$PAR" --nodos "$NODOS" --destino todos \
              --repeticiones "$REPS" --hilos "$NPROC" --scheduling "$S" \
              --chunk "$C" --csv "$CSV" --etiqueta sched
    done
done
echo

# ---------------------------------------------------------------------------
# Evidencia cualitativa: el desglose nivel por nivel muestra CUANTAS amistades
# le toco al hilo mas cargado frente al menos cargado. No va al CSV; el valor
# esta en el log.
echo "[5] Reparto nivel por nivel (evidencia del desbalance)"
corre "detalle-static"  "$PAR" --nodos "$NODOS" --destino todos --repeticiones 3 \
      --hilos "$NPROC" --scheduling static --chunk 0 --detalle
corre "detalle-dynamic" "$PAR" --nodos "$NODOS" --destino todos --repeticiones 3 \
      --hilos "$NPROC" --scheduling dynamic --chunk 16 --detalle
echo

# ---------------------------------------------------------------------------
FILAS=$(( $(wc -l < "$CSV") - 1 ))
echo "====================================================================="
echo " Listo."
echo "   mediciones : docs/resultados/$MAQUINA/mediciones.csv  ($FILAS filas)"
echo "   entorno    : docs/resultados/$MAQUINA/entorno.txt"
echo "   salidas    : docs/resultados/$MAQUINA/salidas/  ($(ls -1 "$LOGS" | wc -l) archivos)"
echo
echo " Siguiente paso:"
echo "   python3 docs/benchmarks/analizar.py"
echo "====================================================================="
