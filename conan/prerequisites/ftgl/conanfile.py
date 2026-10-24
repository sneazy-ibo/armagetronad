from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.files import get
from conan.tools.env import VirtualBuildEnv
from conan.tools.scm import Git
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
    generators = "cmake"

    requires = \
            "freetype/2.9.1@bincrafters/stable"

    default_options = {
    }

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/ulrichard/ftgl.git", target=".")
        git.checkout("16f7667cbd4b1e988ae24128c5dea54479926f85")


    def configure(self):
        tc = CMake(self)
        return tc

    def build(self):
        tc = self.configure()
        tc.build()

    def package_info(self):
        self.cpp_info.libs = ["FTGL"]
        self.cpp_info.includedirs = ["include"]
        self.cpp_info.libdirs = ["lib"]
        self.cpp_info.set_property("pkg_config_name", "ftgl")

    def package(self):
        tc = self.configure()
        tc.install()

