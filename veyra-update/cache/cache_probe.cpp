#include <algorithm>
#include <array>
#include <numeric>
int veyra_compiler_cache_probe(int input) {
    std::array<int,8> values{8,3,5,2,7,1,6,4};
    std::sort(values.begin(),values.end());
    return input+std::accumulate(values.begin(),values.end(),0);
}
