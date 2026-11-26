/*
Differential test for the TZif reader (tzif.h). It is itself the test, not part of the plugin.

Usage: vsid_tz_diff <zoneinfo-folder> [--verbose]
Exit code: 0 = no reader bug found, 1 = at least one BUG (or the built-in self-check failed), 2 = usage error.

For every zone and link of std::chrono's tz database (get_tzdb()) the zone is loaded from the folder and
utcOffset() is compared with time_zone::get_info() at every std::chrono transition +/-1 s and on a daily grid,
1970-01-01 .. 2060-12-31.

A difference to std::chrono can have two causes: the TZif data differs from the data behind std::chrono
(version skew, e.g. older Windows ICU data) or the reader is wrong. To tell them apart a second, independent and
minimal TZif table reader (RefTable below) is used as the oracle for what the *file* says:

 - instants up to the file's last explicit transition ("table domain"): reader and RefTable must agree
   everywhere (also where std::chrono agrees). Disagreement -> BUG. A reader that follows the file but differs from
   std::chrono -> data difference ("SKEW").
 - instants after the last transition ("footer domain", POSIX rule): only std::chrono is available as oracle.
   A mismatch there is SKEW only if the zone also shows a data difference in its table domain on/after 2023-01-01
   (the rules of that zone changed between both data versions), or if the footer is a fixed offset without DST rule
   and the zone's last transition is on/after 2023-01-01 (DST recently abolished, e.g. America/Asuncion).
   Otherwise (or before 2023-01-01) -> BUG.
 - the footer rule itself is verified independently of skew: the footer string of the file is parsed with
   PosixTz::parse and its utcOffset() is compared with the file's own table at the table instants where the rule is in
   effect (last two years of the table, and not before the second to last transition; a "fat" file runs to 2037
   from the same rule). Any mismatch there is a BUG regardless of skew, because the file is its own oracle. An
   unparsable non-empty footer is a BUG as well.
   Not covered: for zones whose table ends early ("slim" files) the footer beyond the table is only compared to
   std::chrono, so a defect that appears only in later years can still be classified as skew for a skewed zone.

Zones missing in the folder are MISSING (informational). A zone that fails to load otherwise (invalid name, corrupt
file, or std::chrono unable to locate it) is a BUG. A missing folder is reported once, exit 1.
Etc/UTC, Europe/Berlin, Australia/Sydney, Europe/Dublin, Africa/Casablanca, Asia/Kolkata, America/Sao_Paulo must
exist and must not be a BUG.

A self-check runs first: deliberately wrong readers (+1 h offset error, off-by-one at transitions, footer rule shifted
by a week, also the footer rule alone) are compared for a few zones, including skewed zones with a DST footer, and
must all be reported as BUG, otherwise the tool exits with 1. The self-check also fails if a listed zone cannot be
loaded or if no zone was checked.
*/
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../tzif.h"

using namespace std::chrono;
using namespace vsid::tzif;

namespace
{
	constexpr sys_seconds rangeStart = sys_days{ 1970y / 1 / 1 };
	constexpr sys_seconds rangeEnd = sys_days{ 2060y / 12 / 31 } + 24h;
	constexpr sys_seconds skewFrom = sys_days{ 2023y / 1 / 1 };

	using OffsetFn = std::function<long long(sys_seconds)>;

	// ---- independent minimal reader of the 64 bit table of a TZif file (no footer) ----
	struct RefTable
	{
		std::vector<int64_t> times;
		std::vector<int> typeIdx;
		std::vector<int> offsets;
		bool fixedFooter = false; // footer has no DST rule (no ','), e.g. "<-03>3"
		std::string footer; // POSIX TZ string of the file (may be empty)

		static uint64_t be(const std::vector<unsigned char>& b, size_t p, int n)
		{
			uint64_t v = 0;
			for (int i = 0; i < n; ++i) v = (v << 8) | b.at(p + i);
			return v;
		}

