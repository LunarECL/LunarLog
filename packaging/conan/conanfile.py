from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get, copy
import os


class LunarLogConan(ConanFile):
    name = "lunarlog"
    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/LunarECL/LunarLog"
    description = "Header-only C++11 structured logging with message templates. Inspired by Serilog."
    topics = (
        "logging",
        "structured-logging",
        "header-only",
        "cpp11",
        "message-templates",
    )
    package_type = "header-library"
    settings = "os", "compiler", "build_type", "arch"
    no_copy_source = True

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def package(self):
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.source_folder, "include"),
            dst=os.path.join(self.package_folder, "include"),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "LunarLog")
        self.cpp_info.set_property("cmake_target_name", "LunarLog::LunarLog")
