// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define GREEN "\x1b[92m"
#define RED   "\x1b[91m"
#define RESET "\x1b[0m"

#define test_print_cmp_(name, cmps, cmp, val, fmt) \
	printf("[%s] %s %s" fmt "\n", name, (cmp) ? GREEN "Passed" RESET : RED "Failed" RESET, cmps, val);

#define test_cmp_(name, cmp, val, fmt) { \
	bool ___CMP___ = cmp; \
	test_print_cmp_(name, #cmp, ___CMP___, val, fmt); \
	if (!___CMP___) return false; \
}

#define test_cmp(name, cmp) \
	test_cmp_(name, (cmp), "", "%s")

#define test_print_cmps(name, cmp, s) { \
	bool ___PCMP___ = cmp; \
	test_print_cmp_(name, #cmp, ___PCMP___, s, "%s") \
}

#define test_print_cmp(name, cmp) \
	test_print_cmps(name, cmp, "")

#define test_passed(name) \
	test_print_cmp_(name, "", true, "", "%s")

#define test_failed(name) { \
	test_print_cmp_(name, "", false, "", "%s"); \
	return false; \
}

#define test_cmpf(name, cmp, val) \
	test_cmp_(name, (cmp), val, ": %.2f")

#define test_cmpi32(name, cmp, val) \
	test_cmp_(name, cmp, val, ": %" PRId32)

#define test_cmpi64(name, cmp, val) \
	test_cmp_(name, (cmp), (int64_t) val, ": %" PRId64)

#define test_cmps(name, cmp, val) \
	test_cmp_(name, cmp, val, ": %s")
