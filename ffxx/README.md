# flipflip's c++ library (ffxx)

Copyright (c) Philippe Kehl (flipflip at oinkzwurgl dot org)

https://oinkzwurgl.org/projaeggd/ffxx/

[License](./LICENSE): GNU General Public License (GPL) v3

## Overview

This is a utility library. It contains various functionality used by the [ffapps](../ffapps/README.md) and  [ffgui](../ffgui/README.md).

## Build and install

(Consider using the top-level CMakeList.txt or Makefile to easily build this library.
See [README](../README.md).)

```sh
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/wherever/you/want/it
cmake --build build
cmake --install build
```

Dev command:

```sh
clear; rm -rf build; cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/tmp/ub && cmake --build build --verbose && cmake --install build
```

## See also

- [../README.md](../README.md)
