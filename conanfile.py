from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class TurbotAIRecipe(ConanFile):
    name = "turbot-ai"
    version = "0.1.0"
    license = "MIT"
    author = "Turbot AI Team"
    url = "https://github.com/lmtdit/turbot-ai"
    description = "Turbot AI - A modern C++23 project"
    topics = ("ai", "cpp23", "turbot")
    settings = "os", "compiler", "build_type", "arch"
    package_type = "application"

    # Dependencies
    requirements = [
        "fmt/10.2.1",
        "spdlog/1.13.0",
        "nlohmann_json/3.11.3",
        "catch2/3.5.3",
        "boost/1.84.0",
        "openssl/3.2.1",
        "sqlite3/3.45.0",
        "libcurl/8.6.0",
    ]

    test_requires = [
        "catch2/3.5.3",
    ]

    # Layout
    def layout(self):
        cmake_layout(self)

    # Generate toolchain
    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    # Build
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    # Package
    def package(self):
        cmake = CMake(self)
        cmake.install()

    # Package info
    def package_info(self):
        self.cpp_info.libs = ["turbot-core", "turbot-network", "turbot-utils", "turbot-storage"]
