#include "veyra/ngx/DlssgLite.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("pushfps-dlss-policy-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto require = [](bool condition) { if (!condition) throw std::runtime_error("DLSS runtime selection failed"); };
    try {
        fs::create_directories(root / "dlss-current");
        require(veyra::ngx::dlssCurrentDirectory(root, true, false).empty());
        std::ofstream(root / "dlss-current" / "nvngx_dlss.dll").put('x');
        std::ofstream(root / "dlss-current" / "nvngx_dlssg.dll").put('x');
        require(veyra::ngx::dlssCurrentDirectory(root, true, false).empty());
        std::ofstream(root / "dlss-current" / "nvngx_dlssd.dll").put('x');
        require(veyra::ngx::dlssCurrentDirectory(root, true, false) == root / "dlss-current");
        require(veyra::ngx::dlssCurrentDirectory(root, true, true).empty());
        require(veyra::ngx::dlssCurrentDirectory(root, false, false).empty());
        fs::remove(root / "dlss-current" / "nvngx_dlssg.dll");
        fs::create_directory(root / "dlss-current" / "nvngx_dlssg.dll");
        require(veyra::ngx::dlssCurrentDirectory(root, true, false).empty());
        fs::remove_all(root);
        std::cout << "DLSS runtime selection: complete, missing, partial, compatibility, vendor, directory checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::error_code ignored; fs::remove_all(root, ignored);
        std::cerr << e.what() << '\n'; return 1;
    }
}
