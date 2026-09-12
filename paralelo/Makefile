# ============================================================================
#  Makefile — version PARALELA (OpenMP)
#
#  Las banderas de optimizacion son IDENTICAS a las de /secuencial. La UNICA
#  diferencia es -fopenmp. Eso es deliberado: si los dos binarios se compilaran
#  distinto, el speedup medido mezclaria el efecto de la paralelizacion con el
#  de la optimizacion, y no se podria atribuir la mejora a nada en concreto.
# ============================================================================
CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
OPENMP    = -fopenmp
INCLUDES  = -I../comun
OBJETIVO  = bfs_paralelo
CABECERAS = ../comun/grafo.hpp ../comun/cronometro.hpp \
            ../comun/opciones.hpp ../comun/resultado.hpp

all: $(OBJETIVO)

$(OBJETIVO): bfs_paralelo.cpp $(CABECERAS)
	$(CXX) $(CXXFLAGS) $(OPENMP) $(INCLUDES) -o $@ bfs_paralelo.cpp

# Corrida rapida de verificacion (red pequena).
prueba: $(OBJETIVO)
	./$(OBJETIVO) --nodos 200000 --repeticiones 3 --detalle

# Corrida con los parametros por defecto del informe.
run: $(OBJETIVO)
	./$(OBJETIVO)

clean:
	rm -f $(OBJETIVO)

.PHONY: all prueba run clean
