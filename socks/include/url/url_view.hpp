//
// Copyright (c) 2018 jackarain (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#pragma once

#include <string>
#include <stdexcept>
#include <iterator>

#include <boost/beast/core/string.hpp>

namespace urls {

	using boost::beast::string_view;

	//--------------------------------------------------------------------------
	// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]

	namespace detail {

		inline bool isdigit(const char c) noexcept
		{
			return c >= '0' && c <= '9';
		}

		inline bool isalpha(const char c) noexcept
		{
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
		}

		inline bool isunreserved(const char c) noexcept
		{
			if (isalpha(c) || isdigit(c) ||
				c == '-' || c == '.' || c == '_' || c == '~' || c <= 0x20 || 0x7f <= c)
				return true;
			return false;
		}

		inline bool isuchar(const char c) noexcept
		{
			if (isunreserved(c) || c == ';' || c == '?' || c == '&' || c == '=')
				return true;
			return false;
		}

		inline bool ishsegment(const char c) noexcept
		{
			if (isuchar(c) ||
				c == ';' || c == ':' || c == '@' || c == '&' || c == '=')
				return true;
			return false;
		}

		inline bool issubdelims(const char c) noexcept
		{
			if (c == '!' || c == '$' || c == '\'' || c == '(' ||
				c == ')' || c == '*' || c == '+' || c == ',' ||
				c == '=')
				return true;
			return false;
		}

		inline bool uri_reserved(const char c) noexcept
		{
			if (c == ';' || c == '/' || c == '?' || c == ':' ||
				c == '@' || c == '&' || c == '=' || c == '+' ||
				c == '$' || c == ',')
				return true;
			return false;
		}

		inline bool uri_mark(const char c) noexcept
		{
			if (c == '-' || c == '_' || c == '.' || c == '!' ||
				c == '~' || c == '*' || c == '\'' || c == '(' ||
				c == ')')
				return true;
			return false;
		}

		static string_view to_hex(unsigned char c) noexcept
		{
			static const char* hexstring[] = {
				"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b", "0c", "0d", "0e", "0f",
				"10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1a", "1b", "1c", "1d", "1e", "1f",
				"20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f",
				"30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3a", "3b", "3c", "3d", "3e", "3f",
				"40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4a", "4b", "4c", "4d", "4e", "4f",
				"50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e", "5f",
				"60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b", "6c", "6d", "6e", "6f",
				"70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7a", "7b", "7c", "7d", "7e", "7f",
				"80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f",
				"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9a", "9b", "9c", "9d", "9e", "9f",
				"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af",
				"b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf",
				"c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb", "cc", "cd", "ce", "cf",
				"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "da", "db", "dc", "dd", "de", "df",
				"e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef",
				"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "fa", "fb", "fc", "fd", "fe", "ff"
			};

			return string_view(hexstring[c]);
		}
	}

	class url_view
	{
	public:
		url_view() = default;

		url_view(const char* s)
		{
			if (!parse(s))
				throw std::invalid_argument("URI malformed");
		}

		url_view(const std::string& s)
		{
			if (!parse(s))
				throw std::invalid_argument("URI malformed");
		}

		url_view(string_view s)
		{
			if (!parse(s))
				throw std::invalid_argument("URI malformed");
		}

		~url_view() = default;

		string_view scheme() noexcept
		{
			return scheme_;
		}

		string_view host() noexcept
		{
			return host_;
		}

		string_view port() noexcept
		{
			if (!port_.empty())
				return port_;
			return knownport();
		}

		string_view username() noexcept
		{
			return username_;
		}

		string_view password() noexcept
		{
			return password_;
		}

		string_view path() noexcept
		{
			return path_;
		}

		string_view query() noexcept
		{
			return query_;
		}

		string_view fragment() noexcept
		{
			return fragment_;
		}

