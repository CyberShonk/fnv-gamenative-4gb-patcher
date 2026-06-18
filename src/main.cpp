#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char* kToolName = "FNV GameNative 4GB + xNVSE Patcher";
constexpr const char* kToolVersion = "0.1.0-alpha";
constexpr const char* kTargetName = "FalloutNV.exe";
constexpr const char* kBackupName = "FalloutNV.exe.gn4gb-backup";
constexpr const char* kTempName = "FalloutNV.exe.gn4gb-temp";
constexpr const char* kSectionName = ".gnvse";
constexpr const char* kMarker = "FNVGN4GB-V1";
constexpr std::uint16_t kMachineI386 = 0x014c;
constexpr std::uint16_t kPe32Magic = 0x010b;
constexpr std::uint16_t kLargeAddressAware = 0x0020;
constexpr std::uint16_t kExecutableImage = 0x0002;
constexpr std::uint16_t kDllFlag = 0x2000;
constexpr std::uint16_t kDynamicBase = 0x0040;
constexpr std::uint32_t kSectionCodeExecuteRead = 0x60000020;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kImportDirectoryIndex = 1;
constexpr std::size_t kSecurityDirectoryIndex = 4;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::uint16_t read_u16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size()) throw Error("Unexpected end of file while reading a 16-bit value.");
    return static_cast<std::uint16_t>(data[offset]) |
           (static_cast<std::uint16_t>(data[offset + 1]) << 8);
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size()) throw Error("Unexpected end of file while reading a 32-bit value.");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

void write_u16(std::vector<std::uint8_t>& data, std::size_t offset, std::uint16_t value) {
    if (offset + 2 > data.size()) throw Error("Unexpected end of file while writing a 16-bit value.");
    data[offset] = static_cast<std::uint8_t>(value & 0xff);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void write_u32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
    if (offset + 4 > data.size()) throw Error("Unexpected end of file while writing a 32-bit value.");
    data[offset] = static_cast<std::uint8_t>(value & 0xff);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
    data[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
}

std::uint32_t align_up(std::uint32_t value, std::uint32_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw Error("Invalid PE alignment value.");
    }
    const std::uint64_t result = (static_cast<std::uint64_t>(value) + alignment - 1) & ~(static_cast<std::uint64_t>(alignment) - 1);
    if (result > std::numeric_limits<std::uint32_t>::max()) throw Error("PE alignment overflow.");
    return static_cast<std::uint32_t>(result);
}

std::vector<std::uint8_t> read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw Error("Unable to open " + path.string() + ".");
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    if (length <= 0) throw Error(path.string() + " is empty.");
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!input) throw Error("Unable to read all of " + path.string() + ".");
    return data;
}

