from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeToolchain
from conan.tools.scm import Git
from conan.tools.files import load, save
import os


##################
# FTGL           #
##################


# created for SDL_image after a suggestion by perplexity.ai

class FtglConan(ConanFile):
    name = "ftgl_aa"
    version = "2.3.0"
    description = "FTGL Font Rendering"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "library"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def requirements(self):
        self.requires("freetype/[>=2.10.4]")
        self.requires("opengl/system")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/ulrichard/ftgl.git", target=self.source_folder)
        git.checkout("16f7667cbd4b1e988ae24128c5dea54479926f85")
        # Fix CMake minimum version requirement
        cmake_file = os.path.join(self.source_folder, "CMakeLists.txt")
        content = load(self, cmake_file)
        content = content.replace("CMAKE_MINIMUM_REQUIRED(VERSION 2.8)", "CMAKE_MINIMUM_REQUIRED(VERSION 3.5)")
        save(self, cmake_file, content)
        # Fix unsigned char* to char* conversion issue in FTVectoriser.cpp
        vectoriser_file = os.path.join(self.source_folder, "src", "FTVectoriser.cpp")
        vcontent = load(self, vectoriser_file)
        vcontent = vcontent.replace(
            "char* tagList = &outline.tags[startIndex];",
            "char* tagList = const_cast<char*>(reinterpret_cast<const char*>(&outline.tags[startIndex]));"
        )
        save(self, vectoriser_file, vcontent)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["FTGL"]
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.set_property("pkg_config_name", "ftgl")

