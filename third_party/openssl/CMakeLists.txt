# 由 gen_cmake.py 自动生成，请勿手工修改
# 支持架构: x86_64/x86/aarch64/arm，按平台选择汇编方案
cmake_minimum_required(VERSION 3.10)
project(openssl C)

option(SHARED_OPENSSL "build shared openssl lib" OFF)

# 上层 add_compile_options 注入的 MSVC 专用参数（如 /utf-8）会传播到
# 汇编语言，nasm 会将其当作输入文件而报错；这里把目录级编译选项
# 限制为 C/CXX，汇编源不受影响
get_directory_property(_openssl_dir_options COMPILE_OPTIONS)
if(_openssl_dir_options)
    set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")
    foreach(_openssl_option IN LISTS _openssl_dir_options)
        add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${_openssl_option}>")
    endforeach()
endif()

# ==================== 目标架构检测 ====================
# 交叉编译（如 MinGW）时 CMAKE_SYSTEM_PROCESSOR 可能为空，
# 同时参考编译器三元组（CMAKE_C_COMPILER(_TARGET)）避免误判
set(OPENSSL_ARCH_HINTS
    "${CMAKE_SYSTEM_PROCESSOR}"
    "${CMAKE_C_COMPILER_TARGET}"
    "${CMAKE_C_COMPILER}"
    "${CMAKE_CXX_COMPILER}"
)
# macOS 单架构交叉编译时以 CMAKE_OSX_ARCHITECTURES 为准；
# 多架构（universal）无法在单目标内区分架构切片，见下方汇编可用性
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _openssl_osx_arch_count)
    if(NOT _openssl_osx_arch_count GREATER 1)
        list(INSERT OPENSSL_ARCH_HINTS 0 "${CMAKE_OSX_ARCHITECTURES}")
    endif()
endif()
set(OPENSSL_ARCH "")
foreach(_hint IN LISTS OPENSSL_ARCH_HINTS)
    if(_hint MATCHES "(x86_64|amd64|AMD64)")
        set(OPENSSL_ARCH "x86_64")
    elseif(_hint MATCHES "(i[3-6]86|x86)")
        set(OPENSSL_ARCH "x86")
    elseif(_hint MATCHES "(aarch64|arm64|ARM64|ARMv8)")
        set(OPENSSL_ARCH "aarch64")
    elseif(_hint MATCHES "(arm)")
        set(OPENSSL_ARCH "arm")
    endif()
    if(OPENSSL_ARCH)
        break()
    endif()
endforeach()
if(NOT OPENSSL_ARCH)
    message(FATAL_ERROR "openssl: unsupported target architecture ${CMAKE_SYSTEM_PROCESSOR} (compiler: ${CMAKE_C_COMPILER})")
endif()

# ==================== 汇编可用性 ====================
# macOS universal（多架构）构建无法在同一目标内为不同架构切片使用
# 各自的汇编实现；缺少 perl/nasm 或平台不支持时同样回退到纯 C。
set(OPENSSL_USE_ASM TRUE)
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _openssl_osx_arch_count)
    if(_openssl_osx_arch_count GREATER 1)
        set(OPENSSL_USE_ASM FALSE)
        message(STATUS "openssl: universal build detected, using portable C sources")
    endif()
endif()
if(OPENSSL_USE_ASM)
    find_program(OPENSSL_PERL_EXECUTABLE NAMES perl perl.exe)
    if(NOT OPENSSL_PERL_EXECUTABLE)
        set(OPENSSL_USE_ASM FALSE)
        message(WARNING "openssl: perl not found, using portable C sources")
    endif()
endif()
if(OPENSSL_USE_ASM AND WIN32 AND MSVC)
    find_program(OPENSSL_NASM_EXECUTABLE NAMES nasm nasm.exe)
    if(NOT OPENSSL_NASM_EXECUTABLE)
        set(OPENSSL_USE_ASM FALSE)
        message(WARNING "openssl: nasm not found, using portable C sources")
    endif()
endif()
if(OPENSSL_USE_ASM)
    if(OPENSSL_ARCH STREQUAL "x86_64")
                if(WIN32 AND MSVC)
                    set(OPENSSL_PERLASM_SCHEME "nasm")
                    set(OPENSSL_ASM_LANG ASM_NASM)
                    set(CMAKE_ASM_NASM_COMPILER "${OPENSSL_NASM_EXECUTABLE}")
                    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -Ox -f win64 -DNEAR")
                elseif(WIN32)
                    set(OPENSSL_PERLASM_SCHEME "mingw64")
                    set(OPENSSL_ASM_LANG ASM)
                elseif(APPLE)
                    set(OPENSSL_PERLASM_SCHEME "macosx")
                    set(OPENSSL_ASM_LANG ASM)
                else()
                    set(OPENSSL_PERLASM_SCHEME "elf")
                    set(OPENSSL_ASM_LANG ASM)
                endif()
    elseif(OPENSSL_ARCH STREQUAL "x86")
                if(WIN32 AND MSVC)
                    set(OPENSSL_PERLASM_SCHEME "win32n")
                    set(OPENSSL_ASM_LANG ASM_NASM)
                    set(CMAKE_ASM_NASM_COMPILER "${OPENSSL_NASM_EXECUTABLE}")
                    set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -Ox -f win32")
                elseif(WIN32)
                    set(OPENSSL_PERLASM_SCHEME "coff")
                    set(OPENSSL_ASM_LANG ASM)
                elseif(APPLE)
                    set(OPENSSL_PERLASM_SCHEME "macosx")
                    set(OPENSSL_ASM_LANG ASM)
                else()
                    set(OPENSSL_PERLASM_SCHEME "elf")
                    set(OPENSSL_ASM_LANG ASM)
                endif()
    elseif(OPENSSL_ARCH STREQUAL "aarch64")
                if(WIN32 AND MSVC)
                    message(WARNING "openssl: assembly for aarch64 is not supported on this platform, using portable C sources")
                    set(OPENSSL_USE_ASM FALSE)
                elseif(WIN32)
                    set(OPENSSL_PERLASM_SCHEME "win64")
                    set(OPENSSL_ASM_LANG ASM)
                elseif(APPLE)
                    set(OPENSSL_PERLASM_SCHEME "ios64")
                    set(OPENSSL_ASM_LANG ASM)
                else()
                    set(OPENSSL_PERLASM_SCHEME "linux64")
                    set(OPENSSL_ASM_LANG ASM)
                endif()
    elseif(OPENSSL_ARCH STREQUAL "arm")
                if(WIN32 AND MSVC)
                    message(WARNING "openssl: assembly for arm is not supported on this platform, using portable C sources")
                    set(OPENSSL_USE_ASM FALSE)
                elseif(WIN32)
                    message(WARNING "openssl: assembly for arm is not supported on this platform, using portable C sources")
                    set(OPENSSL_USE_ASM FALSE)
                elseif(APPLE)
                    message(WARNING "openssl: assembly for arm is not supported on this platform, using portable C sources")
                    set(OPENSSL_USE_ASM FALSE)
                else()
                    set(OPENSSL_PERLASM_SCHEME "linux32")
                    set(OPENSSL_ASM_LANG ASM)
                endif()
    endif()
