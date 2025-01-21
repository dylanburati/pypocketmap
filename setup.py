import os
import platform
import sys
import tempfile
from setuptools import setup, find_packages
from setuptools.extension import Extension
from setuptools.command.build_ext import build_ext as BuildCommand

import distutils.log as logging

parent_dir = "pypocketmap"
if sys.platform == "darwin" and "APPVEYOR" in os.environ:
    os.environ["CC"] = "gcc-8"


def returns_success(compiler, src, extra_postargs):
    """Return a boolean indicating whether funcname is supported on
    the current platform.  The optional arguments can be used to
    augment the compilation environment.
    """
    fd, fname = tempfile.mkstemp(".c", "", text=True)
    f = os.fdopen(fd, "w")
    try:
        f.write(src)
    finally:
        f.close()
    try:
        objects = compiler.compile([fname], extra_postargs=extra_postargs)
    except Exception:
        return False
    finally:
        os.remove(fname)
    files = list(objects)
    try:
        compiler.link_executable(objects, "a.out", libraries=[], library_dirs=[])
        exc_file = os.path.join(compiler.output_dir or "", "a.out")
        files.append(exc_file)
        assert os.spawnv(os.P_WAIT, exc_file, []) == 0
    except Exception:
        return False
    finally:
        for fn in files:
            os.remove(fn)
    return True


class MyBuildCommand(BuildCommand):
    EXTRA_COMPILE_ARGS = {
        "msvc": (
            ["/O2"],
            {"x86_64": ["/arch:AVX2"]},
        ),
        "unix": (
            ["-O3", "-Wformat=0"],
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
        extra_c, per_platform = self.EXTRA_COMPILE_ARGS[ck]
        if ck == "unix" and "CI" not in os.environ:
            extra_c = [*extra_c, "-w"]
        extra_p = per_platform.get(family, [])
        if "-mavx2" in extra_p:
            src = """
            #include <immintrin.h>
            int main (int argc, char** argv) {
            __m256i a = _mm256_set_epi64x(0xffffffff00000000ULL, 0xffffffff00000000ULL,
                                          0xffffffff00000000ULL, 0xffffffff00000000ULL);
            __m256i b = _mm256_set_epi64x(0x00000000ffffffffULL, 0x00000000ffffffffULL,
                                          0x00000000ffffffffULL, 0x00000000ffffffffULL);
            return (int) (1 == _mm256_testz_si256(a, b));
            }
            """.strip()
            if not returns_success(self.compiler, src, ["-mavx2"]):
                self.announce("[setup.py] failed check: -mavx2", logging.ERROR)
                extra_p.remove("-mavx2")
        ext.extra_compile_args = extra_c + extra_p + (ext.extra_compile_args or [])
        self.announce(
            "[setup.py] compiler:{} compiler_family:{} plat_name:{} machine:{} machine_family:{} -> {!r}".format(
                self.compiler.compiler_type,
                ck,
                self.plat_name,
                pk,
                family,
                ext.extra_compile_args,
            ),
            logging.WARN,
        )
        super().build_extension(ext)


module_int64_int64 = Extension(
    "int64_int64",
    sources=[os.path.join(parent_dir, "int64_int64_Py.c")]
)
module_str_float32 = Extension(
    "str_float32",
    sources=[os.path.join(parent_dir, "str_float32_Py.c")]
)
module_str_float64 = Extension(
    "str_float64",
    sources=[os.path.join(parent_dir, "str_float64_Py.c")]
)
module_str_int32 = Extension(
    "str_int32",
    sources=[os.path.join(parent_dir, "str_int32_Py.c")]
)
module_str_int64 = Extension(
    "str_int64",
    sources=[os.path.join(parent_dir, "str_int64_Py.c")],
)
module_str_str = Extension(
    "str_str",
    sources=[os.path.join(parent_dir, "str_str_Py.c")],
)

with open("README.md") as fh:
    long_description = fh.read()

setup(
    name="pypocketmap",
    version="0.0.1-rc1",
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
            "optimization.h",
            "packed.h",
            "polymur-hash.h",
            "simd.h",
        ],
        "pypocketmap._pkt_c": [
            "__init__.py",
            "int64_int64.pyi",
            "str_float32.pyi",
            "str_float64.pyi",
            "str_int32.pyi",
            "str_int64.pyi",
            "str_str.pyi",
        ],
    },
    ext_modules=[
        module_int64_int64,
        module_str_float32,
        module_str_float64,
        module_str_int32,
        module_str_int64,
        module_str_str,
    ],
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
