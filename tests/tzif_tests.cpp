#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "../tzif.h"

using namespace vsid::tzif;
using namespace std::chrono;

namespace
{
	// builds a UTC instant
	sys_seconds utc(int y, unsigned mo, unsigned d, int h = 0, int mi = 0, int s = 0)
	{
		return sys_days{ year{ y } / month{ mo } / day{ d } } + hours{ h } + minutes{ mi } + seconds{ s };
	}

	long long off(const PosixTz& tz, sys_seconds t)
	{
		return tz.utcOffset(t).count();
	}

	void test_valid_names()
	{
		CHECK(isValidZoneName("Europe/Berlin"));
		CHECK(isValidZoneName("UTC"));
		CHECK(isValidZoneName("Etc/GMT+5"));
		CHECK(isValidZoneName("Etc/GMT-14"));
		CHECK(isValidZoneName("America/Argentina/Buenos_Aires"));
	}

	void test_invalid_names()
	{
		CHECK(!isValidZoneName(""));
		CHECK(!isValidZoneName("../x"));
		CHECK(!isValidZoneName("a/../b"));
		CHECK(!isValidZoneName("/etc/passwd"));
		CHECK(!isValidZoneName("C:x"));
		CHECK(!isValidZoneName("a\\b"));
		CHECK(!isValidZoneName("Europe/"));
		CHECK(!isValidZoneName("/Europe"));
		CHECK(!isValidZoneName("a//b"));
		CHECK(!isValidZoneName("Europe/Ber lin"));
		CHECK(!isValidZoneName(std::string(300, 'a')));
	}

	void test_case_is_exact()
	{
		CHECK(isValidZoneName("europe/berlin"));
	}

	void test_posix_fixed()
	{
		auto a = PosixTz::parse("<+01>-1");
		CHECK(a.has_value());
		if (a)
		{
			CHECK_EQ(off(*a, utc(2024, 1, 1)), 3600);
			CHECK_EQ(off(*a, utc(2024, 7, 1)), 3600);
		}

		auto b = PosixTz::parse("UTC0");
		CHECK(b.has_value());
		if (b) CHECK_EQ(off(*b, utc(2024, 7, 1)), 0);

		auto c = PosixTz::parse("IST-5:30");
		CHECK(c.has_value());
		if (c) CHECK_EQ(off(*c, utc(2024, 7, 1)), 19800);
	}

	void test_posix_northern()
	{
		auto tz = PosixTz::parse("CET-1CEST,M3.5.0,M10.5.0/3");
		CHECK(tz.has_value());
		if (!tz) return;
		CHECK_EQ(off(*tz, utc(2024, 3, 31, 0, 59, 59)), 3600);
		CHECK_EQ(off(*tz, utc(2024, 3, 31, 1, 0, 0)), 7200);
		CHECK_EQ(off(*tz, utc(2024, 10, 27, 0, 59, 59)), 7200);
		CHECK_EQ(off(*tz, utc(2024, 10, 27, 1, 0, 0)), 3600);
	}

	void test_posix_southern()
	{
		// Sydney: DST ends first Sunday April 03:00 local DST (UTC+11) = Apr 6 16:00Z
		auto tz = PosixTz::parse("AEST-10AEDT,M10.1.0,M4.1.0/3");
		CHECK(tz.has_value());
		if (!tz) return;
		CHECK_EQ(off(*tz, utc(2024, 1, 15)), 39600);
		CHECK_EQ(off(*tz, utc(2024, 7, 15)), 36000);
		CHECK_EQ(off(*tz, utc(2024, 4, 6, 15, 59, 59)), 39600);
		CHECK_EQ(off(*tz, utc(2024, 4, 6, 16, 0, 0)), 36000);
		// DST starts first Sunday October 02:00 local std (UTC+10) = Oct 5 16:00Z
		CHECK_EQ(off(*tz, utc(2024, 10, 5, 15, 59, 59)), 36000);
		CHECK_EQ(off(*tz, utc(2024, 10, 5, 16, 0, 0)), 39600);
		CHECK_EQ(off(*tz, utc(2024, 12, 31, 23, 0, 0)), 39600);
	}

	void test_posix_negative_dst()
	{
		auto tz = PosixTz::parse("IST-1GMT0,M10.5.0,M3.5.0/1");
		CHECK(tz.has_value());
		if (!tz) return;
		CHECK_EQ(off(*tz, utc(2024, 7, 15)), 3600);
		CHECK_EQ(off(*tz, utc(2024, 1, 15)), 0);
	}