endif()

# ==================== 平台系统宏 ====================
if(WIN32 AND MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_WIN64A)
    else()
        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_WIN32)
    endif()
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MINGW64)
    else()
        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MINGW32)
    endif()
elseif(APPLE)
    set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MACOSX)
else()
    set(OPENSSL_SYS_DEFINES "")
endif()

# ==================== 头文件目录 ====================
set(OPENSSL_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/include/prov
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/fips/include
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/include
)

# ==================== libcrypto 源文件 ====================
set(OPENSSL_CRYPTO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_cfb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ige.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_misc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ofb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_vaes512_intrinsics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_wrap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aligned_alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aria/aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/array_alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ascon/ascon_aead128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_bitstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_d2i_fp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_digest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_dup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_gentm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_i2d_fp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_mbstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_octet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_strex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_strnid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_time_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_utctm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_utf8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/ameth_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_item_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_mime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_moid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_mstbl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/bio_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/bio_ndef.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_param.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_pr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_pu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/evp_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/f_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/f_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/i2d_evp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/nsseq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_pbe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_pbev2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p8_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_bitst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_dec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_fre.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_new.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_scn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_typ.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_algor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_bignum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_int64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_long.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_val.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1_dsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async_wait.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_cfb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_ofb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_skey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_buff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_lbuf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_nbio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_prefix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_readbuff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_cb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_dump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_sock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_sock2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_acpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_bio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_conn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_dgram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_dgram_pair.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_fd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_sock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/ossl_core_bio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_add.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_blind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_const.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_conv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_div.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_exp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_exp2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_gcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_gf2m.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_intern.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_kron.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mont.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mpi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mul.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_nist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_prime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_rand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_recp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_rsa_fips186_5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_shift.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_sqr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_sqrt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_srp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_word.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_x931p.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bsearch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/buffer/buf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/buffer/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_cfb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ctr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ofb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_cfb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_ofb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_skey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmac/cmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_genm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_hdr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_protect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_att.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_cd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_dd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_ec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_env.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_ess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kari.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kemri.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_pwri.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_rsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_sd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_smime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_brotli.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_zlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_zstd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/comp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/comp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp_methods.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_mall.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_mod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_sap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_ssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_algorithm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_fetch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_namemap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cpt_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cpuid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_pbm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cryptlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_b64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_policy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_sct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_sct_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_x509v3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ctype.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cversion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/defaults.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/der_writer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/deterministic_nonce.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_group_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_rfc5114.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_vrf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_dlfcn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_vms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_win32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ebcdic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve25519.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/arch_32/f_impl32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/arch_64/f_impl64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/curve448.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/curve448_tables.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/eddsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/f_generic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/scalar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec2_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec2_smpl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_curve.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_cvt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_deprecated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_kmeth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_mult.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdh_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdh_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_vrf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/eck_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_mont.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_nist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_nistz256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_smpl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_all_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_blocks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_mark.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_save.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/asymcipher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_b64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_md.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/c_allc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/c_alld.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ctrl_params_translate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dec_b64_avx2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dh_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dh_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/digest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dsa_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes_cbc_hmac_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes_cbc_hmac_sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_bf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_camellia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_cast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_chacha20_poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_des.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_des3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_idea.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_old.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc4_hmac_md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_xcbc_d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ec_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ec_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/enc_b64_avx2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/enc_b64_scalar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/encode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_cnf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_fetch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pbe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pkey_type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_rand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/exchange.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kdf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kdf_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/keymgmt_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/keymgmt_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_md5_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_ripemd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_sha.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/m_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/m_sigver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/mac_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/mac_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p5_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p5_crpt2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_dec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_open.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_seal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pbe_scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_gn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/s_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/signature.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/skeymgmt_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ex_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_key_generate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_key_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params_generate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/getenv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hashtable/hashfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hashtable/hashtable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hmac/hmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hpke/hpke.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hpke/hpke_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/http/http_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/indicator_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/initthread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/kdf/kdf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/lhash/lh_stats.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/lhash/lhash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mem_sec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_encoders.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_key_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_ntt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_sample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_kem/ml_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cbc128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ccm128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cfb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ctr128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cts128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/gcm128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ocb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ofb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/siv128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/wrap128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/xts128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/xts128gb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_dir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_fopen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_str.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/o_names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_dat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_xref.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/packet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/param_build.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/param_build_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params_dup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params_from_text.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/passphrase.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_oth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_pk8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_xaux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pvkfmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_add.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_attr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_crt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_decr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_kiss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_mutl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_npas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_p8d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_p8e.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_sbag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/pk12err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/bio_pk7.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_attr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_doit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_mime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_smime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pkcs7err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/poly1305/poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/defn_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_query.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_child.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_predefined.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/punycode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/quic_vlint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/prov_seed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_deprecated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_uniform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/randfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rbtree/rbtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ripemd/rmd_dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ripemd/rmd_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_chk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_mp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_mp_names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_oaep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_pk1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_pss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_saos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_schemes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sp800_56b_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sp800_56b_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_x931.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_x931g.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/self_test_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha1_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha1dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha3_encode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha512.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/siphash/siphash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_adrs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa_hash_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_fors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_hypertree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_wots.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_xmss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_crypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm3/legacy_sm3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm3/sm3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm4/sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sparse_array.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/srp/srp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/srp/srp_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ssl_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/stack/stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_result.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_strings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/internal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_pthread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_req_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_req_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_verify_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/txt_db/txt_db.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/uid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_dir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_node.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_acert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_crl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_aaa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ac_tgt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_admis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_akeya.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_akid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_asid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_attrdesc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_attrmap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_audit_id.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_authattid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_battcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_bcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_bitst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_cpols.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_crld.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_enum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_extku.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_genn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_group_ac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ia5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ind_iss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_iobo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ncons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_no_ass.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_no_rev_avail.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pci.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pcia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pku.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pmaps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_purp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_rolespec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_san.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_sda.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_single_use.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_skid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_soa_id.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_sxnet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_timespec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_tlsf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_usernotice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_utf8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_acert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_att.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_cmp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_d2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_ext.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_lu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_r2x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_trust.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_txt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_v3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_vpm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509aset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509cset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509name.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509rset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_attrib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_crl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_exten.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_ietfatt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_name.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_pubkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_x509a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/baseprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/bio_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/capabilities.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_digests_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ecx_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ecx_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_hkdf_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ml_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ml_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_slh_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_slh_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_wrap_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/digest_to_nid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_seeding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/securitycheck.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/securitycheck_default.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/defltprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/asymciphers/rsa_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/asymciphers/sm2_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha256_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha256_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha512_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha_etm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_hw_ppc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv_polyval.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_aesni.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_armv8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_rv32i.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_rv64i.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_s390x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_t4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ocb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ocb_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_siv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_siv_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_wrp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts_fips.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_ascon_aead128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_camellia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_camellia_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_poly1305_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_cts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_xts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_xts_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_block.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/cshake_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/digestcommon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/md5_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/md5_sha1_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/ml_dsa_mu_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/null_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/ripemd_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sha2_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sha3_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sm3_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_der2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_epki2pki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_msblob2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_pem2der.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_pvk2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_spki2typespki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2any.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2blob.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2ms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2text.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/endecoder_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_common_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_dsa_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_kem_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/dh_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/ecdh_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/ecx_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/kdf_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/argon2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/hkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/hmacdrbg_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/ikev2kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/kbkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/krb5kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/pbkdf2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/pkcs12kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/snmpkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/srtpkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/sshkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/sskdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/tls1_prf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/x942kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ec_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ecx_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/kem_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ml_kem_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/mlx_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/rsa_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/dh_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ec_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ecx_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/kdf_legacy_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/mac_legacy_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ml_dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ml_kem_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/mlx_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/rsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/slh_dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/cmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/gmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/hmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/kmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/poly1305_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/siphash_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_ctr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_hmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seed_src.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seed_src_jitter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_tsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_unix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/test_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/ecdsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/eddsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/mac_legacy_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/ml_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/rsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/slh_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/sm2_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/skeymgmt/aes_skmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/skeymgmt/generic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/storemgmt/file_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/storemgmt/file_store_any2obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/nullprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/prov_running.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/ssl3_cbc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_pad.c
)
set(OPENSSL_PORTABLE_CRYPTO_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_cbc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_cfb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ige.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_misc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_ofb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_vaes512_intrinsics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_wrap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aligned_alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aria/aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/array_alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ascon/ascon_aead128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_bitstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_d2i_fp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_digest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_dup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_gentm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_i2d_fp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_mbstr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_octet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_strex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_strnid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_time_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_utctm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_utf8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/a_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/ameth_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_item_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn1_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_mime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_moid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_mstbl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/asn_pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/bio_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/bio_ndef.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_param.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_pr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/d2i_pu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/evp_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/f_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/f_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/i2d_evp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/nsseq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_pbe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_pbev2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p5_scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/p8_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_bitst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/t_spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_dec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_fre.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_new.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_scn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_typ.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/tasn_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_algor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_bignum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_int64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_long.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1/x_val.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asn1_dsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/asprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/arch/async_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/async/async_wait.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_cfb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_ofb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_skey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_buff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_lbuf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_nbio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_prefix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bf_readbuff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_cb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_dump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_sock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bio_sock2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_acpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_bio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_conn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_dgram.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_dgram_pair.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_fd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/bss_sock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bio/ossl_core_bio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_add.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_asm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_blind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_const.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_conv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_div.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_exp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_exp2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_gcd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_gf2m.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_intern.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_kron.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mont.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mpi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_mul.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_nist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_prime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_rand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_recp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_rsa_fips186_5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_shift.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_sqr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_sqrt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_srp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_word.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_x931p.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bsearch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/buffer/buf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/buffer/buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/camellia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_cbc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_cfb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ctr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_misc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_ofb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_cfb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_ofb64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cast/c_skey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/chacha/chacha_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmac/cmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_genm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_hdr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_protect.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cmp/cmp_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_att.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_cd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_dd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_ec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_env.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_ess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kari.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_kemri.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_pwri.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_rsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_sd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cms/cms_smime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_brotli.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_zlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/c_zstd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/comp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp/comp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/comp_methods.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_mall.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_mod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_sap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/conf/conf_ssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_algorithm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_fetch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/core_namemap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cpt_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cpuid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/crmf/crmf_pbm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cryptlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_b64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_policy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_sct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_sct_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ct/ct_x509v3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ctype.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/cversion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/defaults.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/der_writer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/deterministic_nonce.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_group_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dh/dh_rfc5114.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dsa/dsa_vrf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_dl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_dlfcn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_vms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/dso/dso_win32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ebcdic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve25519.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/arch_32/f_impl32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/arch_64/f_impl64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/curve448.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/curve448_tables.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/eddsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/f_generic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/curve448/scalar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec2_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec2_smpl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_curve.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_cvt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_deprecated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_kmeth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_mult.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ec_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdh_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdh_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecdsa_vrf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/eck_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_mont.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_nist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_oct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_smpl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecx_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/decoder_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/encode_decode/encoder_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_all_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_blocks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_mark.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/err/err_save.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ess/ess_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/asymcipher.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_b64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/bio_md.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/c_allc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/c_alld.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ctrl_params_translate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dec_b64_avx2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dh_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dh_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/digest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/dsa_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes_cbc_hmac_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aes_cbc_hmac_sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_bf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_camellia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_cast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_chacha20_poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_des.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_des3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_idea.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_old.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc4_hmac_md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_rc5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/e_xcbc_d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ec_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/ec_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/enc_b64_avx2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/enc_b64_scalar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/encode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_cnf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_fetch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pbe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_pkey_type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_rand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/evp_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/exchange.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kdf_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kdf_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/keymgmt_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/keymgmt_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_md5_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_ripemd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/legacy_sha.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/m_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/m_sigver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/mac_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/mac_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p5_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p5_crpt2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_dec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_open.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_seal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/p_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pbe_scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_gn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/pmeth_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/s_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/signature.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/evp/skeymgmt_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ex_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_dh.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_key_generate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_key_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params_generate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ffc/ffc_params_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/getenv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hashtable/hashfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hashtable/hashtable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hmac/hmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hpke/hpke.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/hpke/hpke_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/http/http_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/indicator_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/initthread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/kdf/kdf_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/lhash/lh_stats.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/lhash/lhash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/md5/md5_sha1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mem_clr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/mem_sec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_encoders.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_key_compress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_matrix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_ntt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_sample.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_dsa/ml_dsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ml_kem/ml_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cbc128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ccm128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cfb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ctr128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/cts128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/gcm128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ocb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/ofb128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/siv128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/wrap128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/xts128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/modes/xts128gb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_dir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_fopen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/o_str.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/o_names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_dat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/objects/obj_xref.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/packet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/param_build.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/param_build_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params_dup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/params_from_text.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/passphrase.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_oth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_pk8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_pkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pem_xaux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pem/pvkfmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_add.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_asn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_attr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_crt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_decr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_kiss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_mutl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_npas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_p8d.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_p8e.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_sbag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/p12_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs12/pk12err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/bio_pk7.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_attr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_doit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_mime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pk7_smime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/pkcs7/pkcs7err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/poly1305/poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/defn_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_query.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/property/property_string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_child.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/provider_predefined.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/punycode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/quic_vlint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/prov_seed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_deprecated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/rand_uniform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rand/randfile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rbtree/rbtree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ripemd/rmd_dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ripemd/rmd_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_ameth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_backend.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_chk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_crpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_mp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_mp_names.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_oaep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_ossl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_pk1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_pss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_saos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_schemes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sp800_56b_check.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_sp800_56b_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_x931.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/rsa/rsa_x931g.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/self_test_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/keccak1600.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha1_one.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha1dgst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha3_encode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha512.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/siphash/siphash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_adrs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa_hash_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_fors.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_hypertree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_wots.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/slh_dsa/slh_xmss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_crypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm2/sm2_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm3/legacy_sm3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm3/sm3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sm4/sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sparse_array.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/srp/srp_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/srp/srp_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ssl_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/stack/stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_register.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_result.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/store/store_strings.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_posix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/arch/thread_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/thread/internal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_none.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_pthread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/threads_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_req_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_req_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_sign.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_rsp_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ts/ts_verify_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/txt_db/txt_db.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_openssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ui/ui_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/uid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_dir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/by_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_node.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/pcy_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_acert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_crl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/t_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_aaa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ac_tgt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_admis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_akeya.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_akid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_asid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_attrdesc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_attrmap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_audit_id.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_authattid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_battcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_bcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_bitst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_cpols.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_crld.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_enum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_extku.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_genn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_group_ac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ia5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ind_iss.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_int.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_iobo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_ncons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_no_ass.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_no_rev_avail.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pci.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pcia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pcons.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pku.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_pmaps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_prn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_purp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_rolespec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_san.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_sda.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_single_use.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_skid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_soa_id.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_sxnet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_timespec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_tlsf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_usernotice.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_utf8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3_utl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/v3err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_acert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_att.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_cmp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_d2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_ext.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_lu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_r2x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_trust.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_txt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_v3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_vfy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509_vpm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509aset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509cset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509name.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509rset.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509spki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x509type.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_all.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_attrib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_crl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_exten.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_ietfatt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_name.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_pubkey.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_req.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_x509.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crypto/x509/x_x509a.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/baseprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/bio_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/capabilities.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_digests_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ec_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ecx_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ecx_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_hkdf_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ml_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_ml_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_rsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_slh_dsa_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_slh_dsa_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_sm2_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/der/der_wrap_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/digest_to_nid.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_err.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_seeding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/provider_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/securitycheck.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/common/securitycheck_default.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/defltprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/asymciphers/rsa_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/asymciphers/sm2_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha256_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha256_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha512_etm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha_etm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_hw_ppc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_gcm_siv_polyval.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_aesni.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_armv8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_rv32i.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_rv64i.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_s390x.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_hw_t4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ocb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_ocb_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_siv.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_siv_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_wrp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts_fips.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aes_xts_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_aria_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_ascon_aead128.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_camellia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_camellia_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_poly1305.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_chacha20_poly1305_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_cts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_null.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_xts.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/cipher_sm4_xts_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_block.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_ccm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_gcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_gcm_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/ciphers/ciphercommon_hw.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/cshake_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/digestcommon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/md5_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/md5_sha1_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/ml_dsa_mu_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/null_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/ripemd_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sha2_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sha3_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/digests/sm3_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_der2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_epki2pki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_msblob2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_pem2der.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_pvk2key.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/decode_spki2typespki.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2any.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2blob.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2ms.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/encode_key2text.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/endecoder_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_common_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_dsa_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/encode_decode/ml_kem_codecs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/dh_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/ecdh_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/ecx_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/exchange/kdf_exch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/argon2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/hkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/hmacdrbg_kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/ikev2kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/kbkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/krb5kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/pbkdf2.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/pkcs12kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/scrypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/snmpkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/srtpkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/sshkdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/sskdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/tls1_prf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kdfs/x942kdf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ec_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ecx_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/kem_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/ml_kem_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/mlx_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/kem/rsa_kem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/dh_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ec_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ecx_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/kdf_legacy_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/mac_legacy_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ml_dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/ml_kem_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/mlx_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/rsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/keymgmt/slh_dsa_kmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/cmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/gmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/hmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/kmac_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/poly1305_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/macs/siphash_prov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_ctr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/drbg_hmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seed_src.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seed_src_jitter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_cpu_x86.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_tsc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_unix.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_win.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/test_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/ecdsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/eddsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/mac_legacy_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/ml_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/rsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/slh_dsa_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/signature/sm2_sig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/skeymgmt/aes_skmgmt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/skeymgmt/generic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/storemgmt/file_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/storemgmt/file_store_any2obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/nullprov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/providers/prov_running.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/ssl3_cbc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_pad.c
)

