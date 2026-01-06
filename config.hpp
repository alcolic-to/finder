#pragma once

#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "files.hpp"
#include "types.hpp"

/**
 * Global search results configuration.
 */
constexpr usize files_limit = 80;
using Matches = Files::Matches<files_limit>;

#endif // CONFIG_HPP
