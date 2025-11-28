#include "../core/prelude.h"
#include "../core/mem.h"
#include "../core/str.h"
#include "../core/vec.h"
#include "../parser/parser.h"
#include "../parser/parser_debug.h"

mem::Arena node_arena = mem::Arena::create(1024 * 1024); // 1 MB

enum class ASTNodeType {

};

struct ASTNode {

};