		bool parse(string_view url) noexcept
		{
			using namespace detail;
			using boost::beast::iequals;

			enum
			{
				scheme_start,
				scheme,
				slash_start,
				slash,
				urn,
				probe_userinfo_hostname,
				host_start,
				host,
				port_start,
				port,
				path,
				query,
				fragment
			} state = scheme_start;

			auto b = url.data();
			auto e = url.data() + url.size();
			auto part_start = b;
			const char* v6_start = nullptr;
			const char* v6_end = nullptr;
			bool is_ipv6 = false;
			string_view probe;

			while (b != e)
			{
				auto c = *b++;
				switch (state)
				{
				case scheme_start:
					if (isalpha(c))
					{
						state = scheme;
						continue;
					}
					return false;
				case scheme:
					if (isalpha(c) || isdigit(c) ||
						c == '+' || c == '-' || c == '.') // e.g: stratum+tcp://stratum.example.com
						continue;
					if (c == ':')
					{
						scheme_ = string_view(part_start, b - part_start - 1);
						state = slash_start;
						continue;
					}
					return false;
				case slash_start:
					if (c == '/')
					{
						if (iequals(scheme_, "file"))
						{
							if (b + 1 < e)
							{
								if (*b == '/' && *(b + 1) == '/')
									b++;
							}
						}
						state = slash;
					}
					else
					{
						state = urn;
						part_start = --b;
					}
					continue;
				case urn:
					if (b == e)
					{
						path_ = string_view(part_start, b - part_start);
						return true;
					}
					if (c == '?')
					{
						path_ = string_view(part_start, b - part_start - 1);
						part_start = b;
						state = query;
						continue;
					}
					continue;
				case slash:
					if (c == '/')
					{
						state = probe_userinfo_hostname;
						part_start = b;
						continue;
					}
					return false;
				case probe_userinfo_hostname:
					if (c == '[')
					{
						if (v6_start)
							is_ipv6 = false;
						else
						{
							is_ipv6 = true;
							v6_start = b;
						}
					}
					if (c == ']')
					{
						if (is_ipv6 && !v6_end)
						{
							v6_end = b - 1;
							if (!probe.empty())
								probe = string_view();
						}
						else
							is_ipv6 = false;
					}
					if (c == '@')
					{
						if (!probe.empty())
						{
							auto pwd = probe.data() + probe.size() + 1;
							password_ = string_view(pwd, b - pwd - 1);
							username_ = probe;
						}
						else
						{
							username_ = string_view(part_start, b - part_start - 1); // username
						}

						is_ipv6 = false;

						part_start = b;
						state = host_start;
						continue;
					}

					if (c == ':')
					{
						if (probe.empty())
							probe = string_view(part_start, b - part_start - 1); // username or hostname
						continue;
					}

					if (c == '/' || c == '?' || c == '#')
					{
						if (is_ipv6)
						{
							if (!v6_start || !v6_end || v6_start == v6_end)
								return false;

							host_ = string_view(v6_start, v6_end - v6_start); // hostname
							if (!probe.empty()) // port
							{
								auto port = probe.data() + probe.size() + 1;
								port_ = string_view(port, b - port - 1);
							}
						}
						else
						{
							if (v6_start || v6_end)
								return false;

							if (!probe.empty()) // port
							{
								auto port = probe.data() + probe.size() + 1;
								port_ = string_view(port, b - port - 1);
								host_ = probe;
							}
							else // hostname
							{
								host_ = string_view(part_start, b - part_start - 1);
							}
						}

						if (c == '/')
						{
							part_start = --b;
							state = path;
							continue;
						}
						if (c == '?')
						{
							if (b == e)
								return true;
							part_start = b;
							state = query;
							continue;
						}
						if (c == '#')
						{
							if (b == e)
								return true;
							part_start = b;
							state = fragment;
							continue;
						}
					}
					if (b == e)
					{
						if (is_ipv6)
						{
							if (!v6_start || !v6_end || v6_start == v6_end)
								return false;

							host_ = string_view(v6_start, v6_end - v6_start); // hostname
							if (!probe.empty()) // port
							{
								auto port = probe.data() + probe.size() + 1;
								port_ = string_view(port, b - port);
							}
						}
						else
						{
							if (v6_start || v6_end)
								return false;

							if (!probe.empty()) // port
							{
								auto port = probe.data() + probe.size() + 1;
								port_ = string_view(port, b - port);
								host_ = probe; // hostname
							}
							else // hostname
							{
								host_ = string_view(part_start, b - part_start);
							}
						}

						return true;
					}
					if (is_ipv6)
					{
						if (!isdigit(c)
							&& c != 'a' && c != 'A'
							&& c != 'b' && c != 'B'
							&& c != 'c' && c != 'C'
							&& c != 'd' && c != 'D'
							&& c != 'e' && c != 'E'
							&& c != 'f' && c != 'F'
							&& c != ':' && c != ']'
							&& c != '[' && c != '.')
						{
							is_ipv6 = false;
						}
					}
					if (isunreserved(c) || issubdelims(c) || c == '%' || c == ' ' || c == '[' || c == ']')
						continue;
					return false;
				case host_start:
					if (isunreserved(c) || issubdelims(c) || c == '%' || c == '[')
					{
						if (c == '[')
						{
							is_ipv6 = true;
							part_start = b;
						}

						state = host;
						continue;
					}
					return false;
				case host:
					if (b == e) // end
					{
						if (is_ipv6 && c == '/')
							return false;

						if (c == '/' || c == ']')
						{
							host_ = string_view(part_start, b - part_start - 1);
							return true;
						}

						host_ = string_view(part_start, b - part_start);
						return true;
					}
					if (is_ipv6)
					{
						if (c == ']')
						{
							host_ = string_view(part_start, b - part_start - 1);
							if (b == e)
								return true;
							if (*b == ':')
							{
								b++; // skip ':'
								part_start = b;
								state = port_start;
								continue;
							}
							if (*b == '/')
							{
								part_start = b;
								state = path;
								continue;
							}
							if (*b == '?')
							{
								if (b == e)
									return true;
								part_start = ++b; // skip '?'
								state = query;
								continue;
							}
							if (*b == '#')
							{
								if (b == e)
									return true;
								part_start = ++b; // skip '#'
								state = fragment;
								continue;
							}
							return false;
						}
						else if (c == '/')
						{
							return false;
						}
					}
					else
					{
						if (c == ':')
						{
							host_ = string_view(part_start, b - part_start - 1);
							part_start = b;
							state = port_start;
							continue;
						}
					}
					if (c == '/')
					{
						host_ = string_view(part_start, b - part_start - 1);
						part_start = --b;
						state = path;
						continue;
					}
					if (isunreserved(c) || issubdelims(c) || c == '%' || c == ':' || c == ' ')
						continue;
					if (c == '?')
					{
						host_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = query;
						continue;
					}
					if (c == '#')
					{
						host_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = fragment;
						continue;
					}
					return false;
				case port_start:
					if (isdigit(c))
					{
						state = port;
						continue;
					}
					return false;
				case port:
					if (c == '/')
					{
						port_ = string_view(part_start, b - part_start - 1);
						part_start = --b;
						state = path;
						continue;
					}
					if (c == '?')
					{
						port_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = query;
						continue;
					}
					if (c == '#')
					{
						port_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = fragment;
						continue;
					}
					if (b == e) // no path
					{
						port_ = string_view(part_start, b - part_start);
						return true;
					}
					if (isdigit(c))
						continue;
					return false;
				case path:
					if (c == '?')
					{
						path_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = query;
						continue;
					}
					if (c == '#')
					{
						path_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = fragment;
						continue;
					}
					if (b == e)
					{
						path_ = string_view(part_start, b - part_start);
						return true;
					}
					if (isunreserved(c) || issubdelims(c) || c == '%' || c == '/' ||
						c == '&' || c == ':')
						continue;
					return false;
				case query:
					if (c == '#')
					{
						query_ = string_view(part_start, b - part_start - 1);
						if (b == e)
							return true;
						part_start = b;
						state = fragment;
						continue;
					}
					if (b == e)
					{
						query_ = string_view(part_start, b - part_start);
						return true;
					}
					if (ishsegment(c) || issubdelims(c) || c == '/' || c == '?')
						continue;
					return false;
				case fragment:
					if (b == e)
					{
						fragment_ = string_view(part_start, b - part_start);
						return true;
					}
					if (ishsegment(c) || issubdelims(c) || c == '/' || c == '?')
						continue;
					return false;
				}
			}

			return false;
		}