	void test_posix_extensions()
	{
		// negative rule hours
		auto tz = PosixTz::parse("<-03>3<-02>,M3.5.0/-2,M10.5.0/-1");
		CHECK(tz.has_value());
		if (tz)
		{
			// start: Sun 2024-03-31 00:00 - 2h = Sat 22:00 local std (UTC-3) = 01:00Z
			CHECK_EQ(off(*tz, utc(2024, 3, 31, 0, 59, 59)), -10800);
			CHECK_EQ(off(*tz, utc(2024, 3, 31, 1, 0, 0)), -7200);
			// end: Sun 2024-10-27 00:00 - 1h = Sat 23:00 local dst (UTC-2) = 01:00Z
			CHECK_EQ(off(*tz, utc(2024, 10, 27, 0, 59, 59)), -7200);
			CHECK_EQ(off(*tz, utc(2024, 10, 27, 1, 0, 0)), -10800);
		}

		// rule time of 24h: second Sunday March + 24h = Mon 2024-03-11 00:00 local std (UTC-5) = 05:00Z
		auto tz2 = PosixTz::parse("EST5EDT,M3.2.0/24,M11.1.0");
		CHECK(tz2.has_value());
		if (tz2)
		{
			CHECK_EQ(off(*tz2, utc(2024, 3, 11, 4, 59, 59)), -18000);
			CHECK_EQ(off(*tz2, utc(2024, 3, 11, 5, 0, 0)), -14400);
		}
	}

	void test_posix_julian()
	{
		// J60 = Mar 1 (Feb 29 never counted), 0-based n=59 = Feb 29 in a leap year
		auto tz = PosixTz::parse("UTC0DST,J60/0,J300/0");
		CHECK(tz.has_value());
		if (tz)
		{
			CHECK_EQ(off(*tz, utc(2024, 2, 29, 12)), 0);
			CHECK_EQ(off(*tz, utc(2024, 3, 1, 0, 0, 0)), 3600);
		}

		auto tz2 = PosixTz::parse("UTC0DST,59/0,300/0");
		CHECK(tz2.has_value());
		if (tz2)
		{
			CHECK_EQ(off(*tz2, utc(2024, 2, 28, 23, 59, 59)), 0);
			CHECK_EQ(off(*tz2, utc(2024, 2, 29, 0, 0, 0)), 3600);
		}
	}

	void test_posix_bad()
	{
		CHECK(!PosixTz::parse("").has_value());
		CHECK(!PosixTz::parse("CET").has_value());
		CHECK(!PosixTz::parse("CET-1CEST,M13.1.0,M10.5.0").has_value());
		CHECK(!PosixTz::parse("CET-1CEST,M3.5.0").has_value());
		CHECK(!PosixTz::parse("CET-1junk!").has_value());
		CHECK(!PosixTz::parse("<+01-1").has_value());
	}

	// ---- TZif v2 test data ----

	struct TType
	{
		int offset;
		bool dst;
	};

	struct TTrans
	{
		long long at;
		unsigned char type;
	};

	void put32(std::vector<unsigned char>& b, unsigned long long v)
	{
		for (int i = 3; i >= 0; --i) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
	}

	void put64(std::vector<unsigned char>& b, unsigned long long v)
	{
		for (int i = 7; i >= 0; --i) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
	}

	void putHeader(std::vector<unsigned char>& b, unsigned timecnt, unsigned typecnt, unsigned charcnt)
	{
		for (const char c : std::string("TZif2")) b.push_back(static_cast<unsigned char>(c));
		for (int i = 0; i < 15; ++i) b.push_back(0);
		put32(b, 0); // isutcnt
		put32(b, 0); // isstdcnt
		put32(b, 0); // leapcnt
		put32(b, timecnt);
		put32(b, typecnt);
		put32(b, charcnt);
	}

	// builds a TZif v2 file: minimal v1 block, v2 block with the given data, footer between two newlines
	std::vector<unsigned char> makeTzif(const std::vector<TType>& types, const std::vector<TTrans>& trans, const std::string& footer)
	{
		std::vector<unsigned char> b;
		putHeader(b, 0, 1, 1); // v1 block: one type, one char
		for (int i = 0; i < 6; ++i) b.push_back(0); // v1 type
		b.push_back(0); // v1 abbreviation

		putHeader(b, static_cast<unsigned>(trans.size()), static_cast<unsigned>(types.size()), 2);
		for (const auto& t : trans) put64(b, static_cast<unsigned long long>(t.at));
		for (const auto& t : trans) b.push_back(t.type);
		for (const auto& t : types)
		{
			put32(b, static_cast<unsigned int>(t.offset));
			b.push_back(t.dst ? 1 : 0);
			b.push_back(0);
		}
		b.push_back('X');
		b.push_back(0);

		b.push_back('\n');
		for (const char c : footer) b.push_back(static_cast<unsigned char>(c));
		b.push_back('\n');
		return b;
	}

