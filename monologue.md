## Note

I probably messed up in many places and those features are completely impossible to implement (in a sane way), so beware of trusting anything below

## Syntax/Feautre ideas

Statically typed, compiled.

Variables
```mir
let i = 0; # type infered
{
    let i = 10; # shadowed; can have different type
}
# i is 0
let i: Str = "10"; # shadowed; explicit type specification
```

Variables can be implicitly shadowed
```mir
let i = 0; # type Int
i = "str"; # implicitly shadowed; now a different variable with type Str
if true{i=10;}else{i=0;} # implicitly shadowed; all branches implicitly shadow `i` to Int

if true{i="10";} # ILLEGAL, not all branches implicitly shadow `i` to Str
# Theoretically I can also make it implicitly shadowed to Int|Str right after the if statement,
# while still being Str inside of the if statement, and Int before the if statement
```

Functions
```mir
# Template function
fn template(i, j, k) {
    return i;
}
# Both arguments and output can be specified like this
fn defined(i: Int, j: Int, k: Str): Int {
    return i;
}
# Mix between template and defined
fn defined(i: Int, j, k: Str) {
    return i;
}
```

Types
```mir
# Object
let i: Int;
# Tuple
let i: (Int, Str);
# Union
let i: Int|Str;
# Template (ONLY LEGAL IN FUNCTIONS) - not specifying a type and specifying Any type is the same thing
fn template(i, j: Any): Any { return i; }
```

Template arguments/return in functions
```mir
# Template arguments `i`, `j` (and return)
# For each separate combo of `i`,`j` types, a new function is created, with corresponding signiture (return type deduced for each)
fn f(i, j) { return i+j; }
# Template arugment `i`
# When compiling, if there is no `op *(Int, typeof i): Int`, a compilation error will occur
fn f(i, j: Int): Int { return j*i; }
```

Operators on unions
```mir
let i: Int|Str = 0;
let j = 0; # Implicitly type Int

i * j; # Operator with union type as one of operands
# You can define custom ops with union types
# But if none are defined, all ops on unions by default expand to:
switch i {
    type Int: i * j, # This switch arm implicitly casts and shadows `i` to `i: Int` with its value
    type Str: i * j, # This switch arm implicitly casts and shadows `i` to `i: Str` with its value
}
```

Functions and unions
```mir
let i: Int|Str = 0;
let j = 0; # Implicitly type Int
fn add(i: Int, j: Int) {
    return i + j;
}

add(i, j); # Function expects Int as first argument, but receives a union Int|Str
# If a function with signiture `fn(Int|Str, Int)` does not exist, fallback to default expanding the union:
switch i {
    # This switch arm implicitly casts and shadows `i` to `i: Int` with its value
    type Int: add(i, j),

    # This switch arm implicitly casts and shadows `i` to `i: Str` with its value
    # WARNING: if `fn(Str, Int)` or `fn(Any, Int)` don't exist, the compilation will fail
    type Str: add(i, j),
}
```

Regular, Math/Mir and Kotlin style functions
```mir
# Consider this function called `f`
fn f(x) { return x^2; }

# The following notation can be used instead
# It's called Mir (Math) style functions
f(x) = x^2;

# Mir style functions only take a single statement and return its evaluated value
# If it's a problem, a block can be used too:
f(x) = { # do something multiline here that return the intended value};

fn f(x) = x^2; # This is also a valid notation (Kotlin style)
```

What can yield an lvalue? Basically, where can you assign a value using `=`?
- Variable access
- Index operator (`arr[0]`)
- Tuple access operator (`tuple@0`) (IF I DECIDE I WANT IT AT ALL)

Custom operator
```mir
# Custom operators can be defined
# They behave same as custom functions in most cases

# 1 argument will yield a unary (left side) operator
op unop(i) { return i + 1; }

# 2 arguments will yield a binary operator
op binop(i, j) { return (i + j) * i; }

let i = 2;
let j = 10;

i = unop i;
j = i binop j;
```

Order of operations (operators)
- All user defined ops have the same priority
- All user defined ops will always have lower priority than all other operators

Loops
```mir
let condition = true;
while condition {
    # do stuff here
}

# For loops are done through iterators
let arr = [1,2,3,4,5];
for i in arr {
    # do stuff here
}
```

Ranges
```mir
# Ranges can be generated with operators such as `..`, `..=`, `downTo`, etc
for i in 0..10 { # do stuff here }

# Ranges are lazy and can be unbounded
for i in 0..Inf { # do stuff here; be careful, it's an infinite loop }
```