		static std::string encodeURI(string_view str) noexcept
		{
			using namespace ::urls::detail;
			std::string result;

			for (const auto& c : str)
			{
				if (isalpha(c) || isdigit(c) || uri_mark(c) || uri_reserved(c))
				{
					result += c;
				}
				else
				{
					result += '%';
					result += std::string(to_hex((unsigned char)c));
				}
			}

			return result;
		}

		static std::string decodeURI(string_view str)
		{
			using namespace ::urls::detail;
			std::string result;

			auto start = str.cbegin();
			auto end = str.cend();

			for (; start != end; ++start)
			{
				char c = *start;
				if (c == '%')
				{
					auto first = std::next(start);
					if (first == end)
						throw std::invalid_argument("URI malformed");

					auto second = std::next(first);
					if (second == end)
						throw std::invalid_argument("URI malformed");

					if (isdigit(*first))
						c = *first - '0';
					else if (*first >= 'A' && *first <= 'F')
						c = *first - 'A' + 10;
					else if (*first >= 'a' && *first <= 'f')
						c = *first - 'a' + 10;
					else
						throw std::invalid_argument("URI malformed");

					c <<= 4;

					if (isdigit(*second))
						c += *second - '0';
					else if (*second >= 'A' && *second <= 'F')
						c += *second - 'A' + 10;
					else if (*second >= 'a' && *second <= 'f')
						c += *second - 'a' + 10;
					else
						throw std::invalid_argument("URI malformed");

					if (uri_reserved(c) || c == '#')
						c = *start;
					else
						std::advance(start, 2);
				}

				result += c;
			}

			return result;
		}

		static std::string encodeURIComponent(string_view str) noexcept
		{
			using namespace ::urls::detail;
			std::string result;

			for (const auto& c : str)
			{
				if (isalpha(c) || isdigit(c) || uri_mark(c))
				{
					result += c;
				}
				else
				{
					result += '%';
					result += std::string(to_hex((unsigned char)c));
				}
			}

			return result;
		}

