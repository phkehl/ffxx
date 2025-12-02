# flipflip's GUI (ffgui)

Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)

https://oinkzwurgl.org/projaeggd/ffxx/

[License](./LICENSE): GNU General Public License (GPL) v3, and others (see below)

## Overview

This is a *very experimental* GUI for playing with GNSS receivers, mainly u-blox positioning receivers, analysing data,
recording and replaying logs.

![screenshot](./doc/screenshot3.jpg)

## Building

Install dependencies:

```sh
apt install libcurl4-gnutls-dev libglfw3-dev libfreetype-dev zlib1g-dev libglm-dev libeigen3-dev libgl-dev \
    libglu1-mesa-dev libboost-all-dev libyaml-cpp-dev linux-libc-dev libssl-dev libproj-dev proj-bin proj-data git \
    gcc g++ cmake make xxd nlohmann-json3-dev libxrandr-dev libwayland-dev

git clone https://github.com/fixposition/fixposition-sdk.git  3rdparty/fixposition-sdk
git clone https://github.com/phkehl/ubloxcfg.git              3rdparty/ubloxcfg
```

Use the top-level CMakeFiles.txt to build:

```sh
cd ..
cmake -B build -S . -DBUILD_TESTING=OFF -DBUILD_FFGUI=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/wherever/you/want/it
cmake --build build --parallel 8
```

Run ffgui:

```sh
./build/ffgui/ffgui
```

Install (optional):

```sh
cmake --install build
/wherever/you/want/it/bin/ffgui
```

There's a top-level Makefile to make building more easy:

```sh
cd ..
make help  # and follow the instructions
```

## Tips

- All of this is *very experimental*. YMMV.
- If the build fails with compiler warnings, try disabling them (remove `-Werror` from the various CMakeFiles.txt)
- Run `ffgui -h` for more stuff.

## Licenses

- [License](./LICENSE): GNU General Public License (GPL) v3
- Various third-party code comes with its own licenses, see the *.LICENSE files [`3rdparty/`](./3rdparty) and below
- Definitions for various maps are built into the ffgui. Check if the licenses cover your use!
  See [`mapparams.cpp`](./ffgui/stuff/mapparams.cpp).
- See also the 'About' window in the app

## Third-party code

The following third-party code is included in this repository:

- *Dear ImGui*       (<https://github.com/ocornut/imgui>),           see [3rdparty/imgui/](./3rdparty/imgui/)
- *ImPlot*           (<https://github.com/epezent/implot>),          see [3rdparty/implot/](./3rdparty/implot/)
- *PlatformFolders*  (<https://github.com/sago007/PlatformFolders>), see [3rdparty/stuff/](./3rdparty/stuff/)
- *DejaVu* fonts     (<https://dejavu-fonts.github.io/>),            see [3rdparty/fonts/](./3rdparty/fonts/)
- *ProggyClean* font (<https://proggyfonts.net>),                    see [3rdparty/fonts/](./3rdparty/fonts/)
- *ForkAwesome* font (<https://forkaweso.me/Fork-Awesome/>),         see [3rdparty/fonts/](./3rdparty/fonts/)

And many third-party libraries are used as external dependencies. These include:

- *Fixposition SDK*  (<https://github.com/fixposition/fixposition-sdk>)
- *GLFW*             (<https://www.glfw.org/>)
- *libcurl*          (<https://curl.se/>)
- *Freetype*         (<https://freetype.org/>)
- *zlib*             (<https://zlib.net/>)
- *glm*              (<https://github.com/g-truc/glm>)
- *json*             (<https://github.com/nlohmann/json>)
- and more...

## TODOs, ideas

- Implement Logfile::Read(), Seek(), Tell(), Size(), and gzipped read/write/seek/tell/size
- Navigation status page (same stuff as in input window + velocity gage, artificial horizon, etc.)?
- Implement A-GNSS
- Add epoch rate [Hz] to nav status (also for logfile!)
- Messages data win: show tree anyway if hidden but selected message not present (until it is present...)
- Clear all settings (on next restart) option
- Custom message: implement RTCM3
- Custom message: load from / save to file, load from clipboard? sw itch to right tab depending on what message it is?
- Fix docking troubles in GuiWinInput::DrawWindow(), disallow some centre dockings
- Improve GuiWinDataPlot, e.g. markers, lines, lines+markers, ...
- Handle database reset (insert blank epoch?) when seeking in logfile
- File dialog: load dir entries async, and as needed (make _RefreshDir() run in background)
- (started) Implement epoch info page (table with all details / fields of EPOCH_t etc.)
- FIXMEs and TODOs all over...
- Application crashes if (e.g. receiver) thread  is running on close. Explicitly calling _receiver->Stop() in
  GuiWinInputReceiver::~GuiWinInputReceiver(). Maybe this is now fixed with the GuiSettings rework.
  seems to help (and in ~GuiWinInputLogfile)
- GuiWinDataConfig needs some love, and the new tables..
- Gzipped read: maybe replace by gunzipping file into cache dir and then use that?
  Hmmm... lots of implications with the "immediate" in imgui.. :-/
  Can we implement seekpos()? Maybe re-open file at a certain position? And gzseek() is commented-out in zlib.h anyway... (?!)
- Fix memory leaks... valgrind isn't too happy.. :-/ (Is it?)
- Use Ff::Thread in GuiWinPlay
- Write parser for swisstopo wmts capabilities xml to generate mapparams
- Use https://github.com/raysan5/raylib/blob/master/src/raymath.h instead of glm (1 file vs 1000000 files)
- example_emscripten_wgpu, can we do that?
- plot trajectory with error estimates
- OpenGL::SaveState()/RervertState() or something like that
- Something's wrong when stepping logs by epoch: UBX-NAV-EOE is not shown?!
- Waterfall plot of UBX-MON-SPAN
- Fix ImTextureRef stuff
- Check out stuff here: <https://github.com/mahilab/mahi-gui>
- Check out <https://github.com/bkaradzic/bgfx>
- Check out <https://github.com/jnmaloney/WebGui>
- Checkout icons in <https://github.com/leiradel/ImGuiAl/tree/master/fonts>
- <https://gist.github.com/gorbatschow/ce36c15d9265b61d12a1be1783bf0abf>
- Check out <https://github.com/CedricGuillemet/ImGuizmo>
- Check out <https://github.com/WerWolv/ImHex/blob/master/lib/libimhex/source/api/layout_manager.cpp>
- Logfile: throttle by epoch, fall back to marker messages, fall back to message count
- https://github.com/tbeloqui/gst-imgui/blob/master/main.cpp