		static std::optional<RefTable> load(const std::filesystem::path& file)
		{
			std::ifstream in(file, std::ios::binary);
			if (!in) return std::nullopt;
			const std::vector<unsigned char> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			try
			{
				if (b.size() < 44 || b[0] != 'T' || b[1] != 'Z' || b[2] != 'i' || b[3] != 'f' || b[4] < '2') return std::nullopt;
				std::array<uint64_t, 6> c{}; // isut, isstd, leap, time, type, char
				for (int i = 0; i < 6; ++i) c[i] = be(b, 20 + i * 4, 4);
				size_t p = 44 + c[3] * 4 + c[3] + c[4] * 6 + c[5] + c[2] * 8 + c[1] + c[0];
				for (int i = 0; i < 6; ++i) c[i] = be(b, p + 20 + i * 4, 4);
				p += 44;
				RefTable t;
				// layout of the 64 bit block: times, type indices, ttinfo records (6 bytes each)
				for (uint64_t i = 0; i < c[3]; ++i) t.times.push_back(static_cast<int64_t>(be(b, p + i * 8, 8)));
				p += c[3] * 8;
				for (uint64_t i = 0; i < c[3]; ++i) t.typeIdx.push_back(b.at(p + i));
				p += c[3];
				for (uint64_t i = 0; i < c[4]; ++i) t.offsets.push_back(static_cast<int32_t>(be(b, p + i * 6, 4)));
				if (t.offsets.empty()) return std::nullopt;
				p += c[4] * 6 + c[5] + c[2] * 12 + c[1] + c[0]; // ttinfo, abbreviations, leap (12 byte in v2), isstd, isut
				if (p < b.size() && b[p] == '\n')
				{
					const size_t e = static_cast<size_t>(std::find(b.begin() + static_cast<std::ptrdiff_t>(p) + 1, b.end(), static_cast<unsigned char>('\n')) - b.begin());
					const std::string footer(b.begin() + static_cast<std::ptrdiff_t>(p) + 1, b.begin() + static_cast<std::ptrdiff_t>(e));
					t.fixedFooter = footer.find(',') == std::string::npos;
					t.footer = footer;
				}
				return t;
			}
			catch (...) { return std::nullopt; }
		}

		bool inTable(sys_seconds t) const
		{
			return !times.empty() && t.time_since_epoch().count() <= times.back();
		}

		long long at(sys_seconds t) const // only valid if inTable(t)
		{
			const int64_t tt = t.time_since_epoch().count();
			const size_t n = static_cast<size_t>(std::upper_bound(times.begin(), times.end(), tt) - times.begin());
			return n == 0 ? offsets[0] : offsets[typeIdx[n - 1]];
		}
	};

	struct Mismatch
	{
		sys_seconds t{};
		long long got = 0; // reader
		long long want = 0; // reference (RefTable or std::chrono)
	};

	struct ZoneResult
	{
		std::string name;
		size_t checked = 0;
		size_t refBug = 0; // reader != file table
		size_t dataDiff = 0; // table domain: reader follows the file but differs from std::chrono
		size_t dataDiffRecent = 0; // ... on/after 2023-01-01
		size_t footerOld = 0; // footer domain mismatch before 2023-01-01
		size_t footerNew = 0; // footer domain mismatch on/after 2023-01-01
		size_t footerRuleBug = 0; // footer rule (PosixTz) != file table where the rule is in effect (independent of skew)
		bool footerUnparsable = false;
		std::optional<Mismatch> firstRefBug, firstDiff, firstFooter, firstFooterRule;

		// footer mismatch explainable by skew: recent table data difference, or a recently changed zone with a fixed-offset footer
		bool footerExplained = false;

		bool bug() const { return refBug > 0 || footerRuleBug > 0 || footerUnparsable || footerOld > 0 || (footerNew > 0 && !footerExplained); }
		bool skew() const { return !bug() && (dataDiff > 0 || footerNew > 0); }
	};

	void note(std::optional<Mismatch>& slot, const Mismatch& m)
	{
		if (!slot || m.t < slot->t) slot = m;
	}

