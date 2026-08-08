//
// ringbuf.hpp
// ~~~~~~~~~~~
//
// 线程安全的定长日志环形缓冲。
// 每行带单调递增序号（增量拉取用）；gen 为缓冲代次，实例重启重建缓冲时变化。
// 与 golang 版本 internal/launcher/ringbuf.go 行为一致。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_RINGBUF_HPP
#define LAUNCHER_RINGBUF_HPP

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace launcher {

class ringbuf {
public:
	explicit ringbuf(int max = 2000)
		: max_(max > 0 ? max : 2000)
		, gen_(std::chrono::system_clock::now().time_since_epoch().count())
	{}

	// 追加一行，超出容量时丢弃最旧行。
	void add(const std::string& line) {
		std::lock_guard<std::mutex> lock(mu_);
		if (lines_.size() == static_cast<std::size_t>(max_)) {
			lines_.erase(lines_.begin());
			seq_.erase(seq_.begin());
		}
		lines_.push_back(line);
		seq_.push_back(next_);
		next_++;
	}

	// 返回最近 n 行。
	std::vector<std::string> tail(int n) {
		std::lock_guard<std::mutex> lock(mu_);
		if (n <= 0 || n > static_cast<int>(lines_.size()))
			n = static_cast<int>(lines_.size());
		std::vector<std::string> out(lines_.end() - n, lines_.end());
		return out;
	}

	// 返回最近 n 行及其序号。
	void tail_seq(int n, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs) {
		std::lock_guard<std::mutex> lock(mu_);
		if (n <= 0 || n > static_cast<int>(lines_.size()))
			n = static_cast<int>(lines_.size());
		lines.assign(lines_.end() - n, lines_.end());
		seqs.assign(seq_.end() - n, seq_.end());
	}

	// 返回序号大于 pos 的行及其序号。
	void since(std::int64_t pos, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs) {
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

	std::int64_t next_seq() {
		std::lock_guard<std::mutex> lock(mu_);
		return next_;
	}

	std::int64_t generation() {
		std::lock_guard<std::mutex> lock(mu_);
		return gen_;
	}

private:
	std::mutex mu_;
	std::vector<std::string> lines_;
	std::vector<std::int64_t> seq_;
	std::int64_t next_ = 0;
	std::int64_t gen_ = 0;
	int max_ = 2000;
};

} // namespace launcher

#endif // LAUNCHER_RINGBUF_HPP
