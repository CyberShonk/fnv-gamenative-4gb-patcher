#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

namespace fnv_patcher {

const char* tool_name();
const char* tool_version();
const char* log_name();
const char* patcher_architecture();

std::filesystem::path process_executable_path(const char* argv0);

void run_operation(
    const std::string& command,
    const std::filesystem::path& directory,
    std::ostream& output);

void print_help(std::ostream& output);

} // namespace fnv_patcher