void write_file(const fs::path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw Error("Unable to create " + path.string() + ".");
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    output.flush();
    if (!output) throw Error("Unable to write all of " + path.string() + ".");
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string read_c_string(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t max_length = 512) {
    if (offset >= data.size()) throw Error("String points outside the executable.");
    std::string result;
    for (std::size_t i = 0; i < max_length && offset + i < data.size(); ++i) {
        const char ch = static_cast<char>(data[offset + i]);
        if (ch == '\0') return result;
        result.push_back(ch);
    }
    throw Error("Unterminated string in executable.");
}

bool contains_ascii(const std::vector<std::uint8_t>& data, const std::string& text) {
    return std::search(data.begin(), data.end(), text.begin(), text.end()) != data.end();
}

struct Section {
    std::string name;
    std::size_t header_offset{};
    std::uint32_t virtual_size{};
    std::uint32_t virtual_address{};
    std::uint32_t raw_size{};
    std::uint32_t raw_pointer{};
    std::uint32_t characteristics{};
};

struct PeImage {
    std::size_t pe_offset{};
    std::size_t coff_offset{};
    std::size_t optional_offset{};
    std::size_t section_table_offset{};
    std::uint16_t number_of_sections{};
    std::uint16_t size_of_optional_header{};
    std::uint16_t characteristics{};
    std::uint16_t dll_characteristics{};
    std::uint32_t entry_point_rva{};
    std::uint32_t image_base{};
    std::uint32_t section_alignment{};
    std::uint32_t file_alignment{};
    std::uint32_t size_of_headers{};
    std::uint32_t size_of_image{};
    std::uint32_t number_of_rva_and_sizes{};
    std::vector<Section> sections;
};

PeImage parse_pe(const std::vector<std::uint8_t>& data) {
    if (data.size() < 0x100) throw Error("File is too small to be a Fallout New Vegas executable.");
    if (data[0] != 'M' || data[1] != 'Z') throw Error("Missing DOS MZ signature.");

    PeImage pe;
    pe.pe_offset = read_u32(data, 0x3c);
    if (pe.pe_offset + 24 > data.size()) throw Error("PE header points outside the file.");
    if (data[pe.pe_offset] != 'P' || data[pe.pe_offset + 1] != 'E' ||
        data[pe.pe_offset + 2] != 0 || data[pe.pe_offset + 3] != 0) {
        throw Error("Missing PE signature.");
    }

    pe.coff_offset = pe.pe_offset + 4;
    const auto machine = read_u16(data, pe.coff_offset);
    if (machine != kMachineI386) throw Error("Only the 32-bit x86 FalloutNV.exe is supported.");
    pe.number_of_sections = read_u16(data, pe.coff_offset + 2);
    if (pe.number_of_sections == 0 || pe.number_of_sections >= 96) throw Error("Invalid PE section count.");
    pe.size_of_optional_header = read_u16(data, pe.coff_offset + 16);
    pe.characteristics = read_u16(data, pe.coff_offset + 18);
    if ((pe.characteristics & kExecutableImage) == 0 || (pe.characteristics & kDllFlag) != 0) {
        throw Error("Target is not a normal executable image.");
    }

    pe.optional_offset = pe.coff_offset + 20;
    if (pe.optional_offset + pe.size_of_optional_header > data.size()) throw Error("Optional header points outside the file.");
    if (pe.size_of_optional_header < 96 + 16 * 8) throw Error("Optional header is too small for a normal PE32 executable.");
    if (read_u16(data, pe.optional_offset) != kPe32Magic) throw Error("Only PE32 executables are supported.");

    pe.entry_point_rva = read_u32(data, pe.optional_offset + 16);
    pe.image_base = read_u32(data, pe.optional_offset + 28);
    pe.section_alignment = read_u32(data, pe.optional_offset + 32);
    pe.file_alignment = read_u32(data, pe.optional_offset + 36);
    pe.size_of_image = read_u32(data, pe.optional_offset + 56);
    pe.size_of_headers = read_u32(data, pe.optional_offset + 60);
    pe.dll_characteristics = read_u16(data, pe.optional_offset + 70);
    pe.number_of_rva_and_sizes = read_u32(data, pe.optional_offset + 92);
    if (pe.number_of_rva_and_sizes < 5) throw Error("PE data-directory table is incomplete.");
    if ((pe.dll_characteristics & kDynamicBase) != 0) {
        throw Error("This executable uses ASLR/DYNAMIC_BASE, which this alpha patcher intentionally refuses to modify.");
    }

    pe.section_table_offset = pe.optional_offset + pe.size_of_optional_header;
    if (pe.section_table_offset + static_cast<std::size_t>(pe.number_of_sections) * kSectionHeaderSize > data.size()) {
        throw Error("Section table points outside the file.");
    }

    for (std::uint16_t i = 0; i < pe.number_of_sections; ++i) {
        const std::size_t offset = pe.section_table_offset + static_cast<std::size_t>(i) * kSectionHeaderSize;
        Section section;
        section.header_offset = offset;
        for (std::size_t n = 0; n < 8 && data[offset + n] != 0; ++n) {
            section.name.push_back(static_cast<char>(data[offset + n]));
        }
        section.virtual_size = read_u32(data, offset + 8);
        section.virtual_address = read_u32(data, offset + 12);
        section.raw_size = read_u32(data, offset + 16);
        section.raw_pointer = read_u32(data, offset + 20);
        section.characteristics = read_u32(data, offset + 36);
        if (section.raw_size != 0 && static_cast<std::uint64_t>(section.raw_pointer) + section.raw_size > data.size()) {
            throw Error("Section " + section.name + " points outside the file.");
        }
        pe.sections.push_back(section);
    }
    return pe;
}

std::size_t rva_to_offset(const std::vector<std::uint8_t>& data, const PeImage& pe, std::uint32_t rva) {
    if (rva < pe.size_of_headers) {
        if (rva >= data.size()) throw Error("Header RVA points outside the file.");
        return rva;
    }
    for (const auto& section : pe.sections) {
        const std::uint32_t span = std::max(section.virtual_size, section.raw_size);
        if (rva >= section.virtual_address && rva < section.virtual_address + span) {
            const std::uint32_t relative = rva - section.virtual_address;
            if (relative >= section.raw_size) throw Error("RVA points into virtual-only section data.");
            const std::uint64_t offset = static_cast<std::uint64_t>(section.raw_pointer) + relative;
            if (offset >= data.size()) throw Error("RVA resolves outside the file.");
            return static_cast<std::size_t>(offset);
        }
    }
    throw Error("Unable to resolve an RVA to file data.");
}

std::pair<std::uint32_t, std::uint32_t> data_directory(const std::vector<std::uint8_t>& data, const PeImage& pe, std::size_t index) {
    if (index >= pe.number_of_rva_and_sizes) return {0, 0};
    const std::size_t offset = pe.optional_offset + 96 + index * 8;
    if (offset + 8 > pe.optional_offset + pe.size_of_optional_header) throw Error("Data directory exceeds optional header.");
    return {read_u32(data, offset), read_u32(data, offset + 4)};
}

bool has_section(const PeImage& pe, const std::string& name) {
    return std::any_of(pe.sections.begin(), pe.sections.end(), [&](const Section& section) {
        return section.name == name;
    });
}

std::uint32_t find_import_iat_rva(const std::vector<std::uint8_t>& data, const PeImage& pe, const std::string& wanted_name) {
    const auto [import_rva, import_size] = data_directory(data, pe, kImportDirectoryIndex);
    if (import_rva == 0 || import_size < 20) throw Error("Executable has no usable import directory.");
    std::size_t descriptor_offset = rva_to_offset(data, pe, import_rva);

    for (std::size_t descriptor_index = 0; descriptor_index < 256; ++descriptor_index, descriptor_offset += 20) {
        if (descriptor_offset + 20 > data.size()) throw Error("Import descriptor table is truncated.");
        const std::uint32_t original_first_thunk = read_u32(data, descriptor_offset);
        const std::uint32_t name_rva = read_u32(data, descriptor_offset + 12);
        const std::uint32_t first_thunk = read_u32(data, descriptor_offset + 16);
        if (original_first_thunk == 0 && name_rva == 0 && first_thunk == 0) break;
        if (name_rva == 0 || first_thunk == 0) continue;

        const auto dll_name = lower_ascii(read_c_string(data, rva_to_offset(data, pe, name_rva)));
        if (dll_name != "kernel32.dll" && dll_name != "kernelbase.dll") continue;

        const std::uint32_t lookup_rva = original_first_thunk != 0 ? original_first_thunk : first_thunk;
        std::size_t lookup_offset = rva_to_offset(data, pe, lookup_rva);
        for (std::uint32_t index = 0; index < 4096; ++index, lookup_offset += 4) {
            if (lookup_offset + 4 > data.size()) throw Error("Import thunk table is truncated.");
            const std::uint32_t thunk = read_u32(data, lookup_offset);
            if (thunk == 0) break;
            if ((thunk & 0x80000000u) != 0) continue;
            const std::size_t import_name_offset = rva_to_offset(data, pe, thunk);
            const std::string import_name = read_c_string(data, import_name_offset + 2);
            if (import_name == wanted_name) return first_thunk + index * 4;
        }
    }
    throw Error("FalloutNV.exe does not import " + wanted_name + "; refusing to inject a fragile loader shim.");
}

bool looks_like_fallout_nv(const std::vector<std::uint8_t>& data) {
    return contains_ascii(data, "FalloutNV") || contains_ascii(data, "Fallout: New Vegas");
}

bool is_patched(const std::vector<std::uint8_t>& data, const PeImage& pe) {
    return has_section(pe, kSectionName) && contains_ascii(data, kMarker);
}

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value & 0xff));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
}

