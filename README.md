[![MIT License](https://img.shields.io/badge/License-MIT-brightgreen.svg?)](https://github.com/kala13x/libxmedia/blob/main/LICENSE)
[![C/C++ CI](https://github.com/kala13x/libxmedia/actions/workflows/make.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions/workflows/make.yml)
[![CMake](https://github.com/kala13x/libxmedia/actions/workflows/cmake.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions)
[![SMake](https://github.com/kala13x/libxmedia/actions/workflows/smake.yml/badge.svg)](https://github.com/kala13x/libxmedia/actions/workflows/smake.yml)

# libxmedia
A cross-platform library for audio/video transmuxing, built on FFmpeg. Designed for simplicity and performance, it provides an intuitive high-level API and includes a fully functional example.

### Installation
There are several ways to build and install the project but at first, the [libxutils](https://github.com/kala13x/libxutils) library, `FFMPEG` and `freetype` development packages must be installed on the system.

#### Using included script
After installing `libxutils`, `ffpmeg` and `freetype` libraries, the simplest way to build and install a project is to use the included script:

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

### XMedia
`xmedia` is an example command-line tool for transcoding and remuxing media files. It allows you to convert and stream media from one format to another with various customization options.

##### Usage
```bash
xmedia [options]
```

##### Command-line options:
Option     | Syntax    | Type           | Description
-----------|-----------|----------------|---------------------------
`-i`       | path      | string         | Input file or stream path
`-o`       | path      | string         | Output file or stream path
`-e`       | format    | string         | Input format name (example: v4l2)
`-f`       | format    | string         | Output format name (example: mp4)
`-x`       | format    | string         | Video scale format (example: aspect)
`-p`       | format    | string         | Video pixel format (example: yuv420p)
`-s`       | format    | string         | Audio sample format (example: s16p)
`-k`       | num:den   | number:number  | Video frame rate (example: 90000:3000)
`-q`       | number    | number         | Audio sample rate (example: 48000)
`-c`       | count     | number         | Audio channel count (example: 2)
`-v`       | codec     | string         | Output video codec (example: h264)
`-a`       | codec     | string         | Output audio codec (example: mp3)
`-w`       | width     | number         | Output video width (example: 1280)
`-h`       | height    | number         | Output video height (example: 720)
`-b`       | bytes     | number         | IO buffer size (default: 65536)
`-t`       | type      | string         | Timestamp calculation type
`-m`       | path      | string         | Metadata file path
`-n`       | shift     | number         | Fix non-motion PTS/DTS
`-z`       |           |                | Custom output handling
`-l`       |           |                | Loop transcoding/remuxing
`-r`       |           |                | Remux only
`-d`       |           |                | Debug logs
`-u`       |           |                | Usage information

#### Video Scale Formats
- `stretch` Stretch video frames to the given resolution
- `aspect` Scale video frames and protect aspect ratio

##### Example
```bash
xmedia -i /dev/video0 -o dump.mp4 -v h264 -p yuv420p -x aspect -w 1280 -h 720
```

#### Timestamp Calculation Types
- `calculate` Calculate TS based on the elapsed time and clock rate
- `compute` Compute TS based on the sample rate and time base
- `rescale` Rescale original TS using av_packet_rescale_ts()
- `round` Rescale original TS and round to the nearest value
- `source` Use original PTS from the source stream

##### Example
```bash
xmedia -i input.avi -o output.mp4 -t source
```

#### Metadata File Syntax
Metadata files allow you to specify additional information for your media. The syntax is as follows:

- If the line consists of three sections, it will be parsed as a chapter.
- If it consists of two sections, it will be parsed as metadata.
- `hh:mm:ss` time format is used for chapter start/end time.

#### Metadata File Example:
```
00:00:00|00:00:40|Opening chapter
00:00:40|00:10:32|Another chapter
00:10:32|00:15:00|Final chapter
Comment|Created with xmedia
Title|Example meta
Album|Examples
```

##### Example
```bash
xmedia -i file.mp4 -ro remuxed.mp4 -m meta.txt
```