	void check(ZoneResult& r, const RefTable& ref, sys_seconds t, long long chronoOff, const OffsetFn& get)
	{
		if (t < rangeStart || t >= rangeEnd) return;
		++r.checked;
		const long long got = get(t);
		if (ref.inTable(t))
		{
			const long long fileOff = ref.at(t);
			if (got != fileOff)
			{
				++r.refBug;
				note(r.firstRefBug, { t, got, fileOff });
			}
			else if (got != chronoOff)
			{
				++r.dataDiff;
				if (t >= skewFrom) ++r.dataDiffRecent;
				note(r.firstDiff, { t, got, chronoOff });
			}
		}
		else if (got != chronoOff)
		{
			(t < skewFrom ? r.footerOld : r.footerNew)++;
			note(r.firstFooter, { t, got, chronoOff });
		}
	}

	// builds the function that evaluates the footer rule (the self check replaces it with wrong ones)
	using FooterFn = std::function<OffsetFn(const PosixTz&)>;

	// the footer rule against the file's own table where the rule is in effect
	void checkFooterRule(ZoneResult& r, const RefTable& ref, const FooterFn& makeFooter)
	{
		if (ref.footer.empty() || ref.times.empty()) return;
		const auto px = PosixTz::parse(ref.footer);
		if (!px)
		{
			r.footerUnparsable = true;
			return;
		}
		const OffsetFn f = makeFooter ? makeFooter(*px) : OffsetFn{ [&px](sys_seconds t) { return px->utcOffset(t).count(); } };
		const int64_t last = ref.times.back();
		int64_t lo = last;
		if (!ref.fixedFooter)
			lo = std::max<int64_t>(last - 2 * 365 * 86400LL, ref.times.size() >= 2 ? ref.times[ref.times.size() - 2] : last);
		auto one = [&](int64_t tt)
		{
			if (tt < lo || tt > last) return;
			const sys_seconds t{ seconds{ tt } };
			const long long got = f(t);
			const long long want = ref.at(t);
			if (got != want)
			{
				++r.footerRuleBug;
				note(r.firstFooterRule, { t, got, want });
			}
		};
		for (int64_t tt = lo; tt <= last; tt += 86400) one(tt);
		for (const int64_t tr : ref.times)
			for (const int64_t d : { -1LL, 0LL, 1LL }) one(tr + d);
	}

	ZoneResult compareZone(const std::string& name, const time_zone& tz, const RefTable& ref, const OffsetFn& get, const FooterFn& makeFooter = {})
	{
		ZoneResult r;
		r.name = name;
		checkFooterRule(r, ref, makeFooter);

		// the file's own transitions +/-1 s (reader vs file; std::chrono value looked up per instant)
		for (const int64_t tr : ref.times)
		{
			for (const long long d : { -1LL, 0LL, 1LL })
			{
				const sys_seconds t{ seconds{ tr + d } };
				if (t < rangeStart || t >= rangeEnd) continue;
				check(r, ref, t, tz.get_info(t).offset.count(), get);
			}
		}

		// std::chrono segments: transitions +/-1 s and the daily grid inside each segment
		sys_seconds t = rangeStart;
		sys_seconds nextDay = rangeStart;
		while (t < rangeEnd)
		{
			const sys_info info = tz.get_info(t);
			const sys_seconds end = info.end > rangeEnd ? rangeEnd : info.end;
			const long long want = info.offset.count();
			for (const sys_seconds s : { t, t + 1s, end - 2s, end - 1s })
				if (s >= t && s < end) check(r, ref, s, want, get);
			if (nextDay < t) nextDay = t;
			for (; nextDay < end; nextDay += 24h) check(r, ref, nextDay, want, get);
			if (end <= t) break;
			t = end;
		}
		r.footerExplained = r.dataDiffRecent > 0
			|| (ref.fixedFooter && !ref.times.empty() && ref.times.back() >= skewFrom.time_since_epoch().count());
		return r;
	}