std::vector<std::uint8_t> make_loader_payload(
    std::uint32_t image_base,
    std::uint32_t section_rva,
    std::uint32_t original_entry_rva,
    std::uint32_t load_library_iat_rva) {

    std::vector<std::uint8_t> payload;
    payload.reserve(128);
    payload.push_back(0x9c); // pushfd
    payload.push_back(0x60); // pushad
    payload.push_back(0x68); // push imm32: path to nvse_steam_loader.dll
    const std::size_t string_va_patch = payload.size();
    append_u32(payload, 0);
    payload.push_back(0xff); // call dword ptr [LoadLibraryA IAT]
    payload.push_back(0x15);
    append_u32(payload, image_base + load_library_iat_rva);
    payload.push_back(0x61); // popad
    payload.push_back(0x9d); // popfd
    payload.push_back(0xe9); // jmp original entry point
    const std::size_t jump_patch = payload.size();
    append_u32(payload, 0);

    const std::size_t string_offset = payload.size();
    const std::string dll_name = "nvse_steam_loader.dll";
    payload.insert(payload.end(), dll_name.begin(), dll_name.end());
    payload.push_back(0);
    payload.insert(payload.end(), kMarker, kMarker + std::char_traits<char>::length(kMarker));
    payload.push_back(0);
    append_u32(payload, original_entry_rva);

    const std::uint32_t string_va = image_base + section_rva + static_cast<std::uint32_t>(string_offset);
    payload[string_va_patch] = static_cast<std::uint8_t>(string_va & 0xff);
    payload[string_va_patch + 1] = static_cast<std::uint8_t>((string_va >> 8) & 0xff);
    payload[string_va_patch + 2] = static_cast<std::uint8_t>((string_va >> 16) & 0xff);
    payload[string_va_patch + 3] = static_cast<std::uint8_t>((string_va >> 24) & 0xff);

    const std::uint32_t next_instruction_rva = section_rva + static_cast<std::uint32_t>(jump_patch + 4);
    const std::int64_t displacement = static_cast<std::int64_t>(original_entry_rva) - next_instruction_rva;
    if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()) {
        throw Error("Original entry point is too far away for the loader shim.");
    }
    const auto relative = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
    payload[jump_patch] = static_cast<std::uint8_t>(relative & 0xff);
    payload[jump_patch + 1] = static_cast<std::uint8_t>((relative >> 8) & 0xff);
    payload[jump_patch + 2] = static_cast<std::uint8_t>((relative >> 16) & 0xff);
    payload[jump_patch + 3] = static_cast<std::uint8_t>((relative >> 24) & 0xff);
    return payload;
}