Arrays, Slices
```mir
let arr: Int[6] = [1,2,3,4,5,6]; # array; non-expandable
let arr2d: Int[3][2] = [[1,2,3],[4,5,6]]; # 2d array
let arr2dAlternative = [1,2,3; 4,5,6]; # `;` can be used to separate elements kinda like R or MATLAB
let arr3d = [[1,2;3,4],[2,3;4,5]]; # any combination of the previous 2 notations can be used for creating arrays of higher dimensions

let slice: Int[] = arr[0..3]; # using ranges as index, you can make an array slice; slice points to [1,2,3]
let slice2: Int[] = arr3d[0][1][0..]; # slice points to [3,4]
let slice3 = arr3d[1..]; # slice3 has a type of Int[][2][2], but Int[][][] is acceptable; points to [[2,3;4,5]]

# there might be a custom struct that lets you have stuff like arr3d[1..4][2..3][5..] (or equivalent of this)
# it's impossible with normal slices as they're supposed to point to a contiguous memory slice
```

Tuples
```mir
let tuple: (Int, Str) = (10, "Hello");
let implicitTuple = ("Hello", "World", 4); # Has a type of (Str, Str, Int)

# tuples are always flattened
let flatTuple = (10, tuple, "World); # has type of (Int, Int, Str, Str)

let i = 1;
# Element access (STILL UNSURE WHICH SYNTAX IS BEST)
assert_eq(tuple.0 == 10); # member access - cannot use variables
assert_eq(tuple[i] == "Hello"); # index - can use variables
assert_eq(tuple@i == "Hello"); # tuple access op - can use variables

# If last 2 are possible, it's a little bit of a headache
# If I have collections such as Table (Tuple of lists with named columns), I'll need to access by name anyway

let element = tuple[i]; # has a type of Int|Str, since those are the two types present in the tuple
```

Casting
```mir
let i: Int|Str = 69;

# If you want to unwrap and use `i`, there are a couple of options:

# 1 - shadow (or implicitly shadow) `i` as Int by casting `i` to Int; compile time error if such cast doesn't exist
# remember, will be expanded to switch with hands for every possible type
i = i as Int; # `i` now has a type of Int

# 2 - check a union's type using `is` special operator; `i` will have a type of Int up until right after the if statement
if i is Int { i += 10; }

# 3 - switch
switch i {
    is Int: i += 10,
    is Str: i = stoi(i),
}

# 4 - template function
fn add(i): Str {
    return i + " Hello World";
}
i = add(i);
```

Default, Copy, Clone
```mir
# Default is a function that's defined for all types
fn getDef(i: <a>) {
    a::default(); # returns the default for any given type
}

let i = 69;
let j = getDef(i); # will return 0

# Copy is a function that's defined for all types; is basically called when passing stuff around by value
let arr: Int[2][6] = [[1,2,3,4,5,6],[7,8,9,10,11,12]];
# when passing `arr` to the function, effectively `Int[2][6]::copy(arr)` is called; with allocated data, will point to the same data, but if reference counting, that'd be here where it's updated
let arrDef = getDef(arr); # will return empty array of type Int[2][6]

let newArr = arr.clone(); # deep copy, with all elements being freshly allocated
```

CSV File Parsing (Prototype)
```mir
# Note that this function - parse_csv() - is a little weird
# It has no template arguments, but a potentially template output value
# So it can return (Int, Str) or (Str, Str, Str, Float) or anything else

let table: (Int, Str, Str) = file("mytable.csv").parse_csv(); # implicitly use template parse_csv from explicit `table` type
let table = file("mytable.csv").parse_csv<(Int, Str, Str)>(); # implicit `table` type from explicit parse_csv template param

# so, parse_csv can look something like:
fn parse_csv(file): <a is Tuple> {
    let to_return: a;
    for i in a.size {
        a[i] = file.next() as a@i; # a@i returns the type of element `i` in `a`
    }
    return a;
}
```

## Implementation details/notes/remarks

- Implicitly shadowing will *always* make the last value inaccessible
- `is` special operator probably doesn't exist on regular types, only on unions
- `@` special operator only exists on tuples to check a type of an element at a certain index
- If a fn/op and a variable have the same name, when encountering the name and it's ambiguous which one it is, always assume it's variable
  - Warning on a var name that's the same as fn/op name
- I think Unions will be very "fuzzy" in Mir. They'll be converted to and from implicitly all the goddamn time.
  - `f(x: Int|Float) = x^2 as Float; let i: Int = 10; let sq: Float = f(i);` is completely valid and gladly converts from Int to Int|Float when calling the function

## Edge cases/Headaches
- What if I have a function like this? Non-Union types are not supposed to have access to `is` op, since they don't have an attached runtime type.
```mir
fn templateWithCheck(i): Int {
    switch i {
        is Int: i+1,
        is Str: i.len(),
        _: i as Int,
    }
}
```
- Probably, it wouldn't be allowed, unless this modification was made
```mir
fn templateUnion(i: <a is Union>): Int {
    switch i {
        is Int: i+1,
        is Str: i.len(),
        _: i as Int,
    }
}
```

- What if a function wants to return a union? Should also work without return type annotation - if multiple types are returned, return their union.
```mir
fn unionReturn(): Int|Str {
    let line = readln();
    if line[0] == '#' { return 69; }
    return line;
}
```