#include <catch2/catch_test_macros.hpp>
#include "fpga_manager.hpp"
#include "helpers.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace fpga;

// ---------------------------------------------------------------------------
// Fake sysfs manager tree:
//   <tmp>/
//     fpga0/          <- manager_path
//       state
//       name
//       flags         (optional)
//       firmware_name (optional)
//       firmware      (optional)
//     firmware/       <- firmware_dir
//     design.bit      <- dummy bitstream
// ---------------------------------------------------------------------------
struct FakeManager {
    TmpDir tmp;
    fs::path manager_path;
    fs::path firmware_dir;
    fs::path bitstream;

    FakeManager() {
        manager_path = tmp.path / "fpga0";
        firmware_dir = tmp.path / "firmware";
        bitstream    = tmp.path / "design.bit";

        fs::create_directories(manager_path);
        fs::create_directories(firmware_dir);
        { std::ofstream f(bitstream); f << "FAKE_BITSTREAM"; }
    }

    FpgaManagerConfig make_config(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{200})
    {
        FpgaManagerConfig cfg;
        cfg.manager_path = manager_path;
        cfg.firmware_dir = firmware_dir;
        cfg.timeout      = timeout;
        return cfg;
    }

    void set_state(const std::string& s) {
        std::ofstream(manager_path / "state") << s;
    }

    void set_name(const std::string& s) {
        std::ofstream(manager_path / "name") << s;
    }

    void create_attr(const std::string& name, const std::string& value = "") {
        std::ofstream(manager_path / name) << value;
    }

    std::string read_attr(const std::string& name) {
        std::ifstream f(manager_path / name);
        std::string s;
        std::getline(f, s);
        return s;
    }
};

// ── available / state / name ─────────────────────────────────────────────────

TEST_CASE("FpgaManager: available() true when manager_path exists") {
    FakeManager fake;
    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.available());
}

TEST_CASE("FpgaManager: available() false when manager_path absent") {
    TmpDir tmp;
    FpgaManagerConfig cfg;
    cfg.manager_path = tmp.path / "nonexistent";
    REQUIRE_FALSE(FpgaManager(cfg).available());
}

TEST_CASE("FpgaManager: state() reads state sysfs file") {
    FakeManager fake;
    fake.set_state("operating");
    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.state() == "operating");
}

TEST_CASE("FpgaManager: state() returns empty when file missing") {
    FakeManager fake;
    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.state() == "");
}

TEST_CASE("FpgaManager: name() reads name sysfs file") {
    FakeManager fake;
    fake.set_name("Xilinx Zynq FPGA Manager");
    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.name() == "Xilinx Zynq FPGA Manager");
}

// ── load() failure paths ──────────────────────────────────────────────────────

TEST_CASE("FpgaManager: load() fails when manager not available") {
    TmpDir tmp;
    FpgaManagerConfig cfg;
    cfg.manager_path = tmp.path / "nonexistent";
    cfg.firmware_dir = tmp.path;
    auto r = FpgaManager(cfg).load(tmp.path / "fake.bit");
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::ManagerNotFound);
}

TEST_CASE("FpgaManager: load() fails when bitstream file does not exist") {
    FakeManager fake;
    FpgaManager mgr(fake.make_config());
    auto r = mgr.load(fake.tmp.path / "missing.bit");
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::BitstreamNotFound);
}

TEST_CASE("FpgaManager: load() fails when no firmware trigger attribute exists") {
    FakeManager fake;
    fake.set_state("operating");
    // Neither 'firmware_name' nor 'firmware' attr in manager dir
    FpgaManager mgr(fake.make_config());
    auto r = mgr.load(fake.bitstream);
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::TriggerAttrNotFound);
}

TEST_CASE("FpgaManager: load() fails on error state") {
    FakeManager fake;
    fake.create_attr("firmware_name");
    fake.set_state("error_bitfile");
    FpgaManager mgr(fake.make_config());
    auto r = mgr.load(fake.bitstream);
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::StateError);
    REQUIRE(r.state == "error_bitfile");
}

TEST_CASE("FpgaManager: load() fails on unknown state") {
    FakeManager fake;
    fake.create_attr("firmware_name");
    fake.set_state("unknown");
    FpgaManager mgr(fake.make_config());
    auto r = mgr.load(fake.bitstream);
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::StateError);
}

TEST_CASE("FpgaManager: load() times out when state stays in 'programming'") {
    FakeManager fake;
    fake.create_attr("firmware_name");
    fake.set_state("programming");
    FpgaManager mgr(fake.make_config(std::chrono::milliseconds{120}));
    auto r = mgr.load(fake.bitstream);
    REQUIRE_FALSE(r);
    REQUIRE(r.error == FpgaError::Timeout);
    REQUIRE(r.state == "programming");
}

// ── load() success paths ──────────────────────────────────────────────────────
//
// In tests the state file is pre-set to "operating", so wait_operating()
// returns on the first poll without waiting for real hardware.

TEST_CASE("FpgaManager: load() succeeds via firmware_name attribute") {
    FakeManager fake;
    fake.create_attr("firmware_name");
    fake.set_state("operating");

    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.load(fake.bitstream));

    // firmware was copied to firmware_dir
    REQUIRE(fs::exists(fake.firmware_dir / "design.bit"));

    // firmware_name trigger was written
    REQUIRE(fake.read_attr("firmware_name") == "design.bit");
}

TEST_CASE("FpgaManager: load() falls back to 'firmware' attribute when firmware_name absent") {
    FakeManager fake;
    fake.create_attr("firmware");
    fake.set_state("operating");

    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.load(fake.bitstream));
    REQUIRE(fake.read_attr("firmware") == "design.bit");
}

TEST_CASE("FpgaManager: load() prefers firmware_name over firmware") {
    FakeManager fake;
    fake.create_attr("firmware_name");
    fake.create_attr("firmware");
    fake.set_state("operating");

    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.load(fake.bitstream));

    // firmware_name should be written; firmware should remain empty
    REQUIRE(fake.read_attr("firmware_name") == "design.bit");
    REQUIRE(fake.read_attr("firmware") == "");
}

TEST_CASE("FpgaManager: load() writes flags when flags attribute exists") {
    FakeManager fake;
    fake.create_attr("flags", "0");
    fake.create_attr("firmware_name");
    fake.set_state("operating");

    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.load(fake.bitstream, FpgaFlagPartialReconfig));
    REQUIRE(fake.read_attr("flags") == "1");
}

TEST_CASE("FpgaManager: load() skips flags write when flags attribute absent") {
    FakeManager fake;
    // No 'flags' attr — should not fail
    fake.create_attr("firmware_name");
    fake.set_state("operating");

    FpgaManager mgr(fake.make_config());
    REQUIRE(mgr.load(fake.bitstream, FpgaFlagEncryptedBitstream));
}
