#include <Arduino.h>
#include "documents.hpp"
#include "input.hpp"
#include "panel.hpp"

namespace {
prose::UI ui(documents::services());
bool ready=false, redraw=false;
uint32_t bootAt=0, lastDraw=0;
}

void setup() {
    Serial.begin(115200);
    Serial.printf("DSH-Prose Rev D, PSRAM %u bytes\n",unsigned(ESP.getPsramSize()));
    if (!panel::begin() || !documents::begin(ui)) {
        Serial.println("Initialization failed. Verify the N8R2 QSPI PSRAM configuration.");
        return;
    }
    panel::draw(ui);
    const bool touch=input::begin();
    Serial.printf("GT911 %s\n",touch ? "ready" : "unavailable; using front buttons");
    bootAt=millis();
    lastDraw=bootAt;
    ready=true;
}

void loop() {
    if (!ready) { yield(); return; }
    const uint32_t now=millis();
    if (ui.screen()==prose::Screen::Boot && now-bootAt>=2200) {
        ui.finishBoot();
        redraw=true;
    }
    if (input::poll(ui,ui.screen()==prose::Screen::Scribe)) redraw=true;
    if (redraw && now-lastDraw>=85) {
        panel::draw(ui);
        redraw=false;
        lastDraw=now;
    }
    yield();
}
