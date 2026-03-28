#include "core/prelude.h"
#include "core/bitset.h"

#define print(one) { std::cout << one << std::endl; }

int main(int argc, char* argv[]) {
    BitSet bs = BitSet::create();
    print(bs);
    bs.set(5);
    bs.set(7);
    print(bs);
    bs.unset(5);
    bs.set(4);
    print(bs);
    bs.toggle(100);
    print(bs);
}