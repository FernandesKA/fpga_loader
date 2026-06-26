#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fpga::utils {

// Copy src into firmware_dir, return firmware filename on success
bool copy_firmware(const std::filesystem::path& src,
                   const std::filesystem::path& firmware_dir,
                   std::string& out_name);

// Write value to a sysfs node (no trailing newline needed)
bool write_sysfs(const std::filesystem::path& node, const std::string& value);

// Read trimmed content of a sysfs node; empty string on error
std::string read_sysfs(const std::filesystem::path& node);

// Write raw binary blob to a sysfs/configfs node
bool write_binary(const std::filesystem::path& node,
                  const std::vector<uint8_t>& data);

} // namespace fpga::utils
