import os
import sys
from setuptools import setup, Extension, find_packages

parent_dir = "microdict"


if os.name != "nt":
    if sys.platform == "darwin" and "APPVEYOR" in os.environ:
        os.environ["CC"] = "gcc-8"

    module_str_int64 = Extension(
        "str_int64",
        sources=[os.path.join(parent_dir, "str_int64_Py.c")],
        extra_compile_args=["-O3", "-w"],
    )

    os.system("gcc -v")
else:
    # If windows:
    module_str_int64 = Extension(
        "str_int64",
        sources=[os.path.join(parent_dir, "str_int64_Py.c")],
        extra_compile_args=["/O2", "/w"],
    )

with open("README.md") as fh:
    long_description = fh.read()

setup(
    name="microdict",
    version="0.1.1",
    author="Dylan Burati, Touqir Sajed",
    description="The Microdict library - a high performance Python hashtable implementation",
    long_description=long_description,
    long_description_content_type="text/markdown",
    license="MIT",
    url="https://github.com/touqir14/Microdict",
    ext_package="_mdict_c",
    package_data={
        "microdict": [
            "__init__.py",
            "__init__.pyi",
            "abstract.h",
            "bits.h",
            "flags.h",
            "simd.h",
            "wyhash.h",
        ],
        "microdict._mdict_c": ["__init__.py", "str_int64.pyi"],
    },
    ext_modules=[module_str_int64],
    packages=find_packages(),
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: MacOS",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX :: Linux",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.5",
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
)
