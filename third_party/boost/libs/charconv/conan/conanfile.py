from conan import ConanFile
from conan.errors import (
    ConanException,
    ConanInvalidConfiguration,
)
from conan.tools.files import copy
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import (
    cmake_layout,
    CMake,
    CMakeDeps,
    CMakeToolchain,
)
from conan.tools.scm import Version

import os

required_conan_version = ">=1.53.0"

class CharconvConan(ConanFile):
    name = "boost_charconv"
    version = "1.1.1"
    description = "Boost provides free peer-reviewed portable C++ source libraries"
    url = "https://github.com/cppalliance/charconv"
    homepage = "https://github.com/cppalliance/charconv"
    license = "BSL-1.0"
    topics = ("decimal-binary", "conversion")

    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    requires = "boost/[>=1.82.0]"

    @property
    def _min_compiler_version_default_cxx11(self):
        # Minimum compiler version having c++ standard >= 11 by default
        return {
            "apple-clang": 99, # assume apple-clang will default to c++11 in the distant future
            "gcc": 6,
            "clang": 6,
            "Visual Studio": 14,  # guess
            "msvc": 190,  # guess
        }.get(str(self.settings.compiler))

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, 11)
        else:
            version_cxx11_standard_json = self._min_compiler_version_default_cxx11
            if not version_cxx11_standard_json:
                self.output.warning("Assuming the compiler supports c++11 by default")
            elif Version(self.settings.compiler.version) < version_cxx11_standard_json:
                raise ConanInvalidConfiguration("Boost.Charconv requires C++11")

    def layout(self):
        self.folders.root = ".."
        cmake_layout(self, build_folder="bin")

    def export_sources(self):
        src = os.path.join(self.recipe_folder, "..")
        copy(self, "CMakeLists.txt", src, self.export_sources_folder)
        copy(self, "LICENSE",        src, self.export_sources_folder)
        copy(self, "src*",           src, self.export_sources_folder)
        copy(self, "include*",       src, self.export_sources_folder)
        copy(self, "conan/*.cmake",  src, self.export_sources_folder)

    def generate(self):
        boost = self.dependencies["boost"]

        tc = CMakeToolchain(self)
        tc.variables["BOOST_SUPERPROJECT_VERSION"] = boost.ref.version
        tc.variables["CMAKE_PROJECT_boost_charconv_INCLUDE"] = os.path.join(
            self.source_folder, "conan", "targets.cmake")
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        libdir = os.path.join(self.package_folder, "lib")
        copy(self, "*.lib", self.build_folder, libdir, keep_path=False)
        copy(self, "*.so",  self.build_folder, libdir, keep_path=False)
        copy(self, "*.a",   self.build_folder, libdir, keep_path=False)

        copy(
            self,
            "*.dll",
            self.build_folder,
            os.path.join(self.package_folder, "bin"),
            keep_path=False,
        )

        copy(self, "include/*", self.source_folder, self.package_folder)

        copy(self, "LICENSE", self.source_folder, self.package_folder)

    def package_info(self):
        self.cpp_info.libs = ["boost_charconv"]