	// offset of the v2 header in a buffer from makeTzif: 44 (v1 header) + 7 (v1 block)
	constexpr size_t h2 = 51;

	long long epoch(sys_seconds t)
	{
		return t.time_since_epoch().count();
	}

	const std::string cetFooter = "CET-1CEST,M3.5.0,M10.5.0/3";

	// CET / CEST with the 2024 transitions
	std::vector<unsigned char> berlin(const std::string& footer = cetFooter)
	{
		return makeTzif({ { 3600, false }, { 7200, true } },
			{ { epoch(utc(2024, 3, 31, 1)), 1 }, { epoch(utc(2024, 10, 27, 1)), 0 } }, footer);
	}

	long long zoff(const Zone& z, sys_seconds t)
	{
		return z.utcOffset(t).count();
	}

	void test_before_first_transition()
	{
		const auto buf = berlin();
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		CHECK_EQ(zoff(*z, utc(1900, 1, 1)), 3600);
		CHECK_EQ(zoff(*z, utc(2024, 3, 31, 0, 59, 59)), 3600);
	}

	void test_between_transitions()
	{
		const auto buf = berlin();
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		CHECK_EQ(zoff(*z, utc(2024, 3, 31, 0, 59, 59)), 3600);
		CHECK_EQ(zoff(*z, utc(2024, 3, 31, 1, 0, 0)), 7200);
		CHECK_EQ(zoff(*z, utc(2024, 7, 1)), 7200);
		CHECK_EQ(zoff(*z, utc(2024, 10, 27, 0, 59, 59)), 7200);
		CHECK_EQ(zoff(*z, utc(2024, 10, 27, 1, 0, 0)), 3600);
	}

	void test_after_last_uses_footer()
	{
		const auto buf = berlin();
		const auto z = Zone::parse(buf);
		const auto tz = PosixTz::parse(cetFooter);
		CHECK(z.has_value());
		CHECK(tz.has_value());
		if (!z || !tz) return;
		CHECK_EQ(zoff(*z, utc(2038, 2, 1)), off(*tz, utc(2038, 2, 1)));
		CHECK_EQ(zoff(*z, utc(2100, 7, 1)), off(*tz, utc(2100, 7, 1)));
		CHECK_EQ(zoff(*z, utc(2100, 7, 1)), 7200);

		// a footer that differs from the last type proves that the footer decides
		const auto buf2 = berlin("<+09>-9");
		const auto z2 = Zone::parse(buf2);
		CHECK(z2.has_value());
		if (z2) CHECK_EQ(zoff(*z2, utc(2100, 7, 1)), 32400);
	}

	void test_no_transitions_fixed()
	{
		const auto buf = makeTzif({ { 0, false } }, {}, "<+0530>-5:30");
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		CHECK_EQ(zoff(*z, utc(1900, 1, 1)), 19800);
		CHECK_EQ(zoff(*z, utc(2024, 1, 1)), 19800);
		CHECK_EQ(zoff(*z, utc(2100, 1, 1)), 19800);
	}

	void test_empty_footer()
	{
		// last transition leads to type 1 (CEST): without footer it stays in effect
		const auto buf = makeTzif({ { 3600, false }, { 7200, true } }, { { epoch(utc(2024, 3, 31, 1)), 1 } }, "");
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		CHECK_EQ(zoff(*z, utc(2024, 3, 31, 1)), 7200);
		CHECK_EQ(zoff(*z, utc(2100, 1, 1)), 7200);

		// no transitions and no footer: first type
		const auto buf2 = makeTzif({ { 3600, false } }, {}, "");
		const auto z2 = Zone::parse(buf2);
		CHECK(z2.has_value());
		if (z2) CHECK_EQ(zoff(*z2, utc(2100, 1, 1)), 3600);
	}

	void test_extreme_instants()
	{
		// must not crash or run into signed overflow
		const auto buf = berlin();
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		(void)z->utcOffset(sys_seconds::min());
		(void)z->utcOffset(sys_seconds::max());

		const auto tz = PosixTz::parse(cetFooter);
		CHECK(tz.has_value());
		if (!tz) return;
		(void)tz->utcOffset(sys_seconds::min());
		(void)tz->utcOffset(sys_seconds::max());
	}

