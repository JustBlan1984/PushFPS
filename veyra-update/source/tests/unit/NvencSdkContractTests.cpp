#include <windows.h>
#include "veyra/sink/NvencSdkContract.h"
#include <cstdio>
int main() {
    using namespace veyra::sink::nvenc_contract;
    static_assert(requiredDriverApi == 0xd1);
    static_assert(!supportsCompiledAbi(0xc2));
    static_assert(!supportsCompiledAbi(0xd0));
    static_assert(supportsCompiledAbi(0xd1));
    static_assert(supportsCompiledAbi(0xd2));
    static_assert(supportsCompiledAbi(0xe0));
    std::puts("PASS: SDK 13.1 ABI selected; older driver APIs rejected; matching/newer APIs accepted.");
    return 0;
}
