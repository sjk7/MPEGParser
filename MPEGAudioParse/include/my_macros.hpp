#pragma once
// my_macros.hpp
// I know, they are evil ... :-(

#ifdef _MSC_VER
#pragma warning(disable : 26481) // no raw pointers: use gsl::span
#endif

#ifdef _MSC_VER
#pragma warning(disable : 26490) // do not use reinterpret_cast
#endif

#ifndef CAST
#define CAST(type, expr) static_cast<type>(expr)
#endif
