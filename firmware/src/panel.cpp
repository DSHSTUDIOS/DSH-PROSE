#include "panel.hpp"
#include "board.hpp"
#include "dsh_logo.hpp"
#include "opendyslexic_glyphs.hpp"

#include <Arduino.h>
#include <SPI.h>
#include <gdey/GxEPD2_750_GDEY075T7.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono18pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif24pt7b.h>
#include <cstring>

namespace panel {
namespace {
constexpr int stride=board::panelWidth/8;
constexpr size_t frameBytes=stride*board::panelHeight;
GFXcanvas1 frame(board::panelWidth,board::panelHeight);
GxEPD2_750_GDEY075T7 epd(board::epdCs,board::epdDc,board::epdReset,board::epdBusy);
TaskHandle_t worker=nullptr;
SemaphoreHandle_t guard=nullptr;
uint8_t *queued=nullptr, *working=nullptr, *shown=nullptr;
GFXcanvas1* drawing=nullptr;
bool dark=false;

const GFXfont* faceFor(int size, prose::Font face) {
    switch (face) {
    case prose::Font::Label: return nullptr;
    case prose::Font::Sans: return size<=17 ? &FreeSans9pt7b : size<=29 ? &FreeSans12pt7b : &FreeSans18pt7b;
    case prose::Font::Serif:
        return size<=17 ? &FreeSerif9pt7b : size<=27 ? &FreeSerif12pt7b : size<=39 ? &FreeSerif18pt7b : &FreeSerif24pt7b;
    case prose::Font::Mono: return size<=18 ? &FreeMono9pt7b : &FreeMono12pt7b;
    case prose::Font::ReaderSerif:
        return size<=19 ? &FreeSerif9pt7b : size<=22 ? &FreeSerif12pt7b : &FreeSerif18pt7b;
    case prose::Font::ReaderSans:
        return size<=19 ? &FreeSans9pt7b : size<=22 ? &FreeSans12pt7b : &FreeSans18pt7b;
    case prose::Font::ReaderMono:
        return size<=18 ? &FreeMono9pt7b : size<=21 ? &FreeMono12pt7b : &FreeMono18pt7b;
    case prose::Font::OpenDyslexic:
        return size<=15 ? &OpenDyslexic15 : size<=18 ? &OpenDyslexic18 : size<=20 ? &OpenDyslexic20 : &OpenDyslexic21;
    }
    return nullptr;
}
uint16_t pixel(bool foreground) { return foreground!=dark ? 1 : 0; }

void fill(int x,int y,int w,int h,bool black) { frame.fillRect(x,y,w,h,pixel(black)); }
void line(int x0,int y0,int x1,int y1,int size) {
    if (y0==y1 && size>1) frame.fillRect(min(x0,x1),y0-size/2,abs(x1-x0)+1,size,pixel(true));
    else frame.drawLine(x0,y0,x1,y1,pixel(true));
}
void text(int x,int y,int size,prose::Font face,const char* value,bool black) {
    const auto* font=faceFor(size,face);
    frame.setFont(font);
    frame.setTextColor(pixel(black));
    frame.setCursor(x,font ? y : y-7);
    frame.print(value);
}

void icon(prose::Icon kind,int x,int y,int size,bool black) {
    const auto color=pixel(black);
    auto mark=[&](int ax,int ay,int bx,int by) {
        frame.drawLine(x+ax*size/24,y+ay*size/24,x+bx*size/24,y+by*size/24,color);
    };
    auto box=[&](int px,int py,int w,int h) {
        frame.drawRect(x+px*size/24,y+py*size/24,w*size/24,h*size/24,color);
    };
    switch (kind) {
    case prose::Icon::Logo:
        for (int row=0; row<size; ++row) for (int col=0; col<size; ++col) {
            const int u=col*prose::logoSize/size, v=row*prose::logoSize/size;
            if (prose::logoBitmap[v*prose::logoStride+u/8]&(0x80>>(u%8))) frame.drawPixel(x+col,y+row,color);
        }
        break;
    case prose::Icon::Home: mark(3,10,12,3); mark(12,3,21,10); box(6,10,13,11); break;
    case prose::Icon::Book: box(5,4,14,17); mark(6,17,18,17); break;
    case prose::Icon::Notebook: box(6,3,13,18); mark(9,4,9,20); mark(3,8,8,8); mark(3,15,8,15); break;
    case prose::Icon::Left: mark(17,12,5,12); mark(5,12,12,5); mark(5,12,12,19); break;
    case prose::Icon::Right: mark(5,12,19,12); mark(19,12,12,5); mark(19,12,12,19); break;
    case prose::Icon::Pen: mark(4,20,5,15); mark(5,15,16,4); mark(16,4,20,8); mark(20,8,9,19); mark(9,19,4,20); break;
    case prose::Icon::Eraser: mark(4,14,14,4); mark(14,4,20,10); mark(20,10,10,20); mark(10,20,4,14); mark(9,10,15,16); break;
    case prose::Icon::Undo: mark(4,11,18,11); mark(18,11,21,18); mark(4,11,9,5); mark(4,11,9,17); break;
    case prose::Icon::Save: mark(12,3,12,15); mark(12,15,7,10); mark(12,15,17,10); mark(4,17,4,21); mark(4,21,20,21); mark(20,17,20,21); break;
    case prose::Icon::Close: mark(6,6,18,18); mark(6,18,18,6); break;
    case prose::Icon::Settings:
        mark(3,6,21,6); mark(3,12,21,12); mark(3,18,21,18);
        mark(8,3,8,9); mark(16,9,16,15); mark(10,15,10,21);
        break;
    }
}
void ink() {
    if (drawing) frame.drawBitmap(board::canvasX,board::canvasY,drawing->getBuffer(),
                                  board::canvasWidth,board::canvasHeight,pixel(true));
}

void refreshWorker(void*) {
    unsigned partials=0;
    bool initial=true, powered=false;
    for (;;) {
        if (!ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1800))) {
            if (powered) { epd.powerOff(); powered=false; }
            continue;
        }
        xSemaphoreTake(guard,portMAX_DELAY);
        memcpy(working,queued,frameBytes);
        xSemaphoreGive(guard);
        int left=stride, right=-1, top=board::panelHeight, bottom=-1;
        for (int y=0; y<board::panelHeight; ++y) for (int x=0; x<stride; ++x) {
            if (working[y*stride+x]==shown[y*stride+x]) continue;
            left=min(left,x); right=max(right,x); top=min(top,y); bottom=max(bottom,y);
        }
        if (!initial && right<left) continue;
        const int w=(right-left+1)*8, h=bottom-top+1;
        if (initial || partials>=20 || (w*h>200000 && partials>=5)) {
            epd.writeImageForFullRefresh(working,0,0,800,480,true);
            epd.refresh(false);
            partials=0;
        } else {
            const int x=left*8;
            epd.writeImagePart(working,x,top,800,480,x,top,w,h,true);
            epd.refresh(x,top,w,h);
            ++partials;
        }
        powered=true;
        initial=false;
        memcpy(shown,working,frameBytes);
    }
}
} // namespace