if(OPENSSL_USE_ASM)
    if(OPENSSL_ARCH STREQUAL "x86_64")
        list(APPEND OPENSSL_CRYPTO_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_enc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/asm/x86_64-gcc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/rsaz_exp.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/rsaz_exp_x2.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_misc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/sha3_x4_avx512vl.c
            ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_cpu_x86.c
        )
    elseif(OPENSSL_ARCH STREQUAL "x86")
        list(APPEND OPENSSL_CRYPTO_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/sha/keccak1600.c
            ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_cpu_x86.c
        )
    elseif(OPENSSL_ARCH STREQUAL "aarch64")
        list(APPEND OPENSSL_CRYPTO_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_cbc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_core.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/armcap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_enc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_asm.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/camellia.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_cbc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_misc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_sm2p256.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/ec/ecp_sm2p256_table.c
            ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_cpu_arm64.c
        )
    elseif(OPENSSL_ARCH STREQUAL "arm")
        list(APPEND OPENSSL_CRYPTO_SRCS
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/aes/aes_cbc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/armcap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bf/bf_enc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/bn/bn_asm.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/camellia.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_cbc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/crypto/camellia/cmll_misc.c
            ${CMAKE_CURRENT_SOURCE_DIR}/providers/implementations/rands/seeding/rand_cpu_x86.c
        )
    endif()
