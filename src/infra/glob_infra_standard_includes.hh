#ifndef GLOB_INFRA_STANDARD_INCLUDE_HH
#define GLOB_INFRA_STANDARD_INCLUDE_HH
#pragma once

#include <iostream>
#include <iomanip>
#include <vector>
#include <inttypes.h>
#include <assert.h>

typedef unsigned char byte_t;

typedef std::vector<int>     int_vt;
typedef std::vector<int32_t> int32_vt;

typedef unsigned int uint;
typedef std::vector<uint>    uint_vt;
typedef std::vector<uint_vt> uint_vvt;
typedef std::vector<uint_vvt> uint_vvvt;

typedef std::vector<uint32_t> uint32_vt;

typedef std::vector<int64_t> int64_vt;
typedef std::vector<uint64_t> uint64_vt;

typedef std::vector<double> double_vt;

typedef const char* constcharptr_t;

typedef std::vector<bool> bool_vt;

#endif

