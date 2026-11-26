#include "veyra/pipeline/NrInputParameterCache.h"
#include "veyra/diagnostics/NrPassTiming.h"
#include <cassert>
#include <map>
#include <string>
#include <iostream>
using Cache=veyra::pipeline::NrInputParameterCache;
struct Parameters {
    struct Wire {char type;uint32_t bits;bool operator==(const Wire&)const=default;};
    std::map<std::string,Wire> values;unsigned calls=0;
    bool Get(const char* k,unsigned int* out){auto i=values.find(k);if(i==values.end()||i->second.type!='u')return false;*out=i->second.bits;return true;}
    bool Get(const char* k,int* out){auto i=values.find(k);if(i==values.end()||i->second.type!='i')return false;*out=std::bit_cast<int32_t>(i->second.bits);return true;}
    bool Get(const char* k,float* out){auto i=values.find(k);if(i==values.end()||i->second.type!='f')return false;*out=std::bit_cast<float>(i->second.bits);return true;}
    void setU32(const char* k,uint32_t v){values[k]={'u',v};++calls;}
    void setI32(const char* k,int32_t v){values[k]={'i',std::bit_cast<uint32_t>(v)};++calls;}
    void setF32(const char* k,float v){values[k]={'f',std::bit_cast<uint32_t>(v)};++calls;}
};
int main(){
    using veyra::diagnostics::nrDecodeTailMs;
    assert(nrDecodeTailMs(100,400,1000,110,350,1000)==50.0);
    assert(nrDecodeTailMs(100,400,1000,110,400,1000)==0.0);
    assert(!nrDecodeTailMs(100,400,0,110,350,0));
    assert(!nrDecodeTailMs(100,400,1000,110,350,2000));
    assert(!nrDecodeTailMs(100,400,1000,90,350,1000));
    assert(!nrDecodeTailMs(100,400,1000,110,450,1000));
    auto key=Cache::Inputs::make(1920,1080,0,1.f,.75f,1.f,.75f,1,0);
    Cache c,secondLayer;Parameters p;
    assert(c.needsWrite(key));Cache::write(p,key);
    assert(p.calls==21&&p.values.size()==21);
    assert(!p.values.contains("DLSSNR.Reset")&&!p.values.contains("DLSSNR.Color")&&!p.values.contains("DLSSNR.MVec"));
    assert(p.values.at("DLSSNR.ColorSubrectWidth").type=='u');
    assert(p.values.at("DLSSNR.Style").type=='i');
    assert(p.values.at("DLSSNR.LocalToneStrength").bits==std::bit_cast<uint32_t>(.75f));
    assert(Cache::retained(p,key,[](bool okay){return okay;}));
    auto mutated=p;mutated.values["DLSSNR.Style"].bits=4;
    assert(!Cache::retained(mutated,key,[](bool okay){return okay;}));
    mutated=p;mutated.values.erase("DLSSNR.Intensity");
    assert(!Cache::retained(mutated,key,[](bool okay){return okay;}));
    mutated=p;mutated.values["DLSSNR.LocalToneStrength"].type='i';
    assert(!Cache::retained(mutated,key,[](bool okay){return okay;}));
    // A failed evaluation has not committed and must retry all scalar writes.
    assert(c.needsWrite(key));c.completed(key,true,true);
    for(int i=0;i<90;++i){assert(!c.needsWrite(key));c.completed(key,false,true);}
    assert(c.applications==1&&c.reuses==90&&secondLayer.needsWrite(key));
    // Every configurable input independently invalidates the cache.
    for(int field=0;field<9;++field){auto k=key;
        switch(field){case 0:++k.width;break;case 1:++k.height;break;case 2:++k.style;break;
        case 3:++k.autoMask;break;case 4:++k.uiCorrection;break;case 5:++k.intensity;break;
        case 6:++k.tone;break;case 7:++k.structure;break;case 8:++k.skin;break;}
        assert(c.needsWrite(k));
    }
    // Preserve exact float wire bits, including signed zero and NaN payloads.
    auto plus=key,minus=key;plus.intensity=0;minus.intensity=0x80000000;
    c.completed(plus,true,true);assert(c.needsWrite(minus));
    Parameters zeros;Cache::write(zeros,minus);assert(zeros.values.at("DLSSNR.Intensity").bits==0x80000000);
    auto nan=key;nan.skin=0x7fc00021;c.completed(nan,true,true);assert(!c.needsWrite(nan));
    Parameters n;Cache::write(n,nan);assert(n.values.at("DLSSNR.SkinStructureStrength").bits==0x7fc00021);
    c.invalidate();assert(c.needsWrite(nan));
    c.completed(key,true,false);assert(c.needsWrite(key)); // baseline/probe path
    c.reject();assert(c.rejected()&&c.needsWrite(key));
    c.clear();assert(!c.rejected()&&c.needsWrite(key)&&c.applications==0&&c.reuses==0);
    std::cout<<"PASS NR input cache: typed 21-key payload, bit-exact floats, all settings, layer isolation, failure retry, invalidation and baseline\n";
}
