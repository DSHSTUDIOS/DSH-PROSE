#pragma once
#include <FS.h>
#include <stdint.h>

namespace journal {
bool read(fs::FS& fs,const char* base,void* data,size_t bytes,uint32_t& revision);
bool write(fs::FS& fs,const char* base,const void* data,size_t bytes,uint32_t& revision);
uint32_t checksum(const void* data,size_t bytes);
}
