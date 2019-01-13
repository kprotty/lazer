# LZR Brainstorm Design
This is mostly to note down ideas and shouldn't be treated as a specification as its most likely to change very frequently.

## Goals
The goal of lazer is to essentially be a low memory & performant implementation of the BEAM VM (on which Erlang & Elixir run on). Lazer will be targetting x86_64 (and possibly Aarch64) for Windows and Linux primarly. It should also support the same ideas as BEAM: actor-based, functional, immutable and module isolated. The syntax for **Photon** (the scripting language which lazer natively compiles) may also share similar syntax constructs from elixir/erlang such as modules, pattern matching and lambdas.

## Terms
Similar to erlang, lazer defines its value types as Term's. All values are of the base type Term. There are 8 different term types:
- **Number**: IEEE 754 64bit floating point
- **Map**: Associative hashmap from one term to another
- **Atom**: Named constant of bytes
- **Data**: Raw, Typed Data (usually from FFI)
- **List**: Singly Linked-List of terms
- **Tuple**: Fixed-sized, contiguous array of terms
- **Binary**: Fixed-sized, contiguous array of bytes
- **Function**: Reference to executable (native or lazer) code

The storage of terms takes advantage of various design decisions of lazer's targetting architectures: Canonical addresses and Quiet NaNs.

IEE 754 64bit floating point values compromise of 1 sign bit, 11 exponent bits and 52 mantissa bits. A float where all exponent bits are set and the mantissa is 0 is considered `Infinity`. When the mantissa is non-zero, its considered a `NaN` (Not A Number). Theres two types of NaNs: Quiet and Signaling. Quiet NaNs have the upmost mantissa bit set while Signaling NaNs don't. Signaling NaNs trigger a FloatingPointException in the cpu when operated on wihle Quiet NaNs either always return another Quiet NaN or return false for any comparison (even to itself). These properties make it easy to test for Quiet NaNs and show that they allow 51 bits of non-zero payload to be stored inside them.

In x86_64 and Aarch64, 64bits of address space supports addressing 16 exabytes of data. Therefor userland addresses only use 48 bits of address space (which supports 256 terrabytes of data). This means that pointers can be stored inside Quiet NaNs with 51 - 48 = 3 bits to spare. Since there are 8 types, they can be encoded in the 3 bits; Number being 0 representing the max QuietNaN value.

## Actors
Lazer builds itself upon the actor-model. Similar to coroutines, green-threads or fibers, actors are scheduled units of execution. The difference with actors is that they are isolated from each other and communite via message passing. This means communicating with another actor is transparent where the other node could be on another thread, another process or another remote node entirely.

Using this, actors will have their own heap, stack and memory. When actors send messages to others, its either sent over a socket, pipe or moved into a shared heap space and the reference is passed across.

## Heap Spaces

Because of all the different structures in lazer, theres different allocators which organize their data in a unique layout. Most allocators get their memory from a global page provider.

#### Page Provider
The page provider is the default global allocator for other heap spaces. Unlike a global allocator in a general purpose language like C or Rust, it allocates pages aligned to a certain boundary rather than serving general allocation requests of varying sizes. The current alignment is set to be `2MiB` but that could change.

The page allocator knows of 2 types of pages: basic pages and large pages. Basic pages have the size = alignment and is usually most page requests while large pages have size > alignment but still aligned. Large pages are memory mapped separately and, when they become unused, are added to a free list. When the free list of basic pages runs out, it looks to the one of large pages and splits off an `alignment` sized chunk from the top most page. Unused large pages which get split enough are moved to the basic-page free list. If there are no large pages, a chunk of `16MiB` is memory mapped and added (the size of this is also subject to change).

#### Atom Table Heap Space
The atom table can only allocate a fixed sized amount of atoms where, like erlang, it crashes if it goes over that limit. The hash table memory for the atom pointers can be memory mapped upfront and committed incrementally as the table grows. In fact, two spaces of the atom pointers can be mapped and resizing would be similar to a copying garbage collection with a "to" and "from" space. The memory for the atoms themselves can be incrementally allocated from the [Page Provider](#Page-Provider).

#### Actors & Messages Heap Space 
Since actors and messages are all the same size, they can use bitmaps for lock free allocation making them fast and cheap to construct. Both have links to the "next" item in order to implement a wait-free queue with them. They too also use the [Page Provider](#Page-Provider).

#### Map & Actor-Memory Heap Space
Large maps (>16 elements) are stored in their own heap space. This puts less pressure on the [garbage collection](#Garbage-Collection) of actors when copying data. Large maps use a variant of the HAMT algorithm to implement a hash array trie of 4 bits per node. Same hash collisions are handled via a linked list similar to a chaining hashmap. This structure supports immutable updates while copying less memory than a traditional, mutable hash table. The nodes themselves and the chaining linked list are all the same size so can be bitmap allocated as well.

Actors have essentially 3 memory areas: stack, young heap and old heap. The stack is where caller-saved registers and the return address (as well as potentially stack-allocated terms) reside. The young heap is where most allocations are serviced. Its normaly smaller than the old heap and it garbage collected more frequently. Objects which remain in the young heap are moved to the old heap. See [garbage collection](#Garbage-Collection) for more info.

#### Binary & Shared Heap Space
Large binaries (>= 64 bytes) are stored in a shared heap space. Terms send to other actors which arent large binaries or large maps are stored here as well. List, because they are same sized, are bitmap allocated. Tuples, Data and Binaries on the other hand use bump allocation on a per page basis sharing the inner workings of the [Page Provider](#Page-Provider). The tuple, data and binary references are bitmap allocates and the memory they point to can be freely garbage collected. In the case of FFI, native code is required to notify the vm when it wishes to gain control of an address so that the GC wont move the data out from underneath its address then relinquish its ownership when done.

## Garbage Collection
In order to adhere to the low memory constraint, its important that fragmentation is essentially elimated. NO WASTED MEMORY!! /s Therefore heap spaces with variable sized data is required to be garbage collected every so often. Each heap space has its own way of collecting garbage:

### GC: Shared Heap
The shared heap, similar to the page provider, handles allocations on a per-page basis. When basic-pages are depleted, instead of immediately trying to steal from the large pages, it attempts to garbage collect existing basic pages. All basic pages split up into segments which are added to a linked list of "segments to GC". When a mutator thread does a basic-page allocation, it reaches for the segments first, joining in on the collection to help finish up. 

### GC: Actor Memory
TODO (size classes from 2kb -> 2mb (doubling))

## Modules
TODO

## Bytecode
TODO
