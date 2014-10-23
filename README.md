## Overview

libnub is a small wrapper around [libuv](https://github.com/libuv/libuv) to
make it possible to work with handles off the same thread running the event
loop. The API can be summarized in the following points:

* Create/dispose of threads against a specific event loop.

* Push "work" to a child thread from the event loop thread.

* Introduce the "event loop mutex" which will halt execution of a child thread
  until the event loop can be operated upon.

## Documentation

Look at `include/nub.h`.

## Build Instructions

Building only currently works on Linux. Run the following:

```
mkdir out
./build.sh
cd out/
make
```

Can also use `BUILDTYPE=Debug make` to perform the debug build.

## Tests

Currently in a sad state, but can run what currently exists by running
`out/{Debug,Release}/run-nub-tests`.
