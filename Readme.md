# MeasurementKit

MeasurementKit is a library that implements open network measurement
methodologies (performance, censorship, etc.).

This repository contains the node.js bindings for MeasurementKit allowing you
to use MeasurementKit from node.js directly.

## Setup

To get a development environment setup ensure you have:

* node.js
* yarn
* The [developer dependencies of measurement-kit](https://github.com/measurement-kit/measurement-kit/blob/master/doc/tutorial/unix.md#configure)

Be sure to clone this repo with `git clone --recursive` as it contains
sub-modules.

## Compiling

You should first compile the version of measurement-kit you want to bind to by
doing:

```
cd deps/measurement-kit/
./autogen.sh
./configure
make
```

Then you can process with building the bindings with:

```
npm run rebuild
```
