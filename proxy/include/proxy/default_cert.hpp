//
// Copyright (C) 2019 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#ifndef INCLUDE__2023_10_18__DEFAULT_CERT_HPP
#define INCLUDE__2023_10_18__DEFAULT_CERT_HPP


#include <string_view>

// 获取默认的 CA 根证书列表.
std::string_view default_root_certificates();

// 获取默认的 DH 参数.
std::string_view default_dh_param();

#endif // INCLUDE__2023_10_18__DEFAULT_CERT_HPP
