#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>

// RAII temporary directory via mkdtemp(3).
struct TmpDir {
    std::filesystem::path path;

    TmpDir() {
        char tmpl[] = "/tmp/fpga_test_XXXXXX";
        const char* p = mkdtemp(tmpl);
        if (!p) throw std::runtime_error("mkdtemp failed");
        path = p;
    }

    ~TmpDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TmpDir(const TmpDir&)            = delete;
    TmpDir& operator=(const TmpDir&) = delete;
};
