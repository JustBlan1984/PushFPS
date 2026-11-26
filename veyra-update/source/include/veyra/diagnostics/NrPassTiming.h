#pragma once
#include <cstdint>
#include <optional>
namespace veyra::diagnostics {
// NR-start and layer-start are adjacent markers, in that order on the NVIDIA
// path. Their starts need not be nested. Only the completed NR end must lie
// inside the layer span to measure the following decode/transition tail.
inline std::optional<double> nrDecodeTailMs(uint64_t layerBegin,uint64_t layerEnd,uint64_t layerHz,
    uint64_t evalBegin,uint64_t evalEnd,uint64_t evalHz){
    if(!layerHz||layerHz!=evalHz||layerEnd<layerBegin||evalEnd<evalBegin||
       evalEnd<layerBegin||evalEnd>layerEnd)return {};
    return double(layerEnd-evalEnd)*1000.0/layerHz;
}
}
