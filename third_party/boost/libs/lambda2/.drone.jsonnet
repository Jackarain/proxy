# Copyright 2022 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

local library = "lambda2";

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
                'uname -a',
                'echo $DRONE_STAGE_MACHINE',
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
        "Linux 16.04 GCC 4.8",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-4.8', CXXSTD: '11' },
        "g++-4.8",
    ),

    linux_pipeline(
        "Linux 16.04 GCC 4.9",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-4.9', CXXSTD: '11' },
        "g++-4.9",
    ),

    linux_pipeline(
        "Linux 16.04 GCC 5*",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14' },
    ),

    linux_pipeline(
        "Linux 18.04 GCC 6",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'gcc', COMPILER: 'g++-6', CXXSTD: '11,14' },
        "g++-6",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* ARM64",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,2a' },
        arch="arm64",
    ),

    linux_pipeline(
        "Linux 20.04 GCC 9* S390x",
        "cppalliance/droneubuntu2004:multiarch",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,2a' },
        arch="s390x",
    ),

    linux_pipeline(
        "Linux 24.04 GCC 13* UBSAN",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,20,23,2c' } + ubsan,
    ),

    linux_pipeline(
        "Linux 24.04 GCC 13* ASAN",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'gcc', COMPILER: 'g++', CXXSTD: '11,14,17,20,23,2c' } + asan,
    ),

    linux_pipeline(
        "Linux 16.04 Clang 3.5",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-3.5', CXXSTD: '03,11' },
        "clang-3.5",
    ),

    linux_pipeline(
        "Linux 16.04 Clang 3.6",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-3.6', CXXSTD: '03,11,14' },
        "clang-3.6",
    ),

    linux_pipeline(
        "Linux 16.04 Clang 3.7",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-3.7', CXXSTD: '03,11,14' },
        "clang-3.7",
    ),

    linux_pipeline(
        "Linux 16.04 Clang 3.8",
        "cppalliance/droneubuntu1604:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-3.8', CXXSTD: '03,11,14' },
        "clang-3.8",
    ),

    linux_pipeline(
        "Linux 18.04 Clang 3.9",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-3.9', CXXSTD: '03,11,14' },
        "clang-3.9",
    ),

    linux_pipeline(
        "Linux 18.04 Clang 4.0",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-4.0', CXXSTD: '03,11,14' },
        "clang-4.0",
    ),

    linux_pipeline(
        "Linux 18.04 Clang 5.0",
        "cppalliance/droneubuntu1804:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-5.0', CXXSTD: '03,11,14' },
        "clang-5.0",
    ),

    linux_pipeline(
        "Linux 24.04 Clang 20 UBSAN",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-20', CXXSTD: '11,14,17,20,23,2c' } + ubsan,
        "clang-20",
    ),

    linux_pipeline(
        "Linux 24.04 Clang 20 ASAN",
        "cppalliance/droneubuntu2404:1",
        { TOOLSET: 'clang', COMPILER: 'clang++-20', CXXSTD: '11,14,17,20,23,2c' } + asan,
        "clang-20",
    ),

    macos_pipeline(
        "MacOS 10.15 Xcode 12.2",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '03,11,14,1z' },
    ),

    macos_pipeline(
        "MacOS 12.4 Xcode 13.2.1",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '03,11,14,17,20,2b' },
        xcode_version = "13.2.1", osx_version = "monterey", arch = "arm64",
    ),

    macos_pipeline(
        "MacOS 12.4 Xcode 13.4.1",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '03,11,14,17,20,2b' },
        xcode_version = "13.4.1", osx_version = "monterey", arch = "arm64",
    ),

    macos_pipeline(
        "MacOS 14 Xcode 16.2.0",
        { TOOLSET: 'clang', COMPILER: 'clang++', CXXSTD: '03,11,14,17,20,2b' },
        xcode_version = "16.2.0", osx_version = "sonoma", arch = "arm64",
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

    windows_pipeline(
        "Windows VS2026 msvc-14.5",
        "cppalliance/dronevs2026:1",
        { TOOLSET: 'msvc-14.5', CXXSTD: '14,17,20,latest' },
    ),
]
