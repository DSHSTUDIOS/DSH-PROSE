#include "input.hpp"
#include "board.hpp"
#include <Arduino.h>
#include <Wire.h>

namespace input {
namespace {
constexpr uint8_t address=0x5d;
constexpr uint16_t statusReg=0x814e, pointsReg=0x814f;
constexpr uint32_t debounceMs=24, holdMs=550;
struct Key {
    int pin;
    bool raw=false, down=false;
    uint32_t since=0, pressed=0;
    explicit Key(int gpio) : pin(gpio) {}
};
Key keys[]={Key(board::prev),Key(board::mode),Key(board::next)};
uint16_t rawWidth=800, rawHeight=480;
int activeId=-1, lastX=0, lastY=0;
uint32_t lastPoll=0, lastGood=0;
bool touchReady=false;

bool readReg(uint16_t reg,uint8_t* out,size_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg>>8);
    Wire.write(reg&0xff);
    if (Wire.endTransmission(false)!=0 || Wire.requestFrom(address,count)!=count) return false;
    for (size_t i=0; i<count; ++i) out[i]=Wire.read();
    return true;
}
void clearStatus() {
    Wire.beginTransmission(address);
    Wire.write(statusReg>>8);
    Wire.write(statusReg&0xff);
    Wire.write(0);
    Wire.endTransmission();
}
uint16_t word(const uint8_t* p) { return p[0]|uint16_t(p[1])<<8; }

bool touch(prose::UI& ui,bool scribe,uint32_t now) {
    if (!touchReady || now-lastPoll<11) return false;
    lastPoll=now;
    uint8_t status;
    if (!readReg(statusReg,&status,1)) {
        if (activeId<0 || now-lastGood<120) return false;
        ui.pointer(3,lastX,lastY);
        activeId=-1;
        return true;
    }
    lastGood=now;
    if (!(status&0x80)) return false;
    const int count=status&0x0f;
    uint8_t touchData[40];
    const bool valid=count<=5 && (count==0 || readReg(pointsReg,touchData,8*count));
    clearStatus();
    if (!valid) return false;

    int best=-1;
    for (int i=0; i<count; ++i) {
        const auto* p=touchData+8*i;
        if (activeId==p[0]) { best=i; break; }
        if (activeId<0 && (best<0 || word(p+5)<word(touchData+8*best+5))) best=i;
    }
    // In Scribe reject palm-flagged starts and require a single starting contact.
    if (scribe && activeId<0 && ((status&0x40) || count!=1)) best=-1;
    if (best<0) {
        if (activeId<0) return false;
        ui.pointer(2,lastX,lastY);
        activeId=-1;
        return true;
    }

    const auto* point=touchData+8*best;
    const int rawX=min(word(point+1),uint16_t(rawWidth-1));
    const int rawY=min(word(point+3),uint16_t(rawHeight-1));
    const int x=rawWidth>=rawHeight ? rawY*479/(rawHeight-1) : rawX*479/(rawWidth-1);
    const int y=rawWidth>=rawHeight ? 799-rawX*799/(rawWidth-1) : rawY*799/(rawHeight-1);
    if (activeId>=0 && x==lastX && y==lastY) return false;
    ui.pointer(activeId<0 ? 0 : 1,x,y);
    activeId=point[0]; lastX=x; lastY=y;
    return scribe;
}
} // namespace

bool begin() {
    for (auto& key : keys) pinMode(key.pin,INPUT_PULLUP);
    pinMode(board::touchInt,OUTPUT);
    pinMode(board::touchReset,OUTPUT);
    digitalWrite(board::touchReset,LOW);
    digitalWrite(board::touchInt,LOW); // Select the 0x5d I2C address.
    delay(12);
    digitalWrite(board::touchReset,HIGH);
    delay(55);
    pinMode(board::touchInt,INPUT);
    Wire.begin(board::touchSda,board::touchScl,100000);
    Wire.setTimeOut(12);
    uint8_t resolution[4]={};
    touchReady=readReg(0x8146,resolution,sizeof(resolution));
    if (touchReady && word(resolution)>1 && word(resolution+2)>1) {
        rawWidth=word(resolution);
        rawHeight=word(resolution+2);
    }
    return touchReady;
}

bool poll(prose::UI& ui,bool scribe) {
    const uint32_t now=millis();
    bool redraw=touch(ui,scribe,now);
    for (int i=0; i<3; ++i) {
        auto& key=keys[i];
        const bool raw=digitalRead(key.pin)==LOW;
        if (key.raw!=raw) { key.raw=raw; key.since=now; }
        if (key.down==key.raw || now-key.since<debounceMs) continue;
        key.down=raw;
        if (raw) key.pressed=now;
        else {
            if (activeId>=0) { ui.pointer(3,lastX,lastY); activeId=-1; }
            ui.button(prose::Button(i),now-key.pressed>=holdMs);
            redraw=true;
        }
    }
    return redraw;
}
} // namespace input
