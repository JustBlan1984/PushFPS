#pragma once
#include <cstdint>
#include <optional>
namespace veyra::diagnostics {
// Existing per-layer timestamps enclose Evaluate plus the following parity
// decode/transitions. Report only the tail, not the overlapping whole layer.
inline std::optional<double> nrDecodeTailMs(uint64_t layerBegin,uint64_t layerEnd,uint64_t layerHz,
    uint64_t evalBegin,uint64_t evalEnd,uint64_t evalHz){
    if(!layerHz||layerHz!=evalHz||layerEnd<layerBegin||evalEnd<evalBegin||
       evalBegin<layerBegin||evalEnd>layerEnd)return {};
    return double(layerEnd-evalEnd)*1000.0/layerHz;
}
}
