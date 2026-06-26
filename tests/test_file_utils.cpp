#include <catch2/catch_test_macros.hpp>
#include "file_utils.hpp"
#include "helpers.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace fpga::utils;

// ── read_sysfs ───────────────────────────────────────────────────────────────

TEST_CASE("read_sysfs: missing file returns empty string") {
    REQUIRE(read_sysfs("/nonexistent/fpga/state") == "");
}

TEST_CASE("read_sysfs: reads first line and strips trailing newline") {
    TmpDir tmp;
    auto node = tmp.path / "state";
    { std::ofstream f(node); f << "operating\n"; }
    REQUIRE(read_sysfs(node) == "operating");
}

TEST_CASE("read_sysfs: strips trailing carriage return (Windows/sysfs CRLF)") {
    TmpDir tmp;
    auto node = tmp.path / "state";
    { std::ofstream f(node, std::ios::binary); f << "operating\r\n"; }
    REQUIRE(read_sysfs(node) == "operating");
}

TEST_CASE("read_sysfs: strips trailing spaces") {
    TmpDir tmp;
    auto node = tmp.path / "state";
    { std::ofstream f(node); f << "value   "; }
    REQUIRE(read_sysfs(node) == "value");
}

TEST_CASE("read_sysfs: does not strip leading whitespace") {
    TmpDir tmp;
    auto node = tmp.path / "state";
    { std::ofstream f(node); f << "  indented\n"; }
    REQUIRE(read_sysfs(node) == "  indented");
}

// ── write_sysfs ──────────────────────────────────────────────────────────────

TEST_CASE("write_sysfs: writes value to file") {
    TmpDir tmp;
    auto node = tmp.path / "flags";
    REQUIRE(write_sysfs(node, "3"));
    std::ifstream f(node);
    std::string got;
    std::getline(f, got);
    REQUIRE(got == "3");
}

TEST_CASE("write_sysfs: overwrites existing content") {
    TmpDir tmp;
    auto node = tmp.path / "attr";
    write_sysfs(node, "old");
    REQUIRE(write_sysfs(node, "new"));
    std::ifstream f(node);
    std::string got;
    std::getline(f, got);
    REQUIRE(got == "new");
}

TEST_CASE("write_sysfs: returns false for non-existent directory") {
    REQUIRE_FALSE(write_sysfs("/no/such/dir/attr", "x"));
}

// ── write_binary ─────────────────────────────────────────────────────────────

TEST_CASE("write_binary: writes raw bytes to file") {
    TmpDir tmp;
    auto node = tmp.path / "blob";
    std::vector<uint8_t> data = {0x01, 0x02, 0xFE, 0xFF};
    REQUIRE(write_binary(node, data));

    std::ifstream f(node, std::ios::binary);
    std::vector<uint8_t> got((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    REQUIRE(got == data);
}

TEST_CASE("write_binary: handles empty data") {
    TmpDir tmp;
    auto node = tmp.path / "empty";
    std::vector<uint8_t> data;
    REQUIRE(write_binary(node, data));
    REQUIRE(fs::file_size(node) == 0);
}

TEST_CASE("write_binary: returns false for non-existent directory") {
    std::vector<uint8_t> data = {0x42};
    REQUIRE_FALSE(write_binary("/no/such/dir/blob", data));
}

// ── copy_firmware ─────────────────────────────────────────────────────────────

TEST_CASE("copy_firmware: returns false when source does not exist") {
    TmpDir tmp;
    std::string out;
    REQUIRE_FALSE(copy_firmware(tmp.path / "missing.bit", tmp.path, out));
}

TEST_CASE("copy_firmware: copies file and returns its filename") {
    TmpDir tmp;
    auto src_dir = tmp.path / "input";
    auto fw_dir  = tmp.path / "firmware";
    fs::create_directories(src_dir);
    fs::create_directories(fw_dir);

    auto src = src_dir / "design.bit";
    { std::ofstream f(src); f << "FAKE_BITSTREAM"; }

    std::string out;
    REQUIRE(copy_firmware(src, fw_dir, out));
    REQUIRE(out == "design.bit");
    REQUIRE(fs::exists(fw_dir / "design.bit"));

    std::ifstream f(fw_dir / "design.bit");
    std::string content((std::istreambuf_iterator<char>(f)), {});
    REQUIRE(content == "FAKE_BITSTREAM");
}

TEST_CASE("copy_firmware: returns true without copying when src is already in firmware_dir") {
    TmpDir tmp;
    auto src = tmp.path / "design.bit";
    { std::ofstream f(src); f << "DATA"; }

    std::string out;
    REQUIRE(copy_firmware(src, tmp.path, out));
    REQUIRE(out == "design.bit");
}

TEST_CASE("copy_firmware: overwrites existing file in firmware_dir") {
    TmpDir tmp;
    auto src_dir = tmp.path / "input";
    auto fw_dir  = tmp.path / "firmware";
    fs::create_directories(src_dir);
    fs::create_directories(fw_dir);

    auto src = src_dir / "design.bit";
    { std::ofstream f(src); f << "NEW_DATA"; }
    { std::ofstream f(fw_dir / "design.bit"); f << "OLD_DATA"; }

    std::string out;
    REQUIRE(copy_firmware(src, fw_dir, out));

    std::ifstream f(fw_dir / "design.bit");
    std::string content((std::istreambuf_iterator<char>(f)), {});
    REQUIRE(content == "NEW_DATA");
}
