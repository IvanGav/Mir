# Mir

## Compiling

`make`

## Code hierarchy

- `core` - a "standard library"
  - `core/prelude.h` - a file with all required definitions for standard library
- `lang` - defines some language structures without any parsing or anything
- `token` - lexer/tokenizer breaks up source code into tokens
- `son` - parser into sea of nodes ast

## Features (planned, and may be completely changed)

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

## Terminology
- Global level expression - The entire expression (including `;`) that starts in global scope
- Top level expression - The entire expression (including `;`) that starts in any non-global scope
- Primary expression - a sequence that produces a value. May or may not contain operators, function calls, nested scopes (and as a result more nested top level expressions), etc
- Term - a single symbol with any postfix operations applied (member access `.` or index `[...]`)

## Biggest memory related problems (general)
- Buffer overflow
- Use after free
- Double free
- Out of bounds write
- Integer overflow

# Notes on `Simple`

In chapter 2 Node implementation for peephole:
```java
if (!(this instanceof ConstantNode) && type.isConstant()) {
  kill(); // Kill `this` because replacing with a Constant
  return new ConstantNode(type).peephole();
}
```
It's possible for `StartNode` to be killed, as some thing like `Start <- Const(10) <- OpNeg` will kill `OpNeg`, then `Const(10)` and then `Start`. Because it has no uses. But we're about to give it a use - `Const(-10)`. So that's strange. The problem is present in chapter 3 too, I think.