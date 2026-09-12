#!/usr/bin/env python3
# ============================================================================
#  analizar.py — Consolida las mediciones de todas las maquinas.
#
#  Consultora HPC "Tres Punto Uno Cuatro"
#
#  Lee  docs/resultados/<maquina>/mediciones.csv  (+ entorno.txt)
#  Escribe:
#     docs/resultados/RESUMEN.md      tablas de speedup y eficiencia
#     docs/graficas/*.svg             graficas
#
#  Solo usa la libreria estandar de Python a proposito: cada integrante debe
#  poder correrlo en su maquina sin instalar nada. Las graficas se escriben
#  como SVG a mano, lo que ademas las deja versionadas como texto (un diff
#  muestra si una grafica cambio y por que).
#
#  Uso:  python3 docs/benchmarks/analizar.py
# ============================================================================
import csv
import os
import sys

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DIR_RESULTADOS = os.path.join(RAIZ, "docs", "resultados")
DIR_GRAFICAS = os.path.join(RAIZ, "docs", "graficas")

PALETA = ["#2563eb", "#dc2626", "#059669", "#d97706", "#7c3aed", "#0891b2"]


# ---------------------------------------------------------------------------
# Lectura
# ---------------------------------------------------------------------------
def mediana(valores):
    if not valores:
        return None
    v = sorted(valores)
    n = len(v)
    return v[n // 2] if n % 2 else (v[n // 2 - 1] + v[n // 2]) / 2.0


def leer_entorno(ruta):
    datos = {}
    if not os.path.isfile(ruta):
        return datos
    with open(ruta, encoding="utf-8") as f:
        for linea in f:
            if ":" in linea:
                k, v = linea.split(":", 1)
                datos[k.strip()] = v.strip()
    return datos


def leer_resultados():
    """Devuelve {etiqueta_maquina: {'entorno': {...}, 'filas': [dict, ...]}}."""
    maquinas = {}
    if not os.path.isdir(DIR_RESULTADOS):
        return maquinas
    for nombre in sorted(os.listdir(DIR_RESULTADOS)):
        carpeta = os.path.join(DIR_RESULTADOS, nombre)
        csv_path = os.path.join(carpeta, "mediciones.csv")
        if not os.path.isfile(csv_path):
            continue
        filas = []
        with open(csv_path, newline="", encoding="utf-8") as f:
            for fila in csv.DictReader(f):
                try:
                    fila["hilos"] = int(fila["hilos"])
                    fila["chunk"] = int(fila["chunk"])
                    fila["tiempo_ms"] = float(fila["tiempo_ms"])
                    fila["nodos"] = int(fila["nodos"])
                except (ValueError, KeyError, TypeError):
                    continue   # fila corrupta: se ignora, no se inventa
                filas.append(fila)
        if filas:
            maquinas[nombre] = {
                "entorno": leer_entorno(os.path.join(carpeta, "entorno.txt")),
                "filas": filas,
            }
    return maquinas


def med_de(filas, **criterios):
    """Mediana de tiempo_ms de las filas que cumplen todos los criterios."""
    sel = [f["tiempo_ms"] for f in filas
           if all(f.get(k) == v for k, v in criterios.items())]
    return mediana(sel)


# ---------------------------------------------------------------------------
# Graficas SVG (escritas a mano, sin dependencias)
# ---------------------------------------------------------------------------
def _esc(t):
    return (str(t).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


def svg_lineas(ruta, titulo, etiqueta_x, etiqueta_y, series, ideal=False):
    """series: [(nombre, [(x, y), ...]), ...]  con x numerico."""
    W, H = 760, 440
    ML, MR, MT, MB = 70, 190, 50, 60
    ancho, alto = W - ML - MR, H - MT - MB

    xs = [p[0] for _, pts in series for p in pts]
    ys = [p[1] for _, pts in series for p in pts]
    if not xs or not ys:
        return False
    xmax = max(xs)
    ymax = max(ys + ([xmax] if ideal else []))
    ymax = ymax * 1.12 if ymax > 0 else 1.0
    xmin = min(xs)

    def px(x):
        return ML + (0 if xmax == xmin else (x - xmin) / (xmax - xmin) * ancho)

    def py(y):
        return MT + alto - (y / ymax) * alto

    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
         'viewBox="0 0 %d %d" font-family="system-ui,-apple-system,Segoe UI,sans-serif">'
         % (W, H, W, H)]
    o.append('<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H))
    o.append('<text x="%d" y="28" font-size="17" font-weight="600" fill="#111827">%s</text>'
             % (ML, _esc(titulo)))

    # rejilla horizontal
    pasos = 5
    for i in range(pasos + 1):
        v = ymax * i / pasos
        y = py(v)
        o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#e5e7eb"/>'
                 % (ML, y, ML + ancho, y))
        o.append('<text x="%.1f" y="%.1f" font-size="12" fill="#6b7280" '
                 'text-anchor="end">%.2f</text>' % (ML - 8, y + 4, v))

    # eje x: una marca por valor de x distinto
    for x in sorted(set(xs)):
        o.append('<text x="%.1f" y="%.1f" font-size="12" fill="#6b7280" '
                 'text-anchor="middle">%g</text>' % (px(x), MT + alto + 20, x))
    o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#9ca3af"/>'
             % (ML, MT + alto, ML + ancho, MT + alto))
    o.append('<text x="%.1f" y="%d" font-size="13" fill="#374151" '
             'text-anchor="middle">%s</text>'
             % (ML + ancho / 2, H - 18, _esc(etiqueta_x)))
    o.append('<text x="18" y="%.1f" font-size="13" fill="#374151" '
             'text-anchor="middle" transform="rotate(-90 18 %.1f)">%s</text>'
             % (MT + alto / 2, MT + alto / 2, _esc(etiqueta_y)))

    leyenda = []
    if ideal:
        o.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#9ca3af" '
                 'stroke-dasharray="6 4" stroke-width="2"/>'
                 % (px(xmin), py(xmin), px(xmax), py(xmax)))
        leyenda.append(("Ideal (lineal)", "#9ca3af", True))

    for i, (nombre, pts) in enumerate(series):
        color = PALETA[i % len(PALETA)]
        pts = sorted(pts)
        d = " ".join("%s%.1f,%.1f" % ("M" if k == 0 else "L", px(x), py(y))
                     for k, (x, y) in enumerate(pts))
        o.append('<path d="%s" fill="none" stroke="%s" stroke-width="2.5"/>' % (d, color))
        for x, y in pts:
            o.append('<circle cx="%.1f" cy="%.1f" r="4" fill="%s"/>' % (px(x), py(y), color))
        leyenda.append((nombre, color, False))

    ly = MT + 6
    for nombre, color, guion in leyenda:
        extra = ' stroke-dasharray="6 4"' if guion else ''
        o.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="%s" '
                 'stroke-width="2.5"%s/>'
                 % (ML + ancho + 16, ly, ML + ancho + 46, ly, color, extra))
        o.append('<text x="%d" y="%.1f" font-size="12" fill="#374151">%s</text>'
                 % (ML + ancho + 52, ly + 4, _esc(nombre)))
        ly += 22

    o.append('</svg>')
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("\n".join(o))
    return True


