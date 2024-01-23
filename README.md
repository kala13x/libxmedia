[![MIT License](https://img.shields.io/badge/License-MIT-brightgreen.svg?)](https://github.com/kala13x/libxmedia/blob/main/LICENSE)
[![C/C++ CI](https://github.com/kala13x/libxmedia/actions/workflows/make.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions/workflows/make.yml)
[![CMake](https://github.com/kala13x/libxmedia/actions/workflows/cmake.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions)
[![SMake](https://github.com/kala13x/libxmedia/actions/workflows/smake.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions/workflows/smake.yml)

# libxmedia
Implementation of audio/video transmuxing functionality based on FFMPEG API with a fully functional example.

### Installation
There are several ways to build and install the project but at first, the [libxutils](https://github.com/kala13x/libxutils) library and the `FFMPEG` development packages must be installed on the system.

#### Using included script
The simplest way to build and install a project is to use the included build script:

```bash
git clone https://github.com/kala13x/libxmedia.git && ./libxmedia/build.sh --install
```

List options that build script supports:

- `--tool=<tool>` Specify `Makefile` generation tool or use included `Makefile`.
- `--install` Install library and the tools after the build.
- `--cleanup` Cleanup object files after build/installation.
- `--example` Include example in the build.

You can either choose `cmake`, `smake` or `make` as the tool argument, but `cmake` is recommended on platforms other than the Linux.
If the tool will not be specified the script will use `make` (included Makefile) as default.

#### Using CMake
If you have a `CMake` tool installed in your operating system, here is how project can be built and installed using `cmake`:

```bash
git clone https://github.com/kala13x/libxmedia.git
cd libxmedia
cmake . && make
sudo make install
```

#### Using SMake
[SMake](https://github.com/kala13x/smake) is a simple Makefile generator tool for the Linux/Unix operating systems:

```bash
git clone https://github.com/kala13x/libxmedia.git
cd libxmedia
smake && make
sudo make install
```

#### Using Makefile
The project can also be built with a pre-generated `Makefile` for the Linux.

```bash
git clone https://github.com/kala13x/libxmedia.git
cd libxmedia
make
sudo make install
```
