//
// ringbuf.hpp
// ~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// 线程安全的定长日志环形缓冲。每行带单调递增序号（增量拉取用）；
// gen 为缓冲代次，实例重启重建缓冲时变化。
//

#ifndef LAUNCHER_RINGBUF_HPP
#define LAUNCHER_RINGBUF_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace launcher {

class ringbuf
{
public:
	explicit ringbuf(int max = 2000);

	// 追加一行，超出容量时丢弃最旧行。
	void add(const std::string& line);

	// 返回最近 n 行。
	std::vector<std::string> tail(int n);

	// 返回最近 n 行及其序号。
	void tail_seq(int n, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs);

	// 返回序号大于 pos 的行及其序号。
	void since(std::int64_t pos, std::vector<std::string>& lines, std::vector<std::int64_t>& seqs);

	std::int64_t next_seq();

	std::int64_t generation();

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
