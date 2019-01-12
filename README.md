# Lazer [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/kprotty/lazer/blob/master/LICENSE)

A programming language written with memory usage and concurrency in mind. Goal has been to support the best of elixir while still keeping a low memory profile. Lazer contains both a lazer script compiler and the virtual machine (specs will be defined soon). Many thanks to, and inspirations from, BEAM (erlang/elixir vm) as well as Ponylang.

## Compiling

To run tests:
```
cargo test -p lzr
```

To build lazer executable:
```
cargo build --release
```