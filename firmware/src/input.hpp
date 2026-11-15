#pragma once
#include "prose_ui.hpp"

namespace input {
bool begin();
bool poll(prose::UI& ui, bool scribe);
}
