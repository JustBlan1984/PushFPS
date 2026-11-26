#pragma once
#include "veyra/ngx/DlssNrParameters.h"
#include <bit>
#include <cstdint>

namespace veyra::pipeline {
// Scalar INPUT parameters only. Resource bindings and Reset are deliberately
// excluded and remain per-evaluation writes. One cache per feature/parameter block.
class NrInputParameterCache {
public:
    struct Inputs {
        uint32_t width=0,height=0;
        int32_t style=0,autoMask=0,uiCorrection=0;
        uint32_t intensity=0,tone=0,structure=0,skin=0;
        bool operator==(const Inputs&)const=default;
        static Inputs make(uint32_t w,uint32_t h,int32_t style,float intensity,
            float tone,float structure,float skin,int32_t mask,int32_t ui){
            return {w,h,style,mask,ui,std::bit_cast<uint32_t>(intensity),
                std::bit_cast<uint32_t>(tone),std::bit_cast<uint32_t>(structure),std::bit_cast<uint32_t>(skin)};
        }
    };
    bool needsWrite(const Inputs& next)const{return !valid_||!(last_==next);}
    // Commit only AFTER a successful EvaluateFeature. Diagnostic callbacks and
    // disabled/other runtime paths never retain an input cache.
    void completed(const Inputs& next,bool wrote,bool retain){
        if(wrote)++applications;else ++reuses;
        last_=next;valid_=retain;
    }
    void invalidate(){valid_=false;}
    bool rejected()const{return rejected_;}
    void reject(){valid_=false;rejected_=true;}
    void clear(){last_={};valid_=rejected_=false;applications=reuses=0;nextLog100ns=0;}
    uint64_t applications=0,reuses=0;
    int64_t nextLog100ns=0;
    template<class Parameters>static void write(Parameters& pb,const Inputs& in){
        namespace p=veyra::ngx::dlssnr;
        pb.setU32(p::kColorSubrectWidth,in.width);pb.setU32(p::kColorSubrectHeight,in.height);
        pb.setU32(p::kOutputSubrectWidth,in.width);pb.setU32(p::kOutputSubrectHeight,in.height);
        pb.setU32(p::kMVecSubrectWidth,in.width);pb.setU32(p::kMVecSubrectHeight,in.height);
        pb.setU32(p::kDepthSubrectWidth,in.width);pb.setU32(p::kDepthSubrectHeight,in.height);
        pb.setF32(p::kMVecScaleX,1.0f);pb.setF32(p::kMVecScaleY,1.0f);
        pb.setI32(p::kDepthInverted,1);pb.setI32(p::kIndicatorInvertX,0);
        pb.setI32(p::kIndicatorInvertY,0);pb.setI32(p::kEnabled,1);
        pb.setI32(p::kStyle,in.style);pb.setF32(p::kIntensity,std::bit_cast<float>(in.intensity));
        pb.setF32(p::kLocalToneStrength,std::bit_cast<float>(in.tone));
        pb.setF32(p::kLocalStructureStrength,std::bit_cast<float>(in.structure));
        pb.setF32(p::kSkinStructureStrength,std::bit_cast<float>(in.skin));
        pb.setI32(p::kUseAutoMask,in.autoMask);pb.setI32(p::kUICorrection,in.uiCorrection);
    }
private:
    template<class Parameters,class IsSuccess>struct Reader {
        Parameters& raw;IsSuccess success;bool matches=true;
        void setU32(const char* key,uint32_t expected){
            unsigned int value=0;const auto result=raw.Get(key,&value);
            matches=success(result)&&value==expected&&matches;
        }
        void setI32(const char* key,int32_t expected){
            int value=0;const auto result=raw.Get(key,&value);
            matches=success(result)&&value==expected&&matches;
        }
        void setF32(const char* key,float expected){
            float value=0;const auto result=raw.Get(key,&value);
            matches=success(result)&&std::bit_cast<uint32_t>(value)==std::bit_cast<uint32_t>(expected)&&matches;
        }
    };
public:
    template<class Parameters,class IsSuccess>static bool retained(Parameters& raw,const Inputs& in,IsSuccess success){
        Reader<Parameters,IsSuccess> reader{raw,success};
        write(reader,in);return reader.matches;
    }
private:
    Inputs last_{};bool valid_=false,rejected_=false;
};
}
