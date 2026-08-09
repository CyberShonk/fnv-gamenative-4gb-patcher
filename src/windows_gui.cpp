#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "patcher.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kWindowTitle[] = L"FNV GameNative 4GB Patcher";

constexpr int kIdVerify = 1001;
constexpr int kIdPatch = 1002;
constexpr int kIdRestore = 1003;
constexpr int kIdOutput = 1004;
constexpr int kIdTarget = 1005;
constexpr int kIdClose = 1006;

struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class CaptureLogBuffer final : public std::streambuf {
public:
    CaptureLogBuffer(std::streambuf* capture, std::streambuf* log)
        : capture_(capture), log_(log) {}

protected:
    int overflow(int ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) {
            return traits_type::not_eof(ch);
        }

        const char value = traits_type::to_char_type(ch);
        if (traits_type::eq_int_type(capture_->sputc(value), traits_type::eof()) ||
            traits_type::eq_int_type(log_->sputc(value), traits_type::eof())) {
            return traits_type::eof();
        }
        return ch;
    }

    std::streamsize xsputn(const char* data, std::streamsize size) override {
        const std::streamsize capture_written = capture_->sputn(data, size);
        const std::streamsize log_written = log_->sputn(data, size);
        return std::min(capture_written, log_written);
    }

    int sync() override {
        return capture_->pubsync() == 0 && log_->pubsync() == 0 ? 0 : -1;
    }

private:
    std::streambuf* capture_;
    std::streambuf* log_;
};

struct SessionResult {
    int exit_status = 1;
    std::string output;
};

struct AppState {
    fs::path executable_path;
    fs::path directory;
    fs::path log_path;
    HWND output = nullptr;
    HWND target = nullptr;
    std::wstring window_class;
};

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    auto convert = [&](UINT code_page) -> std::wstring {
        const int required = MultiByteToWideChar(
            code_page,
            code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (required <= 0) {
            return {};
        }

        std::wstring result(static_cast<std::size_t>(required), L'\0');
        const int written = MultiByteToWideChar(
            code_page,
            code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            required);
        if (written != required) {
            return {};
        }
        return result;
    };

    std::wstring result = convert(CP_UTF8);
    if (!result.empty()) {
        return result;
    }
    return convert(CP_ACP);
}

std::wstring path_text(const fs::path& path) {
    return path.wstring();
}

void write_standard_handle(DWORD kind, const std::string& text) {
    if (text.empty()) {
        return;
    }

    HANDLE handle = GetStdHandle(kind);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }

    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t remaining = text.size() - offset;
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::size_t>(remaining, 1U << 20U));
        DWORD written = 0;
        if (!WriteFile(handle, text.data() + offset, chunk, &written, nullptr) || written == 0) {
            return;
        }
        offset += written;
    }
}

std::string first_command_argument(const char* command_line) {
    if (command_line == nullptr) {
        return {};
    }

    const char* cursor = command_line;
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
        ++cursor;
    }
    if (*cursor == '\0') {
        return {};
    }

    std::string result;
    if (*cursor == '"') {
        ++cursor;
        while (*cursor != '\0' && *cursor != '"') {
            result.push_back(*cursor++);
        }
        return result;
    }

    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) == 0) {
        result.push_back(*cursor++);
    }
    return result;
}

SessionResult run_session(
    const std::string& command,
    const fs::path& executable_path,
    const fs::path& directory,
    const fs::path& log_path) {

    std::ofstream log(log_path, std::ios::out | std::ios::app);
    if (!log) {
        return {
            1,
            "ERROR: Unable to create persistent log beside the patcher: " +
                log_path.string() + "\nFinal exit status: 1\n"};
    }

    std::ostringstream capture;
    CaptureLogBuffer tee(capture.rdbuf(), log.rdbuf());
    std::ostream output(&tee);

    output << "\n=== FNV GameNative patcher session ===\n"
           << "Patcher version: " << fnv_patcher::tool_version() << "\n"
           << "Patcher architecture: " << fnv_patcher::patcher_architecture() << "\n"
           << "Executable path: " << executable_path << "\n"
           << "Selected target directory: " << directory << "\n"
           << "Persistent log: " << log_path << "\n"
           << "Requested operation: " << command << "\n"
           << "Required-file validation: pending\n";

    try {
        output << fnv_patcher::tool_name() << " " << fnv_patcher::tool_version() << "\n";
        fnv_patcher::run_operation(command, directory, output);
        output << (command == "--help" || command == "-h"
                       ? "Required-file validation: not applicable to help output.\n"
                       : "Required-file validation: completed for the requested operation.\n")
               << "Verification result: operation completed without an uncaught error.\n"
               << "Final exit status: 0\n";
        output.flush();
        return {0, capture.str()};
    } catch (const std::exception& error) {
        output << "ERROR: " << error.what() << "\n"
               << "Final exit status: 1\n";
        output.flush();
        return {1, capture.str()};
    }
}

void set_output(HWND control, const std::string& text) {
    const std::wstring wide = widen(text);
    SetWindowTextW(control, wide.c_str());
    SendMessageW(control, EM_SETSEL, 0, 0);
    SendMessageW(control, EM_SCROLLCARET, 0, 0);
}

