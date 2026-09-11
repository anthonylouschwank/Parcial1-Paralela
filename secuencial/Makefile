# ============================================================================
#  Makefile — version SECUENCIAL
#
#  IMPORTANTE: aqui NO se usa -fopenmp, a proposito. La baseline no debe
#  siquiera enlazar libgomp; si lo hiciera, estariamos midiendo el speedup
#  contra un binario que ya paga parte del costo del runtime paralelo.
#
#  El resto de banderas de optimizacion es IDENTICO al de /paralelo, para que
#  la unica diferencia entre ambos binarios sea la paralelizacion.
# ============================================================================
CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
INCLUDES  = -I../comun
OBJETIVO  = bfs_secuencial
CABECERAS = ../comun/grafo.hpp ../comun/cronometro.hpp \
            ../comun/opciones.hpp ../comun/resultado.hpp

all: $(OBJETIVO)

$(OBJETIVO): bfs_secuencial.cpp $(CABECERAS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bfs_secuencial.cpp

# Corrida rapida para verificar que todo funciona (red pequena).
prueba: $(OBJETIVO)
	./$(OBJETIVO) --nodos 200000 --repeticiones 3

# Corrida con los parametros por defecto del informe.
run: $(OBJETIVO)
	./$(OBJETIVO)

clean:
	rm -f $(OBJETIVO)

.PHONY: all prueba run clean
