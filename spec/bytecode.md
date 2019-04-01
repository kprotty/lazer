## Module
A module is the top-level object of the bytecode. Each bytecode file can be a module (usually what is expected). A module contains constants & functions with bytecode. All bytes are in little-endian.

```rust
struct Module {
    header: u24
    version: u8
    module_type: u8
}
```

* **header**: a constant "LZR" string to denote the following bytes as bytecode
* **version**: the version of the bytecode, 0 for now
* **module_type**: The type of the module. Currently there are 3 different types of modules and it could change later:
    - **Archive** *(0)*: a collection of modules stuffed into a single file or memory buffer
    - **Native** *(1)*: a module which links to a shared library usually compiled in C
    - **Bytecode** *(2)*: your average module representing a single compilation unit
    
### Module > Archive (module_type = 0)
A compilation of modules packed into a single file or memory buffer. Used for loading and serializing multiple modules or files in a single project. Compression may be added later on

```rust
struct ArchiveModule {
    num_sections: u8
    modules: Module[num_sections]
}
```

If there are more than `max(u8) == 255` modules, one could always have another nested `Archive` module inside the array of modules for tree-like extensions of many modules.

### Module > Native (module_type = 1)
Representing a module which links to a shared library for something like C FFI, it simply contains the name of the module, a path to the library as well and the native functions to look for. FFI will be defined later.

```rust
struct NativeModule {
    num_sections: u8
    module_name: Atom
    library_path: Binary8
    functions: Atom[num_sections]
}
```

[Atom]() and [Binary8]() are defined later on for the [Bytecode]() module. Since they share the same structure, its easier to link them here.

### Module > Bytecode (module_type = 2)
The most common type of module. This is usually the product of a compiler compromising of constants and actual lazer bytecode to execute.

```rust
struct BytecodeModule {
    flags: u8
    ?stack_map: StackMap
    ?source_info: SourceInfo
    ?exceptions: ExceptionTable
    constants: Constants
    bytecode: Bytecode
}
```

Each bit in the `flags` represents an attribute regarding current module with the bits undefined remaining reserved:
    * `00000001`: represents if the module contains a StackMap struct
    * `00000010`: represents if the module contains a SourceInfo struct
    * `00000100`: represents if the module contains an ExceptionTable struct

#### Module > Bytecode > StackMap
The stack map is used to plot out which registers are live at different points in the bytecode for the garbage collector to use when
```rust
struct StackMap {
    num_entries: u32
    stack_maps: RegisterMap[num_entries]
}

struct SourceInfo {
    source
}

struct ExceptionTable {

}

struct Constants {

}

struct Bytecode {

}
```