#pragma once

#include <iostream>

// Minimal test helpers: print file:line on failure and set a global failure flag
inline bool g_testFailed = false;

#define CHECK(cond) \
	do { \
		if (!(cond)) \
		{ \
			std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond << std::endl; \
			g_testFailed = true; \
		} \
	} while (0)

#define CHECK_EQ(a, b) \
	do { \
		const auto va_ = (a); \
		const auto vb_ = (b); \
		if (!(va_ == vb_)) \
		{ \
			std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK_EQ failed: " #a " == " #b \
				<< " (" << va_ << " vs " << vb_ << ")" << std::endl; \
			g_testFailed = true; \
		} \
	} while (0)
