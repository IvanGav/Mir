# Mir

It's a low level-ish language. Still very much wip.

I plan to use llvm for this project.

# TODO
- rework ; reading and account for them in blocks

# Compiling

`$ sudo apt install llvm-dev`
`$ make`

# Notes for different systems

In `core/hash.h`, the type `usize` is the same as `u64` on my machine... not sure if that's true in general.

# Features (planned, and may be completely changed)

Types:
- Signed integer types: `i8`, `i16`, etc
- Unsigned integer types: `u8`, `u16`, etc
- Floating point types: `f32`, `f64`, etc
- Raw pointer type: `type*`
- Fixed sized array type: `type[size]`
  - Compile time known size, effectively a raw pointer, but semantically is an array and has size
  - `sizeof(type[size]) == sizeof(type*)`
- Slice type: `type[]`
  - Effectively `struct Slice { data: type*, len: usize }`
  - `sizeof(type[size]) == sizeof(type*) + sizeof(usize)`
- Struct types: `CustomName` where `struct CustomName { ... }` was declared
- Enum type: `CustomEnum` where `enum CustomEnum { Element1, Element2, ... }`
  - Naming elments by `CustomEnum::Element1`

Flow control/loops:
- `if condition { ... } else { ... }`
- `while condition { ... }`

Dereference/Member access/Address of:
- After `struct A { num: u8 }; let i: A = A { 10 };`,
- `let addr_i: A* = &i; // address of i gives A pointer`
- `let val_i: A = *i; // dereferencing a raw pointer gives its underlying value`
- `let num_i: u8 = i.num; // . operator dereferences until it finds an object`
- `let double_inderection: A** = &addr_i;`
- `let num_i: u8 = double_inderection.num; // until it finds an object`
- `let arr: A[2] = [A { 1 }, A { 2 }];`
- `let arr_ptr: A[2]* = &arr;`
- `//let deref: u8 = arr_ptr.num; // ERROR: dereferencing a pointer once leads to an array, not a pointer or an object`
- `let deref: u8 = arr_ptr[0].num; // correct: indexing also dereferences until array is found`

Functions:
- `fn name(arg: type, arg2: type2)->type_return { ... };`

Structs:
```rust
// declare
struct Name { field1: type1, field2: type2 };
// member functions
impl Name {
  fn constructor( ... )->Name { ... };
  fn member_fn(this*, ... )->return_type { ... };
  fn copy_fn(this, ... )->return_type { ... };
};
```

Operators:
- Binary: 
  - Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo)
  - Logical: `||`, `&&` (short circuit)
  - Bitwise: `|`, `&`, `^` (xor)
- Unary: `-` (arithmetic negate), `*` (dereference), `&` (address of), `!` (logical not), `~` (bitwise not)

If extra time:
- Allow references to a rvalue: `let i = 0; let j: u8** = &&i;`
- Union type: `CustomUnion` where `union CustomUnion { e1: type1, e2: type2, ... }`
- Reference type: `let i = 10; let j: u8& = &i;`
  - References are immutable; `const*`
  - Can only call const member functions of objects: `fn const_member_fn(this&, ... )->return_type { ... };`
- Unsized array type: `type..` or `type[?]`
  - Acts like an array, but does not have size. Basically any C/C++ array pointer

# Biggest memory related problems (general)
- Buffer overflow
- Use after free
- Double free
- Out of bounds write
- Integer overflow