	std::string fmt(sys_seconds t)
	{
		return std::format("{:%Y-%m-%d %H:%M:%S}", t);
	}

	std::string detail(const ZoneResult& r)
	{
		std::string s;
		auto add = [&](const char* what, const std::optional<Mismatch>& m)
		{
			if (m) s += std::format(" [{} {} reader={} other={}]", what, fmt(m->t), m->got, m->want);
		};
		add("reader!=file from", r.firstRefBug);
		add("data diff from", r.firstDiff);
		add("footer diff from", r.firstFooter);
			add("footer RULE != file table from", r.firstFooterRule);
			if (r.footerUnparsable) s += " [footer not parsable]";
		return s;
	}

	// ---- self check: deliberately wrong readers must be reported as BUG ----
	bool selfCheck(const std::filesystem::path& folder)
	{
		struct Mutant { const char* name; std::function<OffsetFn(const Zone&)> make; FooterFn footer; };
		const auto plain = [](const Zone& z) -> OffsetFn { return [&z](sys_seconds t) { return z.utcOffset(t).count(); }; };
		const FooterFn shifted = [](const PosixTz& px) -> OffsetFn { return [&px](sys_seconds t) { return px.utcOffset(t - 168h).count(); }; };
		const std::vector<Mutant> mutants{
			{ "offset +1h from 2000", [](const Zone& z) -> OffsetFn { return [&z](sys_seconds t) { return z.utcOffset(t).count() + (t >= sys_days{ 2000y / 1 / 1 } ? 3600 : 0); }; }, {} },
			{ "off by one second at transitions", [](const Zone& z) -> OffsetFn { return [&z](sys_seconds t) { return z.utcOffset(t - 1s).count(); }; }, {} },
			{ "footer rule shifted by 7 days", [](const Zone& z) -> OffsetFn { return [&z](sys_seconds t) { return z.utcOffset(t >= sys_days{ 2040y / 1 / 1 } ? t - 168h : t).count(); }; }, shifted },
			{ "footer rule shifted by 7 days (footer check alone)", plain, shifted },
		};

		bool ok = true;
		size_t zonesChecked = 0;
		// America/Nuuk and Europe/Chisinau changed their rules recently (skewed against older data) and have a DST footer
		for (const char* name : { "Europe/Berlin", "America/New_York", "Australia/Sydney", "America/Nuuk", "Europe/Chisinau" })
		{
			auto lr = loadZone(folder, name);
			auto ref = RefTable::load(folder / name);
			if (lr.status != LoadStatus::Ok || !lr.zone || !ref)
			{
				std::cout << "selfcheck FAILED: " << name << " not loadable\n";
				ok = false;
				continue;
			}
			const time_zone* tz = nullptr;
			try { tz = locate_zone(name); }
			catch (...)
			{
				std::cout << "selfcheck FAILED: " << name << " unknown to std::chrono\n";
				ok = false;
				continue;
			}
			++zonesChecked;

			const auto base = compareZone(name, *tz, *ref, plain(*lr.zone));
			std::cout << "selfcheck " << name << " unmodified: " << (base.bug() ? "BUG" : base.skew() ? "skew" : "identical") << "\n";
			if (base.bug())
			{
				std::cout << "selfcheck FAILED: unmodified " << name << " reported as BUG\n";
				ok = false;
			}
			for (const auto& m : mutants)
			{
				const bool caught = compareZone(name, *tz, *ref, m.make(*lr.zone), m.footer).bug();
				std::cout << "selfcheck " << name << " / " << m.name << ": " << (caught ? "detected" : "NOT DETECTED") << "\n";
				if (!caught) ok = false;
			}
		}
		if (zonesChecked == 0)
		{
			std::cout << "selfcheck FAILED: no zone was checked\n";
			ok = false;
		}
		return ok;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cerr << "usage: vsid_tz_diff <zoneinfo-folder> [--verbose]\n";
		return 2;
	}
	const std::filesystem::path folder = argv[1];
	bool verbose = false;
	for (int i = 2; i < argc; ++i) if (std::string(argv[i]) == "--verbose") verbose = true;

