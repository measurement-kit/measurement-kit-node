# MeasurementKit

MeasurementKit is a library that implements open network measurement
methodologies (performance, censorship, etc.).

This repository contains the node.js bindings for MeasurementKit allowing you
to use MeasurementKit from node.js directly.

⚠️: This repository is currently discontinued. We may want to revive it in
the furure, but at present you should not use this code.

## Setup

To get a development environment setup ensure you have:

* node.js
* yarn
* The [developer dependencies of measurement-kit](https://github.com/measurement-kit/measurement-kit/blob/master/doc/tutorial/unix.md#configure)

Ensure you have the latest version of measurement-kit installed in
`/usr/local/include/measurement_kit` & `/usr/local/lib/libmeasurement*`

Then run:

```
npm install
```

## Recompiling

To rebuild the bindings run:

```
npm run rebuild
```
