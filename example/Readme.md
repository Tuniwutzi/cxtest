# Examples

These examples are standalone projects using `cxtest` for their testing.

## string-utils

This is a very barebones, artificial example that just shows the basic usage of `cxtest`. It contains a few bespoke string handling functions to show off compiletime-only, runtime-only and dual tests.

## units

A C++26 take on a minimal units library. Loosely inspired by the basics of `mp-units`, without any of the rigor or correctness. The actual workhorse class (`Quantity`) is deliberately minimal, because it contains the greatest complexity regarding computation without overflow. The focus of the example is very much on the compile time facilities that construct the types.

This example shows off how consteval facilities (dimension, unit) can be tested alongside constexpr facilities (rational, quantity) in `cxtest` in a close-to-reality scenario.
