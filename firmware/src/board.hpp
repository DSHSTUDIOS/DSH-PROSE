#pragma once
#include <stdint.h>

namespace board {
constexpr int epdCs=10, epdMosi=11, epdSck=12, epdDc=13, epdReset=14, epdBusy=21;
constexpr int touchSda=15, touchScl=16, touchInt=17, touchReset=18;
constexpr int sdData=4, sdCmd=5, sdClock=6, sdDetect=7;
constexpr int prev=1, mode=2, next=42;
constexpr int panelWidth=800, panelHeight=480;
constexpr int canvasX=29, canvasY=154, canvasWidth=424, canvasHeight=504;
}
