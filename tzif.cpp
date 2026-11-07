/*
vSID is a plugin for the Euroscope controller software on the Vatsim network.
The aim of vSID is to ease the work of any controller that edits and assigns
SIDs to flightplans.

Copyright (C) 2024 Gameagle (Philip Maier)
Repo @ https://github.com/Gameagle/vSID

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "tzif.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <system_error>

namespace vsid
{
	namespace tzif
	{
		bool isValidZoneName(std::string_view name) noexcept
		{
			if (name.empty() || name.size() > 255) return false;

			int segments = 1;
			size_t segLen = 0;

			for (const char c : name)
			{
				if (c == '/')
				{
					if (segLen == 0) return false;
					if (++segments > 3) return false;
					segLen = 0;
					continue;
				}

				const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
					c == '_' || c == '+' || c == '-';
				if (!ok) return false;
				++segLen;
			}

			return segLen != 0;
		}

		// parser helpers (anonymous namespace: internal linkage, they are not part of the public interface)
		namespace
		{
		struct Cursor
		{
			std::string_view s;
			size_t pos = 0;

			bool atEnd() const { return pos >= s.size(); }
			char peek() const { return atEnd() ? '\0' : s[pos]; }
		};

		bool isAlpha(char c)
		{
			return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		}

		bool isDigit(char c)
		{
			return c >= '0' && c <= '9';
		}

		bool isLeap(int y)
		{
			return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
		}

		bool parseName(Cursor& c)
		{
			if (c.peek() == '<')
			{
				++c.pos;
				size_t len = 0;
				while (!c.atEnd() && c.peek() != '>')
				{
					const char ch = c.peek();
					if (!isAlpha(ch) && !isDigit(ch) && ch != '+' && ch != '-') return false;
					++c.pos;
					++len;
				}
				if (c.atEnd() || len == 0) return false;
				++c.pos; // '>'
				return true;
			}

			size_t len = 0;
			while (isAlpha(c.peek()))
			{
				++c.pos;
				++len;
			}
			return len >= 3;
		}

		bool parseNumber(Cursor& c, int maxDigits, long long& out)
		{
			int digits = 0;
			long long v = 0;
			while (isDigit(c.peek()))
			{
				if (++digits > maxDigits) return false;
				v = v * 10 + (c.peek() - '0');
				++c.pos;
			}
			if (digits == 0) return false;
			out = v;
			return true;
		}

		// [+-]hh[:mm[:ss]] in seconds (sign applied)
		bool parseTime(Cursor& c, long long maxHours, long long& out)
		{
			long long sign = 1;
			if (c.peek() == '+' || c.peek() == '-')
			{
				if (c.peek() == '-') sign = -1;
				++c.pos;
			}

			long long h = 0, m = 0, sec = 0;
			if (!parseNumber(c, 3, h) || h > maxHours) return false;
			if (c.peek() == ':')
			{
				++c.pos;
				if (!parseNumber(c, 2, m) || m > 59) return false;
				if (c.peek() == ':')
				{
					++c.pos;
					if (!parseNumber(c, 2, sec) || sec > 59) return false;
				}
			}

			out = sign * (h * 3600 + m * 60 + sec);
			return true;
		}
		}

		std::optional<PosixTz> PosixTz::parse(std::string_view footer)
		{
			Cursor c{ footer, 0 };
			PosixTz tz;

			if (!parseName(c)) return std::nullopt;

			long long off = 0;
			if (!parseTime(c, 24, off)) return std::nullopt;
			tz.stdOffset = -off; // POSIX: positive = west of Greenwich

			if (c.atEnd()) return tz;

			if (!parseName(c)) return std::nullopt;
			tz.hasDst = true;
			tz.dstOffset = tz.stdOffset + 3600;

			if (!c.atEnd() && c.peek() != ',')
			{
				if (!parseTime(c, 24, off)) return std::nullopt;
				tz.dstOffset = -off;
			}

			if (c.atEnd())
			{
				// no rule given: POSIX leaves it implementation defined, use the US default like tzcode
				tz.start.type = RuleType::Month;
				tz.start.month = 3;
				tz.start.week = 2;
				tz.start.weekday = 0;
				tz.end.type = RuleType::Month;
				tz.end.month = 11;
				tz.end.week = 1;
				tz.end.weekday = 0;
				return tz;
			}

			auto parseRule = [&c](Rule& r) -> bool
				{
					long long v = 0;
					if (c.peek() == 'M')
					{
						++c.pos;
						r.type = RuleType::Month;
						if (!parseNumber(c, 2, v) || v < 1 || v > 12) return false;
						r.month = static_cast<int>(v);
						if (c.peek() != '.') return false;
						++c.pos;
						if (!parseNumber(c, 1, v) || v < 1 || v > 5) return false;
						r.week = static_cast<int>(v);
						if (c.peek() != '.') return false;
						++c.pos;
						if (!parseNumber(c, 1, v) || v > 6) return false;
						r.weekday = static_cast<int>(v);
					}
					else if (c.peek() == 'J')
					{
						++c.pos;
						r.type = RuleType::Julian1;
						if (!parseNumber(c, 3, v) || v < 1 || v > 365) return false;
						r.day = static_cast<int>(v);
					}
					else
					{
						r.type = RuleType::Julian0;
						if (!parseNumber(c, 3, v) || v > 365) return false;
						r.day = static_cast<int>(v);
					}

					r.time = 7200;
					if (c.peek() == '/')
					{
						++c.pos;
						if (!parseTime(c, 167, r.time)) return false;
					}
					return true;
				};

			if (c.peek() != ',') return std::nullopt;
			++c.pos;
			if (!parseRule(tz.start)) return std::nullopt;
			if (c.peek() != ',') return std::nullopt;
			++c.pos;
			if (!parseRule(tz.end)) return std::nullopt;
			if (!c.atEnd()) return std::nullopt;

			return tz;
		}

		long long PosixTz::transition(const Rule& rule, int year, long long localOffset)
		{
			const std::chrono::sys_days jan1{ std::chrono::year{ year } / std::chrono::January / 1 };
			long long dayNum = 0;

			switch (rule.type)
			{
			case RuleType::Month:
			{
				const std::chrono::year_month ym = std::chrono::year{ year } / std::chrono::month{ static_cast<unsigned>(rule.month) };
				const std::chrono::sys_days first{ ym / 1 };
				const int firstWd = static_cast<int>(std::chrono::weekday{ first }.c_encoding());
				const int lastDom = static_cast<int>(static_cast<unsigned>((ym / std::chrono::last).day()));
				int dom = 1 + ((rule.weekday - firstWd + 7) % 7) + 7 * (rule.week - 1);
				while (dom > lastDom) dom -= 7;
				dayNum = static_cast<long long>(first.time_since_epoch().count()) + (dom - 1);
				break;
			}
			case RuleType::Julian1:
			{
				int n = rule.day - 1;
				if (isLeap(year) && rule.day >= 60) ++n; // Feb 29 is never counted
				dayNum = static_cast<long long>(jan1.time_since_epoch().count()) + n;
				break;
			}
			case RuleType::Julian0:
				dayNum = static_cast<long long>(jan1.time_since_epoch().count()) + rule.day;
				break;
			}

			return dayNum * 86400LL + rule.time - localOffset;
		}

		std::chrono::seconds PosixTz::utcOffset(std::chrono::sys_seconds t) const
		{
			if (!hasDst) return std::chrono::seconds{ stdOffset };

			// clamp to about +-28000 years (inside std::chrono::year's range) so the arithmetic below cannot overflow
			constexpr long long limit = 900'000'000'000LL;
			const long long tt = std::clamp(static_cast<long long>(t.time_since_epoch().count()), -limit, limit);
			const std::chrono::sys_seconds localTime{ std::chrono::seconds{ tt + stdOffset } };
			const std::chrono::sys_days localDay = std::chrono::floor<std::chrono::days>(localTime);
			const int y = static_cast<int>(std::chrono::year_month_day{ localDay }.year());

			const long long s = transition(start, y, stdOffset); // DST starts in standard time
			const long long e = transition(end, y, dstOffset); // DST ends in daylight time

			const bool inDst = (s <= e) ? (tt >= s && tt < e) : !(tt >= e && tt < s);

			return std::chrono::seconds{ inDst ? dstOffset : stdOffset };
		}

		// TZif reader helpers (anonymous namespace: internal linkage). All offsets are validated by the caller against the buffer,
		// readBE additionally refuses to read outside of it.
		namespace
		{

		constexpr size_t headerSize = 44;
		constexpr size_t maxFooterSize = 256;
		constexpr uint64_t maxFileSize = 1024 * 1024; // real zone files are a few KB
		constexpr int maxUtcOffset = 93600; // 26h, real data stays below 17h

		uint64_t readBE(std::span<const unsigned char> b, uint64_t pos, size_t n)
		{
			if (n > b.size() || pos > b.size() - n) return 0;

			uint64_t v = 0;
			for (size_t i = 0; i < n; ++i) v = (v << 8) | b[static_cast<size_t>(pos) + i];
			return v;
		}

		// counts: isutcnt, isstdcnt, leapcnt, timecnt, typecnt, charcnt (each at most 2^32 - 1)
		bool readHeader(std::span<const unsigned char> b, uint64_t pos, unsigned char& version, std::array<uint64_t, 6>& counts)
		{
			if (pos > b.size() || b.size() - pos < headerSize) return false;

			if (b[static_cast<size_t>(pos)] != 'T' || b[static_cast<size_t>(pos) + 1] != 'Z' ||
				b[static_cast<size_t>(pos) + 2] != 'i' || b[static_cast<size_t>(pos) + 3] != 'f') return false;

			version = b[static_cast<size_t>(pos) + 4];
			for (size_t i = 0; i < counts.size(); ++i) counts[i] = readBE(b, pos + 20 + 4 * i, 4);

			return true;
		}

		// size of the data block following a header; no overflow possible with 32 bit counts in 64 bit
		uint64_t blockSize(const std::array<uint64_t, 6>& c, uint64_t timeSize)
		{
			return c[3] * (timeSize + 1) + c[4] * 6 + c[5] + c[2] * (timeSize + 4) + c[1] + c[0];
		}
		}

		std::optional<Zone> Zone::parse(std::span<const unsigned char> bytes)
		{
			unsigned char ver = 0;
			std::array<uint64_t, 6> h1{};
			if (!readHeader(bytes, 0, ver, h1)) return std::nullopt;
			if (ver < '2' || ver > '9') return std::nullopt; // v1 has no 64 bit data and no footer

			// skip the v1 block
			uint64_t pos = headerSize + blockSize(h1, 4);
			if (pos > bytes.size()) return std::nullopt;

			std::array<uint64_t, 6> h{};
			if (!readHeader(bytes, pos, ver, h)) return std::nullopt;
			if (ver < '2' || ver > '9') return std::nullopt;
			pos += headerSize;

			// leap second records are not evaluated; accepting them would silently yield wrong instants
			if (h[2] != 0) return std::nullopt;

			const uint64_t isutcnt = h[0], isstdcnt = h[1], timecnt = h[3], typecnt = h[4], charcnt = h[5];
			if (typecnt == 0 || typecnt > 256 || charcnt == 0) return std::nullopt;
			if ((isutcnt != 0 && isutcnt != typecnt) || (isstdcnt != 0 && isstdcnt != typecnt)) return std::nullopt;

			const uint64_t size = blockSize(h, 8);
			if (size > bytes.size() - pos) return std::nullopt; // also bounds every allocation below by the buffer size

			const uint64_t timesPos = pos;
			const uint64_t idxPos = timesPos + timecnt * 8;
			const uint64_t typesPos = idxPos + timecnt;
			const uint64_t dataEnd = pos + size;

			// footer: '\n' <posix tz> '\n'
			if (dataEnd >= bytes.size() || bytes[static_cast<size_t>(dataEnd)] != '\n') return std::nullopt;
			uint64_t footerEnd = dataEnd + 1;
			while (footerEnd < bytes.size() && bytes[static_cast<size_t>(footerEnd)] != '\n') ++footerEnd;
			if (footerEnd >= bytes.size()) return std::nullopt;
			if (footerEnd - (dataEnd + 1) > maxFooterSize) return std::nullopt;

			Zone zone;

			for (uint64_t i = 0; i < typecnt; ++i)
			{
				const uint64_t rec = typesPos + i * 6;
				const int64_t utoff = static_cast<int32_t>(static_cast<uint32_t>(readBE(bytes, rec, 4)));
				const unsigned char isdst = bytes[static_cast<size_t>(rec) + 4];
				const unsigned char abbr = bytes[static_cast<size_t>(rec) + 5];

				if (utoff > maxUtcOffset || utoff < -maxUtcOffset) return std::nullopt;
				if (isdst > 1 || abbr >= charcnt) return std::nullopt;

				zone.offsets.push_back(static_cast<int>(utoff));
			}

			zone.times.reserve(static_cast<size_t>(timecnt));
			zone.typeIndex.reserve(static_cast<size_t>(timecnt));
			for (uint64_t i = 0; i < timecnt; ++i)
			{
				const long long t = static_cast<long long>(readBE(bytes, timesPos + i * 8, 8));
				const unsigned char idx = bytes[static_cast<size_t>(idxPos + i)];

				if (idx >= typecnt) return std::nullopt;
				if (!zone.times.empty() && t <= zone.times.back()) return std::nullopt;

				zone.times.push_back(t);
				zone.typeIndex.push_back(idx);
			}

			const std::string_view rule(reinterpret_cast<const char*>(bytes.data()) + dataEnd + 1,
				static_cast<size_t>(footerEnd - (dataEnd + 1)));
			if (!rule.empty())
			{
				zone.footer = PosixTz::parse(rule);
				if (!zone.footer) return std::nullopt;
			}

			return zone;
		}

		std::chrono::seconds Zone::utcOffset(std::chrono::sys_seconds t) const
		{
			if (offsets.empty()) return std::chrono::seconds{ 0 }; // default constructed zone

			const long long tt = t.time_since_epoch().count();

			if (!times.empty() && tt <= times.back())
			{
				const auto it = std::upper_bound(times.begin(), times.end(), tt);
				if (it == times.begin()) return std::chrono::seconds{ offsets[0] }; // before the first transition: type 0 (RFC 8536)
				return std::chrono::seconds{ offsets[typeIndex[static_cast<size_t>(it - times.begin()) - 1]] };
			}

			if (footer) return footer->utcOffset(t);

			// no footer: the last transition stays in effect
			return std::chrono::seconds{ offsets[times.empty() ? 0 : typeIndex.back()] };
		}

		LoadResult loadZone(const std::filesystem::path& folder, std::string_view name)
		{
			if (!isValidZoneName(name)) return { LoadStatus::InvalidName, std::nullopt };

			try
			{
				std::error_code ec;

				if (!std::filesystem::is_directory(folder, ec) || ec) return { LoadStatus::FolderMissing, std::nullopt };

				const std::filesystem::path root = std::filesystem::weakly_canonical(folder, ec);
				if (ec) return { LoadStatus::FolderMissing, std::nullopt };

				// the name is validated ASCII, so the conversion is lossless
				const std::filesystem::path file = std::filesystem::weakly_canonical(root / std::filesystem::path(std::string(name)), ec);
				if (ec) return { LoadStatus::NotFound, std::nullopt };

				// the resolved path (symlinks followed) must stay below the folder
				const std::filesystem::path rel = file.lexically_relative(root);
				if (rel.empty() || rel == "." || *rel.begin() == "..") return { LoadStatus::InvalidName, std::nullopt };

				if (!std::filesystem::is_regular_file(file, ec) || ec) return { LoadStatus::NotFound, std::nullopt };

				const uint64_t fileSize = std::filesystem::file_size(file, ec);
				if (ec) return { LoadStatus::Corrupt, std::nullopt };
				if (fileSize > maxFileSize) return { LoadStatus::Corrupt, std::nullopt };

				std::ifstream in(file, std::ios::binary);
				// the file exists (checked above) but cannot be opened (permissions, sharing violation): a broken installation, not a missing zone
				if (!in) return { LoadStatus::Corrupt, std::nullopt };

				std::vector<unsigned char> data(static_cast<size_t>(fileSize));
				in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
				if (static_cast<uint64_t>(in.gcount()) != fileSize) return { LoadStatus::Corrupt, std::nullopt };

				std::optional<Zone> zone = Zone::parse(data);
				if (!zone) return { LoadStatus::Corrupt, std::nullopt };

				return { LoadStatus::Ok, std::move(zone) };
			}
			catch (...)
			{
				return { LoadStatus::Corrupt, std::nullopt };
			}
		}

		std::optional<std::string> readVersion(const std::filesystem::path& folder)
		{
			try
			{
				std::ifstream in(folder / "+VERSION", std::ios::binary);
				if (!in) return std::nullopt;

				// only the beginning is of interest, never read an unbounded line
				char buf[256];
				in.read(buf, sizeof(buf));
				std::string_view text(buf, static_cast<size_t>(in.gcount()));

				const size_t nl = text.find('\n');
				if (nl != std::string_view::npos) text = text.substr(0, nl);

				const char* ws = " \t\r\n";
				const size_t first = text.find_first_not_of(ws);
				if (first == std::string_view::npos) return std::nullopt;
				const size_t last = text.find_last_not_of(ws);

				return std::string(text.substr(first, last - first + 1));
			}
			catch (...)
			{
				return std::nullopt;
			}
		}
	}
}