std::vector<std::uint8_t> patch_image(std::vector<std::uint8_t> data) {
    PeImage pe = parse_pe(data);
    if (!looks_like_fallout_nv(data)) throw Error("Executable does not contain expected Fallout New Vegas identity strings.");
    if (has_section(pe, ".bind")) {
        throw Error("The Steam wrapper section '.bind' is still present. Run GameNative's Unpack Files operation first, then run this patcher.");
    }
    if (is_patched(data, pe)) return data;

    const auto [security_offset, security_size] = data_directory(data, pe, kSecurityDirectoryIndex);
    if (security_offset != 0 || security_size != 0) {
        throw Error("Executable contains an Authenticode security directory. This alpha patcher will not invalidate signed files.");
    }

    const std::uint32_t load_library_iat_rva = find_import_iat_rva(data, pe, "LoadLibraryA");

    const std::size_t new_header_offset = pe.section_table_offset + static_cast<std::size_t>(pe.number_of_sections) * kSectionHeaderSize;
    const auto first_raw_it = std::min_element(pe.sections.begin(), pe.sections.end(), [](const Section& a, const Section& b) {
        const auto a_ptr = a.raw_size == 0 ? std::numeric_limits<std::uint32_t>::max() : a.raw_pointer;
        const auto b_ptr = b.raw_size == 0 ? std::numeric_limits<std::uint32_t>::max() : b.raw_pointer;
        return a_ptr < b_ptr;
    });
    if (first_raw_it == pe.sections.end() || first_raw_it->raw_pointer == std::numeric_limits<std::uint32_t>::max()) {
        throw Error("Executable has no file-backed sections.");
    }
    if (new_header_offset + kSectionHeaderSize > first_raw_it->raw_pointer ||
        new_header_offset + kSectionHeaderSize > pe.size_of_headers) {
        throw Error("There is not enough safe PE-header padding to add the xNVSE loader section.");
    }
    for (std::size_t i = new_header_offset; i < new_header_offset + kSectionHeaderSize; ++i) {
        if (data[i] != 0) throw Error("The next PE section-header slot is not empty; refusing to overwrite unknown header data.");
    }

    const Section& last = *std::max_element(pe.sections.begin(), pe.sections.end(), [](const Section& a, const Section& b) {
        return a.virtual_address < b.virtual_address;
    });
    const std::uint32_t new_section_rva = align_up(last.virtual_address + std::max(last.virtual_size, last.raw_size), pe.section_alignment);
    const std::uint32_t new_raw_pointer = align_up(static_cast<std::uint32_t>(data.size()), pe.file_alignment);
    const auto payload = make_loader_payload(pe.image_base, new_section_rva, pe.entry_point_rva, load_library_iat_rva);
    const std::uint32_t new_raw_size = align_up(static_cast<std::uint32_t>(payload.size()), pe.file_alignment);
    const std::uint32_t new_size_of_image = align_up(new_section_rva + static_cast<std::uint32_t>(payload.size()), pe.section_alignment);

    if (data.size() < new_raw_pointer) data.resize(new_raw_pointer, 0);
    data.resize(static_cast<std::size_t>(new_raw_pointer) + new_raw_size, 0);
    std::copy(payload.begin(), payload.end(), data.begin() + new_raw_pointer);

    std::array<std::uint8_t, 8> name{};
    std::copy(kSectionName, kSectionName + std::char_traits<char>::length(kSectionName), name.begin());
    std::copy(name.begin(), name.end(), data.begin() + static_cast<std::ptrdiff_t>(new_header_offset));
    write_u32(data, new_header_offset + 8, static_cast<std::uint32_t>(payload.size()));
    write_u32(data, new_header_offset + 12, new_section_rva);
    write_u32(data, new_header_offset + 16, new_raw_size);
    write_u32(data, new_header_offset + 20, new_raw_pointer);
    write_u32(data, new_header_offset + 24, 0);
    write_u32(data, new_header_offset + 28, 0);
    write_u16(data, new_header_offset + 32, 0);
    write_u16(data, new_header_offset + 34, 0);
    write_u32(data, new_header_offset + 36, kSectionCodeExecuteRead);

    write_u16(data, pe.coff_offset + 2, static_cast<std::uint16_t>(pe.number_of_sections + 1));
    write_u16(data, pe.coff_offset + 18, static_cast<std::uint16_t>(pe.characteristics | kLargeAddressAware));
    write_u32(data, pe.optional_offset + 4, read_u32(data, pe.optional_offset + 4) + new_raw_size); // SizeOfCode
    write_u32(data, pe.optional_offset + 16, new_section_rva); // AddressOfEntryPoint
    write_u32(data, pe.optional_offset + 56, new_size_of_image); // SizeOfImage
    write_u32(data, pe.optional_offset + 64, 0); // CheckSum

    const PeImage patched = parse_pe(data);
    if (!is_patched(data, patched)) throw Error("Internal verification failed after constructing the patched executable.");
    if ((patched.characteristics & kLargeAddressAware) == 0) throw Error("Internal verification failed to enable Large Address Aware.");
    return data;
}