	const auto start = steady_clock::now();
	try
	{
		if (!std::filesystem::is_directory(folder) || loadZone(folder, "Etc/UTC").status == LoadStatus::FolderMissing)
		{
			std::cerr << "error: zoneinfo folder not found: " << folder.string() << "\n";
			return 1;
		}
		const auto& db = get_tzdb();
		const auto ver = readVersion(folder);
		std::cout << "zoneinfo version: " << (ver ? *ver : "(no +VERSION)") << "\n";
		std::cout << "std::chrono tzdb version: " << db.version << "\n";

		const bool selfOk = selfCheck(folder);

		std::vector<std::string> names;
		for (const auto& z : db.zones) names.emplace_back(z.name());
		for (const auto& l : db.links) names.emplace_back(l.name());
		std::sort(names.begin(), names.end());
		names.erase(std::unique(names.begin(), names.end()), names.end());

		std::vector<std::string> missing, skew, bug;
		std::map<std::string, ZoneResult> results;
		std::set<std::string> found;
		size_t identical = 0, checked = 0;
		for (const auto& name : names)
		{
			auto lr = loadZone(folder, name);
			if (lr.status == LoadStatus::InvalidName)
			{
				bug.push_back(name + " (invalid zone name rejected by the reader)");
				continue;
			}
			if (lr.status == LoadStatus::NotFound)
			{
				missing.push_back(name);
				continue;
			}
			found.insert(name);
			if (lr.status != LoadStatus::Ok || !lr.zone)
			{
				bug.push_back(name + " (load status " + std::to_string(static_cast<int>(lr.status)) + ")");
				continue;
			}
			auto ref = RefTable::load(folder / name);
			if (!ref)
			{
				bug.push_back(name + " (file not readable by the reference reader)");
				continue;
			}
			const time_zone* tz = nullptr;
			try { tz = locate_zone(name); }
			catch (...)
			{
				bug.push_back(name + " (std::chrono locate_zone failed)");
				continue;
			}

			auto r = compareZone(name, *tz, *ref, [&](sys_seconds t) { return lr.zone->utcOffset(t).count(); });
			checked += r.checked;
			if (r.bug()) bug.push_back(name);
			else if (r.skew()) skew.push_back(name);
			else ++identical;
			if (r.bug() || r.skew()) results.emplace(name, r);
			if (verbose && (r.bug() || r.skew())) std::cout << (r.bug() ? "BUG " : "SKEW ") << name << detail(r) << "\n";
		}

		for (const char* k : { "Etc/UTC", "Europe/Berlin", "Australia/Sydney", "Europe/Dublin", "Africa/Casablanca", "Asia/Kolkata", "America/Sao_Paulo" })
		{
			if (!found.contains(k)) bug.push_back(std::string(k) + " (required zone missing)");
		}

		std::cout << "zones: " << names.size() << " identical: " << identical << " checked instants: " << checked << "\n";
		std::cout << "MISSING: " << missing.size() << "\n";
		if (verbose || missing.size() <= 20)
			for (const auto& n : missing) std::cout << "  MISSING " << n << "\n";
		else
			std::cout << "  (list suppressed, use --verbose)\n";
		std::cout << "SKEW: " << skew.size() << "\n";
		for (const auto& n : skew) std::cout << "  SKEW " << n << detail(results.at(n)) << "\n";
		std::cout << "BUG: " << bug.size() << "\n";
		for (const auto& n : bug)
		{
			std::cout << "  BUG " << n;
			if (auto it = results.find(n); it != results.end()) std::cout << detail(it->second);
			std::cout << "\n";
		}
		if (!selfOk) std::cout << "SELFCHECK FAILED\n";
		std::cout << "elapsed s: " << duration_cast<seconds>(steady_clock::now() - start).count() << "\n";
		return (bug.empty() && selfOk) ? 0 : 1;
	}
	catch (const std::exception& e)
	{
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}
}
