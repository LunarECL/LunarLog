from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import copy
import os


class LunarLogConan(ConanFile):
    name = "lunarlog"
    version = "1.29.0"
    description = "Header-only C++11 structured logging with message templates"
    license = "MIT"
    url = "https://github.com/LunarECL/LunarLog"
    homepage = "https://github.com/LunarECL/LunarLog"
    topics = ("logging", "structured-logging", "header-only", "cpp11", "message-templates")
    package_type = "header-library"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    no_copy_source = True

    def export_sources(self):
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        copy(self, "include/*", self.recipe_folder, self.export_sources_folder)
        copy(self, "LICENSE", self.recipe_folder, self.export_sources_folder)

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure(variables={
            "LUNARLOG_BUILD_TESTS": "OFF",
            "LUNARLOG_BUILD_EXAMPLES": "OFF",
            "LUNARLOG_BUILD_BENCHMARKS": "OFF",
        })
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        copy(self, "*.hpp", os.path.join(self.source_folder, "include"),
             os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "LunarLog")
        self.cpp_info.set_property("cmake_target_name", "LunarLog::LunarLog")
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs.append("pthread")

    def package_id(self):
        self.info.clear()