def svg_barras(ruta, titulo, etiqueta_y, barras):
    """barras: [(etiqueta, valor, color_idx), ...]"""
    if not barras:
        return False
    W = max(560, 60 + 52 * len(barras) + 40)
    H = 420
    ML, MT, MB = 70, 50, 110
    ancho, alto = W - ML - 30, H - MT - MB
    vmax = max(v for _, v, _ in barras) * 1.15

    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
         'viewBox="0 0 %d %d" font-family="system-ui,-apple-system,Segoe UI,sans-serif">'
         % (W, H, W, H)]
    o.append('<rect width="%d" height="%d" fill="#ffffff"/>' % (W, H))
    o.append('<text x="%d" y="28" font-size="17" font-weight="600" fill="#111827">%s</text>'
             % (ML, _esc(titulo)))

    for i in range(6):
        v = vmax * i / 5
        y = MT + alto - (v / vmax) * alto
        o.append('<line x1="%d" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#e5e7eb"/>'
                 % (ML, y, ML + ancho, y))
        o.append('<text x="%d" y="%.1f" font-size="12" fill="#6b7280" '
                 'text-anchor="end">%.0f</text>' % (ML - 8, y + 4, v))

    paso = ancho / len(barras)
    bw = min(38, paso * 0.66)
    mejor = min(v for _, v, _ in barras)
    for i, (etq, v, ci) in enumerate(barras):
        x = ML + paso * i + (paso - bw) / 2
        h = (v / vmax) * alto
        y = MT + alto - h
        color = PALETA[ci % len(PALETA)]
        borde = ' stroke="#111827" stroke-width="2"' if v == mejor else ''
        o.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" '
                 'rx="3"%s/>' % (x, y, bw, h, color, borde))
        o.append('<text x="%.1f" y="%.1f" font-size="11" fill="#374151" '
                 'text-anchor="middle">%.0f</text>' % (x + bw / 2, y - 6, v))
        o.append('<text x="%.1f" y="%.1f" font-size="11" fill="#6b7280" '
                 'text-anchor="end" transform="rotate(-45 %.1f %.1f)">%s</text>'
                 % (x + bw / 2, MT + alto + 16, x + bw / 2, MT + alto + 16, _esc(etq)))
    o.append('<line x1="%d" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#9ca3af"/>'
             % (ML, MT + alto, ML + ancho, MT + alto))
    o.append('<text x="18" y="%.1f" font-size="13" fill="#374151" '
             'text-anchor="middle" transform="rotate(-90 18 %.1f)">%s</text>'
             % (MT + alto / 2, MT + alto / 2, _esc(etiqueta_y)))
    o.append('<text x="%d" y="%d" font-size="11" fill="#6b7280">'
             'borde negro = configuracion mas rapida</text>' % (ML, H - 12))
    o.append('</svg>')
    with open(ruta, "w", encoding="utf-8") as f:
        f.write("\n".join(o))
    return True


