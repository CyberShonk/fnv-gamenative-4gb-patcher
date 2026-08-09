#include "patcher.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace fs = std::filesystem;

namespace {

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class TeeStreamBuffer final : public std::streambuf {
public:
    TeeStreamBuffer(std::streambuf* console, std::streambuf* log)
        : console_(console), log_(log) {}

protected:
    int overflow(int ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }
        const char value = traits_type::to_char_type(ch);
        if (traits_type::eq_int_type(console_->sputc(value), traits_type::eof()) ||
            traits_type::eq_int_type(log_->sputc(value), traits_type::eof())) {
            return traits_type::eof();
        }
        return ch;
    }

    std::streamsize xsputn(const char* data, std::streamsize size) override {
        const std::streamsize console_written = console_->sputn(data, size);
        const std::streamsize log_written = log_->sputn(data, size);
        return std::min(console_written, log_written);
    }

    int sync() override {
        return console_->pubsync() == 0 && log_->pubsync() == 0 ? 0 : -1;
    }

private:
    std::streambuf* console_;
    std::streambuf* log_;
};

class PersistentLog final {
public:
    explicit PersistentLog(const fs::path& path)
        : file_(path, std::ios::out | std::ios::app),
          stdout_original_(std::cout.rdbuf()),
          stderr_original_(std::cerr.rdbuf()),
          stdout_tee_(stdout_original_, file_.rdbuf()),
          stderr_tee_(stderr_original_, file_.rdbuf()) {
        if (!file_) {
            throw Error("Unable to create persistent log beside the patcher: " + path.string());
        }
        std::cout.rdbuf(&stdout_tee_);
        std::cerr.rdbuf(&stderr_tee_);
    }

    PersistentLog(const PersistentLog&) = delete;
    PersistentLog& operator=(const PersistentLog&) = delete;

    ~PersistentLog() {
        std::cout.flush();
        std::cerr.flush();
        std::cout.rdbuf(stdout_original_);
        std::cerr.rdbuf(stderr_original_);
    }

private:
    std::ofstream file_;
    std::streambuf* stdout_original_;
    std::streambuf* stderr_original_;
    TeeStreamBuffer stdout_tee_;
    TeeStreamBuffer stderr_tee_;
};

} // namespace

int main(int argc, char** argv) {
    std::optional<PersistentLog> persistent_log;
    try {
        const fs::path executable_path =
            fnv_patcher::process_executable_path(argc > 0 ? argv[0] : nullptr);
        const fs::path directory = executable_path.parent_path();
        const fs::path log_path = directory / fnv_patcher::log_name();
        persistent_log.emplace(log_path);

        const std::string command = argc >= 2 ? argv[1] : "--patch";
        std::cout << "\n=== FNV GameNative patcher session ===\n"
                  << "Patcher version: " << fnv_patcher::tool_version() << "\n"
                  << "Patcher architecture: " << fnv_patcher::patcher_architecture() << "\n"
                  << "Executable path: " << executable_path << "\n"
                  << "Selected target directory: " << directory << "\n"
                  << "Persistent log: " << log_path << "\n"
                  << "Requested operation: " << command << "\n"
                  << "Required-file validation: pending\n";

        std::cout << fnv_patcher::tool_name() << " " << fnv_patcher::tool_version() << "\n";
        fnv_patcher::run_operation(command, directory, std::cout);

        std::cout << (command == "--help" || command == "-h"
                          ? "Required-file validation: not applicable to help output.\n"
                          : "Required-file validation: completed for the requested operation.\n")
                  << "Verification result: operation completed without an uncaught error.\n"
                  << "Final exit status: 0\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << "\n"
                  << "Final exit status: 1\n";
        return 1;
    }
}
