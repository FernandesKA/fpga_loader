/**
 * @file fpga_manager.cpp
 * @author FernandesKA (fernandes.kir@yandex.ru)
 * @brief FPGA Manager implementation: firmware copy, sysfs trigger,
 *        and state polling until the PL reaches 'operating'.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "fpga_manager.hpp"
#include "file_utils.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace fpga {

static constexpr const char* kStateOperating = "operating";

FpgaManager::FpgaManager(FpgaManagerConfig cfg) : cfg_(std::move(cfg)) {}

bool FpgaManager::available() const
{
    std::error_code ec;
    return std::filesystem::exists(cfg_.manager_path, ec);
}

std::string FpgaManager::state() const
{
    return utils::read_sysfs(cfg_.manager_path / "state");
}

std::string FpgaManager::name() const
{
    return utils::read_sysfs(cfg_.manager_path / "name");
}

LoadResult FpgaManager::load(const std::filesystem::path& bitstream, uint32_t flags)
{
    if (!available())
        return {FpgaError::ManagerNotFound,
                "fpga manager not found at " + cfg_.manager_path.string()};

    if (cfg_.verbose)
        std::printf("[fpga] manager: %s\n", cfg_.manager_path.c_str());

    std::error_code ec;
    if (!std::filesystem::exists(bitstream, ec))
        return {FpgaError::BitstreamNotFound, "bitstream not found: " + bitstream.string()};

    std::string firmware_name;
    if (!utils::copy_firmware(bitstream, cfg_.firmware_dir, firmware_name))
        return {FpgaError::FirmwareCopyFailed,
                "failed to copy " + bitstream.string() + " to " + cfg_.firmware_dir.string()};

    if (cfg_.verbose)
        std::printf("[fpga] firmware: %s/%s\n",
                    cfg_.firmware_dir.c_str(), firmware_name.c_str());

    if (auto r = write_flags(flags); !r) return r;
    if (auto r = trigger(firmware_name); !r) return r;
    return wait_operating();
}

LoadResult FpgaManager::write_flags(uint32_t flags)
{
    auto node = cfg_.manager_path / "flags";
    std::error_code ec;
    if (!std::filesystem::exists(node, ec))
        return {FpgaError::Ok};

    if (!utils::write_sysfs(node, std::to_string(flags)))
        return {FpgaError::FlagsWriteFailed, "cannot write flags to " + node.string()};
    return {FpgaError::Ok};
}

LoadResult FpgaManager::trigger(const std::string& firmware_name)
{
    // Mainline kernels >=4.12 expose "firmware_name"; older Xilinx BSPs use "firmware"
    for (const char* attr : {"firmware_name", "firmware"}) {
        auto node = cfg_.manager_path / attr;
        std::error_code ec;
        if (!std::filesystem::exists(node, ec)) continue;

        if (cfg_.verbose)
            std::printf("[fpga] writing '%s' to %s\n", firmware_name.c_str(), node.c_str());

        if (!utils::write_sysfs(node, firmware_name))
            return {FpgaError::TriggerWriteFailed,
                    "failed to trigger FPGA load via " + node.string()};
        return {FpgaError::Ok};
    }

    return {FpgaError::TriggerAttrNotFound,
            "no firmware trigger attribute found under " + cfg_.manager_path.string() +
            " (tried 'firmware_name' and 'firmware')"};
}

LoadResult FpgaManager::wait_operating()
{
    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + cfg_.timeout;

    while (clock::now() < deadline) {
        std::string s = state();

        if (cfg_.verbose)
            std::printf("[fpga] state: %s\n", s.c_str());

        if (s == kStateOperating)
            return {FpgaError::Ok, {}, s};

        if (s.find("error") != std::string::npos)
            return {FpgaError::StateError, "FPGA manager entered error state: '" + s + "'", s};

        if (s == "unknown")
            return {FpgaError::StateError,
                    "FPGA manager state is 'unknown' after programming request", s};

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::string s = state();
    return {FpgaError::Timeout, "timeout waiting for FPGA state 'operating'", s};
}

} // namespace fpga