else()
    set(OPENSSL_CRYPTO_SRCS ${OPENSSL_PORTABLE_CRYPTO_SRCS})
endif()

# ==================== libssl 源文件 ====================
set(OPENSSL_SSL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/bio_ssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_srtp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_transcript.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dgram_demux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dtls_conn_lookup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dtls_record_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_helper.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_internal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_ssl_apis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/methods.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/pqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/priority_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/cc_newreno.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/json_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/qlog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/qlog_event_helpers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_ackm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_cfq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_channel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_demux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_fc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_fifd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_lcidm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_method.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rcidm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_reactor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_reactor_wait_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_shared.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rstream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rx_depack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_srt_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_srtm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_sstream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_statm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_stream_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_strm_reas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_thread_assist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tls_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tserver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_txp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_txpim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_wire.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_wire_pkt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/uint_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/dtls_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls13_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls1_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_multib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tlsany_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/rec_layer_d1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/rec_layer_s3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/poll_builder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/poll_immediate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/rio_notifier.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_cert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_cert_comp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_ciph.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_err_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_mcnf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_rsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_rsa_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_sess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_stat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_txt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_utst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_cust.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_srvr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_dtls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_srvr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_trce.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls13_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls_srp.c
)
set(OPENSSL_PORTABLE_SSL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/bio_ssl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_srtp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/d1_transcript.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dgram_demux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dtls_conn_lookup.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/dtls_record_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_helper.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_internal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_ssl_apis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ech/ech_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/methods.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/pqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/priority_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/cc_newreno.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/json_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/qlog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/qlog_event_helpers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_ackm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_cfq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_channel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_demux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_fc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_fifd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_lcidm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_method.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_obj.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rcidm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_reactor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_reactor_wait_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_rx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_shared.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_tx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_record_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rstream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_rx_depack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_srt_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_srtm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_sstream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_statm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_stream_map.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_strm_reas.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_thread_assist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tls_api.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_tserver.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_txp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_txpim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_wire.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/quic_wire_pkt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/quic/uint_set.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/dtls_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls13_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls1_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tls_multib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/methods/tlsany_meth.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/rec_layer_d1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/record/rec_layer_s3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/poll_builder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/poll_immediate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/rio/rio_notifier.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/s3_msg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_asn1.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_cert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_cert_comp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_ciph.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_err_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_mcnf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_rsa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_rsa_legacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_sess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_stat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_txt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/ssl_utst.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_cust.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/extensions_srvr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_clnt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_dtls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/statem/statem_srvr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/t1_trce.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls13_enc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls_depr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl/tls_srp.c
)

