# Lazer [![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/kprotty/lazer/blob/master/LICENSE)

A programming language written with memory usage and concurrency in mind. Goal has been to support the best of elixir while still keeping a low memory profile. Lazer contains both a lazer script compiler and the virtual machine (specs will be defined soon). Many thanks to, and inspirations from, BEAM (erlang/elixir vm) as well as Ponylang.

## Compiling
```
mkdir build
cd build
cmake -G "Unix Makefiles" ..
make
```
## Planned Tasks
- [x] Terms
- [x] Hashing
- [x] Memory Mapping
- [ ] Atoms & Atom table
- [ ] Modules & Functions
- [ ] Actors & Scheduling
- [ ] GC & Messages
- [ ] Interp Execution
- [ ] ASM Compiler
- [ ] Source Lexer & Parser
- [ ] Source IR & CodeGen
- [ ] Bootstrap ASM Compiler
- [ ] Bootstrap Source Compiler
- [ ] Codegen Optimizations
- [ ] Runtime Optimizations
- [ ] FFI Capabilities
- [ ] Asynchronous IO
- [ ] Library Wrapping