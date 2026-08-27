//
// wintun_install.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_27__WINTUN_INSTALL_HPP
#define INCLUDE__2026_08_27__WINTUN_INSTALL_HPP

#if defined(_WIN32)

namespace proxy {

	// 确保 wintun 驱动已安装: 若未安装, 则从 exe 内嵌资源解压
	// wintun.sys/inf/cat 并用 pnputil 安装, 返回驱动是否可用.
	bool ensure_wintun_driver() noexcept;

} // namespace proxy

#endif // _WIN32

#endif // INCLUDE__2026_08_27__WINTUN_INSTALL_HPP
