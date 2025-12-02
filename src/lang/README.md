# Directory

This directory contains files that describe language's specific features

# Types

- Top:      Unknown Type
- Known:    Int, Tuple, Ptr, Array, Slice, Struct
- Bottom:   Union of Types

Int
- Top:      -
- Known:    Known Int range
- Bottom:   Impossible Int (contridicting range)

Tuple
- Top:      Any Tuple arity
- Known:    Known Tuple arity
- Bottom:   Impossible Tuple (multiple arity)

Ptr
- Top:      Any Ptr
- Known:    -
- Bottom:   Nullable Ptr

Array
- Top:      Any Array size
- Known:    Known Array size
- Bottom:   Impossible Array (multiple size)

Slice
- Top:      Any Slice size
- Known:    Known Slice size range
- Bottom:   Impossible Slice (???)

Struct // TODO maybe? not?
- Top:      Any Struct
- Known:    Known Struct arity
- Bottom:   Impossible Struct (conflicting members or arity)

# Ramble Ramble

Recursive definitions. They appear in 2 places:
- Recursive functions
- Recursive structs

Functions are one thing and I'll deal with them later.

As for structs. Say, linked list:
```mir
struct LL {
    next: LL*,
    val: u64,
};
```
Structs are the only structure that can have this recursive definitions.
Even if there are typedefs.
Slices can't:
```mir
type myslice = myslice2[];
type myslice2 = myslice[];
```
They're not capable of holding other data besides the arbitrary nestings of themselves. So that doesn't make sense to do.
Same with pointers (slices *are* pointers). Loop definitions are possible but useless and nonsensical.

So any time I *care* about recursive definitions is within structs.

Wait.
```mir
type myslice = (myslice2*, u8);
type myslice2 = (myslice*, u64);
```
Um. If I support nameless definitions of tuples, this *is* possible.
But tuples are basically structs.
Nevermind, I can follow Rust's path. Rust says `type aliases cannot be recursive` Mentions to use struct, enum or union to break the recursive type alias definition:
```rust
type S1 = (Box<Option<S1>>, u8);
```

So yes. Structs. Enums. Unions. Enums and unions are basically structs for all my purposes (I think?).

And again, following Rust's footsteps, I don't intend to have a way to declare "final" or "default initialized" fields in structs.

So the only time I can have a non-resolved recursive type is *within struct definition*.

So after I define the struct, I can double check any non-resolved type names. Or even better, if I want to support having usage before definition (I do absolutely want to support it. We are not living in 90s.), I can do a full pass through source and after that resolve any named types.

Although, doing only 1 pass is *so* good. And given I still have 1 character lookahead... I'd like to have only 1 pass as much as possible.

I'm giving up on making this a transitional parser towards SoN implementation. I'll probably take *some* work, but I'll have to rewrite it, because *everything* about the SoN AST is so different.

With that in mind, I can always treat named types as incomplete. Then make a single pass through the AST to resolve all of those incomplete types.