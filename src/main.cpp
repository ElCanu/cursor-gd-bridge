// Debug links as a console app (main). Every other config links as a GUI app (wWinMain)
// so Release has no console window. BRIDGE_CONSOLE is set by CMake to match the subsystem.

#include "app.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#if !defined(BRIDGE_CONSOLE)
#error BRIDGE_CONSOLE must be defined by the build
#endif

#if BRIDGE_CONSOLE
int main() {
    return bridge::Run();
}
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    return bridge::Run();
}
#endif
