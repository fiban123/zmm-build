#pragma once

#ifndef EXPORT_MACROS_H
#define EXPORT_MACROS_H

#undef API

#if defined(_WIN32) || defined(__CYGWIN__)
// Logic for Windows
#ifdef BUILDING_SO
#define API extern __declspec(dllexport)
#else
#define API __declspec(dllimport)
#endif
#else
// Logic for Linux/macOS
#define API __attribute__((visibility("default")))
#endif

#define UNUSED __attribute__((unused))

#endif
