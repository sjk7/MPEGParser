#pragma once
// my_macros.hpp
// I know, they are evil ... :-(
#ifndef CAST
#define CAST(type, expr) static_cast<type>(expr)
#endif
