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

To build the bindings run:

```
npm run rebuild
```

On macOS be sure to run this:

```
OPENSSL_ROOT_DIR=/usr/local/opt/openssl/ npm run rebuild
```