# ---------------------------------------------------------------------------
# Informe
# ---------------------------------------------------------------------------
def tabla_escalabilidad(filas, etiqueta, base_frontera, base_mejor):
    """Devuelve (lineas_markdown, puntos_speedup, puntos_eficiencia)."""
    hilos = sorted({f["hilos"] for f in filas
                    if f["implementacion"] == "paralelo" and f["etiqueta"] == etiqueta})
    lineas, p_speed, p_efi = [], [], []
    if not hilos:
        return lineas, p_speed, p_efi

    t1 = med_de(filas, implementacion="paralelo", etiqueta=etiqueta, hilos=hilos[0])
    lineas.append("| Hilos | Mediana (ms) | Speedup vs. secuencial | Eficiencia | "
                  "Speedup vs. 1 hilo | Eficiencia |")
    lineas.append("|---:|---:|---:|---:|---:|---:|")
    for p in hilos:
        t = med_de(filas, implementacion="paralelo", etiqueta=etiqueta, hilos=p)
        if t is None or t <= 0:
            continue
        s_sec = (base_frontera / t) if base_frontera else float("nan")
        s_esc = (t1 / t) if t1 else float("nan")
        lineas.append("| %d | %.2f | %.2fx | %.0f%% | %.2fx | %.0f%% |"
                      % (p, t, s_sec, 100 * s_sec / p, s_esc, 100 * s_esc / p))
        p_speed.append((p, s_sec))
        p_efi.append((p, 100 * s_sec / p))

    if base_mejor and base_frontera and base_mejor < base_frontera * 0.95:
        mejor_par = min(med_de(filas, implementacion="paralelo", etiqueta=etiqueta,
                               hilos=p) or 1e18 for p in hilos)
        lineas.append("")
        lineas.append("> El mejor secuencial en este escenario no es el BFS por "
                      "frontera (%.2f ms) sino el BFS de cola clasico (%.2f ms), "
                      "porque puede abandonar a media capa al descubrir el destino. "
                      "Contra ese, el speedup real es **%.2fx**, no el de la tabla."
                      % (base_frontera, base_mejor, base_mejor / mejor_par))
    return lineas, p_speed, p_efi


