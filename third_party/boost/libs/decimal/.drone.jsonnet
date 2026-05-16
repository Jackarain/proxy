# Copyright 2022, 2023 Peter Dimov
# Copyright 2024, 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

local library = "decimal";

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
    "clone": {
       "retries": 5
    },
    steps:
    [
        {
            name: "everything",
            image: image,
            privileged: true,
            environment: environment,
            commands:
            [
                'set -e',
                'echo $DRONE_STAGE_MACHINE',
                'uname -a',
                'curl -sSL --retry 5 https://apt.llvm.org/llvm-snapshot.gpg.key | sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/llvm-snapshot.gpg',
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
                'echo $DRONE_STAGE_MACHINE',
                'sw_vers',
                'uname -a',
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
                'echo $env:DRONE_STAGE_MACHINE',
                'cmd /C .drone\\\\drone.bat ' + library,
            ]
        }
    ]
};

[
    linux_pipeline(
        "Linux 18.04 GCC 8 32/64",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-8', CXXSTD: '03,11,14,17', ADDRMD: '32,64' },
        "g++-8-multilib",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* 32/64",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '03,11,14,17,2a', ADDRMD: '32,64' },
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* ARM64",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '03,11,14,17,2a' },
        arch="arm64",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* ARM64 - ASAN",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '03,11,14,17,2a' } + asan,
        arch="arm64",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* S390x",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '03,11,14,17,2a' },
        arch="s390x",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 10 32/64",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-10', CXXSTD: '03,11,14,17,20', ADDRMD: '32,64' },
        "g++-10-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 11* 32/64",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '03,11,14,17,2a', ADDRMD: '32,64' },
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 03",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '03', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 11",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '11', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 14",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '14', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 17",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '17', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 20",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '20', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 32 ASAN 2b",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '2b', ADDRMD: '32' } + asan,
        "g++-12-multilib",
    ),

    linux_pipeline(
        "Linux 22.04 GCC 12 64 ASAN",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-12', CXXSTD: '14,20', ADDRMD: '64' } + asan,
        "g++-12-multilib",
    ),
    
    linux_pipeline(
        "Linux 24.04 GCC 13 32",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '03,11,14,17,20,23', ADDRMD: '32', CXXFLAGS: "-fexcess-precision=fast" },
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 13 64",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '03,11,14,17,20,23', ADDRMD: '64', CXXFLAGS: "-fexcess-precision=fast" },
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 13 GNU 32",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '03,11,14,17,20,23', ADDRMD: '32', CXXFLAGS: "-fexcess-precision=fast", CXXSTDDIALECT: "gnu" },
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 13 GNU 64",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-13', CXXSTD: '03,11,14,17,20,23', ADDRMD: '64', CXXFLAGS: "-fexcess-precision=fast", CXXSTDDIALECT: "gnu" },
        "g++-13-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 14 32",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-14', CXXSTD: '03,11,14,17,20,23', ADDRMD: '32', CXXFLAGS: "-fexcess-precision=fast" },
        "g++-14-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 14 64",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-14', CXXSTD: '03,11,14,17,20,23', ADDRMD: '64', CXXFLAGS: "-fexcess-precision=fast" },
        "g++-14-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 14 GNU 32",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-14', CXXSTD: '03,11,14,17,20,23', ADDRMD: '32', CXXFLAGS: "-fexcess-precision=fast", CXXSTDDIALECT: "gnu" },
        "g++-14-multilib",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 14 GNU 64",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-14', CXXSTD: '03,11,14,17,20,23', ADDRMD: '64', CXXFLAGS: "-fexcess-precision=fast", CXXSTDDIALECT: "gnu" },
        "g++-14-multilib",
    ),

    linux_pipeline(
        "Linux 18.04 Clang 6.0",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-6.0', CXXSTD: '03,11,14,17' },
        "clang-6.0",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 7",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-7', CXXSTD: '03,11,14,17' },
        "clang-7",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 8",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-8', CXXSTD: '03,11,14,17' },
        "clang-8",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 9",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-9', CXXSTD: '03,11,14,17,2a' },
        "clang-9",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 10",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-10', CXXSTD: '03,11,14,17,2a' },
        "clang-10",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 11",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-11', CXXSTD: '03,11,14,17,2a' },
        "clang-11",
    ),

    linux_pipeline(
        "Linux 20.04 Clang 12",
        "cppalliance/droneubuntu2004:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-12', CXXSTD: '03,11,14,17,2a' },
        "clang-12",
    ),

    linux_pipeline(
        "Linux 22.04 Clang 13",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-13', CXXSTD: '03,11,14,17,20' },
        "clang-13",
    ),

    linux_pipeline(
        "Linux 22.04 Clang 14 UBSAN",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-14', CXXSTD: '03,11,14,17,20,2b' } + ubsan,
        "clang-14",
    ),

    linux_pipeline(
        "Linux 22.04 Clang 14 ASAN",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-14', CXXSTD: '03,11,14,17,20,2b' } + asan,
        "clang-14",
    ),

    linux_pipeline(
        "Linux 22.04 Clang 15",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-15', CXXSTD: '03,11,14,17,20,2b' },
        "clang-15",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-15 main"],
    ),

    linux_pipeline(
        "Linux 22.04 Clang 16",
        "cppalliance/droneubuntu2204:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-16', CXXSTD: '03,11,14,17,20,2b' },
        "clang-16",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-16 main"],
    ),

    linux_pipeline(
        "Linux 24.04 Clang 17",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-17', CXXSTD: '03,11,14,17,20,2b' },
        "clang-17",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-17 main"],
    ),

    linux_pipeline(
        "Linux 24.04 Clang 18",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-18', CXXSTD: '03,11,14,17,20,2b' },
        "clang-18",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main"],
    ),

    linux_pipeline(
        "Linux 24.04 Clang 19",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-19', CXXSTD: '03,11,14,17,20,2b' },
        "clang-19",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-19 main"],
    ),

    linux_pipeline(
        "Linux 24.04 Clang 20",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-20', CXXSTD: '03,11,14,17,20,2b' },
        "clang-20",
        ["deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-20 main"],
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
