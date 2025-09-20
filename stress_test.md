# Trying to poke my syntax/ideas for flaws

```mir
fn rand(): Float;
fn getpassword(): Float;
fn getSomething(i: Int): Int|Float;

let n = 1; # Int

{
    # 1
    for i in 0..Int::MAX {
        if rand() < 0.01 {
            n = rand(); # Float
        }
    }

    # 2
    i = if readln() == "1234" { getpassword() ## Float ## } else { 10 }

    # 3
    i = getSomething(i); # Int|Float
    
    # after any of those three, the type of `n` is now Int|Float
    # but user expects it to be Int, let's say
}

let m = n + 10; # type of m is Int|Float
##
expands to:
let m = switch n {
    is Int: n + 10, # +(Int,Int)
    is Float: n + 10, # +(Float,Int)
}
##

# but user expects `m` to be Int too

fn myintfn(i: Int) = return i as Float;

let l = myintfn(m); # COMPILE TIME ERROR, see below
##
expands to:
let l = switch m {
    is Int: myintfn(m), # myintfn(Int)
    is Float: myintfn(m), # myintfn(Float) - ERROR, FUNCTION DOES NOT EXIST
}
##

# How to correct?
# Before the `let l` line, do one of these:

# 1
if m is Float {
    m = round(m); # do something that convets Float to Int
}
# The condition "Took away" Float from the Union, upon entry, and returns any new types that `m` can take on inside - only Int in this case

# 2
m = round(m); # as long as there is a dummy `round(Int): Int` function

# 3
m = if m is Float { round(m) };
```

The above problem, in a more practical example can be something like this:
```mir

let table: Table<(Int,Str,Str)> = file("a.csv").read_csv(); # Annoying to do, why do I need to specify what the table's entries are? Moreover, it's ugly.

let table = file("a.csv").read_csv("UID" to Int, "Name" to Str, "Color" to Str); # is best
let table = file("a.csv").read_csv(Int, Str, Str); # also acceptable, as long as there are names for columns in the csv file

# 2 problems
# `file("a.csv")` returns File|()
# `read_csv` takes ((Str,Type),(Str,Type),(Str,Type)) arguments regardless of whether

# I want it to look like this:
enum State { CA, NY, NV, OR } # as enum, has a function `State::valueOf(Str):State`
let table = file("a.csv").read_csv("UID" to Int, "Name" to Str, "State" to State);
```

Big problem?
```mir
# If functions are allowed to take `Type` and act based on that, the returned state will change based on what's provided, without any way of calculating all options during compilation
fn bigproblem(var, t: Type) {
    return var as t; # ILLEGAL, can't actually cast like that; type acts kinda like a union
}

#
fn bigproblem(var: Str, type: 'a is Type) {
    switch type {
        is Str: return var,
        is Int: return var as Int,
        is Bool: return var as Bool,
        is Enum: return a::valueOf(var),
    }
    return ();
}

# Another example of this problem is the Py's shape thing
fn getArrayWithShape(arr: Int[]) {
    # can return Int[10] or Int[25] or Int[10][10] or Int[2][10][25][100]
    # who the fuck knows
    # even if I tried to do something with slices, it's so fucked
}

# and these shaped arrays are actually a big advantage of Py's dynamic typing
# and like yeah, I can probably use Vec instead, but that's a shitty solution to my problem

# to simplify my previous problematic function:
fn interpretAs(var: Str, type) {
    # type can be: primitive or custom enum
}
```

Other experiments related to the one above
```mir
fn maybeproblem(var) {
    return var + 1;
}

maybeproblem("im str"); # when compiled, will try generating function `maybeproblem(Str)` where `+(Str,Int)` is called

fn problematicunionarg(i: 'a is Union) {
    switch i {
        is Int: i+1,
        is Str: i.len(),
        _: i,
    }
}

let u1: Int|String; let u2: Int|Float; let u3: Str; let u4: Int[]|Map<Int,Str>
problematicunionarg(u1); # the generated function will have: `switch i { is Int: i+1, is Str: i.len() }` and return `Int`
problematicunionarg(u2); # the generated function will have: `switch i { is Int: i+1, _: i }` and return `Int|Float`
problematicunionarg(u3); # the generated function will have: `switch i { _: i }` = `i` and return `Str`
problematicunionarg(u4); # the generated function will have: `i` return `Int[]|Map<Int,Str>`
```