void run_gui_operation(HWND window, AppState& state, const char* command) {
    if (std::string(command) == "--patch") {
        const int answer = MessageBoxW(
            window,
            L"Patch FalloutNV.exe and FalloutNV.exe.unpacked.exe?\n\n"
            L"The patcher validates both files first and creates managed backups before replacement.",
            L"Confirm patch",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (answer != IDYES) {
            return;
        }
    } else if (std::string(command) == "--restore") {
        const int answer = MessageBoxW(
            window,
            L"Restore the managed Fallout New Vegas executable backups?",
            L"Confirm restore",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
        if (answer != IDYES) {
            return;
        }
    }

    const SessionResult result = run_session(
        command,
        state.executable_path,
        state.directory,
        state.log_path);
    set_output(state.output, result.output);

    if (result.exit_status != 0) {
        MessageBoxW(
            window,
            L"The requested operation did not complete. Review the status output and persistent log.",
            L"FNV GameNative 4GB Patcher",
            MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }

        case WM_COMMAND:
            if (state == nullptr) {
                break;
            }
            switch (LOWORD(wparam)) {
                case kIdVerify:
                    run_gui_operation(window, *state, "--verify");
                    return 0;
                case kIdPatch:
                    run_gui_operation(window, *state, "--patch");
                    return 0;
                case kIdRestore:
                    run_gui_operation(window, *state, "--restore");
                    return 0;
                case kIdClose:
                    DestroyWindow(window);
                    return 0;
                default:
                    break;
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

HWND create_label(
    HWND parent,
    HINSTANCE instance,
    const wchar_t* text,
    int x,
    int y,
    int width,
    int height,
    int id = 0) {
    return CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        width,
        height,
        parent,
        id == 0 ? nullptr : reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance,
        nullptr);
}

HWND create_button(
    HWND parent,
    HINSTANCE instance,
    const wchar_t* text,
    int id,
    int x,
    int y,
    int width) {
    return CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        x,
        y,
        width,
        34,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        instance,
        nullptr);
}

int run_gui(HINSTANCE instance, int show_command, AppState& state) {
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = state.window_class.c_str();

    if (RegisterClassExW(&window_class) == 0) {
        return 10;
    }

    HWND window = CreateWindowExW(
        0,
        state.window_class.c_str(),
        kWindowTitle,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        760,
        610,
        nullptr,
        nullptr,
        instance,
        &state);

    if (window == nullptr) {
        return 20;
    }

    const std::wstring version_line =
        std::wstring(L"Version ") + widen(fnv_patcher::tool_version()) +
        L"    Architecture: " + widen(fnv_patcher::patcher_architecture());
    create_label(window, instance, version_line.c_str(), 24, 20, 690, 24);

    create_label(window, instance, L"Target directory", 24, 56, 690, 22);
    state.target = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        path_text(state.directory).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        24,
        80,
        690,
        28,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdTarget)),
        instance,
        nullptr);

    create_label(
        window,
        instance,
        L"Run GameNative's Unpack Files operation before patching. Verify is read-only.",
        24,
        122,
        690,
        22);

    create_button(window, instance, L"Verify", kIdVerify, 24, 160, 120);
    create_button(window, instance, L"Patch", kIdPatch, 158, 160, 120);
    create_button(window, instance, L"Restore", kIdRestore, 292, 160, 120);
    create_button(window, instance, L"Close", kIdClose, 594, 160, 120);

    create_label(window, instance, L"Status", 24, 212, 690, 22);
    state.output = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
        24,
        238,
        690,
        300,
        window,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOutput)),
        instance,
        nullptr);

    if (state.output == nullptr || state.target == nullptr) {
        DestroyWindow(window);
        return 30;
    }

    ShowWindow(window, show_command == 0 ? SW_SHOWNORMAL : show_command);
    UpdateWindow(window);

    run_gui_operation(window, state, "--verify");

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    return static_cast<int>(message.wParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR command_line, int show_command) {
    try {
        AppState state;
        state.executable_path = fnv_patcher::process_executable_path(nullptr);
        state.directory = state.executable_path.parent_path();
        state.log_path = state.directory / fnv_patcher::log_name();
        state.window_class = state.executable_path.filename().wstring();
        if (state.window_class.empty()) {
            state.window_class = L"FNVGameNativePatcher.exe";
        }

        const std::string command = first_command_argument(command_line);
        if (!command.empty()) {
            const SessionResult result = run_session(
                command,
                state.executable_path,
                state.directory,
                state.log_path);
            write_standard_handle(
                result.exit_status == 0 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE,
                result.output);
            return result.exit_status;
        }

        return run_gui(instance, show_command, state);
    } catch (const std::exception& error) {
        const std::string message =
            std::string("ERROR: ") + error.what() + "\nFinal exit status: 1\n";
        write_standard_handle(STD_ERROR_HANDLE, message);
        MessageBoxW(
            nullptr,
            widen(message).c_str(),
            L"FNV GameNative 4GB Patcher",
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
