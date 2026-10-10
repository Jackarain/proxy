#!/usr/bin/env python3
"""由 OpenSSL 的 Configure/Makefile 生成可用于 add_subdirectory 的 CMakeLists.txt。

生成的 CMake 在配置阶段按目标平台/架构选择源文件清单与 perlasm 方案，
在构建阶段用 perl 生成汇编源，因此同一份 CMakeLists.txt 配合已提交的
生成头文件即可适配 linux/macos/windows 上的 x86_64/x86/aarch64/arm。
macOS universal（多架构）或缺少 perl/nasm 时自动回退到纯 C 实现。
"""
import os
import re
import shutil
import subprocess
import tempfile
import argparse

# ==================== 配置项 ====================
EXCLUDE_DIRS = {
    'test', 'apps', 'doc', 'demos', 'fuzz', 'util', 'os-dep',
    'vms', 'ms', 'tools', 'bugs', 'external'
}

# 导出项目目录时额外跳过的目录：外部子模块/测试工程、开发与版本控制目录
EXPORT_SKIP_DIRS = EXCLUDE_DIRS | {
    'VMS', 'ms', 'dev',
    'cloudflare-quiche', 'gost-engine', 'krb5', 'oqs-provider',
    'pkcs11-provider', 'pyca-cryptography', 'python-ecdsa',
    'tlsfuzzer', 'tlslite-ng', 'wycheproof',
    '.git', '.github', '.claude', '.circleci', '.ctags.d', '.vscode',
    '__pycache__',
}

# 导出项目目录时跳过的构建产物后缀
EXPORT_SKIP_SUFFIXES = (
    '.o', '.obj', '.d', '.a', '.so', '.pc', '.tmp', '.pyc', '.pyo', '.log',
)

# 汇编源扩展名（.s 为纯汇编，.S 需预处理，.asm 为 NASM）
ASM_EXTS = ('.s', '.S', '.asm')

# 支持的架构：
#   configure  生成该架构源清单所用的 Configure 目标
#   processor  CMAKE_SYSTEM_PROCESSOR 匹配正则
#   schemes    MSVC/MinGW/Apple/Unix 下的 perlasm scheme（None 表示不支持）
#   nasm       MSVC 下 NASM 的汇编参数
ARCHES = {
    'x86_64': {
        'configure': 'linux-x86_64',
        'processor': 'x86_64|amd64|AMD64',
        'schemes': {'msvc': 'nasm', 'mingw': 'mingw64',
                    'apple': 'macosx', 'unix': 'elf'},
        'nasm': '-Ox -f win64 -DNEAR',
    },
    'x86': {
        'configure': 'linux-elf',
        'processor': 'i[3-6]86|x86',
        'schemes': {'msvc': 'win32n', 'mingw': 'coff',
                    'apple': 'macosx', 'unix': 'elf'},
        'nasm': '-Ox -f win32',
    },
    'aarch64': {
        'configure': 'linux-aarch64',
        'processor': 'aarch64|arm64|ARM64|ARMv8',
        'schemes': {'msvc': None, 'mingw': 'win64',
                    'apple': 'ios64', 'unix': 'linux64'},
        'nasm': '',
    },
    'arm': {
        'configure': 'linux-armv4',
        'processor': 'arm',
        'schemes': {'msvc': None, 'mingw': None,
                    'apple': None, 'unix': 'linux32'},
        'nasm': '',
    },
}

# 默认 Configure 选项；--no-asm 时会追加 no-asm，--opt 可再追加自定义项
BASE_CONFIGURE_OPTS = [
    'no-shared', 'no-buildtest-c++', 'no-devcryptoeng', 'no-engine', 'no-dso',
    'no-whirlpool', 'no-afalgeng', 'no-des', 'no-idea', 'no-seed', 'no-blake2',
    'no-rc2', 'no-rc4', 'no-rc5', 'no-ssl2', 'no-ssl3', 'no-legacy', 'no-docs',
    'no-http', 'no-brotli-dynamic', 'no-brotli', 'no-h3demo',
    'no-dynamic-engine', 'no-tests', 'no-acvp-tests', 'no-allocfail-tests',
    'no-demos', 'no-external-tests', 'no-fips', 'no-fips-jitter',
    'no-fips-post', 'no-fips-securitychecks', 'no-md2', 'no-md4',
    'no-unit-test', 'no-zlib', 'no-zstd', 'no-zlib-dynamic',
    'no-zstd-dynamic', 'no-winstore',
]

SRC = '${CMAKE_CURRENT_SOURCE_DIR}/'


def build_configure_opts(enable_asm=True, extra=None):
    opts = list(BASE_CONFIGURE_OPTS) + list(extra or [])
    if not enable_asm:
        opts.append('no-asm')
    return opts

# ==================== 基础工具 ====================


def run_cmd(cmd, cwd=None):
    print('--> 执行: ' + ' '.join(cmd))
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        print(f'错误信息:\n{res.stderr}')
        raise RuntimeError('命令执行失败: ' + ' '.join(cmd))
    return res.stdout


def should_exclude(path):
    parts = path.replace('\\', '/').split('/')
    return any(p in EXCLUDE_DIRS for p in parts)


def strip_srcdir(path, builddir, srcdir):
    """把源目录前缀从 Makefile 中的路径去掉，得到源码树相对路径"""
    prefix = os.path.relpath(srcdir, builddir).replace(os.sep, '/') + '/'
    p = path.replace(os.sep, '/')
    if p.startswith(prefix):
        p = p[len(prefix):]
    while p.startswith('./'):
        p = p[2:]
    return p


