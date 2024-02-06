import os
import platform
import sys
from setuptools import setup, find_packages
from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext as BuildCommand

import distutils.log as logging

parent_dir = "pypocketmap"
if sys.platform == "darwin" and "APPVEYOR" in os.environ:
    os.environ["CC"] = "gcc-8"


class MyBuildCommand(BuildCommand):
    EXTRA_COMPILE_ARGS = {
        "msvc": (
            ["/O2", "/w"],
            {"x86_64": ["/arch:AVX2"]},
        ),
        "unix": (
            ["-O3", "-w"],
            # arm64 intentionally excluded, NEON is a default feature according to
            # https://github.com/numpy/numpy/blob/v1.26.4/meson_cpu/arm/meson.build#L23-L26
            {"x86_64": ["-mavx2"], "arm32": ["-mfpu=neon"]},
        ),
    }

    def build_extension(self, ext):
        ck = self.compiler.compiler_type
        if ck not in self.EXTRA_COMPILE_ARGS:
            ck = "unix"
        pk = platform.machine()
        family = pk.lower()
        if family == "amd64":
            family = "x86_64"
        elif family.startswith("arm") or family.startswith("aarch"):
            family = "arm64" if ("64" in family) else "arm32"
        if family == "x86_64" and self.plat_name and "32" in self.plat_name:
            family = "i386"
        extra_c, extra_p = self.EXTRA_COMPILE_ARGS[ck]
        ext.extra_compile_args = (
            extra_c + extra_p.get(family, []) + (ext.extra_compile_args or [])
        )
        self.announce(
            "compiler:{} compiler_family:{} plat_name:{} machine:{} machine_family:{} -> {!r}".format(
                self.compiler.compiler_type, ck, self.plat_name, pk, family, ext.extra_compile_args
            ),
            logging.WARN,
        )
        super().build_extension(ext)


module_str_int64 = Extension(
    "str_int64",
    sources=[os.path.join(parent_dir, "str_int64_Py.c")],
)

with open("README.md") as fh:
    long_description = fh.read()

setup(
    name="pypocketmap",
    version="0.0.0-alpha",
    author="Dylan Burati, Touqir Sajed",
    description="pypocketmap - a memory-efficient hashtable for CPython",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="MIT, Apache 2.0",
    url="https://github.com/dylanburati/pypocketmap",
    ext_package="_pkt_c",
    package_data={
        "pypocketmap": [
            "__init__.py",
            "__init__.pyi",
            "abstract.h",
            "bits.h",
            "flags.h",
            "simd.h",
            "wyhash.h",
        ],
        "pypocketmap._pkt_c": ["__init__.py", "str_int64.pyi"],
    },
    ext_modules=[module_str_int64],
    packages=find_packages(),
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: C",
        "Topic :: Software Development",
        "Topic :: Utilities",
        "Topic :: Software Development :: Libraries",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    cmdclass={"build_ext": MyBuildCommand},
)
