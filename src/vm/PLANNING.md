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
REG = registers[32]
SP = stack pointer

code_word = 4 bytes
RET = REG[0], ARGS = REG[1..]
R  = is_memory:1 offset:7 =
R6 = is_memory:1 offset:5 =
    is_memory ? SP[offset] : REG[offset]

call func:R flags:16 = 
    call function in atom `func`
    if flags & TAIL_CALL, dont push PC
    if flags & SAVE_ARGS, save REG[...(arity + 1)]
    result stored in REG[0], args in REG[1:arity+1]

ret _:24 =
    PC = pop(SP)

move dst:R src:R dst_pos:R =
    SRC = REG[src]
    REG[dst_pos] = REG[dst]
    REG[dst] = SRC

load dst:R type:8 meta:8 = 
    REG[dst] = FOLLOWING_TERM
    atom  - size:meta
    map   - size:meta
    list  - size:meta
    tuple - size:meta

loadc dst:R vtype:8 meta:8 =
    REG[dst] = FOLLOWING_VALUE
    (vtype:0 - u8) value:meta
    (vtype:1 - i8) value:meta
    (vtype:2 - i32) value:32
    (vtype:3 - f32) value:32
    (vtype:4 - f64) value:64
    (vtype:6 - lit) offset:32
    (vtype:5 - slit) offset:meta

tuple dst:R size:8 live:8 (where code_word = [a0, a1, a2, a3]) =
list dst:R size:8 live:8 (where code_word = [a0, a1, a2, a3]) =
map dst:R size:8 live:8 (where put = [(a0, a1), (a2, a3)]) = 
put a0:R6 a1:R6 a2:R6 a3:R6 =
    ensure type[size] on heap
    if gc, REG[...live] are live
    following [size] `put`s mutate append alloc'd type

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
