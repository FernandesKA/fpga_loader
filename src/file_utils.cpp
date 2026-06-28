/**
 * @file file_utils.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief File and sysfs I/O helpers implementation.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "file_utils.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace fpga::utils {

bool copy_firmware(const std::filesystem::path& src,
                   const std::filesystem::path& firmware_dir,
                   std::string& out_name)
{
    std::error_code ec;
    if (!std::filesystem::exists(src, ec))
        return false;

    out_name = src.filename().string();
    std::filesystem::path dst = firmware_dir / out_name;

    if (std::filesystem::equivalent(src, dst, ec))
        return true;

    std::filesystem::copy_file(
        src, dst,
        std::filesystem::copy_options::overwrite_existing,
        ec);

    return !ec;
}

bool write_sysfs(const std::filesystem::path& node, const std::string& value)
{
    std::ofstream f(node);
    if (!f) {
        std::fprintf(stderr, "error: cannot open %s for writing\n", node.c_str());
        return false;
    }
    f << value;
    return f.good();
}

std::string read_sysfs(const std::filesystem::path& node)
{
    std::ifstream f(node);
    if (!f) return {};
    std::string s;
    std::getline(f, s);
    // trim trailing whitespace
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
        s.pop_back();
    return s;
}

bool write_binary(const std::filesystem::path& node,
                  const std::vector<uint8_t>& data)
{
    std::ofstream f(node, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::fprintf(stderr, "error: cannot open %s for writing\n", node.c_str());
        return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    return f.good();
}

} // namespace fpga::utils