if(OPENSSL_USE_ASM)
else()
    set(OPENSSL_SSL_SRCS ${OPENSSL_PORTABLE_SSL_SRCS})
endif()

# ==================== 架构相关编译宏 ====================
if(OPENSSL_ARCH STREQUAL "x86_64")
    set(OPENSSL_CRYPTO_ASM_DEFINES
        AES_ASM
        BSAES_ASM
        CMLL_ASM
        ECP_NISTZ256_ASM
        GHASH_ASM
        KECCAK1600_ASM
        MD5_ASM
        OPENSSL_BN_ASM_GF2m
        OPENSSL_BN_ASM_MONT
        OPENSSL_BN_ASM_MONT5
        OPENSSL_CPUID_OBJ
        OPENSSL_IA32_SSE2
        OPENSSL_SM3_ASM
        POLY1305_ASM
        SHA1_ASM
        SHA256_ASM
        SHA512_ASM
        SM4_ASM
        VPAES_ASM
        X25519_ASM
    )
    set(OPENSSL_SSL_ASM_DEFINES
        AES_ASM
    )
elseif(OPENSSL_ARCH STREQUAL "x86")
    set(OPENSSL_CRYPTO_ASM_DEFINES
        AES_ASM
        CMLL_ASM
        ECP_NISTZ256_ASM
        GHASH_ASM
        MD5_ASM
        OPENSSL_BN_ASM_GF2m
        OPENSSL_BN_ASM_MONT
        OPENSSL_BN_ASM_PART_WORDS
        OPENSSL_CPUID_OBJ
        OPENSSL_IA32_SSE2
        POLY1305_ASM
        RMD160_ASM
        SHA1_ASM
        SHA256_ASM
        SHA512_ASM
        VPAES_ASM
    )
    set(OPENSSL_SSL_ASM_DEFINES
        AES_ASM
    )
elseif(OPENSSL_ARCH STREQUAL "aarch64")
    set(OPENSSL_CRYPTO_ASM_DEFINES
        BSAES_ASM
        ECP_NISTZ256_ASM
        ECP_SM2P256_ASM
        KECCAK1600_ASM
        MD5_ASM
        OPENSSL_BN_ASM_MONT
        OPENSSL_CPUID_OBJ
        OPENSSL_SM3_ASM
        POLY1305_ASM
        SHA1_ASM
        SHA256_ASM
        SHA512_ASM
        SM4_ASM
        VPAES_ASM
        VPSM4_ASM
    )
    set(OPENSSL_SSL_ASM_DEFINES "")
