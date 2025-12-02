# flipflip's c++ stuff

Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)

https://oinkzwurgl.org/projaeggd/ffxx/

This repository contains:

- The [ffxx](./ffxx/README.md) library (stream, epoch, ...)
- The [ffapps](./ffapps/README.md) utilities (streamtool, streammux, ...)
- The [ffgui](./ffgui/README.md), a *very experimental* GUI for playing with GNSS receivers and similar sensors. *Very*
  unstable! *Will* crash!

This is for Linux (and [GCC](https://gcc.gnu.org/)).

## Building

```sh
apt install libcurl4-gnutls-dev libglfw3-dev libfreetype-dev zlib1g-dev libglm-dev libeigen3-dev libgl-dev \
    libglu1-mesa-dev libboost-all-dev libyaml-cpp-dev linux-libc-dev libssl-dev libproj-dev proj-bin proj-data git \
    gcc g++ cmake make xxd nlohmann-json3-dev libxrandr-dev libwayland-dev

git clone https://github.com/fixposition/fixposition-sdk.git  3rdparty/fixposition-sdk
git clone https://github.com/phkehl/ubloxcfg.git              3rdparty/ubloxcfg

echo -e "BUILD_TYPE=Debug\nBUILD_FFGUI=ON" > config.mk

make build

./build/Debug/ffgui/ffgui
```

Say `make help` for help.

## Licenses

See the LICENSE files for the various packages in this repository: [ffxx/LICENSE](./ffxx/LICENSE),
[ffapps/LICENSE](./ffapps/LICENSE), [ffgui/LICENSE](./ffgui/LICENSE). Note also the various licenses of the
[3rdparty/](./3rdparty) directory. [ffgui/README.md](./ffgui/README.md) has some more details.