bool begin() {
    if (!frame.getBuffer()) return false;
    queued=static_cast<uint8_t*>(ps_malloc(frameBytes));
    working=static_cast<uint8_t*>(ps_malloc(frameBytes));
    shown=static_cast<uint8_t*>(ps_calloc(frameBytes,1));
    guard=xSemaphoreCreateMutex();
    if (!queued || !working || !shown || !guard) return false;
    frame.setRotation(1);
    frame.setTextWrap(false);
    SPI.begin(board::epdSck,-1,board::epdMosi,board::epdCs);
    epd.selectSPI(SPI,SPISettings(4000000,MSBFIRST,SPI_MODE0));
    epd.init(115200,true,10,false);
    return xTaskCreate(refreshWorker,"epaper",4096,nullptr,1,&worker)==pdPASS;
}
int measure(const char* value,int size,prose::Font face) {
    frame.setFont(faceFor(size,face));
    int16_t x,y;
    uint16_t w,h;
    frame.getTextBounds(value,0,0,&x,&y,&w,&h);
    return w;
}
void draw(const prose::UI& ui) {
    if (!worker) return;
    ui.draw({fill,line,text,icon,ink,[](bool enabled){dark=enabled;}});
    xSemaphoreTake(guard,portMAX_DELAY);
    memcpy(queued,frame.getBuffer(),frameBytes);
    xSemaphoreGive(guard);
    xTaskNotifyGive(worker);
}
void setInk(GFXcanvas1* inkCanvas) { drawing=inkCanvas; }
} // namespace panel