# 需要从源码树补齐的生成文件所在目录（这些文件被 .gitignore 忽略，clone 不会带上）
GENERATED_ROOTS = ('include', 'crypto', 'ssl', 'providers')

# clone 后在目标目录中额外删除的文件
EXPORT_SKIP_TARGET_FILES = ('.gitmodules',)


def clone_project(srcdir, outdir):
    """git clone 当前目录到 outdir，并删除其中的排除目录"""
    srcdir = os.path.abspath(srcdir)
    out_abs = os.path.abspath(outdir)

    if not os.path.isdir(os.path.join(srcdir, '.git')):
        raise SystemExit(f'{srcdir} 不是 git 仓库，无法 clone')
    if out_abs == srcdir:
        raise SystemExit('输出目录不能是源码目录本身')
    if out_abs == os.path.sep or srcdir.startswith(out_abs + os.path.sep):
        raise SystemExit(f'输出目录不合法: {out_abs}')
    if os.path.exists(out_abs):
        if os.path.isdir(os.path.join(out_abs, '.git')):
            raise SystemExit(f'{out_abs} 含有 .git，疑似仓库，请换用空目录')
        shutil.rmtree(out_abs)
    os.makedirs(os.path.dirname(out_abs), exist_ok=True)

    run_cmd(['git', 'clone', '--quiet', srcdir, out_abs])

    removed = []
    for name in sorted(EXPORT_SKIP_DIRS):
        path = os.path.join(out_abs, name)
        if not os.path.exists(path):
            continue
        if os.path.isdir(path) and not os.path.islink(path):
            shutil.rmtree(path, ignore_errors=True)
        else:
            os.remove(path)
        removed.append(name)
    for name in EXPORT_SKIP_TARGET_FILES:
        path = os.path.join(out_abs, name)
        if os.path.exists(path):
            os.remove(path)
    print(f'--> 已 clone 到 {out_abs}，删除目录: {", ".join(removed)}')
    return out_abs


