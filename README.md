# DMX Showfile (OLA-format) to MKV converter

A converter that reads OLA showfiles and dumps them losslessly as video files
using the FFV1 codec in strict intra-frame mode to an MKV file.

## But why?

A few folks over at the OLA repo felt that adding some kind of pause 
/ fast-forward / reverse / seek options might be useful for the existing
playback / recorder program.

That sounded like something that an existing player could do as well, so
this project started as an experiment to see if that can be done.

## How does it work?

It encodes DMX channel data and universe information in video frames, with
one line per universe.

Each line stores an universe number and the DMX channel
data associated with that universe. 

Frames use the 8-bit grayscale / Y800 pixel format. 

## What's the downsides of this?

Because the OLA showfile produced by the OLA recorder may not capture the
initial state of all universes at time 0 (it only captures a universe's channel
information when it changes), and because not all universes are recorded when 
a channel changes, playing back a showfile can produce wildly different results
depending on the state of the universes when the showfile is played.

However, when storing these showfiles as a video, the initial channel values
must be known to generate the first frame. 

Because of that, the converter
**requires** that the showfile contain channel information for all universes
at time 0. Those might need to be added by manually editing the showfile.

Furthermore, the number of universes that change within a single frame / delay
period of the showfile is variable: seeking within a video that also does
the same (to correctly preserve the number of DMX frames sent showfile) might
not produce what users expect. Consider the following:

- User plays converted video from 0s to 10s
- Universe 0 changes at 1s
- Universe 1 changes at 2s
- User seeks back to 1s

The user now sees a combination of universe 0 at 1s and universe 1 at 2s.

To better match expectations, this converter stores the channel
data of every universe in each frame. Depending on your use case, this might
not be appropriate - The conversion is *not* strictly "lossless" since it 
doesn't preserve the information required to generate a showfile that would, 
when played back using the OLA recorder / playback tool, 
send the same amount of DMX frames as the original.

(Though not currently implemented, a tag could be
embedded in the video frames to indicate that a universe did not actually
change during a frame, and its data was only included to make seeking
more consistent.)

## Building

This project uses the Meson build system.

Dependencies:

- `libavcodec`
- `libavformat`
- `libavutil`
- `boost`
- `cxxopts`
- C++17 capable C++ compiler & C++ standard library.

See `meson.build` for the exact versions required. (Note that the versions were
just the ones I had installed on bullseye, so they might be higher than
what's actually required.)

To install these dependencies on Debian bullseye:

```terminal
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev \
                     libboost-dev libcxxopts-dev
```

Setup the build with Meson:

```terminal
meson builddir
```

Build with Ninja:

```terminal
cd builddir && ninja
```

## Usage

1. Ensure that there is an initial state for all universes in the showfile
   before any non-zero wait time.

   i.e.: If you have 3 universes (0, 1, 2), there must be three lines in 
   the showfile like:

   ```
   OLA Show
   0 ch0,ch1,ch2.....
   0
   1 ch0,ch1,ch2.....
   0
   2 ch0,ch1,ch2.....
   0
   ```

   This is not allowed:

   ```
   OLA Show
   0 ch0,ch1,ch2.....
   100
   1 ch0,ch1,ch2.....
   100
   ```

2. Run the converter on the input showfile, specifying the number of universes
   contained within the file (that is required to avoid making two passes over
   the input data):
   
   ```terminal
    ./ola_video_convert -u 1 -o converted.mkv -i showfile.show
   ```

## Playing back

`contrib/yuv_to_ola.py` can be used to convert VLC's YUV output and send DMX
frames to the example OLA streaming client with
[this series](https://github.com/OpenLightingProject/ola/pull1683) applied.

Usage:

```terminal
cvlc converted.mkv --extraintf=http --http-host=127.0.0.1 --http-port 9090 \
--http-password password \
--yuv-file >(contrib/yuv_to_ola.py | ola_streaming_client -s) \
--yuv-chroma Y800 -V yuv
```

DMX traffic should be present.

The example above also opens up the VLC web browser interface at 
`http://127.0.0.1:9090` with password `password` so playback can be
controlled. 

NOTE: VLC may spew error messages regarding failed ARGB conversions, but it
seems to work anyway. If desired, the `-q` option can be provided when starting
`cvlc` to squelch them.