def main():
    maquinas = leer_resultados()
    if not maquinas:
        print("No hay mediciones en docs/resultados/.")
        print("Corra primero:  ./docs/benchmarks/ejecutar.sh --integrante \"...\" "
              "--maquina etiqueta")
        return 1

    os.makedirs(DIR_GRAFICAS, exist_ok=True)
    L = []
    L.append("# Resultados consolidados")
    L.append("")
    L.append("*Generado por `docs/benchmarks/analizar.py`. No editar a mano: "
             "se regenera.*")
    L.append("")
    L.append("Todos los tiempos son **medianas** de las repeticiones. Se usa la "
             "mediana y no el promedio porque en una laptop cualquier tarea del "
             "sistema operativo produce valores atipicos que arrastran el promedio.")
    L.append("")

    # --- Equipos ---
    L.append("## Equipos de medicion")
    L.append("")
    L.append("| Maquina | Integrante | CPU | Núcleos físicos | Hilos lógicos | RAM | Compilador |")
    L.append("|---|---|---|---:|---:|---|---|")
    for nombre, d in maquinas.items():
        e = d["entorno"]
        L.append("| `%s` | %s | %s | %s | %s | %s | %s |" % (
            nombre, e.get("integrante", "?"), e.get("cpu", "?"),
            e.get("nucleos_fisicos", "?"), e.get("hilos_logicos", "?"),
            e.get("ram", "?"), e.get("compilador", "?")))
    L.append("")

    series_speed_B, series_efi_B, series_speed_A = [], [], []

    for nombre, d in maquinas.items():
        filas = d["filas"]
        e = d["entorno"]
        integrante = e.get("integrante", nombre)
        L.append("---")
        L.append("")
        L.append("## %s — maquina `%s`" % (integrante, nombre))
        L.append("")

        def sep_miles(x):
            try:
                return f"{int(x):,}".replace(",", " ")
            except (TypeError, ValueError):
                return str(x)

        nodos = filas[0]["nodos"]
        aristas = filas[0].get("aristas", "?")
        nucleos = e.get("nucleos_fisicos", "?")
        L.append("Red de **%s usuarios** y **%s amistades**, semilla fija."
                 % (sep_miles(nodos), sep_miles(aristas)))
        L.append("")

        for etq, titulo, nota in (
            ("escenarioB", "Escenario B — recorrido completo de la red",
             "Metrica principal de escalabilidad: la version secuencial y la "
             "paralela recorren exactamente el mismo numero de amistades, asi que "
             "la comparacion es limpia."),
            ("escenarioA", "Escenario A — ruta minima usuario X -> usuario Y",
             "La consulta real del enunciado. El corte anticipado hace que el "
             "trabajo dependa del nivel en que aparece el destino."),
        ):
            bf = med_de(filas, implementacion="secuencial", etiqueta=etq)
            bc = med_de(filas, implementacion="secuencial_cola", etiqueta=etq)
            base_mejor = min(x for x in (bf, bc) if x) if (bf or bc) else None

            lineas, p_s, p_e = tabla_escalabilidad(filas, etq, bf, base_mejor)
            if not lineas:
                continue
            L.append("### " + titulo)
            L.append("")
            L.append(nota)
            L.append("")
            if bf:
                L.append("Baseline secuencial (BFS por frontera, binario sin "
                         "OpenMP): **%.2f ms**." % bf)
                if bc:
                    L.append("BFS de cola clasico, misma maquina: %.2f ms." % bc)
                L.append("")
            L.extend(lineas)
            L.append("")
            L.append("> Esta maquina tiene **%s nucleos fisicos**. Mas alla de ese "
                     "numero los hilos comparten unidades de ejecucion "
                     "(Hyper-Threading/SMT), asi que la eficiencia baja por "
                     "definicion: no hay mas nucleos entre los que repartir."
                     % nucleos)
            L.append("")
            if etq == "escenarioB":
                series_speed_B.append((integrante, p_s))
                series_efi_B.append((integrante, p_e))
            else:
                series_speed_A.append((integrante, p_s))

        # --- Politicas de scheduling ---
        # Orden pedagogico (static -> dynamic -> guided), no alfabetico.
        ORDEN_SCH = {"static": 0, "dynamic": 1, "guided": 2}
        combos = sorted({(f["scheduling"], f["chunk"]) for f in filas
                         if f["etiqueta"] == "sched"},
                        key=lambda c: (ORDEN_SCH.get(c[0], 9), c[1]))
        if combos:
            L.append("### Politicas de reparto de carga")
            L.append("")
            L.append("Esta es la respuesta directa a la pregunta del enunciado: "
                     "como repartir la exploracion sin que un hilo quede solo con "
                     "las amistades de un hub.")
            L.append("")
            L.append("| Scheduling | Chunk | Mediana (ms) | Frente a la mejor |")
            L.append("|---|---:|---:|---:|")
            medidos = []
            for sch, ch in combos:
                t = med_de(filas, etiqueta="sched", scheduling=sch, chunk=ch)
                if t:
                    medidos.append((sch, ch, t))
            if medidos:
                mejor = min(t for _, _, t in medidos)
                orden = ORDEN_SCH
                for sch, ch, t in medidos:
                    L.append("| %s | %s | %.2f | %+.1f%% |"
                             % (sch, ch if ch else "default", t,
                                100 * (t - mejor) / mejor))
                ruta = os.path.join(DIR_GRAFICAS, "scheduling-%s.svg" % nombre)
                svg_barras(ruta,
                           "Politica de scheduling — %s" % integrante,
                           "mediana (ms)",
                           [("%s/%s" % (s, c if c else "def"), t,
                             orden.get(s, 0)) for s, c, t in medidos])
                L.append("")
                L.append("![Scheduling](../graficas/scheduling-%s.svg)" % nombre)
            L.append("")

    # --- Graficas comparativas ---
    L.append("---")
    L.append("")
    L.append("## Graficas")
    L.append("")
    if svg_lineas(os.path.join(DIR_GRAFICAS, "speedup-escenarioB.svg"),
                  "Speedup vs. secuencial — recorrido completo",
                  "hilos", "speedup (x)", series_speed_B, ideal=True):
        L.append("![Speedup escenario B](../graficas/speedup-escenarioB.svg)")
        L.append("")
    if svg_lineas(os.path.join(DIR_GRAFICAS, "eficiencia-escenarioB.svg"),
                  "Eficiencia paralela — recorrido completo",
                  "hilos", "eficiencia (%)", series_efi_B):
        L.append("![Eficiencia escenario B](../graficas/eficiencia-escenarioB.svg)")
        L.append("")
    if svg_lineas(os.path.join(DIR_GRAFICAS, "speedup-escenarioA.svg"),
                  "Speedup vs. secuencial — ruta X -> Y",
                  "hilos", "speedup (x)", series_speed_A, ideal=True):
        L.append("![Speedup escenario A](../graficas/speedup-escenarioA.svg)")
        L.append("")

    salida = os.path.join(DIR_RESULTADOS, "RESUMEN.md")
    with open(salida, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")

    print("Maquinas procesadas : %d  (%s)" % (len(maquinas), ", ".join(maquinas)))
    print("Informe             : docs/resultados/RESUMEN.md")
    print("Graficas            : docs/graficas/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