		static std::string decodeURIComponent(string_view str)
		{
			using namespace ::urls::detail;
			std::string result;

			auto start = str.cbegin();
			auto end = str.cend();

			for (; start != end; ++start)
			{
				char c = *start;
				if (c == '%')
				{
					auto first = std::next(start);
					if (first == end)
						throw std::invalid_argument("URI malformed");

					auto second = std::next(first);
					if (second == end)
						throw std::invalid_argument("URI malformed");

					if (isdigit(*first))
						c = *first - '0';
					else if (*first >= 'A' && *first <= 'F')
						c = *first - 'A' + 10;
					else if (*first >= 'a' && *first <= 'f')
						c = *first - 'a' + 10;
					else
						throw std::invalid_argument("URI malformed");

					c <<= 4;

					if (isdigit(*second))
						c += *second - '0';
					else if (*second >= 'A' && *second <= 'F')
						c += *second - 'A' + 10;
					else if (*second >= 'a' && *second <= 'f')
						c += *second - 'a' + 10;
					else
						throw std::invalid_argument("URI malformed");

					std::advance(start, 2);
				}

				result += c;
			}

			return result;
		}

		class qs_iterator
		{
		public:
			using value_type = std::pair<string_view, string_view>;
			using difference_type = std::ptrdiff_t;
			using reference = qs_iterator;
			using pointer = const value_type*;
			using iterator_category = std::forward_iterator_tag;

			qs_iterator() = default;

			explicit qs_iterator(const string_view& s) : qs_(s)
			{
				make_value();
			}

			~qs_iterator() = default;

			value_type operator*() const noexcept
			{
				return value_;
			}

			pointer operator->() const noexcept
			{
				return &value_;
			}

			reference operator++() noexcept
			{
				increment();
				return *this;
			}

			reference operator++(int) noexcept
			{
				reference tmp = *this;
				increment();
				return tmp;
			}

			bool operator==(const qs_iterator &other) const noexcept
			{
				if (value_.first == other.value_.first &&
					value_.second == other.value_.second)
					return true;
				return false;
			}

			bool operator!=(const qs_iterator &other) const noexcept
			{
				return !(*this == other);
			}

			string_view key() const noexcept
			{
				return value_.first;
			}

			string_view value() const noexcept
			{
				return value_.second;
			}

		protected:
			void make_value() noexcept
			{
				auto f = qs_.find_first_of('=');
				if (f == string_view::npos)
					return;

				auto e = qs_.find_first_of('&');
				if (e == string_view::npos)
					e = qs_.size();

				value_.first = string_view(qs_.data(), f);
				value_.second = string_view(qs_.data() + f + 1, e - f - 1);
			}

			void increment()
			{
				auto second = value_.second.data() + value_.second.size() + 1;
				auto end = qs_.data() + qs_.size();

				value_ = {};

				if (end <= second)
					return;

				qs_ = string_view(second, end - second);
				make_value();
			}

		protected:
			string_view qs_;
			value_type value_;
		};

		qs_iterator qs_begin() const noexcept
		{
			return qs_iterator(query_);
		}

		qs_iterator qs_end() const noexcept
		{
			return qs_iterator();
		}

	private:

		string_view knownport() noexcept
		{
			using boost::beast::iequals;

			if (iequals(scheme_, "ftp"))
				return string_view("21");
			else if (iequals(scheme_, "ssh"))
				return string_view("22");
			else if (iequals(scheme_, "telnet"))
				return string_view("23");
			else if (iequals(scheme_, "gopher"))
				return string_view("70");
			else if (iequals(scheme_, "http") || iequals(scheme_, "ws"))
				return string_view("80");
			else if (iequals(scheme_, "nntp"))
				return string_view("119");
			else if (iequals(scheme_, "ldap"))
				return string_view("389");
			else if (iequals(scheme_, "https") || iequals(scheme_, "wss"))
				return string_view("443");
			else if (iequals(scheme_, "rtsp"))
				return string_view("554");
			else if (iequals(scheme_, "socks") || iequals(scheme_, "socks4") || iequals(scheme_, "socks5"))
				return string_view("1080");
			else if (iequals(scheme_, "sip"))
				return string_view("5060");
			else if (iequals(scheme_, "sips"))
				return string_view("5061");
			else if (iequals(scheme_, "xmpp"))
				return string_view("5222");
			else
				return string_view("0");
		}

	private:
		string_view scheme_;
		string_view username_;
		string_view password_;
		string_view host_;
		string_view port_;
		string_view path_;
		string_view query_;
		string_view fragment_;
	};

}
