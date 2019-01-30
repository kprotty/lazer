############ Setup ############
Ninja
CMake
Clang
VsCode + CMake Tools 

header: ['L':8, 'Z':8, 'R':8, module_type:8]

module_type:0 - Native
    size: 32
    atom_name: [u8; size & 0xff]
    libpath: cstr
    functions: [cstr; size >> 8]

module_type:1 - Script
    num_literals: 16
    num_code_words: 32
    literals: [ByteTerm; num_literals]
    bytecode: [u8; num_code_words]

ByteTerm(type:8 (0) - byte):
    value: 8
ByteTerm(type:8 (1) - short):
    value: 16
ByteTerm(type:8 (2) - int):
    value: 32
ByteTerm(type:8 (3) - float):
    value: 32
ByteTerm(type:8 (4) - double):
    value: 64
ByteTerm(type:8 (5) - byte):
    value: 8
ByteTerm(type:8 (6) - byte):
    value: 8
ByteTerm(type:8 (7) - atom):
    size: 8
    data: [u8; size]
ByteTerm(type:8 (8) - list):
    size: 8
    data: [u8; ByteTerm]
ByteTerm(type:8 (9) - list16):
    size: 16
    data: [u8; ByteTerm]
ByteTerm(type:8 (10) - list32):
    size: 32
    data: [u8; ByteTerm]
ByteTerm(type:8 (11) - tuple):
    size: 8
    data: [u8; ByteTerm]
ByteTerm(type:8 (12) - tuple16):
    size: 16
    data: [u8; ByteTerm]
ByteTerm(type:8 (13) - tuple32):
    size: 32
    data: [u8; ByteTerm]
ByteTerm(type:8 (14) - map):
    size: 8
    data: [u8; (ByteTerm, ByteTerm)]
ByteTerm(type:8 (15) - map16):
    size: 16
    data: [u8; (ByteTerm, ByteTerm)]
ByteTerm(type:8 (16) - map32):
    size: 32
    data: [u8; (ByteTerm, ByteTerm)]

########### Execution Code #############
arguments stored in REG[..arity]
maybe switch to ordered registers since theres stack?
maybe only store registers when context switching?

R6 = type:1 offset:5 =
R = type:2 _:1 offset:5 =
    type(0): register
    type(1): memory
    type(2): upvalue

mov dst:R src:R pos:R =
    DST = REG[dst]
    REG[dst] = REG[src]
    REG[pos] = DST

ret value:R _:16 =
    set function result to REG[R]
    pop PC from SP & jump to that


call ret:R offset:16 =
    push PC to SP
    store result in REG[R]
    call atom at constant offset `offset`

get dst:R term:R index:R =
    REG[dst] = GET(REG[term], REG[index])
drop dst:R term:R index:R =
    REG[dst] = DROP(REG[term], REG[index])
put dst:R6 term:R6 key:R6 val:R6 [regmask:32] =
    if gc, then REGs in regmask are live
    REG[dst] = PUT(REG[term], REG[key], REG[val])

push a0:R6 a1:R6 a2:R6 a3:R6 =
map dst:R size:16 [regmask:32] =
list dst:R size:16 [regmask:32] =
tuple dst:R size:16 [regmask:32] =
    if gc, then REGs in regmask are live
    REG[dst] = allocate type(size) on heap
    following `size` push inst mutate append to it

loadi dst:R value:16 =
    load int16 into REG[dst]
loadu dst:R value:16 =
    load uint16 into REG[dst]
loadc dst:R offset:16 =
    load constant offset into REG[dst]
load32 dst:R type:16 value:32 =
    type(0): value = int32
    type(1): value = uint32
    type(2): value = float32
load64 dst:R type:16 value:64 =
    type(0): value = float64

eq lhs:R6 rhs:R6 jump:16 =
ne lhs:R6 rhs:R6 jump:16 =
lt lhs:R6 rhs:R6 jump:16 =
gt lhs:R6 rhs:R6 jump:16 =
lte lhs:R6 rhs:R6 jump:16 =
gte lhs:R6 rhs:R6 jump:16 =

not dst:R src:R
neg dst:R src:R
add dst:R left:R right:R
sub dst:R left:R right:R
mul dst:R left:R right:R
div dst:R left:R right:R
mod dst:R left:R right:R
shr dst:R left:R right:R
shl dst:R left:R right:R
xor dst:R left:R right:R
and dst:R left:R right:R
or dst:R left:R right:R  
