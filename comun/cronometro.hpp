#pragma once
// ============================================================================
//  cronometro.hpp — Medicion de tiempo de pared.
//
//  Se usa steady_clock (y no omp_get_wtime) para que la version secuencial se
//  pueda compilar SIN OpenMP y aun asi ambas midan con exactamente el mismo
//  reloj. Un baseline que enlaza libgomp no es un baseline limpio.
// ============================================================================
#include <chrono>

class Cronometro {
    std::chrono::steady_clock::time_point t0;
public:
    Cronometro() { reiniciar(); }
    void reiniciar() { t0 = std::chrono::steady_clock::now(); }
    double ms() const {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    }
};