def copy_generated_files(srcdir, outdir):
    """把 .gitignore 忽略但编译必需的生成文件补进目标目录"""
    rels = []
    for extra in (['--others', '--exclude-standard'],
                  ['--others', '--ignored', '--exclude-standard']):
        out = run_cmd(['git', 'ls-files'] + extra
                      + ['--'] + list(GENERATED_ROOTS), cwd=srcdir)
        rels.extend(line for line in out.splitlines() if line.strip())

    copied = 0
    for rel in sorted(set(rels)):
        if rel.endswith(EXPORT_SKIP_SUFFIXES):
            continue
        src = os.path.join(srcdir, rel)
        if not os.path.isfile(src):
            continue
        dst = os.path.join(outdir, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
    print(f'--> 已补齐 {copied} 个生成文件')
    return copied


def get_config_value(text, key, default=None):
    match = re.search(rf'"{re.escape(key)}"\s*=>\s*"([^"]*)"', text)
    return match.group(1) if match else default


def get_target_defines(text, target_name):
    """从 configdata.pm 的 defines 段读取某个库需要的汇编相关宏"""
    pos = text.find('"defines" => {')
    if pos == -1:
        return []
    start = text.find('{', pos)
    depth = 0
    end = -1
    for i in range(start, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                end = i
                break
    if end == -1:
        return []
    match = re.search(rf'"{re.escape(target_name)}"\s*=>\s*\[(.*?)\]',
                      text[start:end + 1], re.DOTALL)
    if not match:
        return []
    return re.findall(r'"([^"]+)"', match.group(1))


def get_include_dirs(makefile_text):
    """从 Makefile 的编译命令中提取源码树内的 -I 目录"""
    found = {'${CMAKE_CURRENT_SOURCE_DIR}',
             '${CMAKE_CURRENT_SOURCE_DIR}/include'}

    def add_dir(item):
        if item.startswith('/') or should_exclude(item):
            return
        clean = os.path.normpath(item)
        if clean != '.':
            found.add(f'${{CMAKE_CURRENT_SOURCE_DIR}}/{clean}')

    for pat in (r'^CNF_INCLUDES\s*=\s*(.*)', r'^INCLUDES\s*=\s*(.*)'):
        match = re.search(pat, makefile_text, re.MULTILINE)
        if match:
            line = match.group(1).replace('\\\n', ' ')
            for item in re.findall(r'-I\s*([^\s]+)', line):
                add_dir(item)

    for line in makefile_text.splitlines():
        if '$(LIB_CFLAGS)' not in line:
            continue
        for item in re.findall(r'-I\s*([^\s]+)', line):
            if os.path.isdir(item):
                add_dir(item)
    return sorted(found)


def read_openssl_version():
    if not os.path.exists('VERSION.dat'):
        return '0.0.0'
    values = {}
    with open('VERSION.dat', encoding='utf-8') as f:
        for line in f:
            if '=' in line:
                key, value = line.strip().split('=', 1)
                values[key] = value.strip().strip('"')
    version = (f"{values.get('MAJOR', '0')}.{values.get('MINOR', '0')}"
               f".{values.get('PATCH', '0')}")
    tag = values.get('PRE_RELEASE_TAG', '')
    return f'{version}-{tag}' if tag else version


# ==================== Makefile 解析 ====================


def parse_object_deps(makefile_text):
    """解析 `obj: dep...` 规则，得到 .o 的全部依赖"""
    joined = re.sub(r'\\\n', ' ', makefile_text)
    deps = {}
    for obj, rest in re.findall(r'^(\S+\.o):\s*(.*)$', joined, re.MULTILINE):
        deps.setdefault(obj, []).extend(rest.split())
    return deps


def parse_lib_manifest(makefile_text, target, builddir, srcdir):
    """解析库目标，返回 (C 源清单, 汇编清单[(base, ext, pl), ...])"""
    joined = re.sub(r'\\\n', ' ', makefile_text)
    deps = {}
    for obj, rest in re.findall(r'^(\S+\.(?:o|obj)):\s*(.*)$', joined, re.MULTILINE):
        deps.setdefault(obj, []).extend(rest.split())

    generators = {}
    for out, dd in re.findall(r'^(\S+\.(?:s|S|asm)):\s*(.*)$', joined, re.MULTILINE):
        for dep in dd.split():
            if dep.endswith('.pl'):
                generators[out] = dep
                break

    match = re.search(rf'^{re.escape(target)}\s*:\s*((?:.*\\\n)*.*)',
                      makefile_text, re.MULTILINE)
    if not match:
        return [], []

    c_files, asm_files = [], []
    for obj in match.group(1).replace('\\\n', ' ').split():
        if not obj.endswith(('.o', '.obj')):
            continue
        src = next((d for d in deps.get(obj, [])
                    if d.endswith(('.c',) + ASM_EXTS)), None)
        if not src:
            continue
        if src.endswith(ASM_EXTS):
            pl = generators.get(src)
            if not pl:
                continue
            base = os.path.splitext(strip_srcdir(src, builddir, srcdir))[0]
            ext = os.path.splitext(src)[1]
            asm_files.append((base, ext, strip_srcdir(pl, builddir, srcdir)))
        else:
            c_files.append(strip_srcdir(src, builddir, srcdir))
    return sorted(set(c_files)), sorted(set(asm_files))


def generate_missing_sources(makefile_text, sources):
    """调用 make 生成缺失的 C/头文件（如 *.h.in、der_*_gen.c 等）"""
    rules = set(re.findall(r'^(\S+):', makefile_text, re.MULTILINE))
    targets = sorted(s for s in sources if s in rules)
    if not targets:
        print('⚠️  没有需要生成的缺失源文件')
        return
    print(f'--> 生成缺失源文件: {len(targets)} 个')
    for i in range(0, len(targets), 100):
        run_cmd(['make'] + targets[i:i + 100])


def collect_missing_sources(makefile_text, obj_deps):
    """找出库编译需要但当前不存在的 C/头文件"""
    missing = set()
    for target in ('libcrypto.a', 'libssl.a'):
        match = re.search(rf'^{re.escape(target)}\s*:\s*((?:.*\\\n)*.*)',
                          makefile_text, re.MULTILINE)
        if not match:
            continue
        for obj in match.group(1).replace('\\\n', ' ').split():
            if not obj.endswith('.o'):
                continue
            for dep in obj_deps.get(obj, []):
                if dep.endswith(ASM_EXTS):
                    continue
                if not os.path.exists(dep) and not should_exclude(dep):
                    missing.add(dep)
    return missing


# ==================== 各架构清单 ====================


def collect_arch_data(srcdir, workdir, opts, arches=None):
    data = {}
    for arch in (arches or ARCHES):
        cfg = ARCHES[arch]
        bd = os.path.join(workdir, arch)
        os.makedirs(bd, exist_ok=True)
        run_cmd([os.path.join(srcdir, 'Configure'), cfg['configure']]
                + list(opts), cwd=bd)
        with open(os.path.join(bd, 'Makefile'), encoding='utf-8',
                  errors='replace') as f:
            makefile_text = f.read()
        with open(os.path.join(bd, 'configdata.pm'), encoding='utf-8',
                  errors='replace') as f:
            configdata = f.read()

        crypto_c, crypto_asm = parse_lib_manifest(makefile_text, 'libcrypto.a',
                                                  bd, srcdir)
        ssl_c, ssl_asm = parse_lib_manifest(makefile_text, 'libssl.a', bd, srcdir)
        data[arch] = {
            'crypto_c': crypto_c,
            'crypto_asm': crypto_asm,
            'ssl_c': ssl_c,
            'ssl_asm': ssl_asm,
            'crypto_defines': get_target_defines(configdata, 'libcrypto'),
            'ssl_defines': get_target_defines(configdata, 'libssl'),
            'include_dirs': get_include_dirs(makefile_text),
        }
        print(f'    {arch}: crypto C {len(crypto_c)} 汇编 {len(crypto_asm)}'
              f'  ssl C {len(ssl_c)} 汇编 {len(ssl_asm)}')
    return data


# ==================== CMake 生成 ====================


def emit_scheme_block(arch, indent):
    """按平台选择 perlasm scheme 与汇编器"""
    cfg = ARCHES[arch]
    schemes = cfg['schemes']
    lines = []

    def branch(cond, key, lang):
        scheme = schemes.get(key)
        lines.append(indent + cond)
        if scheme is None:
            lines.append(indent + '    message(WARNING "openssl: assembly for '
                         + arch + ' is not supported on this platform, '
                         'using portable C sources")')
            lines.append(indent + '    set(OPENSSL_USE_ASM FALSE)')
            return
        lines.append(indent + f'    set(OPENSSL_PERLASM_SCHEME "{scheme}")')
        lines.append(indent + f'    set(OPENSSL_ASM_LANG {lang})')
        if key == 'msvc':
            lines.append(indent + '    set(CMAKE_ASM_NASM_COMPILER "${OPENSSL_NASM_EXECUTABLE}")')
            if cfg['nasm']:
                lines.append(indent + '    set(CMAKE_ASM_NASM_FLAGS '
                             f'"${{CMAKE_ASM_NASM_FLAGS}} {cfg["nasm"]}")')

    branch('if(WIN32 AND MSVC)', 'msvc', 'ASM_NASM')
    branch('elseif(WIN32)', 'mingw', 'ASM')
    branch('elseif(APPLE)', 'apple', 'ASM')
    branch('else()', 'unix', 'ASM')
    lines.append(indent + 'endif()')
    return lines


def emit_arch_chain(lines, entries, indent=''):
    """生成 if(OPENSSL_ARCH ...) 分支链"""
    if not entries:
        return
    first = True
    for arch, body in entries:
        lines.append(indent + ('if' if first else 'elseif')
                     + f'(OPENSSL_ARCH STREQUAL "{arch}")')
        first = False
        lines.extend(indent + '    ' + b for b in body)
    lines.append(indent + 'endif()')


def format_sources(paths, indent='    '):
    return [indent + SRC + p for p in paths]


def append_block(name, paths):
    return [f'list(APPEND {name}'] + format_sources(paths) + [')']


ASM_MACRO = r'''
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
'''.strip('\n')

DEFINE_HELPER = r'''
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
'''.strip('\n')


def generate_cmake(arch_data, include_dirs, openssl_version, portable=None):
    archs = list(ARCHES)
    has_asm = any(d['crypto_asm'] or d['ssl_asm'] for d in arch_data.values())
    if portable is None:
        portable = {'crypto_c': [], 'ssl_c': [],
                    'crypto_defines': [], 'ssl_defines': []}

    crypto_common = sorted(set.intersection(
        *[set(d['crypto_c']) for d in arch_data.values()]))
    ssl_common = sorted(set.intersection(
        *[set(d['ssl_c']) for d in arch_data.values()]))

    lines = []
    lines.append('# 由 gen_cmake.py 自动生成，请勿手工修改')
    lines.append('# 支持架构: ' + '/'.join(archs)
                 + ('，按平台选择汇编方案' if has_asm else '，纯 C 构建'))
    lines.append('cmake_minimum_required(VERSION 3.10)')
    lines.append('project(openssl C)')
    lines.append('')
    lines.append('option(SHARED_OPENSSL "build shared openssl lib" OFF)')
    lines.append('')
    lines.append('# 上层 add_compile_options 注入的 MSVC 专用参数（如 /utf-8）会传播到')
    lines.append('# 汇编语言，nasm 会将其当作输入文件而报错；这里把目录级编译选项')
    lines.append('# 限制为 C/CXX，汇编源不受影响')
    lines.append('get_directory_property(_openssl_dir_options COMPILE_OPTIONS)')
    lines.append('if(_openssl_dir_options)')
    lines.append('    set_property(DIRECTORY PROPERTY COMPILE_OPTIONS "")')
    lines.append('    foreach(_openssl_option IN LISTS _openssl_dir_options)')
    lines.append('        add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:${_openssl_option}>")')
    lines.append('    endforeach()')
    lines.append('endif()')
    lines.append('')
    lines.append('# ==================== 目标架构检测 ====================')
    lines.append('# 交叉编译（如 MinGW）时 CMAKE_SYSTEM_PROCESSOR 可能为空，')
    lines.append('# 同时参考编译器三元组（CMAKE_C_COMPILER(_TARGET)）避免误判')
    lines.append('set(OPENSSL_ARCH_HINTS')
    lines.append('    "${CMAKE_SYSTEM_PROCESSOR}"')
    lines.append('    "${CMAKE_C_COMPILER_TARGET}"')
    lines.append('    "${CMAKE_C_COMPILER}"')
    lines.append('    "${CMAKE_CXX_COMPILER}"')
    lines.append(')')
    lines.append('# macOS 单架构交叉编译时以 CMAKE_OSX_ARCHITECTURES 为准；')
    lines.append('# 多架构（universal）无法在单目标内区分架构切片，见下方汇编可用性')
    lines.append('if(APPLE AND CMAKE_OSX_ARCHITECTURES)')
    lines.append('    list(LENGTH CMAKE_OSX_ARCHITECTURES _openssl_osx_arch_count)')
    lines.append('    if(NOT _openssl_osx_arch_count GREATER 1)')
    lines.append('        list(INSERT OPENSSL_ARCH_HINTS 0 "${CMAKE_OSX_ARCHITECTURES}")')
    lines.append('    endif()')
    lines.append('endif()')
    lines.append('set(OPENSSL_ARCH "")')
    lines.append('foreach(_hint IN LISTS OPENSSL_ARCH_HINTS)')
    first = True
    for arch, cfg in ARCHES.items():
        lines.append(('    if' if first else '    elseif')
                     + f'(_hint MATCHES "({cfg["processor"]})")')
        lines.append(f'        set(OPENSSL_ARCH "{arch}")')
        first = False
    lines.append('    endif()')
    lines.append('    if(OPENSSL_ARCH)')
    lines.append('        break()')
    lines.append('    endif()')
    lines.append('endforeach()')
    lines.append('if(NOT OPENSSL_ARCH)')
    lines.append('    message(FATAL_ERROR "openssl: unsupported target architecture ${CMAKE_SYSTEM_PROCESSOR} (compiler: ${CMAKE_C_COMPILER})")')
    lines.append('endif()')
    lines.append('')
    if has_asm:
        lines.append('# ==================== 汇编可用性 ====================')
        lines.append('# macOS universal（多架构）构建无法在同一目标内为不同架构切片使用')
        lines.append('# 各自的汇编实现；缺少 perl/nasm 或平台不支持时同样回退到纯 C。')
        lines.append('set(OPENSSL_USE_ASM TRUE)')
        lines.append('if(APPLE AND CMAKE_OSX_ARCHITECTURES)')
        lines.append('    list(LENGTH CMAKE_OSX_ARCHITECTURES _openssl_osx_arch_count)')
        lines.append('    if(_openssl_osx_arch_count GREATER 1)')
        lines.append('        set(OPENSSL_USE_ASM FALSE)')
        lines.append('        message(STATUS "openssl: universal build detected, using portable C sources")')
        lines.append('    endif()')
        lines.append('endif()')
        lines.append('if(OPENSSL_USE_ASM)')
        lines.append('    find_program(OPENSSL_PERL_EXECUTABLE NAMES perl perl.exe)')
        lines.append('    if(NOT OPENSSL_PERL_EXECUTABLE)')
        lines.append('        set(OPENSSL_USE_ASM FALSE)')
        lines.append('        message(WARNING "openssl: perl not found, using portable C sources")')
        lines.append('    endif()')
        lines.append('endif()')
        lines.append('if(OPENSSL_USE_ASM AND WIN32 AND MSVC)')
        lines.append('    find_program(OPENSSL_NASM_EXECUTABLE NAMES nasm nasm.exe)')
        lines.append('    if(NOT OPENSSL_NASM_EXECUTABLE)')
        lines.append('        set(OPENSSL_USE_ASM FALSE)')
        lines.append('        message(WARNING "openssl: nasm not found, using portable C sources")')
        lines.append('    endif()')
        lines.append('endif()')
        lines.append('if(OPENSSL_USE_ASM)')
        scheme_entries = [(a, emit_scheme_block(a, '        ')) for a in archs]
        emit_arch_chain(lines, scheme_entries, indent='    ')
        lines.append('endif()')
        lines.append('')
    else:
        lines.append('set(OPENSSL_USE_ASM FALSE)')
        lines.append('')
    lines.append('# ==================== 平台系统宏 ====================')
    lines.append('if(WIN32 AND MSVC)')
    lines.append('    if(CMAKE_SIZEOF_VOID_P EQUAL 8)')
    lines.append('        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_WIN64A)')
    lines.append('    else()')
    lines.append('        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_WIN32)')
    lines.append('    endif()')
    lines.append('elseif(WIN32)')
    lines.append('    if(CMAKE_SIZEOF_VOID_P EQUAL 8)')
    lines.append('        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MINGW64)')
    lines.append('    else()')
    lines.append('        set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MINGW32)')
    lines.append('    endif()')
    lines.append('elseif(APPLE)')
    lines.append('    set(OPENSSL_SYS_DEFINES OPENSSL_SYS_MACOSX)')
    lines.append('else()')
    lines.append('    set(OPENSSL_SYS_DEFINES "")')
    lines.append('endif()')
    lines.append('')
    lines.append('# ==================== 头文件目录 ====================')
    lines.append('set(OPENSSL_INCLUDES')
    lines.extend('    ' + d for d in include_dirs)
    lines.append(')')
    lines.append('')
    lines.append('# ==================== libcrypto 源文件 ====================')
    lines.append('set(OPENSSL_CRYPTO_SRCS')
    lines.extend(format_sources(crypto_common))
    lines.append(')')
    if has_asm:
        lines.append('set(OPENSSL_PORTABLE_CRYPTO_SRCS')
        lines.extend(format_sources(portable['crypto_c']))
        lines.append(')')
    extras = [(a, append_block('OPENSSL_CRYPTO_SRCS',
                               sorted(set(arch_data[a]['crypto_c'])
                                      - set(crypto_common))))
              for a in archs]
    extras = [(a, b) for a, b in extras
              if set(arch_data[a]['crypto_c']) - set(crypto_common)]
    if has_asm:
        lines.append('')
        lines.append('if(OPENSSL_USE_ASM)')
        emit_arch_chain(lines, extras, indent='    ')
        lines.append('else()')
        lines.append('    set(OPENSSL_CRYPTO_SRCS ${OPENSSL_PORTABLE_CRYPTO_SRCS})')
        lines.append('endif()')
    elif extras:
        lines.append('')
        emit_arch_chain(lines, extras)
    lines.append('')
    lines.append('# ==================== libssl 源文件 ====================')
    lines.append('set(OPENSSL_SSL_SRCS')
    lines.extend(format_sources(ssl_common))
    lines.append(')')
    if has_asm:
        lines.append('set(OPENSSL_PORTABLE_SSL_SRCS')
        lines.extend(format_sources(portable['ssl_c']))
        lines.append(')')
    ssl_extras = [(a, append_block('OPENSSL_SSL_SRCS',
                                   sorted(set(arch_data[a]['ssl_c'])
                                          - set(ssl_common))))
                  for a in archs]
    ssl_extras = [(a, b) for a, b in ssl_extras
                  if set(arch_data[a]['ssl_c']) - set(ssl_common)]
    if has_asm:
        lines.append('')
        lines.append('if(OPENSSL_USE_ASM)')
        emit_arch_chain(lines, ssl_extras, indent='    ')
        lines.append('else()')
        lines.append('    set(OPENSSL_SSL_SRCS ${OPENSSL_PORTABLE_SSL_SRCS})')
        lines.append('endif()')
    elif ssl_extras:
        lines.append('')
        emit_arch_chain(lines, ssl_extras)
    lines.append('')

    # 各架构的汇编宏
    lines.append('# ==================== 架构相关编译宏 ====================')
    def_defs = []
    for a in archs:
        body = []
        defs = arch_data[a]['crypto_defines']
        if defs:
            body.append('set(OPENSSL_CRYPTO_ASM_DEFINES')
            body.extend(f'    {d}' for d in defs)
            body.append(')')
        else:
            body.append('set(OPENSSL_CRYPTO_ASM_DEFINES "")')
        ssl_defs = arch_data[a]['ssl_defines']
        if ssl_defs:
            body.append('set(OPENSSL_SSL_ASM_DEFINES')
            body.extend(f'    {d}' for d in ssl_defs)
            body.append(')')
        else:
            body.append('set(OPENSSL_SSL_ASM_DEFINES "")')
        def_defs.append((a, body))
    emit_arch_chain(lines, def_defs)
    lines.append('')

    if has_asm:
        lines.append('# ==================== 汇编优化 ====================')
        lines.append('set(OPENSSL_CRYPTO_ASM_SRCS "")')
        lines.append('set(OPENSSL_SSL_ASM_SRCS "")')
        lines.append('')
        lines.append('if(OPENSSL_USE_ASM)')
        lines.append('    enable_language(${OPENSSL_ASM_LANG})')
        lines.append('')
        lines.append('    # 汇编源不参与 C 预处理，去掉 -MD/-MF 依赖参数，避免无用参数告警')
        lines.append('    set(CMAKE_DEPFILE_FLAGS_ASM "")')
        lines.append('')
        # 每个架构各自设置 OPENSSL_ASM_DEFINES
        asm_defs = []
        for a in archs:
            body = ['set(OPENSSL_ASM_DEFINES', '    -DL_ENDIAN']
            body.extend(f'    -D{d}' for d in arch_data[a]['crypto_defines'])
            body.append(')')
            asm_defs.append((a, body))
        emit_arch_chain(lines, asm_defs, indent='    ')
        lines.append('')
        lines.append('    # cl.exe 不接受 perlasm 探测使用的 GNU 风格参数，其报错会让')
        lines.append('    # MSBuild 判定构建失败；因此仅在非 MSVC 下向 perlasm 传递 CC')
        lines.append('    set(OPENSSL_PERLASM_ENV "")')
        lines.append('    if(NOT MSVC)')
        lines.append('        set(OPENSSL_PERLASM_ENV "CC=${CMAKE_C_COMPILER}")')
        lines.append('    endif()')
        lines.append('')
        lines.extend('    ' + ln if ln else ln for ln in ASM_MACRO.split('\n'))
        lines.append('')
        asm_calls = []
        for a in archs:
            body = []
            for base, ext, pl in arch_data[a]['crypto_asm']:
                body.append('    ' + f'openssl_add_asm(OPENSSL_CRYPTO_ASM_SRCS {base} {ext} {pl})')
            for base, ext, pl in arch_data[a]['ssl_asm']:
                body.append('    ' + f'openssl_add_asm(OPENSSL_SSL_ASM_SRCS {base} {ext} {pl})')
            if body:
                asm_calls.append((a, body))
        emit_arch_chain(lines, asm_calls, indent='    ')
        lines.append('endif()')
        lines.append('')
    else:
        lines.append('# ==================== 无汇编构建 ====================')
        lines.append('')

    lines.append('# ==================== 目标 ====================')
    lines.append('if (SHARED_OPENSSL)')
    lines.append('    add_library(crypto SHARED ${OPENSSL_CRYPTO_SRCS} ${OPENSSL_CRYPTO_ASM_SRCS})')
    lines.append('    add_library(ssl SHARED ${OPENSSL_SSL_SRCS} ${OPENSSL_SSL_ASM_SRCS})')
    lines.append('else()')
    lines.append('    add_library(crypto STATIC ${OPENSSL_CRYPTO_SRCS} ${OPENSSL_CRYPTO_ASM_SRCS})')
    lines.append('    add_library(ssl STATIC ${OPENSSL_SSL_SRCS} ${OPENSSL_SSL_ASM_SRCS})')
    lines.append('endif()')
    lines.append('')
    lines.append('set_target_properties(crypto PROPERTIES FOLDER "third_party/openssl")')
    lines.append('set_target_properties(ssl PROPERTIES FOLDER "third_party/openssl")')
    lines.append('')
    lines.append('target_include_directories(crypto PUBLIC ${OPENSSL_INCLUDES})')
    lines.append('target_include_directories(ssl PUBLIC ${OPENSSL_INCLUDES})')
    lines.append('')
    lines.append(DEFINE_HELPER)
    lines.append('')
    lines.append('openssl_target_defines(crypto')
    lines.append('    OPENSSL_NO_ASYNC')
    lines.append('    OPENSSL_NO_ENGINE')
    lines.append('    OPENSSL_NO_RC1')
    lines.append('    OPENSSL_NO_RC3')
    lines.append('    OPENSSL_NO_MD1')
    lines.append('    OPENSSL_NO_MD2')
    lines.append('    OPENSSL_NO_MD3')
    lines.append('    OPENSSL_NO_MD4')
    lines.append('    OPENSSL_NO_MDC2')
    lines.append('    OPENSSL_NO_WHIRLPOOL')
    lines.append('    OPENSSL_NO_COMP')
    lines.append('    OPENSSLDIR=""')
    lines.append('    ENGINESDIR=""')
    lines.append('    MODULESDIR=""')
    lines.append('    WIN32_LEAN_AND_MEAN')
    lines.append('    ${OPENSSL_SYS_DEFINES}')
    lines.append(')')
    lines.append('')
    lines.append('openssl_target_defines(ssl OPENSSL_NO_ENGINE)')
    lines.append('')
    if has_asm:
        portable_crypto_defs = ' '.join(portable['crypto_defines'])
        portable_ssl_defs = ' '.join(portable['ssl_defines'])
        lines.append('if(OPENSSL_USE_ASM)')
        lines.append('    openssl_target_defines(crypto ${OPENSSL_CRYPTO_ASM_DEFINES})')
        lines.append('    openssl_target_defines(ssl ${OPENSSL_SSL_ASM_DEFINES})')
        lines.append('else()')
        lines.append('    openssl_target_defines(crypto OPENSSL_NO_ASM'
                     + (f' {portable_crypto_defs}' if portable_crypto_defs else '') + ')')
        lines.append('    openssl_target_defines(ssl OPENSSL_NO_ASM'
                     + (f' {portable_ssl_defs}' if portable_ssl_defs else '') + ')')
        lines.append('endif()')
        lines.append('')
        lines.append('# 上层目录级 add_definitions 注入的 -D 对纯汇编无意义，')
        lines.append('# clang 会报无用参数告警，这里只针对 ASM 语言屏蔽')
        lines.append('if(OPENSSL_USE_ASM AND CMAKE_ASM_COMPILER_ID MATCHES "Clang")')
        lines.append('    target_compile_options(crypto PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>)')
        lines.append('    target_compile_options(ssl PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-Wno-unused-command-line-argument>)')
        lines.append('endif()')
        lines.append('')
    else:
        lines.append('openssl_target_defines(crypto OPENSSL_NO_ASM)')
        lines.append('')
    lines.append('if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT ANDROID)')
    lines.append('    message(FATAL_ERROR "openssl: 32-bit builds are only supported on Android")')
    lines.append('endif()')
    lines.append('')
    lines.append('# libssl 依赖 libcrypto：同时让公共宏（WIN32_LEAN_AND_MEAN、')
    lines.append('# OPENSSL_SYS_*、OPENSSL_NO_* 等）传播到 ssl')
    lines.append('target_link_libraries(ssl PUBLIC crypto)')
    lines.append('')
    lines.append('target_link_libraries(crypto ${CMAKE_THREAD_LIBS_INIT})')
    lines.append('if(MINGW)')
    lines.append('    target_link_libraries(crypto PUBLIC ws2_32)')
    lines.append('endif()')
    lines.append('if (MSVC)')
    lines.append('    openssl_target_defines(crypto DSO_WIN32 OPENSSL_SYS_WIN64 L_ENDIAN)')
    lines.append('    target_link_libraries(crypto PUBLIC bcrypt.lib crypt32.lib)')
    lines.append('endif()')
    lines.append('')
    lines.append('set(OPENSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)')
    lines.append('set(OPENSSL_LIBRARIES crypto ssl PARENT_SCOPE)')
    lines.append('set(OPENSSL_FOUND TRUE PARENT_SCOPE)')
    lines.append('set(OpenSSL_FOUND TRUE PARENT_SCOPE)')
    lines.append(f'set(OPENSSL_VERSION "{openssl_version}" PARENT_SCOPE)')
    lines.append('set(LIB_EAY_LIBRARY crypto PARENT_SCOPE)')
    lines.append('set(SSL_EAY_LIBRARY ssl PARENT_SCOPE)')
    lines.append('set(OPENSSL_SSL_LIBRARY ssl PARENT_SCOPE)')
    lines.append('set(OPENSSL_CRYPTO_LIBRARY crypto PARENT_SCOPE)')
    lines.append('add_library(OpenSSL::Crypto ALIAS crypto)')
    lines.append('add_library(OpenSSL::SSL ALIAS ssl)')
    lines.append('')

    return '\n'.join(lines)


# ==================== configuration.h 补丁 ====================

PATCH_BEGIN = '/* gen_cmake: patch begin */'
PATCH_END = '/* gen_cmake: patch end */'


def get_patch_code():
    return f'''{PATCH_BEGIN}
/* 跨平台字长与 RC4 类型，覆盖 Configure 在本机生成的值 */
#undef THIRTY_TWO_BIT
#undef SIXTY_FOUR_BIT
#undef SIXTY_FOUR_BIT_LONG
#undef BN_LLONG

#if defined(_WIN64)
#  define SIXTY_FOUR_BIT
#elif defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || \\
      defined(_M_ARM64) || defined(__powerpc64__) || defined(__ppc64__) || \\
      defined(__s390x__) || defined(__mips64) || defined(__mips64__) || \\
      defined(__riscv) || defined(__sparc64__) || defined(__ia64__) || \\
      (defined(__LP64__) && __LP64__)
#  if defined(_WIN32)
#    define SIXTY_FOUR_BIT
#  else
#    define SIXTY_FOUR_BIT_LONG
#  endif
#else
#  define THIRTY_TWO_BIT
#  define BN_LLONG
#endif

#undef RC4_INT
#if (defined(__aarch64__) || defined(__arm__) || defined(_M_ARM)) && !defined(__APPLE__)
#  define RC4_INT unsigned char
#else
#  define RC4_INT unsigned int
#endif
{PATCH_END}
'''


def patch_configuration_h():
    config_path = os.path.join('include', 'openssl', 'configuration.h')
    if not os.path.exists(config_path):
        print(f'⚠️  未找到 {config_path}，跳过 patch')
        return

    with open(config_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # 去掉上一次注入的补丁，保证幂等
    content = re.sub(re.escape(PATCH_BEGIN) + r'.*?' + re.escape(PATCH_END) + r'\n?',
                     '', content, flags=re.DOTALL)

    cpp_guard = '#ifdef __cplusplus\n}\n#endif\n'
    if cpp_guard not in content:
        cpp_guard = '#ifdef __cplusplus\n}\n#endif'
    if cpp_guard not in content:
        print('❌ 未找到插入位置，跳过 patch')
        return

    with open(config_path, 'w', encoding='utf-8') as f:
        f.write(content.replace(cpp_guard, get_patch_code() + cpp_guard))
    print(f'✅ 已更新 {config_path}')


# ==================== 入口 ====================


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description='生成可用于 add_subdirectory 的 OpenSSL CMakeLists.txt')
    parser.add_argument('-a', '--arches', default='all',
                        help='要支持的架构，逗号分隔（默认 all）: '
                             + ','.join(ARCHES))
    parser.add_argument('--no-asm', action='store_true',
                        help='关闭汇编优化，生成纯 C 回退版本')
    parser.add_argument('-o', '--output', required=True, metavar='DIR',
                        help='导出的项目目录（必填，脚本会写入 CMakeLists.txt）')
    parser.add_argument('--opt', action='append', default=[], metavar='OPT',
                        help='追加 Configure 选项，可重复')
    parser.add_argument('--no-patch-config', action='store_true',
                        help='不修改 include/openssl/configuration.h')
    parser.add_argument('--keep-temp', action='store_true',
                        help='保留各架构的临时 Configure 目录')
    parser.add_argument('--list-arches', action='store_true',
                        help='列出支持的架构后退出')
    return parser.parse_args(argv)


def select_arches(spec):
    names = [n.strip() for n in spec.split(',') if n.strip()]
    if not names or 'all' in names:
        return list(ARCHES)
    unknown = [n for n in names if n not in ARCHES]
    if unknown:
        raise SystemExit(f'不支持的架构: {", ".join(unknown)}'
                         f'（可选: {", ".join(ARCHES)}）')
    return [n for n in ARCHES if n in names]


def safe_git_clean(patterns):
    """尽力清理生成物；非 git 仓库时忽略"""
    if not os.path.exists('.git'):
        return
    try:
        run_cmd(['git', 'clean', '-f'] + patterns)
    except RuntimeError:
        pass


def main(argv=None):
    global ARCHES
    args = parse_args(argv)
    if args.list_arches:
        for name, cfg in ARCHES.items():
            print(f'{name:8} -> {cfg["configure"]}')
        return

    ARCHES = {name: ARCHES[name] for name in select_arches(args.arches)}
    enable_asm = not args.no_asm
    opts = build_configure_opts(enable_asm, args.opt)
    output = os.path.abspath(args.output)

    print(f'架构: {", ".join(ARCHES)}  汇编: {"on" if enable_asm else "off"}')
    print(f'输出: {output}')

    srcdir = os.getcwd()
    if os.path.exists('Makefile'):
        safe_git_clean(['*.d', '*.o', '*.tmp', '*.a', '*.pc', '*.pm', 'Makefile'])

    print('步骤 1: 配置本机目标并生成缺失的源文件...')
    run_cmd([os.path.join(srcdir, 'Configure')] + list(opts))
    run_cmd(['make', 'build_generated'])
    with open('Makefile', encoding='utf-8', errors='replace') as f:
        host_makefile = f.read()
    obj_deps = parse_object_deps(host_makefile)
    generate_missing_sources(host_makefile,
                             collect_missing_sources(host_makefile, obj_deps))

    include_dirs = set(get_include_dirs(host_makefile))

    print('步骤 2: 为各架构解析源清单...')
    workdir = tempfile.mkdtemp(prefix='openssl-cmake-')
    portable = None
    try:
        arch_data = collect_arch_data(srcdir, workdir, opts)
        if enable_asm:
            print('    portable: 解析纯 C 回退清单...')
            # 纯 C（no-asm）配置下各架构的源清单一致，取任一架构即可
            portable_arch = next(iter(ARCHES))
            portable = collect_arch_data(
                srcdir, os.path.join(workdir, 'portable'),
                opts + ['no-asm'], arches=[portable_arch])[portable_arch]
    finally:
        if args.keep_temp:
            print(f'    临时目录已保留: {workdir}')
        else:
            shutil.rmtree(workdir, ignore_errors=True)
    for d in arch_data.values():
        include_dirs.update(d['include_dirs'])

    print('步骤 3: 生成 CMakeLists.txt 内容...')
    cmake_content = generate_cmake(arch_data, sorted(include_dirs),
                                   read_openssl_version(), portable)

    if args.no_patch_config:
        print('步骤 4: 跳过 configuration.h 修补')
    else:
        print('步骤 4: 修补 configuration.h ...')
        patch_configuration_h()

    print('步骤 5: clone 项目目录...')
    clone_project(srcdir, output)

    print('步骤 6: 补齐生成文件...')
    copy_generated_files(srcdir, output)

    cmake_path = os.path.join(output, 'CMakeLists.txt')
    with open(cmake_path, 'w', encoding='utf-8') as f:
        f.write(cmake_content)
    print(f'--> 已写入 {cmake_path}')

    print('步骤 7: 清理临时文件...')
    safe_git_clean(['*.d', '*.o', '*.tmp', '*.a', '*.pc'])
    print(f'\n🎉 全部完成！项目已导出到 {output}')


if __name__ == '__main__':
    main()
