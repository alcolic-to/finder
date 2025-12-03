/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef OS_HPP
#define OS_HPP

#include <filesystem>
#include <format>
#include <variant>

#include "types.hpp"

// OS specific preprocessor definitions.
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_UNKNOWN
#endif

namespace os {

constexpr char path_sep = std::filesystem::path::preferred_separator;
const inline std::string path_sep_str = std::string{path_sep}; // NOLINT

struct Coordinates {
    i16 x;
    i16 y;
};

using ConsoleInput = std::variant<os::Coordinates, i32>;

bool is_esc(i32 input);
bool is_term(i32 input);
bool is_backspace(i32 input);
bool is_ctrl_j(i32 input);
bool is_ctrl_k(i32 input);
bool is_ctrl_h(i32 input);
bool is_ctrl_l(i32 input);
bool is_ctrl_f(i32 input);
bool is_ctrl_i(i32 input);
bool is_ctrl_p(i32 input);
bool is_ctrl_y(i32 input);
bool is_ctrl_q(i32 input);
bool is_ctrl_u(i32 input);
bool is_ctrl_d(i32 input);
bool is_ctrl_g(i32 input);

void* init_console_in_handle();
void* init_console_out_handle();
void close_console(void* in_handle, void* out_handle);
Coordinates console_window_size(void* out_handle);
void console_scan(void* in_handle, ConsoleInput& input);

std::string root_dir();

template<bool throws = true>
i32 copy_to_clipboard(const std::string& str);

template<bool throws = true>
i32 exec_cmd_internal(const std::string& cmd);

template<bool throws = true, class... Args>
i32 exec_cmd(const std::format_string<Args...>& str, Args&&... args)
{
    std::string cmd = std::format(str, std::forward<Args>(args)...);
    return exec_cmd_internal<throws>(cmd);
}

} // namespace os

#endif // OS_HPP