	void test_corrupt()
	{
		const std::vector<unsigned char> good = berlin();
		CHECK(Zone::parse(good).has_value());

		// empty, tiny
		CHECK(!Zone::parse(std::vector<unsigned char>{}).has_value());
		CHECK(!Zone::parse(std::vector<unsigned char>{ 'T', 'Z', 'i' }).has_value());

		// wrong magic
		auto bad = good;
		bad[0] = 'X';
		CHECK(!Zone::parse(bad).has_value());

		// header only
		CHECK(!Zone::parse(std::vector<unsigned char>(good.begin(), good.begin() + 44)).has_value());

		// v2 header without data
		CHECK(!Zone::parse(std::vector<unsigned char>(good.begin(), good.begin() + h2 + 44)).has_value());

		// hostile transition count (timecnt at +32 in the header)
		bad = good;
		bad[h2 + 32] = bad[h2 + 33] = bad[h2 + 34] = bad[h2 + 35] = 0xFF;
		CHECK(!Zone::parse(bad).has_value());

		// hostile type count and char count
		bad = good;
		bad[h2 + 36] = bad[h2 + 37] = bad[h2 + 38] = bad[h2 + 39] = 0xFF;
		CHECK(!Zone::parse(bad).has_value());
		bad = good;
		bad[h2 + 40] = bad[h2 + 41] = bad[h2 + 42] = bad[h2 + 43] = 0xFF;
		CHECK(!Zone::parse(bad).has_value());

		// hostile v1 counts
		bad = good;
		bad[32] = bad[33] = bad[34] = bad[35] = 0xFF;
		CHECK(!Zone::parse(bad).has_value());

		// no types at all
		bad = good;
		bad[h2 + 36] = bad[h2 + 37] = bad[h2 + 38] = bad[h2 + 39] = 0;
		CHECK(!Zone::parse(bad).has_value());

		// every truncation of a valid file fails and must not crash
		for (size_t n = 0; n < good.size(); ++n)
		{
			CHECK(!Zone::parse(std::vector<unsigned char>(good.begin(), good.begin() + n)).has_value());
		}

		// v1-only (version byte 0)
		bad = good;
		bad[4] = 0;
		CHECK(!Zone::parse(bad).has_value());

		// type index out of range
		CHECK(!Zone::parse(makeTzif({ { 3600, false }, { 7200, true } }, { { 100, 2 } }, cetFooter)).has_value());

		// transitions not ascending
		CHECK(!Zone::parse(makeTzif({ { 3600, false } }, { { 200, 0 }, { 100, 0 } }, "")).has_value());
		CHECK(!Zone::parse(makeTzif({ { 3600, false } }, { { 100, 0 }, { 100, 0 } }, "")).has_value());

		// invalid dst flag (type record starts right after the header, no transitions)
		bad = makeTzif({ { 3600, false } }, {}, "");
		bad[h2 + 44 + 4] = 7;
		CHECK(!Zone::parse(bad).has_value());

		// abbreviation index out of range
		bad = makeTzif({ { 3600, false } }, {}, "");
		bad[h2 + 44 + 5] = 9;
		CHECK(!Zone::parse(bad).has_value());

		// missing footer newline (last and first)
		bad = good;
		bad.pop_back();
		CHECK(!Zone::parse(bad).has_value());
		bad = makeTzif({ { 3600, false } }, {}, "");
		bad[h2 + 44 + 6 + 2] = 'x'; // newline before the footer
		CHECK(!Zone::parse(bad).has_value());

		// unparsable footer
		CHECK(!Zone::parse(makeTzif({ { 3600, false } }, {}, "garbage!")).has_value());
	}

	void test_empty_zone()
	{
		const Zone z;
		CHECK_EQ(zoff(z, utc(2024, 7, 1)), 0);
		(void)z.utcOffset(sys_seconds::min());
		(void)z.utcOffset(sys_seconds::max());
	}

	void test_leap_seconds_rejected()
	{
		const auto good = makeTzif({ { 3600, false } }, {}, "");
		CHECK(Zone::parse(good).has_value());

		// one leap record (8 byte time + 4 byte correction), placed after the type and abbreviation data
		auto bad = good;
		bad[h2 + 28 + 3] = 1; // leapcnt
		const size_t leapPos = h2 + 44 + 6 + 2;
		bad.insert(bad.begin() + static_cast<std::ptrdiff_t>(leapPos), 12, static_cast<unsigned char>(0));
		CHECK(!Zone::parse(bad).has_value());
	}

