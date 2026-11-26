#pragma once
#include <cstdint>
#include <ffnvcodec/nvEncodeAPI.h>

// One complete SDK ABI, including every structure-version macro. Do not
// negotiate an older API using structures compiled from newer headers.
static_assert(NVENCAPI_MAJOR_VERSION == 13 && NVENCAPI_MINOR_VERSION == 1,
              "This build requires the pinned NVIDIA Video Codec SDK 13.1 headers");
namespace veyra::sink::nvenc_contract {
inline constexpr std::uint32_t requiredDriverApi =
    (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
inline constexpr bool supportsCompiledAbi(std::uint32_t driverApi) {
    return driverApi >= requiredDriverApi;
}
}