elseif(OPENSSL_ARCH STREQUAL "arm")
    set(OPENSSL_CRYPTO_ASM_DEFINES
        AES_ASM
        BSAES_ASM
        ECP_NISTZ256_ASM
        GHASH_ASM
        KECCAK1600_ASM
        OPENSSL_BN_ASM_GF2m
        OPENSSL_BN_ASM_MONT
        OPENSSL_CPUID_OBJ
        POLY1305_ASM
        SHA1_ASM
        SHA256_ASM
        SHA512_ASM
    )
    set(OPENSSL_SSL_ASM_DEFINES "")
endif()

# ==================== 汇编优化 ====================
set(OPENSSL_CRYPTO_ASM_SRCS "")
set(OPENSSL_SSL_ASM_SRCS "")

if(OPENSSL_USE_ASM)
    enable_language(${OPENSSL_ASM_LANG})

    # 汇编源不参与 C 预处理，去掉 -MD/-MF 依赖参数，避免无用参数告警
    set(CMAKE_DEPFILE_FLAGS_ASM "")

    if(OPENSSL_ARCH STREQUAL "x86_64")
        set(OPENSSL_ASM_DEFINES
            -DL_ENDIAN
            -DAES_ASM
            -DBSAES_ASM
            -DCMLL_ASM
            -DECP_NISTZ256_ASM
            -DGHASH_ASM
            -DKECCAK1600_ASM
            -DMD5_ASM
            -DOPENSSL_BN_ASM_GF2m
            -DOPENSSL_BN_ASM_MONT
            -DOPENSSL_BN_ASM_MONT5
            -DOPENSSL_CPUID_OBJ
            -DOPENSSL_IA32_SSE2
            -DOPENSSL_SM3_ASM
            -DPOLY1305_ASM
            -DSHA1_ASM
            -DSHA256_ASM
            -DSHA512_ASM
            -DSM4_ASM
            -DVPAES_ASM
            -DX25519_ASM
        )
    elseif(OPENSSL_ARCH STREQUAL "x86")
        set(OPENSSL_ASM_DEFINES
            -DL_ENDIAN
            -DAES_ASM
            -DCMLL_ASM
            -DECP_NISTZ256_ASM
            -DGHASH_ASM
            -DMD5_ASM
            -DOPENSSL_BN_ASM_GF2m
            -DOPENSSL_BN_ASM_MONT
            -DOPENSSL_BN_ASM_PART_WORDS
            -DOPENSSL_CPUID_OBJ
            -DOPENSSL_IA32_SSE2
            -DPOLY1305_ASM
            -DRMD160_ASM
            -DSHA1_ASM
            -DSHA256_ASM
            -DSHA512_ASM
            -DVPAES_ASM
        )
    elseif(OPENSSL_ARCH STREQUAL "aarch64")
        set(OPENSSL_ASM_DEFINES
            -DL_ENDIAN
            -DBSAES_ASM
            -DECP_NISTZ256_ASM
            -DECP_SM2P256_ASM
            -DKECCAK1600_ASM
            -DMD5_ASM
            -DOPENSSL_BN_ASM_MONT
            -DOPENSSL_CPUID_OBJ
            -DOPENSSL_SM3_ASM
            -DPOLY1305_ASM
            -DSHA1_ASM
            -DSHA256_ASM
            -DSHA512_ASM
            -DSM4_ASM
            -DVPAES_ASM
            -DVPSM4_ASM
        )
    elseif(OPENSSL_ARCH STREQUAL "arm")
        set(OPENSSL_ASM_DEFINES
            -DL_ENDIAN
            -DAES_ASM
            -DBSAES_ASM
            -DECP_NISTZ256_ASM
            -DGHASH_ASM
            -DKECCAK1600_ASM
            -DOPENSSL_BN_ASM_GF2m
            -DOPENSSL_BN_ASM_MONT
            -DOPENSSL_CPUID_OBJ
            -DPOLY1305_ASM
            -DSHA1_ASM
            -DSHA256_ASM
            -DSHA512_ASM
        )
    endif()

    # cl.exe 不接受 perlasm 探测使用的 GNU 风格参数，其报错会让
    # MSBuild 判定构建失败；因此仅在非 MSVC 下向 perlasm 传递 CC
    set(OPENSSL_PERLASM_ENV "")
    if(NOT MSVC)
        set(OPENSSL_PERLASM_ENV "CC=${CMAKE_C_COMPILER}")
    endif()

    set(OPENSSL_ASM_DIR "${CMAKE_CURRENT_BINARY_DIR}/openssl_asm")
    set(OPENSSL_PERL_INCLUDES
        "-I${CMAKE_CURRENT_SOURCE_DIR}"
        "-I${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    macro(openssl_add_asm _list _base_rel _orig_ext _pl_rel)
        if(WIN32 AND MSVC)
            set(_asm_ext ".asm")
        else()
            set(_asm_ext "${_orig_ext}")
        endif()
        set(_asm_out "${OPENSSL_ASM_DIR}/${_base_rel}${_asm_ext}")
        get_filename_component(_asm_dir "${_asm_out}" DIRECTORY)
        add_custom_command(OUTPUT "${_asm_out}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_asm_dir}"
            COMMAND ${CMAKE_COMMAND} -E env ${OPENSSL_PERLASM_ENV}
                    "${OPENSSL_PERL_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/${_pl_rel}"
                    ${OPENSSL_PERLASM_SCHEME}
                    ${OPENSSL_PERL_INCLUDES}
                    ${OPENSSL_ASM_DEFINES}
                    "${_asm_out}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_pl_rel}"
            COMMENT "Generating ASSEMBLY ${_base_rel}${_asm_ext}"
            VERBATIM)
        list(APPEND ${_list} "${_asm_out}")
    endmacro()

    if(OPENSSL_ARCH STREQUAL "x86_64")
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-cfb-avx512 .s crypto/aes/asm/aes-cfb-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-vaes-x86_64 .s crypto/aes/asm/aes-vaes-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-x86_64 .s crypto/aes/asm/aes-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-mb-x86_64 .s crypto/aes/asm/aesni-mb-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-sha1-x86_64 .s crypto/aes/asm/aesni-sha1-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-sha256-x86_64 .s crypto/aes/asm/aesni-sha256-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-x86_64 .s crypto/aes/asm/aesni-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-xts-avx512 .s crypto/aes/asm/aesni-xts-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/bsaes-x86_64 .s crypto/aes/asm/bsaes-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/vpaes-x86_64 .s crypto/aes/asm/vpaes-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-2k-avx512 .s crypto/bn/asm/rsaz-2k-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-2k-avxifma .s crypto/bn/asm/rsaz-2k-avxifma.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-3k-avx512 .s crypto/bn/asm/rsaz-3k-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-3k-avxifma .s crypto/bn/asm/rsaz-3k-avxifma.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-4k-avx512 .s crypto/bn/asm/rsaz-4k-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-4k-avxifma .s crypto/bn/asm/rsaz-4k-avxifma.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-avx2 .s crypto/bn/asm/rsaz-avx2.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/rsaz-x86_64 .s crypto/bn/asm/rsaz-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/x86_64-gf2m .s crypto/bn/asm/x86_64-gf2m.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/x86_64-mont .s crypto/bn/asm/x86_64-mont.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/x86_64-mont5 .s crypto/bn/asm/x86_64-mont5.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/camellia/cmll-x86_64 .s crypto/camellia/asm/cmll-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/chacha/chacha-x86_64 .s crypto/chacha/asm/chacha-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/ecp_nistz256-x86_64 .s crypto/ec/asm/ecp_nistz256-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/x25519-x86_64 .s crypto/ec/asm/x25519-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/md5/md5-x86_64 .s crypto/md5/asm/md5-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ml_dsa/ml_dsa_ntt-x86_64 .s crypto/ml_dsa/asm/ml_dsa_ntt-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/aes-gcm-avx512 .s crypto/modes/asm/aes-gcm-avx512.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/aesni-gcm-x86_64 .s crypto/modes/asm/aesni-gcm-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/ghash-x86_64 .s crypto/modes/asm/ghash-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/poly1305/poly1305-x86_64 .s crypto/poly1305/asm/poly1305-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/keccak1600-x86_64 .s crypto/sha/asm/keccak1600-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/keccak1600x4-avx512vl .s crypto/sha/asm/keccak1600x4-avx512vl.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha1-mb-x86_64 .s crypto/sha/asm/sha1-mb-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha1-x86_64 .s crypto/sha/asm/sha1-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha256-mb-x86_64 .s crypto/sha/asm/sha256-mb-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha256-x86_64 .s crypto/sha/asm/sha512-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha512-x86_64 .s crypto/sha/asm/sha512-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm3/sm3-x86_64 .S crypto/sm3/asm/sm3-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm4/sm4-x86_64 .S crypto/sm4/asm/sm4-x86_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/x86_64cpuid .s crypto/x86_64cpuid.pl)
    elseif(OPENSSL_ARCH STREQUAL "x86")
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-586 .S crypto/aes/asm/aes-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesni-x86 .S crypto/aes/asm/aesni-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/vpaes-x86 .S crypto/aes/asm/vpaes-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bf/bf-586 .S crypto/bf/asm/bf-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/bn-586 .S crypto/bn/asm/bn-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/co-586 .S crypto/bn/asm/co-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/x86-gf2m .S crypto/bn/asm/x86-gf2m.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/x86-mont .S crypto/bn/asm/x86-mont.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/camellia/cmll-x86 .S crypto/camellia/asm/cmll-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/chacha/chacha-x86 .S crypto/chacha/asm/chacha-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/ecp_nistz256-x86 .S crypto/ec/asm/ecp_nistz256-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/md5/md5-586 .S crypto/md5/asm/md5-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/ghash-x86 .S crypto/modes/asm/ghash-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/poly1305/poly1305-x86 .S crypto/poly1305/asm/poly1305-x86.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ripemd/rmd-586 .S crypto/ripemd/asm/rmd-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha1-586 .S crypto/sha/asm/sha1-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha256-586 .S crypto/sha/asm/sha256-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha512-586 .S crypto/sha/asm/sha512-586.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/x86cpuid .S crypto/x86cpuid.pl)
    elseif(OPENSSL_ARCH STREQUAL "aarch64")
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-sha1-armv8 .S crypto/aes/asm/aes-sha1-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-sha256-armv8 .S crypto/aes/asm/aes-sha256-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-sha512-armv8 .S crypto/aes/asm/aes-sha512-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesv8-armx .S crypto/aes/asm/aesv8-armx.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/bsaes-armv8 .S crypto/aes/asm/bsaes-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/vpaes-armv8 .S crypto/aes/asm/vpaes-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/arm64cpuid .S crypto/arm64cpuid.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/armv8-mont .S crypto/bn/asm/armv8-mont.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/chacha/chacha-armv8 .S crypto/chacha/asm/chacha-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/chacha/chacha-armv8-sve .S crypto/chacha/asm/chacha-armv8-sve.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/ecp_nistz256-armv8 .S crypto/ec/asm/ecp_nistz256-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/ecp_sm2p256-armv8 .S crypto/ec/asm/ecp_sm2p256-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/md5/md5-aarch64 .S crypto/md5/asm/md5-aarch64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/aes-gcm-armv8-unroll8_64 .S crypto/modes/asm/aes-gcm-armv8-unroll8_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/aes-gcm-armv8_64 .S crypto/modes/asm/aes-gcm-armv8_64.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/ghashv8-armx .S crypto/modes/asm/ghashv8-armx.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/poly1305/poly1305-armv8 .S crypto/poly1305/asm/poly1305-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/poly1305/poly1305-armv9-sve2 .S crypto/poly1305/asm/poly1305-armv9-sve2.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/keccak1600-armv8 .S crypto/sha/asm/keccak1600-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha1-armv8 .S crypto/sha/asm/sha1-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha256-armv8 .S crypto/sha/asm/sha512-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha512-armv8 .S crypto/sha/asm/sha512-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm3/sm3-armv8 .S crypto/sm3/asm/sm3-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm4/sm4-armv8 .S crypto/sm4/asm/sm4-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm4/vpsm4-armv8 .S crypto/sm4/asm/vpsm4-armv8.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sm4/vpsm4_ex-armv8 .S crypto/sm4/asm/vpsm4_ex-armv8.pl)
    elseif(OPENSSL_ARCH STREQUAL "arm")
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aes-armv4 .S crypto/aes/asm/aes-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/aesv8-armx .S crypto/aes/asm/aesv8-armx.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/aes/bsaes-armv7 .S crypto/aes/asm/bsaes-armv7.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/armv4cpuid .S crypto/armv4cpuid.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/armv4-gf2m .S crypto/bn/asm/armv4-gf2m.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/bn/armv4-mont .S crypto/bn/asm/armv4-mont.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/chacha/chacha-armv4 .S crypto/chacha/asm/chacha-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/ec/ecp_nistz256-armv4 .S crypto/ec/asm/ecp_nistz256-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/ghash-armv4 .S crypto/modes/asm/ghash-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/modes/ghashv8-armx .S crypto/modes/asm/ghashv8-armx.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/poly1305/poly1305-armv4 .S crypto/poly1305/asm/poly1305-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/keccak1600-armv4 .S crypto/sha/asm/keccak1600-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha1-armv4-large .S crypto/sha/asm/sha1-armv4-large.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha256-armv4 .S crypto/sha/asm/sha256-armv4.pl)
            openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS crypto/sha/sha512-armv4 .S crypto/sha/asm/sha512-armv4.pl)
    endif()
