# Use, modification, and distribution are
# subject to the Boost Software License, Version 1.0. (See accompanying
# file LICENSE.txt)
#
# Copyright Rene Rivera 2020.
# Copyright Alan de Freitas 2022.

# For Drone CI we use the Starlark scripting language to reduce duplication.
# As the yaml syntax for Drone CI is rather limited.
#
#

def main(ctx):
    return generate(
        # Compilers
        [
            'gcc >=4.8',
            'clang >=3.8',
            'msvc >=14.1',
            'arm64-gcc latest',
            's390x-gcc latest',
            # 'freebsd-gcc latest',
            'apple-clang *',
            'arm64-clang latest',
            's390x-clang latest',
            'freebsd-clang latest',
            'x86-msvc latest'
        ],
        # Standards
        '>=11', cache_dir='cache') + [
               linux_cxx("GCC 12 (no-mutex)", "g++-12", packages="g++-12", buildscript="drone", buildtype="boost",
                         image="cppalliance/droneubuntu2204:1",
                         environment={'B2_TOOLSET': 'gcc-12', 'B2_DEFINES': 'BOOST_URL_DISABLE_THREADS=1',
                                      'B2_CXXSTD': '17'}, globalenv={'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}),
           ]


# from https://github.com/boostorg/boost-ci
load("@boost_ci//ci/drone/:functions.star", "linux_cxx", "windows_cxx", "osx_cxx", "freebsd_cxx")


# Generate a list of jobs for the specified ranges of compiler versions
# compiler_ranges: The main parameter is the compiler ranges, including the range of versions we want to test
#                  for each compiler
# cxx_range: range of C++ standards to be tested with each compiler
# max_cxx: maximum number of C++ standards to be tested with each compiler. The `max_cxx` most recent
#          supported standards will be tested for each
# coverage: whether we should create an extra special coverage job
# docs: whether we should create an extra special docs job
# asan: whether we should create an extra special asan job
# tsan: whether we should create an extra special tsan job
# ubsan: whether we should create an extra special ubsan job
# cmake: whether we should create an extra special cmake job
def generate(compiler_ranges, cxx_range, max_cxx=2, coverage=True, docs=True, asan=True, tsan=False, ubsan=True,
             cmake=True, cache_dir=None, packages=[]):
    # swap: `packages` variable will be reused
    packages_requested = list(packages)
    packages = []

    # Get compiler versions we should test
    # Each compiler is a tuple (represented as a list) of:
    # - compiler name
    # - compiler version
    # - job type (one of 'boost', 'codecov', 'cmake-install', 'cmake', 'asan', 'tsan', 'ubsan', 'docs')
    compilers = []
    latest_compilers = []
    oldest_compilers = []
    for compiler_range in compiler_ranges:
        start = len(compilers)
        latest = None
        oldest = None
        for [compiler, version_str] in compilers_in_range(compiler_range):
            desc = [compiler, version_str, 'boost']
            compilers.append(desc)
            if latest == None:
                latest = version_str
            else:
                version = parse_semver(version_str)
                if version_match(version, parse_semver_range('>' + latest)):
                    latest = version_str
            if oldest == None:
                oldest = version_str
            else:
                version = parse_semver(version_str)
                if version_match(version, parse_semver_range('<' + oldest)):
                    oldest = version_str
        if oldest == latest:
            oldest = None
        if latest != None:
            for i in range(start, len(compilers)):
                if compilers[i][1] == latest:
                    latest_compilers.append(compilers[i])
                    compilers = compilers[:i] + compilers[i + 1:]
                    break
        if oldest != None:
            for i in range(start, len(compilers)):
                if compilers[i][1] == oldest:
                    oldest_compilers.append(compilers[i])
                    compilers = compilers[:i] + compilers[i + 1:]
                    break

    # Choose compilers for special job types
    latest_gcc = None
    latest_apple_clang = None
    for desc in latest_compilers:
        [compiler, version_str, type] = desc
        if compiler == 'gcc':
            latest_gcc = desc
        if compiler == 'apple-clang':
            latest_apple_clang = desc

    # Create special job types
    if latest_gcc != None:
        if coverage:
            coverage_desc = latest_gcc[:]
            coverage_desc[2] = 'codecov'
            latest_compilers = [coverage_desc] + latest_compilers
        if cmake:
            # cmake_desc = latest_gcc[:]
            # cmake_desc[2] = 'cmake-install'
            # compilers = [cmake_desc] + compilers

            cmake_desc = latest_gcc[:]
            cmake_desc[2] = 'cmake'
            compilers = [cmake_desc] + compilers
        if asan:
            asan_desc = latest_gcc[:]
            asan_desc[2] = 'asan'
            compilers = [asan_desc] + compilers
        if tsan:
            tsan_desc = latest_gcc[:]
            tsan_desc[2] = 'tsan'
            compilers = [tsan_desc] + compilers
        if ubsan:
            ubsan_desc = latest_gcc[:]
            ubsan_desc[2] = 'ubsan'
            compilers = [ubsan_desc] + compilers
        if docs:
            docs_desc = latest_gcc[:]
            docs_desc[2] = 'docs'
            compilers = [docs_desc] + compilers

    if latest_apple_clang != None:
        if asan:
            asan_desc = latest_apple_clang[:]
            asan_desc[2] = 'asan'
            compilers = [asan_desc] + compilers
        if tsan:
            tsan_desc = latest_apple_clang[:]
            tsan_desc[2] = 'tsan'
            compilers = [tsan_desc] + compilers
        if ubsan:
            ubsan_desc = latest_apple_clang[:]
            ubsan_desc[2] = 'ubsan'
            compilers = [ubsan_desc] + compilers

    compilers = latest_compilers + oldest_compilers + compilers

    # Standards we should test whenever possible
    cxxs = cxxs_in_range(cxx_range)

    # Create job descriptions
    jobs = []
    images_used = []
    for desc in compilers:
        # Parse version
        [compiler, version_str, type] = desc
        version = parse_semver(version_str)

        # The standards this compiler supports
        compiler_cxxs = []
        for cxx in cxxs:
            if compiler_supports(compiler, version, cxx):
                compiler_cxxs.append(cxx)
        if not compiler_cxxs:
            continue

        # Limit the number of standards we should test
        if max_cxx != None and len(compiler_cxxs) > max_cxx:
            compiler_cxxs = compiler_cxxs[-max_cxx:]

        # The packages required
        packages = []
        if type == 'docs':
            packages = ['docbook', 'docbook-xml', 'docbook-xsl', 'xsltproc', 'libsaxonhe-java', 'default-jre-headless',
                        'flex', 'libfl-dev', 'bison', 'unzip', 'rsync']
        elif compiler.endswith('gcc'):
            if version_str == '8.0.1':
                # The image supports 8.0.1 and 8.4 via apt-get
                # Explicitly testing 8.0.1 is useful since gcc >=8 <8.4 contains a bug regarding ambiguity in implicit
                # conversions to boost::core::string_view in C++17 mode.
                packages = ['--allow-downgrades', 'libmpx2=8-20180414-1ubuntu2', 'libgcc-8-dev=8-20180414-1ubuntu2',
                            'cpp-8=8-20180414-1ubuntu2',
                            'gcc-8-base=8-20180414-1ubuntu2', 'gcc-8=8-20180414-1ubuntu2', 'g++-8=8-20180414-1ubuntu2',
                            'libstdc++-8-dev=8-20180414-1ubuntu2']
            elif version[0] >= 5:
                packages = ['g++-' + str(version[0])]
            else:
                packages = ['g++-' + str(version[0]) + '.' + str(version[1])]
        elif compiler.endswith('clang') and compiler != 'apple-clang':
            if version[0] >= 7:
                packages = ['clang-' + str(version[0])]
            else:
                packages = ['clang-' + str(version[0]) + '.' + str(version[1])]
            if version[0] >= 13:
                packages.append('libstdc++-10-dev')
            elif version[0] >= 9:
                packages.append('libstdc++-9-dev')
            elif version[0] >= 6:
                packages.append('libstdc++-8-dev')
            elif version[0] >= 5:
                packages.append('libstdc++-7-dev')
            elif version[0] >= 4:
                packages.append('libstdc++-6-dev')

            if version[0] >= 6 and version[0] <= 7:
                packages.append('libc6-dbg')
                packages.append('libc++-dev')
            if version[0] == 6:
                packages.append('libc++abi-dev')

        if packages_requested:
            packages.extend(packages_requested)

        # Create uppercase compiler name
        uccompiler_names = {
            'gcc': 'GCC',
            's390x-gcc': 'S390x-GCC',
            'arm64-gcc': 'ARM64-GCC',
            'freebsd-gcc': 'FreeBSD-GCC',
            'clang': 'Clang',
            'apple-clang': 'Apple-Clang',
            's390x-clang': 'S390x-Clang',
            'arm64-clang': 'ARM64-Clang',
            'freebsd-clang': 'FreeBSD-Clang',
            'msvc': 'MSVC (x64)',
            'x86-msvc': 'MSVC (x86)'
        }

        uccompiler = uccompiler_names[compiler] if compiler in uccompiler_names else compiler

        # Create job name
        name = uccompiler + ' ' + version_str
        if desc in latest_compilers and type != 'codecov':
            name += ' (Latest)'
        elif desc in oldest_compilers:
            name += ' (Oldest)'
        name += ': C++'
        if len(compiler_cxxs) == 1:
            name += compiler_cxxs[0]
        elif len(compiler_cxxs) > 1:
            name += compiler_cxxs[0] + ' - C++' + compiler_cxxs[-1]

        if type == 'docs':
            name = 'Docs'
        elif type == 'codecov':
            name = 'Coverage (' + name + ')'
        elif type == 'asan':
            name = 'ASan (' + name + ')'
        elif type == 'tsan':
            name = 'TSan (' + name + ')'
        elif type == 'ubsan':
            name = 'UBSan (' + name + ')'
        elif type == 'cmake':
            name = 'CMake (' + name + ')'
        elif type == 'cmake-install':
            name = 'CMake Install (' + name + ')'

        # Compiler executable
        compiler_exec = ''
        if type == 'docs' or compiler == 'apple-clang':
            compiler_exec = 'g++'
        elif compiler.endswith('gcc'):
            if version[0] >= 5:
                compiler_exec = 'g++-' + str(version[0])
            else:
                compiler_exec = 'g++-' + str(version[0]) + '.' + str(version[1])
        elif compiler.endswith('clang'):
            if version[0] >= 7:
                compiler_exec = 'clang++-' + str(version[0])
            else:
                compiler_exec = 'clang++-' + str(version[0]) + '.' + str(version[1])

        # environment['COMMENT']
        environment = {}
        if type == 'codecov':
            environment['COMMENT'] = 'codecov.io'
            environment['LCOV_BRANCH_COVERAGE'] = '0'
        elif type in ['asan', 'tsan', 'ubsan', 'docs']:
            environment['COMMENT'] = type

        # environment['B2_VARIANT']
        if type in ['asan', 'tsan', 'ubsan']:
            environment['B2_VARIANT'] = 'debug'

        # environment['B2_ASAN']
        privileged = None
        if type == 'asan':
            environment['B2_ASAN'] = '1'
            environment['DRONE_EXTRA_PRIVILEGED'] = 'True'
            privileged = True

        # environment['B2_TSAN']
        if type == 'tsan':
            environment['B2_TSAN'] = '1'

        # environment['B2_UBSAN']
        if type == 'ubsan':
            environment['B2_UBSAN'] = '1'
            if compiler.endswith('gcc'):
                environment['B2_LINKFLAGS'] = '-fuse-ld=gold'

        # environment['B2_CXXSTD']
        if type not in ['docs', 'cmake', 'cmake-install']:
            environment['B2_CXXSTD'] = ','.join(compiler_cxxs)
        elif type == 'codecov':
            environment['B2_CXXSTD'] = compiler_cxxs[0]

        # environment['B2_TOOLSET']
        if compiler.endswith('gcc'):
            if version[0] >= 5:
                environment['B2_TOOLSET'] = 'gcc-' + str(version[0])
            else:
                environment['B2_TOOLSET'] = 'gcc-' + str(version[0]) + '.' + str(version[1])
        elif compiler == 'apple-clang':
            environment['B2_TOOLSET'] = 'clang'
        elif compiler.endswith('clang'):
            if version[0] >= 7:
                environment['B2_TOOLSET'] = 'clang-' + str(version[0])
            else:
                environment['B2_TOOLSET'] = 'clang-' + str(version[0]) + '.' + str(version[1])
        elif compiler.endswith('msvc'):
            environment['B2_TOOLSET'] = 'msvc-' + str(version[0]) + '.' + str(version[1])
            if compiler == 'x86-msvc':
                environment['B2_ADDRESS_MODEL'] = '32'

        # environment['B2_DEFINES']
        if type in ['codecov', 'asan', 'tsan', 'ubsan']:
            environment['B2_DEFINES'] = 'BOOST_NO_STRESS_TEST=1'

        # environment['CODECOV_TOKEN']
        if type == 'codecov':
            environment['CODECOV_TOKEN'] = {"from_secret": "codecov_token"}
            environment['COVERALLS_REPO_TOKEN'] = {"from_secret": "coveralls_repo_token"}

        # environment['CMAKE_INSTALL_TEST']
        if type == 'cmake-install':
            environment['CMAKE_INSTALL_TEST'] = '1'

        # environment['B2_LINKFLAGS']. switch gcc11 to a var?
        if compiler == 'freebsd-gcc':
            environment['B2_LINKFLAGS'] = '-Wl,-rpath=/usr/local/lib/gcc11'

        # Build type in script
        buildtype = type
        if type in ['asan', 'tsan', 'ubsan']:
            buildtype = 'boost'
        elif type == 'cmake':
            buildtype = 'cmake-superproject'
        elif type == 'cmake-install':
            buildtype = 'cmake-install'

        # Image to run the job
        image = None
        if type == 'docs':
            image = 'cppalliance/droneubuntu1804:1'
        elif compiler == 'gcc':
            if version[0] >= 12:
                image = 'cppalliance/droneubuntu2204:1'
            elif version[0] >= 5:
                image = 'cppalliance/droneubuntu1804:1'
            else:
                image = 'cppalliance/droneubuntu1604:1'
        elif compiler in ['s390x-gcc', 's390x-clang']:
            image = 'cppalliance/droneubuntu2204:multiarch'
        elif compiler in ['arm64-gcc', 'arm64-clang']:
            image = 'cppalliance/droneubuntu2004:multiarch'
        elif compiler == 'clang':
            if version[0] >= 13:
                image = 'cppalliance/droneubuntu2204:1'
            elif version[0] >= 10:
                image = 'cppalliance/droneubuntu2004:1'
            elif version[0] >= 4:
                image = 'cppalliance/droneubuntu1804:1'
            else:
                image = 'cppalliance/droneubuntu1604:1'
        elif compiler.endswith('msvc'):
            if version_match(version_str, '>=14.3'):
                image = 'cppalliance/dronevs2022'
            elif version_match(version_str, '>=14.2'):
                image = 'cppalliance/dronevs2019'
            elif version_match(version_str, '>=14.1'):
                image = 'cppalliance/dronevs2017'
            else:
                image = 'cppalliance/dronevs2015'

        # llvm
        llvm_os = None
        llvm_ver = None
        if compiler.endswith('clang'):
            llvm_ver = version_str
            if version[0] == 4:
                llvm_os = 'xenial'
            elif image == 'cppalliance/droneubuntu1804:1':
                llvm_os = 'bionic'
            elif image in ['cppalliance/droneubuntu2004:1', 'cppalliance/droneubuntu2004:multiarch']:
                llvm_os = 'focal'
            elif image == 'cppalliance/droneubuntu2204:1':
                llvm_os = 'jammy'

        if cache_dir != None and image_supports_caching(image, compiler):
            environment['drone_cache_mount'] = cache_dir
            if image in images_used or buildtype != "boost":
                environment['drone_cache_rebuild'] = 'false'
            else:
                environment['drone_cache_rebuild'] = 'true'
                images_used += [image]

        # arch
        arch = None
        if compiler.startswith('arm64-'):
            arch = 'arm64'
        elif compiler.startswith('s390x-'):
            arch = 's390x'

        # Append job
        globalenv = {'B2_CI_VERSION': '1', 'B2_VARIANT': 'release'}
        if compiler.endswith('msvc'):
            jobs.append(
                windows_cxx(
                    name,
                    '',
                    image=image,
                    buildtype=buildtype,
                    buildscript="drone",
                    environment=environment,
                    globalenv=globalenv))
        elif compiler == 'apple-clang':
            jobs.append(
                osx_cxx(
                    name,
                    compiler_exec,
                    packages=' '.join(packages),
                    buildscript="drone",
                    buildtype=buildtype,
                    xcode_version="13.4.1",
                    environment=environment,
                    globalenv=globalenv))
        elif compiler.startswith('freebsd'):
            jobs.append(
                freebsd_cxx(
                    name,
                    compiler_exec,
                    buildscript="drone",
                    buildtype=buildtype,
                    freebsd_version="13.1",
                    environment=environment,
                    globalenv=globalenv))
        else:
            jobs.append(
                linux_cxx(
                    name,
                    compiler_exec,
                    packages=' '.join(packages),
                    llvm_os=llvm_os,
                    llvm_ver=llvm_ver,
                    buildscript="drone",
                    buildtype=buildtype,
                    image=image,
                    environment=environment,
                    arch=arch,
                    globalenv=globalenv,
                    privileged=privileged))

    return jobs


# Returns whether the specified compiler/version support a given standard
# Ex: Checking if GCC 14 supports C++11
# - compiler_supports('gcc', '14', '11') -> True
def compiler_supports(compiler, version, cxx):
    if compiler.endswith('gcc'):
        return \
            (cxx == '20' and version_match(version, '>=11')) or \
            (cxx == '17' and version_match(version, '>=7')) or \
            (cxx == '14' and version_match(version, '>=6')) or \
            (cxx == '11' and version_match(version, '>=4')) or \
            (cxx == '03' or cxx == '98')
    elif compiler == 'apple-clang':
        return cxx == '11' or cxx == '14' or cxx == '17'
    elif compiler.endswith('clang'):
        return \
            (cxx == '20' and version_match(version, '>=12')) or \
            (cxx == '17' and version_match(version, '>=6')) or \
            (cxx == '14' and version_match(version, '>=4')) or \
            (cxx == '11' and version_match(version, '>=3')) or \
            (cxx == '03' or cxx == '98')
    elif compiler.endswith('msvc'):
        return \
            (cxx == '20' and version_match(version, '>=14.3')) or \
            (cxx == '17' and version_match(version, '>=14.1')) or \
            (cxx == '14' and version_match(version, '>=14.1')) or \
            (cxx == '11' and version_match(version, '>=14.1')) or \
            (cxx == '03' or cxx == '98')
    return False


# Check if the image supports caching
# This is based on an exclude-list since we want to assume
# new images will support caching
def image_supports_caching(image_str, compiler_str):
    return image_str != None and not (image_str[:19] == 'cppalliance/dronevs' or compiler_str[:6] == 's390x-')

# Get list of available compiler versions in a semver range
# - compilers_in_range('gcc >=10') -> [('gcc', '12'), ('gcc', '11'), ('gcc', '10')]
def compilers_in_range(compiler_range_str):
    supported = {
        'gcc': ['12', '11', '10', '9', '8.4', '7', '6', '5', '4.9', '4.8'],
        's390x-gcc': ['12'],
        'arm64-gcc': ['11'],
        'freebsd-gcc': ['11', '10', '9', '8'],
        'clang': ['15', '14', '13', '12', '11', '10', '9', '8', '7', '6.0', '5.0', '4.0', '3.8'],
        'apple-clang': ['13.4.1'],
        's390x-clang': ['14'],
        'arm64-clang': ['12'],
        'freebsd-clang': ['15', '14', '13', '12', '11', '10'],
        'msvc': ['14.3', '14.2', '14.1'],
        'x86-msvc': ['14.1'],
    }
    # Split compiler and semver version
    parts = compiler_range_str.split(' ', 1)
    name = parts[0]
    semver_requirements_str = parts[1].strip()

    # The shortcut "<compiler-name> latest" is also supported
    if semver_requirements_str == 'latest':
        return [(name, supported[name][0])]

    # Create list of available compilers in the range
    compilers = []
    semver_requirements = parse_semver_range(semver_requirements_str)
    for supported_version_str in supported[name]:
        if version_match(supported_version_str, semver_requirements):
            compilers.append((name, supported_version_str))
    return compilers


# Get the C++ standard versions in a range of versions
# Each C++ standard version can be represented by the two or
# four char year version.
# - cxxs_in_range('>=98 <=11') -> ['98', '03', '11']
# - cxxs_in_range('>=1998 <=2011') -> ['98', '03', '11']
def cxxs_in_range(ranges):
    supported_cxx = [1998, 2003, 2011, 2014, 2017, 2020]
    cxxs = []

    # Replace with 4 digit major so semver makes sense
    if type(ranges) == "string":
        ranges = parse_semver_range(ranges)
    disjunction = ranges

    for conjunction in disjunction:
        for comparator in conjunction:
            if comparator[1] < 98:
                comparator[1] += 2000
            elif comparator[1] < 100:
                comparator[1] += 1900

    # Extract matching versions as 2 digit string
    for v in supported_cxx:
        if version_match((v, 0, 0), disjunction):
            cxxs.append(str(v)[-2:])
    return cxxs


# Check if a semver range contains a version
# Each parameter can be a string or a version or range that has already been parsed.
# - version_match('1.2.4', '>1.2.3') -> True
# - version_match('1.0.0', '>1.2.3 || <0.1.2') -> True
# - version_match('0.0.0', '>1.2.3 <2.0.0 || <0.1.2') -> True
def version_match(v, ranges):
    if type(v) == "string":
        v = parse_semver(v)
    if type(ranges) == "string":
        ranges = parse_semver_range(ranges)
    disjunction = ranges
    any_in_disjunction = False
    for conjunction in disjunction:
        all_in_conjunction = True
        for comparator in conjunction:
            match = False
            op = comparator[0]
            if op.startswith('>'):
                if v[0] > comparator[1]:
                    match = True
                elif v[0] < comparator[1]:
                    match = False
                elif v[1] > comparator[2]:
                    match = True
                elif v[1] < comparator[2]:
                    match = False
                elif v[2] > comparator[3]:
                    match = True
                else:
                    match = op == '>=' and v[2] == comparator[3]
            elif op.startswith('<'):
                if v[0] < comparator[1]:
                    match = True
                elif v[0] > comparator[1]:
                    match = False
                elif v[1] < comparator[2]:
                    match = True
                elif v[1] > comparator[2]:
                    match = False
                elif v[2] < comparator[3]:
                    match = True
                else:
                    match = op == '<=' and v[2] == comparator[3]
            if op == '=':
                match = v[0:2] == comparator[1:3]
            if not match:
                all_in_conjunction = False
                break
        if all_in_conjunction:
            any_in_disjunction = True
            break
    return any_in_disjunction


# Parse a semver range
# A semver range contains one or more semver comparators.
# Each comparator can have one of the following operators:
# - *: all versions
# - =, >, <, >=, <=: version comparison
# - ^: any compatible version (greater than X and less than next major release)
# - ~: any patch-level change (greater than X and less than next minor release)
# - ~: any patch-level change (greater than X and less than next minor release)
# - -: the operator "-" is not supported (replace "X - Y" with ">=X <=Y")
# Disjunctions are separated by "||" and each disjunction
# contains a conjunction of one or more predicates.
# The result type is a list of disjunctions of conjunctions.
# Ex:
# - parse_semver_range('>1.2.3') -> [[['>', 1, 2, 3]]]
# - parse_semver_range('>1.2.3 || <0.1.2') -> [[['>', 1, 2, 3]], [['<', 0, 1, 2]]]
# - parse_semver_range('>1.2.3 <2.0.0 || <0.1.2') -> [[['>', 1, 2, 3], ['<', 2, 0, 0]], [['<', 0, 1, 2]]]
def parse_semver_range(range_strs):
    # req :== range * ( "||" range )
    # range :== ( compiler " " op semver ) | "*"
    # op :== '>' | '<' | '>=' | '<=' | '=' | '^'
    range_strs = range_strs.strip()
    ranges = []
    disjunction_str = range_strs.split('||')
    for conjunction_str in disjunction_str:
        ranges.append([])
        for comparator_str in conjunction_str.split(' '):
            # "*" is special in that it's not followed by a version
            if comparator_str == '*':
                ranges[-1].append(('>=', 0, 0, 0))
                continue
            # split operator from semver string
            op = '='
            if comparator_str[:2] in ['>=', '<=']:
                op = comparator_str[:2]
                version_str = comparator_str[2:].strip()
            elif comparator_str[0] in ['>', '<', '=', '^', '~']:
                op = comparator_str[0]
                version_str = comparator_str[1:].strip()
            else:
                version_str = comparator_str
            # semver = major "." minor "." patch
            # prerelease tags are not supported
            [major, minor, patch] = parse_semver(version_str)
            # store the components of the conjunction
            if op in ['>=', '<=', '>', '<', '=']:
                ranges[-1].append([op, major, minor, patch])
            elif op == '^':
                if major > 0:
                    ranges[-1].append(['>=', major, minor, patch])
                    ranges[-1].append(['<', major + 1, 0, 0])
                else:
                    ranges[-1].append(['>=', 0, minor, patch])
                    ranges[-1].append(['<', 0, minor + 1, 0])
            elif op == '~':
                minor_is_defined = version_str.find('.') != -1
                if minor_is_defined:
                    ranges[-1].append(['>=', major, minor, patch])
                    ranges[-1].append(['<', major, minor + 1, 0])
                else:
                    ranges[-1].append(['>=', major, minor, patch])
                    ranges[-1].append(['<', major + 1, 0, 0])
    return ranges


# Parse a semver string
# Ex:
# - parse_semver('1.2.3') -> [1, 2, 3]
# - parse_semver('1')     -> [1, 0, 0]
def parse_semver(version_str):
    version_parts = version_str.split('.')
    major = int(version_parts[0]) if len(version_parts) > 0 else 0
    minor = int(version_parts[1]) if len(version_parts) > 1 else 0
    patch = int(version_parts[2]) if len(version_parts) > 2 else 0
    return [major, minor, patch]