	void test_type0_dst_before_first_transition()
	{
		// RFC 8536: instants before the first transition use type 0, even if it is a DST type
		const auto buf = makeTzif({ { 7200, true }, { 3600, false } }, { { epoch(utc(2024, 10, 27, 1)), 1 } }, "");
		const auto z = Zone::parse(buf);
		CHECK(z.has_value());
		if (!z) return;
		CHECK_EQ(zoff(*z, utc(1900, 1, 1)), 7200);
		CHECK_EQ(zoff(*z, utc(2024, 10, 27, 0, 59, 59)), 7200);
		CHECK_EQ(zoff(*z, utc(2024, 10, 27, 1)), 3600);
	}

	std::filesystem::path makeTempFolder()
	{
		const auto p = std::filesystem::temp_directory_path() / "vsid_tzif_test";
		std::error_code ec;
		std::filesystem::remove_all(p, ec);
		std::filesystem::create_directories(p, ec);
		return p;
	}

	void writeFile(const std::filesystem::path& p, const std::vector<unsigned char>& data)
	{
		std::error_code ec;
		std::filesystem::create_directories(p.parent_path(), ec);
		std::ofstream f(p, std::ios::binary);
		f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	}

	void test_load_zone_status()
	{
		const auto folder = makeTempFolder();

		CHECK(loadZone(folder / "does_not_exist", "Europe/Berlin").status == LoadStatus::FolderMissing);
		CHECK(loadZone(folder, "Europe/Berlin").status == LoadStatus::NotFound);

		writeFile(folder / "Europe" / "Garbage", std::vector<unsigned char>{ 'n', 'o', 'p', 'e', 'x', 'y', 'z' });
		CHECK(loadZone(folder, "Europe/Garbage").status == LoadStatus::Corrupt);

		// invalid names are rejected without touching the filesystem (folder does not even exist)
		CHECK(loadZone(folder / "does_not_exist", "../x").status == LoadStatus::InvalidName);
		CHECK(loadZone(folder, "../x").status == LoadStatus::InvalidName);
		CHECK(loadZone(folder, "").status == LoadStatus::InvalidName);
		CHECK(loadZone(folder, "C:x").status == LoadStatus::InvalidName);

		// a directory is not a zone
		CHECK(loadZone(folder, "Europe").status == LoadStatus::NotFound);

		writeFile(folder / "Europe" / "Berlin", berlin());
		const auto r = loadZone(folder, "Europe/Berlin");
		CHECK(r.status == LoadStatus::Ok);
		CHECK(r.zone.has_value());
		if (r.zone) CHECK_EQ(zoff(*r.zone, utc(2024, 7, 1)), 7200);

		// oversized file is treated as corrupt, not read into memory
		writeFile(folder / "Big", std::vector<unsigned char>(2 * 1024 * 1024, 0));
		CHECK(loadZone(folder, "Big").status == LoadStatus::Corrupt);

		std::error_code ec;
		std::filesystem::remove_all(folder, ec);
	}

	void test_read_version()
	{
		const auto folder = makeTempFolder();

		CHECK(!readVersion(folder).has_value());
		CHECK(!readVersion(folder / "does_not_exist").has_value());

		const std::string v = "2025b\n";
		writeFile(folder / "+VERSION", std::vector<unsigned char>(v.begin(), v.end()));
		const auto r = readVersion(folder);
		CHECK(r.has_value());
		if (r) CHECK_EQ(*r, std::string("2025b"));

		const std::string v2 = "  2025c \r\nsecond line\n";
		writeFile(folder / "+VERSION", std::vector<unsigned char>(v2.begin(), v2.end()));
		const auto r2 = readVersion(folder);
		CHECK(r2.has_value());
		if (r2) CHECK_EQ(*r2, std::string("2025c"));

		std::error_code ec;
		std::filesystem::remove_all(folder, ec);
	}
}

int main()
{
	test_valid_names();
	test_invalid_names();
	test_case_is_exact();
	test_posix_fixed();
	test_posix_northern();
	test_posix_southern();
	test_posix_negative_dst();
	test_posix_extensions();
	test_posix_julian();
	test_posix_bad();
	test_before_first_transition();
	test_between_transitions();
	test_after_last_uses_footer();
	test_no_transitions_fixed();
	test_empty_footer();
	test_extreme_instants();
	test_corrupt();
	test_empty_zone();
	test_leap_seconds_rejected();
	test_type0_dst_before_first_transition();
	test_load_zone_status();
	test_read_version();

	if (g_testFailed)
	{
		std::cerr << "FAILED" << std::endl;
		return 1;
	}

	std::cout << "all tests passed" << std::endl;
	return 0;
}
