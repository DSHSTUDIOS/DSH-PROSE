#pragma once
#include <Adafruit_GFX.h>
#include "prose_ui.hpp"

namespace panel {
bool begin();
int measure(const char* text, int size, prose::Font face);
void draw(const prose::UI& ui);
void setInk(GFXcanvas1* ink);
}