void replace_target_safely(const fs::path& target, const fs::path& temp, const fs::path& backup) {
    std::error_code ec;
    fs::remove(target, ec);
    if (ec) throw Error("Unable to replace FalloutNV.exe: " + ec.message());
    ec.clear();
    fs::rename(temp, target, ec);
    if (!ec) return;

    std::error_code restore_error;
    fs::copy_file(backup, target, fs::copy_options::overwrite_existing, restore_error);
    if (restore_error) {
        throw Error("Failed to install the patched executable and failed to restore the backup. Manual recovery file: " + backup.string());
    }
    throw Error("Failed to install the patched executable; the original was restored: " + ec.message());
}

void verify_environment(const fs::path& directory) {
    if (!fs::exists(directory / "nvse_steam_loader.dll")) {
        throw Error("nvse_steam_loader.dll was not found. Install the current xNVSE files in the Fallout New Vegas game folder first.");
    }
    if (!fs::exists(directory / "nvse_1_4.dll")) {
        throw Error("nvse_1_4.dll was not found. Install the current xNVSE files in the Fallout New Vegas game folder first.");
    }
}

void patch(const fs::path& directory) {
    const fs::path target = directory / kTargetName;
    const fs::path backup = directory / kBackupName;
    const fs::path temp = directory / kTempName;
    if (!fs::exists(target)) throw Error("FalloutNV.exe was not found in the current folder.");
    verify_environment(directory);

    const auto original = read_file(target);
    const auto parsed = parse_pe(original);
    if (is_patched(original, parsed)) {
        std::cout << "FalloutNV.exe is already patched for GameNative 4GB + xNVSE loading.\n";
        return;
    }

    if (!fs::exists(backup)) {
        std::error_code ec;
        fs::copy_file(target, backup, fs::copy_options::none, ec);
        if (ec) throw Error("Unable to create the non-EXE backup " + backup.string() + ": " + ec.message());
    } else {
        const auto backup_data = read_file(backup);
        const auto backup_pe = parse_pe(backup_data);
        if (is_patched(backup_data, backup_pe)) {
            throw Error("Existing backup is already patched. Move it aside and restore a clean GameNative-unpacked FalloutNV.exe before continuing.");
        }
        if (backup_data != original) {
            throw Error("An existing backup does not match the current unpatched FalloutNV.exe. Preserve it manually, then remove or rename " + std::string(kBackupName) + " before patching this executable.");
        }
    }

    auto patched = patch_image(original);
    write_file(temp, patched);
    const auto round_trip = read_file(temp);
    const auto round_trip_pe = parse_pe(round_trip);
    if (!is_patched(round_trip, round_trip_pe) || (round_trip_pe.characteristics & kLargeAddressAware) == 0) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        throw Error("Temporary patched executable failed verification.");
    }
    replace_target_safely(target, temp, backup);
    const auto installed = read_file(target);
    const auto installed_pe = parse_pe(installed);
    if (!is_patched(installed, installed_pe) || (installed_pe.characteristics & kLargeAddressAware) == 0) {
        throw Error("Installed FalloutNV.exe failed final verification. Restore with --restore before launching the game.");
    }
    std::cout << "Patched FalloutNV.exe successfully.\n"
              << "  - Large Address Aware enabled\n"
              << "  - nvse_steam_loader.dll will load before the original entry point\n"
              << "  - Original saved as " << kBackupName << "\n";
}