endif()

# ==================== 目标 ====================
if (SHARED_OPENSSL)
    add_library(crypto SHARED ${OPENSSL_CRYPTO_SRCS} ${OPENSSL_CRYPTO_ASM_SRCS})
    add_library(ssl SHARED ${OPENSSL_SSL_SRCS} ${OPENSSL_SSL_ASM_SRCS})
else()
    add_library(crypto STATIC ${OPENSSL_CRYPTO_SRCS} ${OPENSSL_CRYPTO_ASM_SRCS})
    add_library(ssl STATIC ${OPENSSL_SSL_SRCS} ${OPENSSL_SSL_ASM_SRCS})
endif()

set_target_properties(crypto PROPERTIES FOLDER "third_party/openssl")
set_target_properties(ssl PROPERTIES FOLDER "third_party/openssl")

target_include_directories(crypto PUBLIC ${OPENSSL_INCLUDES})
target_include_directories(ssl PUBLIC ${OPENSSL_INCLUDES})

# 宏只挂到 C 源文件上：VS/MSBuild 生成器不会为 NASM 项按语言过滤
# target 级宏，宏会被一并送给 nasm 而导致解析失败；INTERFACE 部分
# 仍向下游使用者传播。
function(openssl_target_defines _target)
    if(ARGC GREATER 1)
        target_compile_definitions(${_target} INTERFACE ${ARGN})
        get_target_property(_openssl_target_srcs ${_target} SOURCES)
        set(_openssl_c_srcs "")
        foreach(_openssl_src IN LISTS _openssl_target_srcs)
            if(_openssl_src MATCHES "[.]c$")
                list(APPEND _openssl_c_srcs "${_openssl_src}")
            endif()
        endforeach()
        set_property(SOURCE ${_openssl_c_srcs} APPEND PROPERTY COMPILE_DEFINITIONS ${ARGN})
    endif()
