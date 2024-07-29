# Copyright 2022 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

local library = "endian";

local triggers =
{
    branch: [ "master", "develop", "feature/*" ]
};

local ubsan = { UBSAN: '1', UBSAN_OPTIONS: 'print_stacktrace=1' };
local asan = { ASAN: '1' };

local linux_pipeline(name, image, environment, packages = "", sources = [], arch = "amd64") =
{
    name: name,
    kind: "pipeline",
    type: "docker",
    trigger: triggers,
    platform:
    {
        os: "linux",
        arch: arch
    },
    steps:
    [
        {
            name: "everything",
            image: image,
            environment: environment,
            commands:
            [
                'set -e',
                'wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -',
            ] +
            (if sources != [] then [ ('apt-add-repository "' + source + '"') for source in sources ] else []) +
            (if packages != "" then [ 'apt-get update', 'apt-get -y install ' + packages ] else []) +
            [
                'export LIBRARY=' + library,
                './.drone/drone.sh',
            ]
        }
    ]
};

local macos_pipeline(name, environment, xcode_version = "12.2", osx_version = "catalina", arch = "amd64") =
{
    name: name,
    kind: "pipeline",
    type: "exec",
    trigger: triggers,
    platform: {
        "os": "darwin",
        "arch": arch
    },
    node: {
        "os": osx_version
    },
    steps: [
        {
            name: "everything",
            environment: environment + { "DEVELOPER_DIR": "/Applications/Xcode-" + xcode_version + ".app/Contents/Developer" },
            commands:
            [
                'export LIBRARY=' + library,
                './.drone/drone.sh',
            ]
        }
    ]
};

local windows_pipeline(name, image, environment, arch = "amd64") =
{
    name: name,
    kind: "pipeline",
    type: "docker",
    trigger: triggers,
    platform:
    {
        os: "windows",
        arch: arch
    },
    "steps":
    [
        {
            name: "everything",
            image: image,
            environment: environment,
            commands:
            [
                'cmd /C .drone\\\\drone.bat ' + library,
            ]
        }
    ]
};

[
    linux_pipeline(
        "Linux 14.04 GCC 4.6 32/64",
        "cppalliance/droneubuntu1404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-4.6', CXXSTD: '0x', ADDRMD: '32,64' },
        "g++-4.6-multilib",
    ),

    linux_pipeline(
        "Linux 14.04 GCC 4.7 32/64",
        "cppalliance/droneubuntu1404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-4.7', CXXSTD: '0x', ADDRMD: '32,64' },
        "g++-4.7-multilib",
    ),

    linux_pipeline(
        "Linux 14.04 GCC 4.8* 32/64",
        "cppalliance/droneubuntu1404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11', ADDRMD: '32,64' },
    ),

    linux_pipeline(
        "Linux 16.04 GCC 4.9 32/64",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-4.9', CXXSTD: '11', ADDRMD: '32,64' },
        "g++-4.9-multilib",
    ),

    linux_pipeline(
        "Linux 16.04 GCC 5* 32/64",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14', ADDRMD: '32,64' },
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9 ARM64",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,2a' },
        arch="arm64",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9 S390x",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,2a' },
        arch="s390x",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32/64",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '11,14,17,20', ADDRMD: '32,64' },
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 23.04 GCC 13 32 UBSAN",
        "cppalliance/droneubuntu2304:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '11,14,17,20,2b', ADDRMD: '32' } + ubsan,
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 23.04 GCC 13 64 UBSAN",
        "cppalliance/droneubuntu2304:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '11,14,17,20,2b', ADDRMD: '64' } + ubsan,
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 23.04 GCC 13 32 ASAN",
        "cppalliance/droneubuntu2304:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '11,14,17,20,2b', ADDRMD: '32' } + asan,
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 23.04 GCC 13 64 ASAN",
        "cppalliance/droneubuntu2304:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '11,14,17,20,2b', ADDRMD: '64' } + asan,
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 Clang 15",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-15', CXXSTD: '11,14,17,20,2b' },
        "clang-15",
    ),

    linux_pipeline(
        "Linux 23.04 Clang 16",
        "cppalliance/droneubuntu2304:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-16', CXXSTD: '11,14,17,20,2b' },
        "clang-16",
    ),

    linux_pipeline(
        "Linux 23.10 Clang 17 UBSAN",
        "cppalliance/droneubuntu2310:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-17', CXXSTD: '11,14,17,20,2b' } + ubsan,
        "clang-17",
    ),

    linux_pipeline(
        "Linux 23.10 Clang 17 ASAN",
        "cppalliance/droneubuntu2310:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-17', CXXSTD: '11,14,17,20,2b' } + asan,
        "clang-17",
    ),

    macos_pipeline(
        "MacOS 10.15 Xcode 12.2 UBSAN",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '11,14,1z' } + ubsan,
    ),

    macos_pipeline(
        "MacOS 10.15 Xcode 12.2 ASAN",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '11,14,1z' } + asan,
    ),

    macos_pipeline(
        "MacOS 12.4 Xcode 13.4.1 UBSAN",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '11,14,17,20,2b' } + ubsan,
        xcode_version = "13.4.1", osx_version = "monterey", arch = "arm64",
    ),

    macos_pipeline(
        "MacOS 12.4 Xcode 13.4.1 ASAN",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '11,14,17,20,2b' } + asan,
        xcode_version = "13.4.1", osx_version = "monterey", arch = "arm64",
    ),

    windows_pipeline(
        "Windows VS2015 msvc-14.0",
        "cppalliance/dronevs2015",
        { TOOLSET: 'msvc-14.0', CXXSTD: '14,latest', B2_DONT_EMBED_MANIFEST: '1' },
    ),

    windows_pipeline(
        "Windows VS2017 msvc-14.1",
        "cppalliance/dronevs2017",
        { TOOLSET: 'msvc-14.1', CXXSTD: '14,17,latest' },
    ),

    windows_pipeline(
        "Windows VS2019 msvc-14.2",
        "cppalliance/dronevs2019",
        { TOOLSET: 'msvc-14.2', CXXSTD: '14,17,20,latest' },
    ),

    windows_pipeline(
        "Windows VS2022 msvc-14.3",
        "cppalliance/dronevs2022:1",
        { TOOLSET: 'msvc-14.3', CXXSTD: '14,17,20,latest' },
    ),
]
