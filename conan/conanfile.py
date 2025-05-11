from conan import ConanFile
from conan.tools.files import copy
from conan.tools.env import VirtualRunEnv
import os

# activate with
# conan install . --build=missing 

class Pkg(ConanFile):
    generators = \
            "AutotoolsToolchain", \
            "PkgConfigDeps"

    requires = \
            "libcurl/[>=7]", \
            "sdl_ttf/[>2.0]", \
            "sdl_image/[>2.0]", \
            "sdl_mixer/[>2.0]", \
            "glew/[>=2.2 <3.0]", \
            "protobuf/[>=2.4 <3.22]", \
            "freetype/[>2.0]", \
            "libxml2/[>=2.9.10]"

    default_options = {
        "sdl/*:x11": False,
        "sdl/*:pulseaudio": False,
        "sdl_mixer/*:flac": False,
        "sdl_mixer/*:opus": False,
        "sdl_mixer/*:modplug": False,
        "sdl_mixer/*:tinymidi": False,
        "sdl_image/*:xv": False,
        "sdl_image/*:lbm": False,
        "sdl_image/*:qoi": False,
        "sdl_image/*:svg": False,
        "sdl_image/*:tga": False,
        "sdl_image/*:xcf": False,
        "sdl_image/*:xpm": False,
        "sdl_image/*:with_libtiff": False,
        "sdl_image/*:with_libwebp": False,
        "libcurl/*:with_ssl": False,
        "libcurl/*:with_https": False,
        "libcurl/*:with_ftp": False,
        "libcurl/*:with_file": False,
        "libcurl/*:with_rtsp": False,
        "libcurl/*:with_dict": False,
        "libcurl/*:with_telnet": False,
        "libcurl/*:with_tftp": False,
        "libcurl/*:with_pop3": False,
        "libcurl/*:with_imap": False,
        "libcurl/*:with_smtp": False,
        "libcurl/*:with_gopher": False,
        "libcurl/*:shared": True,
        "libcurl/*:static": False,
        "libxml2/*:html": False,
        "libxml2/*:http": False,
        "libxml2/*:ftp": False,
        "libxml2/*:zlib": False,
        "libxml2/*:iconv": False,
        "libxml2/*:shared": True
    }

    keep_imports = True

    def requirements(self):
        # force resolving conflict between mixer and image
        self.requires("sdl/2.30.8", force=True)
        pass

    def generate(self):

        # copy libraries
        libs_path = os.path.join(self.build_folder, "lib")
        for dep_name, dep in self.dependencies.items():
            dirs = dep.cpp_info.libdirs + dep.cpp_info.bindirs
            for dir in dirs:
                for extension in [ "*.so.*", "*.dylib*", "*.dll" ]:
                    copy(self, extension, dir, libs_path)

        # modiy LD_LIBRARY_PATH
        run_env = VirtualRunEnv(self)
        run_env.environment().append_path("LD_LIBRARY_PATH", libs_path)
        run_env.generate()

