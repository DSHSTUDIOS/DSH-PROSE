#include "journal.hpp"
#include <esp_rom_crc.h>

namespace journal {
namespace {
struct Header {
    uint32_t signature, revision, length, crc;
};
constexpr uint32_t signature=0x50524f31;
void pathFor(char* path,const char* base,uint32_t slot) {
    snprintf(path,96,"%s.%u",base,unsigned(slot));
}
bool inspect(fs::FS& fs,const char* path,size_t bytes,Header& h) {
    auto file=fs.open(path);
    if (!file || file.read(reinterpret_cast<uint8_t*>(&h),sizeof(h))!=sizeof(h) ||
        h.signature!=signature || h.length!=bytes || file.size()!=sizeof(h)+bytes) return false;
    uint32_t crc=0;
    uint8_t block[256];
    while (bytes>0) {
        const size_t count=bytes<sizeof(block) ? bytes : sizeof(block);
        if (file.read(block,count)!=count) return false;
        crc=esp_rom_crc32_le(crc,block,count);
        bytes-=count;
    }
    return crc==h.crc;
}
}

uint32_t checksum(const void* data,size_t size) {
    return esp_rom_crc32_le(0,static_cast<const uint8_t*>(data),size);
}
bool read(fs::FS& fs,const char* base,void* data,size_t bytes,uint32_t& revision) {
    char path[96];
    Header newest{};
    int slot=-1;
    for (int i=0;i<2;++i) {
        Header h{};
        pathFor(path,base,i);
        if (inspect(fs,path,bytes,h) && (slot<0 || int32_t(h.revision-newest.revision)>0)) {
            newest=h;
            slot=i;
        }
    }
    if (slot<0) return false;
    pathFor(path,base,slot);
    auto file=fs.open(path);
    if (!file || !file.seek(sizeof(Header)) ||
        file.read(static_cast<uint8_t*>(data),bytes)!=bytes ||
        checksum(data,bytes)!=newest.crc) return false;
    revision=newest.revision;
    return true;
}
bool write(fs::FS& fs,const char* base,const void* data,size_t bytes,uint32_t& revision) {
    const Header header{signature,revision+1,uint32_t(bytes),checksum(data,bytes)};
    char path[96];
    pathFor(path,base,header.revision&1);
    auto file=fs.open(path,"w");
    if (!file) return false;
    const bool complete=file.write(reinterpret_cast<const uint8_t*>(&header),sizeof(header))==sizeof(header) &&
                        file.write(static_cast<const uint8_t*>(data),bytes)==bytes;
    file.flush();
    file.close();
    Header verify{};
    if (!complete || !inspect(fs,path,bytes,verify) || verify.revision!=header.revision) return false;
    revision=header.revision;
    return true;
}
}
