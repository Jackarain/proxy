//
// ringbuf.cpp
// ~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "ringbuf.hpp"

#include <chrono>

namespace launcher {

ringbuf::ringbuf(int max)
	: max_(max > 0 ? max : 2000)
	, gen_(std::chrono::system_clock::now().time_since_epoch().count())
{}

void ringbuf::add(const std::string& line)
{
	std::lock_guard<std::mutex> lock(mu_);
	if (lines_.size() == static_cast<std::size_t>(max_)) {
		lines_.erase(lines_.begin());
		seq_.erase(seq_.begin());
	}
	lines_.push_back(line);
	seq_.push_back(next_);
	next_++;
}

std::vector<std::string> ringbuf::tail(int n)
{
	std::lock_guard<std::mutex> lock(mu_);
	if (n <= 0 || n > static_cast<int>(lines_.size()))
		n = static_cast<int>(lines_.size());
	std::vector<std::string> out(lines_.end() - n, lines_.end());
	return out;
}

void ringbuf::tail_seq(int n, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs)
{
	std::lock_guard<std::mutex> lock(mu_);
	if (n <= 0 || n > static_cast<int>(lines_.size()))
		n = static_cast<int>(lines_.size());
	lines.assign(lines_.end() - n, lines_.end());
	seqs.assign(seq_.end() - n, seq_.end());
}

void ringbuf::since(std::int64_t pos, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs)
{
	std::lock_guard<std::mutex> lock(mu_);
	lines.clear();
	seqs.clear();
	for (std::size_t i = 0; i < seq_.size(); i++) {
		if (seq_[i] > pos) {
			lines.push_back(lines_[i]);
			seqs.push_back(seq_[i]);
		}
	}
}

std::int64_t ringbuf::next_seq()
{
	std::lock_guard<std::mutex> lock(mu_);
	return next_;
}

std::int64_t ringbuf::generation()
{
	std::lock_guard<std::mutex> lock(mu_);
	return gen_;
}

} // namespace launcher
