#include <catch2/catch_test_macros.hpp>
#include "dt_overlay.hpp"
#include "helpers.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace fpga;

// ---------------------------------------------------------------------------
// Fake configfs tree:
//   <tmp>/
//     device-tree/
//       overlays/       <- overlay_root (only present when "mounted")
//         <name>/
//           dtbo
//           status
//     test.dtbo         <- dummy overlay blob
// ---------------------------------------------------------------------------
struct FakeConfigfs {
    TmpDir tmp;
    fs::path overlay_root;
    fs::path dtbo_file;

    explicit FakeConfigfs(bool mounted = true) {
        overlay_root = tmp.path / "device-tree" / "overlays";
        if (mounted) fs::create_directories(overlay_root);

        dtbo_file = tmp.path / "test.dtbo";
        // Minimal FDT magic header
        const uint8_t magic[] = {0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x00, 0x00, 0x08};
        std::ofstream f(dtbo_file, std::ios::binary);
        f.write(reinterpret_cast<const char*>(magic), sizeof(magic));
    }

    DtOverlay make_overlay() { return DtOverlay(tmp.path); }

    // Create a fake overlay directory with an optional pre-set status.
    fs::path create_overlay_dir(const std::string& name,
                                const std::string& status = "")
    {
        auto dir = overlay_root / name;
        fs::create_directory(dir);
        if (!status.empty()) std::ofstream(dir / "status") << status;
        return dir;
    }
};

// ── is_mounted ───────────────────────────────────────────────────────────────

TEST_CASE("DtOverlay: is_mounted() true when device-tree dir exists") {
    FakeConfigfs fake;
    REQUIRE(fake.make_overlay().is_mounted());
}

TEST_CASE("DtOverlay: is_mounted() false when device-tree dir absent") {
    FakeConfigfs fake(/*mounted=*/false);
    REQUIRE_FALSE(fake.make_overlay().is_mounted());
}

// ── apply() failure paths ─────────────────────────────────────────────────────

TEST_CASE("DtOverlay: apply() fails when configfs not mounted") {
    FakeConfigfs fake(false);
    REQUIRE_FALSE(fake.make_overlay().apply("my_overlay", fake.dtbo_file));
}

TEST_CASE("DtOverlay: apply() fails when dtbo file does not exist") {
    FakeConfigfs fake;
    REQUIRE_FALSE(fake.make_overlay().apply("my_overlay",
                                             fake.tmp.path / "missing.dtbo"));
}

// The "applied" status is written by the kernel after dtbo is written to the
// configfs node; it cannot be simulated in pure userspace. A full apply()
// success test therefore requires real hardware or a kernel stub. The tests
// above cover all reachable failure branches.

// ── apply() writes dtbo blob ──────────────────────────────────────────────────

TEST_CASE("DtOverlay: apply() creates overlay dir and writes dtbo") {
    // apply() will return false (status stays empty without kernel), but we
    // can confirm it got far enough to write the dtbo file.
    FakeConfigfs fake;
    auto dir = fake.overlay_root / "my_overlay";

    fake.make_overlay().apply("my_overlay", fake.dtbo_file);

    REQUIRE(fs::exists(dir / "dtbo"));

    // Content must match the original dtbo
    std::ifstream a(dir / "dtbo", std::ios::binary);
    std::ifstream b(fake.dtbo_file, std::ios::binary);
    std::string ca((std::istreambuf_iterator<char>(a)), {});
    std::string cb((std::istreambuf_iterator<char>(b)), {});
    REQUIRE(ca == cb);
}

TEST_CASE("DtOverlay: apply() removes stale overlay before re-applying") {
    FakeConfigfs fake;
    // Empty dir simulates a stale overlay from a previous run (real kernel dirs
    // are always empty and can be removed with rmdir, not recursive removal).
    fake.create_overlay_dir("my_overlay");

    auto dir = fake.overlay_root / "my_overlay";
    // apply() removes the stale dir, creates a fresh one, and writes dtbo.
    // It returns false (status stays empty without kernel), but dtbo must exist.
    fake.make_overlay().apply("my_overlay", fake.dtbo_file);

    REQUIRE(fs::exists(dir / "dtbo"));
}

// ── remove() ─────────────────────────────────────────────────────────────────

TEST_CASE("DtOverlay: remove() returns true when overlay does not exist") {
    FakeConfigfs fake;
    REQUIRE(fake.make_overlay().remove("nonexistent"));
}

TEST_CASE("DtOverlay: remove() deletes existing overlay directory") {
    FakeConfigfs fake;
    auto dir = fake.create_overlay_dir("my_overlay");
    REQUIRE(fake.make_overlay().remove("my_overlay"));
    REQUIRE_FALSE(fs::exists(dir));
}

// ── status() ─────────────────────────────────────────────────────────────────

TEST_CASE("DtOverlay: status() returns empty when overlay does not exist") {
    FakeConfigfs fake;
    REQUIRE(fake.make_overlay().status("nonexistent") == "");
}

TEST_CASE("DtOverlay: status() reads status file") {
    FakeConfigfs fake;
    fake.create_overlay_dir("my_overlay", "applied");
    REQUIRE(fake.make_overlay().status("my_overlay") == "applied");
}

// ── list() ───────────────────────────────────────────────────────────────────

TEST_CASE("DtOverlay: list() returns empty when no overlays exist") {
    FakeConfigfs fake;
    REQUIRE(fake.make_overlay().list().empty());
}

TEST_CASE("DtOverlay: list() returns all overlay directories with their status") {
    FakeConfigfs fake;
    fake.create_overlay_dir("overlay_a", "applied");
    fake.create_overlay_dir("overlay_b", "applied");
    // A plain file inside overlays/ must be ignored
    std::ofstream(fake.overlay_root / "not_a_dir.txt") << "noise";

    auto result = fake.make_overlay().list();
    REQUIRE(result.size() == 2);

    std::map<std::string, std::string> by_name(result.begin(), result.end());
    REQUIRE(by_name.at("overlay_a") == "applied");
    REQUIRE(by_name.at("overlay_b") == "applied");
}

TEST_CASE("DtOverlay: list() reflects current status of each overlay") {
    FakeConfigfs fake;
    fake.create_overlay_dir("ov_loading",  "loading");
    fake.create_overlay_dir("ov_applied",  "applied");
    fake.create_overlay_dir("ov_removing", "removing");

    auto result = fake.make_overlay().list();
    std::map<std::string, std::string> by_name(result.begin(), result.end());

    REQUIRE(by_name.at("ov_loading")  == "loading");
    REQUIRE(by_name.at("ov_applied")  == "applied");
    REQUIRE(by_name.at("ov_removing") == "removing");
}