void restore(const fs::path& directory) {
    const fs::path target = directory / kTargetName;
    const fs::path backup = directory / kBackupName;
    const fs::path temp = directory / kTempName;
    if (!fs::exists(backup)) throw Error("No " + std::string(kBackupName) + " backup was found.");
    const auto backup_data = read_file(backup);
    const auto backup_pe = parse_pe(backup_data);
    if (is_patched(backup_data, backup_pe)) throw Error("Backup contains this patch and cannot be used for restoration.");
    write_file(temp, backup_data);

    replace_target_safely(target, temp, backup);
    const auto restored = read_file(target);
    const auto restored_pe = parse_pe(restored);
    if (is_patched(restored, restored_pe)) throw Error("Restored executable still contains the GameNative patch marker.");
    std::cout << "Restored FalloutNV.exe from " << kBackupName << ".\n";
}

void verify(const fs::path& directory) {
    const fs::path target = directory / kTargetName;
    if (!fs::exists(target)) throw Error("FalloutNV.exe was not found in the current folder.");
    const auto data = read_file(target);
    const auto pe = parse_pe(data);
    std::cout << "Verification report\n"
              << "  PE32 x86: yes\n"
              << "  Fallout NV identity marker: " << (looks_like_fallout_nv(data) ? "yes" : "no") << "\n"
              << "  Steam .bind wrapper present: " << (has_section(pe, ".bind") ? "yes" : "no") << "\n"
              << "  Large Address Aware: " << ((pe.characteristics & kLargeAddressAware) ? "yes" : "no") << "\n"
              << "  GameNative xNVSE patch marker: " << (is_patched(data, pe) ? "yes" : "no") << "\n"
              << "  nvse_steam_loader.dll present: " << (fs::exists(directory / "nvse_steam_loader.dll") ? "yes" : "no") << "\n"
              << "  nvse_1_4.dll present: " << (fs::exists(directory / "nvse_1_4.dll") ? "yes" : "no") << "\n";
}

void print_help() {
    std::cout << kToolName << " " << kToolVersion << "\n\n"
              << "Run this tool from the Fallout New Vegas game folder after GameNative has unpacked FalloutNV.exe.\n\n"
              << "Usage:\n"
              << "  FNVGameNativePatcher.exe            Patch FalloutNV.exe\n"
              << "  FNVGameNativePatcher.exe --verify   Show current state\n"
              << "  FNVGameNativePatcher.exe --restore  Restore the original backup\n"
              << "  FNVGameNativePatcher.exe --help     Show this help\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::cout << kToolName << " " << kToolVersion << "\n";
        const fs::path directory = fs::current_path();
        const std::string command = argc >= 2 ? argv[1] : "--patch";
        if (command == "--patch") patch(directory);
        else if (command == "--verify") verify(directory);
        else if (command == "--restore") restore(directory);
        else if (command == "--help" || command == "-h") print_help();
        else throw Error("Unknown argument: " + command + ". Use --help.");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << "\n";
        return 1;
    }
}