endfunction()

openssl_target_defines(crypto
    OPENSSL_NO_ASYNC
    OPENSSL_NO_ENGINE
    OPENSSL_NO_RC1
    OPENSSL_NO_RC3
    OPENSSL_NO_MD1
    OPENSSL_NO_MD2
    OPENSSL_NO_MD3
    OPENSSL_NO_MD4
    OPENSSL_NO_MDC2
    OPENSSL_NO_WHIRLPOOL
    OPENSSL_NO_COMP
    OPENSSLDIR=""
    ENGINESDIR=""
    MODULESDIR=""
    WIN32_LEAN_AND_MEAN
    ${OPENSSL_SYS_DEFINES}
)

openssl_target_defines(ssl OPENSSL_NO_ENGINE)

if(OPENSSL_USE_ASM)
    openssl_target_defines(crypto ${OPENSSL_CRYPTO_ASM_DEFINES})
    openssl_target_defines(ssl ${OPENSSL_SSL_ASM_DEFINES})
else()
    openssl_target_defines(crypto OPENSSL_NO_ASM)
    openssl_target_defines(ssl OPENSSL_NO_ASM)
endif()

# 上层目录级 add_definitions 注入的 -D 对纯汇编无意义，
# clang 会报无用参数告警，这里只针对 ASM 语言屏蔽
if(OPENSSL_USE_ASM AND CMAKE_ASM_COMPILER_ID MATCHES "Clang")
    target_compile_options(crypto PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>)
    target_compile_options(ssl PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT ANDROID)
    message(FATAL_ERROR "openssl: 32-bit builds are only supported on Android")
endif()

# libssl 依赖 libcrypto：同时让公共宏（WIN32_LEAN_AND_MEAN、
# OPENSSL_SYS_*、OPENSSL_NO_* 等）传播到 ssl
target_link_libraries(ssl PUBLIC crypto)

target_link_libraries(crypto ${CMAKE_THREAD_LIBS_INIT})
if(MINGW)
    target_link_libraries(crypto PUBLIC ws2_32)
endif()
if (MSVC)
    openssl_target_defines(crypto DSO_WIN32 OPENSSL_SYS_WIN64 L_ENDIAN)
    target_link_libraries(crypto PUBLIC bcrypt.lib crypt32.lib)
endif()

set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
set(OPENSSL_LIBRARIES crypto ssl PARENT_SCOPE)
set(OPENSSL_FOUND TRUE PARENT_SCOPE)
set(OpenSSL_FOUND TRUE PARENT_SCOPE)
set(OPENSSL_VERSION "4.2.0-dev" PARENT_SCOPE)
set(LIB_EAY_LIBRARY crypto PARENT_SCOPE)
set(SSL_EAY_LIBRARY ssl PARENT_SCOPE)
set(OPENSSL_SSL_LIBRARY ssl PARENT_SCOPE)
set(OPENSSL_CRYPTO_LIBRARY crypto PARENT_SCOPE)
add_library(OpenSSL::Crypto ALIAS crypto)
add_library(OpenSSL::SSL ALIAS ssl)
