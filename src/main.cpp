#include "hash.hpp"

#include <sys/mman.h>

#define MAX_U15     0x7FFF
#define OFFSET_VARS 0x0010

#define CAP_CHARS  (1 << 19)
#define CAP_TOKENS (1 << 17)
#define CAP_INSTS  MAX_U15
#define CAP_LABELS 4513
#define CAP_VARS   131

STATIC_ASSERT(CAP_VARS <= (MAX_U15 - OFFSET_VARS));

enum TokenTag {
    TOKEN_U15 = 0,
    TOKEN_STR,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_AT,
    TOKEN_EQUALS,
    TOKEN_SCOLON,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_BANG,
    TOKEN_AMPERS,
    TOKEN_PIPE,
};

union TokenBody {
    String as_string;
    u16    as_u15;
};

struct Token {
    TokenBody body;
    TokenTag  tag;
    u32       offset;
};

// clang-format off
enum SymbolComp {
    COMP_ZERO         = 0x2A,
    COMP_ONE          = 0x3F,
    COMP_NEGATIVE_ONE = 0x3A,
    COMP_D            = 0x0C,
    COMP_A            = 0x30,
    COMP_M            = 0x70,
    COMP_NOT_D        = 0x0D,
    COMP_NOT_A        = 0x31,
    COMP_NOT_M        = 0x71,
    COMP_NEGATIVE_D   = 0x0F,
    COMP_NEGATIVE_A   = 0x33,
    COMP_NEGATIVE_M   = 0x73,
    COMP_D_PLUS_1     = 0x1F,
    COMP_A_PLUS_1     = 0x37,
    COMP_M_PLUS_1     = 0x77,
    COMP_D_MINUS_1    = 0x0E,
    COMP_A_MINUS_1    = 0x32,
    COMP_M_MINUS_1    = 0x72,
    COMP_D_PLUS_A     = 0x02,
    COMP_D_PLUS_M     = 0x42,
    COMP_D_MINUS_A    = 0x13,
    COMP_D_MINUS_M    = 0x53,
    COMP_A_MINUS_D    = 0x07,
    COMP_M_MINUS_D    = 0x47,
    COMP_D_AND_A      = 0x00,
    COMP_D_AND_M      = 0x40,
    COMP_D_OR_A       = 0x15,
    COMP_D_OR_M       = 0x55,
};

enum SymbolDest {
    DEST_NULL = 0x00,
    DEST_M    = 0x01,
    DEST_D    = 0x02,
    DEST_MD   = 0x03,
    DEST_A    = 0x04,
    DEST_AM   = 0x05,
    DEST_AD   = 0x06,
    DEST_AMD  = 0x07,
};

enum SymbolJump {
    JUMP_NULL = 0x00,
    JUMP_JGT  = 0x01,
    JUMP_JEQ  = 0x02,
    JUMP_JGE  = 0x03,
    JUMP_JLT  = 0x04,
    JUMP_JNE  = 0x05,
    JUMP_JLE  = 0x06,
    JUMP_JMP  = 0x07,
};

enum SymbolPreDef {
    PREDEF_R0_SP   = 0x0000,
    PREDEF_R1_LCL  = 0x0001,
    PREDEF_R2_ARG  = 0x0002,
    PREDEF_R3_THIS = 0x0003,
    PREDEF_R4_THAT = 0x0004,
    PREDEF_R5      = 0x0005,
    PREDEF_R6      = 0x0006,
    PREDEF_R7      = 0x0007,
    PREDEF_R8      = 0x0008,
    PREDEF_R9      = 0x0009,
    PREDEF_R10     = 0x000A,
    PREDEF_R11     = 0x000B,
    PREDEF_R12     = 0x000C,
    PREDEF_R13     = 0x000D,
    PREDEF_R14     = 0x000E,
    PREDEF_R15     = 0x000F,
    PREDEF_SCREEN  = 0x4000,
    PREDEF_KBD     = 0x6000,
};

static const char* BYTES[] = {
    "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111", "1000",
    "1001", "1010", "1011", "1100", "1101", "1110", "1111",
};
// clang-format on

enum InstTag {
    INST_UNRESOLVED = 0,
    INST_ADDRESS,
    INST_COMPUTE,
};

struct InstCompute {
    SymbolComp comp;
    SymbolDest dest;
    SymbolJump jump;
};

union InstBody {
    String      as_string;
    u16         as_u15;
    InstCompute as_compute;
};

struct Inst {
    InstBody body;
    InstTag  tag;
};

struct Memory {
    const char*                    path;
    char                           chars[CAP_CHARS];
    Token                          tokens[CAP_TOKENS];
    Inst                           insts[CAP_INSTS];
    Table<String, u16, CAP_LABELS> labels;
    Table<String, u16, CAP_VARS>   vars;
    u32                            len_chars;
    u32                            len_tokens;
    u32                            len_insts;
};

#define EXIT_PRINT(memory, x)     \
    {                             \
        print(stderr, memory, x); \
        EXIT();                   \
    }

#define EXIT_IF_PRINT(condition, memory, x) \
    {                                       \
        if (condition) {                    \
            print(stderr, memory, x);       \
            EXIT_WITH(#condition);          \
        }                                   \
    }

#define IS_ALPHA(x) \
    ((('A' <= (x)) && ((x) <= 'Z')) || (('a' <= (x)) && ((x) <= 'z')))

#define IS_DIGIT(x) (('0' <= (x)) && ((x) <= '9'))

#define IS_PUNCT(x) \
    (((x) == '_') || ((x) == '.') || ((x) == '$') || ((x) == ':'))

#define IS_ALPHA_OR_DIGIT_OR_PUNCT(x) \
    (IS_ALPHA(x) || IS_DIGIT(x) || IS_PUNCT(x))

static Token* alloc_token(Memory* memory) {
    EXIT_IF(CAP_TOKENS <= memory->len_tokens);
    return &memory->tokens[memory->len_tokens++];
}

static Inst* alloc_inst(Memory* memory) {
    EXIT_IF(CAP_INSTS <= memory->len_insts);
    return &memory->insts[memory->len_insts++];
}

static void set_chars_from_file(Memory* memory, const char* path) {
    File* file = fopen(path, "r");
    EXIT_IF(!file);
    fseek(file, 0, SEEK_END);
    memory->len_chars = static_cast<u32>(ftell(file));
    EXIT_IF(CAP_CHARS <= (memory->len_chars + 1));
    rewind(file);
    EXIT_IF(fread(memory->chars, sizeof(char), memory->len_chars, file) !=
            memory->len_chars);
    fclose(file);
    memory->chars[memory->len_chars] = '\0';
    memory->path = path;
}

static bool set_digits(const char* chars, u32* i, u16* a) {
    *a = 0;
    while (IS_DIGIT(chars[*i])) {
        const u16 b = (*a * 10) + static_cast<u16>(chars[(*i)++] - '0');
        if (b < *a) {
            return false;
        }
        *a = b;
    }
    return true;
}

static void print(File* stream, Memory* memory, u32 offset) {
    Vec2<u32> position = {1, 1};
    EXIT_IF(memory->len_chars <= offset);
    for (u32 i = 0; i < offset; ++i) {
        if (memory->chars[i] == '\n') {
            ++position.y;
            position.x = 1;
        } else {
            ++position.x;
        }
    }
    fprintf(stream, "%s:%u:%u\n", memory->path, position.y, position.x);
}

template <TokenTag X>
static void set_token_with(Memory* memory, u32* i) {
    Token* token = alloc_token(memory);
    token->tag = X;
    token->offset = *i;
    ++(*i);
}

static void set_tokens(Memory* memory) {
    memory->len_tokens = 0;
    for (u32 i = 0; i < memory->len_chars;) {
        switch (memory->chars[i]) {
        case '/': {
            ++i;
            EXIT_IF_PRINT(memory->len_chars <= i,
                          memory,
                          memory->len_chars - 1);
            EXIT_IF_PRINT(memory->chars[i] != '/', memory, i);
            for (; i < memory->len_chars; ++i) {
                if (memory->chars[i] == '\n') {
                    break;
                }
            }
            break;
        }
        case ' ':
        case '\t':
        case '\r':
        case '\n': {
            ++i;
            break;
        }
        case '(': {
            set_token_with<TOKEN_LPAREN>(memory, &i);
            break;
        }
        case ')': {
            set_token_with<TOKEN_RPAREN>(memory, &i);
            break;
        }
        case '@': {
            set_token_with<TOKEN_AT>(memory, &i);
            break;
        }
        case '=': {
            set_token_with<TOKEN_EQUALS>(memory, &i);
            break;
        }
        case ';': {
            set_token_with<TOKEN_SCOLON>(memory, &i);
            break;
        }
        case '+': {
            set_token_with<TOKEN_PLUS>(memory, &i);
            break;
        }
        case '-': {
            set_token_with<TOKEN_MINUS>(memory, &i);
            break;
        }
        case '!': {
            set_token_with<TOKEN_BANG>(memory, &i);
            break;
        }
        case '&': {
            set_token_with<TOKEN_AMPERS>(memory, &i);
            break;
        }
        case '|': {
            set_token_with<TOKEN_PIPE>(memory, &i);
            break;
        }
        default: {
            EXIT_IF_PRINT(!(IS_ALPHA_OR_DIGIT_OR_PUNCT(memory->chars[i])),
                          memory,
                          i);
            Token* token = alloc_token(memory);
            token->offset = i;
            if (IS_DIGIT(memory->chars[i])) {
                u32 j = i;
                if (!set_digits(memory->chars, &i, &token->body.as_u15) ||
                    (MAX_U15 < token->body.as_u15))
                {
                    EXIT_PRINT(memory, j);
                }
                token->tag = TOKEN_U15;
                continue;
            }
            u32 j = i;
            for (; j < memory->len_chars; ++j) {
                if (!(IS_ALPHA_OR_DIGIT_OR_PUNCT(memory->chars[j]))) {
                    break;
                }
            }
            EXIT_IF_PRINT(i == j, memory, i);
            token->body.as_string = (String){&memory->chars[i], j - i};
            token->tag = TOKEN_STR;
            i = j;
        }
        }
    }
}

template <SymbolPreDef X>
static Inst* get_predef_with(Memory* memory) {
    Inst* inst = alloc_inst(memory);
    inst->tag = INST_ADDRESS;
    inst->body.as_u15 = static_cast<u16>(X);
    return inst;
}

static Inst* get_predef(Memory* memory, String string) {
    if ((string == TO_STR("R0")) || (string == TO_STR("SP"))) {
        return get_predef_with<PREDEF_R0_SP>(memory);
    }
    if ((string == TO_STR("R1")) || (string == TO_STR("LCL"))) {
        return get_predef_with<PREDEF_R1_LCL>(memory);
    }
    if ((string == TO_STR("R2")) || (string == TO_STR("ARG"))) {
        return get_predef_with<PREDEF_R2_ARG>(memory);
    }
    if ((string == TO_STR("R3")) || (string == TO_STR("THIS"))) {
        return get_predef_with<PREDEF_R3_THIS>(memory);
    }
    if ((string == TO_STR("R4")) || (string == TO_STR("THAT"))) {
        return get_predef_with<PREDEF_R4_THAT>(memory);
    }
    if (string == TO_STR("R5")) {
        return get_predef_with<PREDEF_R5>(memory);
    }
    if (string == TO_STR("R6")) {
        return get_predef_with<PREDEF_R6>(memory);
    }
    if (string == TO_STR("R7")) {
        return get_predef_with<PREDEF_R7>(memory);
    }
    if (string == TO_STR("R8")) {
        return get_predef_with<PREDEF_R8>(memory);
    }
    if (string == TO_STR("R9")) {
        return get_predef_with<PREDEF_R9>(memory);
    }
    if (string == TO_STR("R10")) {
        return get_predef_with<PREDEF_R10>(memory);
    }
    if (string == TO_STR("R11")) {
        return get_predef_with<PREDEF_R11>(memory);
    }
    if (string == TO_STR("R12")) {
        return get_predef_with<PREDEF_R12>(memory);
    }
    if (string == TO_STR("R13")) {
        return get_predef_with<PREDEF_R13>(memory);
    }
    if (string == TO_STR("R14")) {
        return get_predef_with<PREDEF_R14>(memory);
    }
    if (string == TO_STR("R15")) {
        return get_predef_with<PREDEF_R15>(memory);
    }
    if (string == TO_STR("SCREEN")) {
        return get_predef_with<PREDEF_SCREEN>(memory);
    }
    if (string == TO_STR("KBD")) {
        return get_predef_with<PREDEF_KBD>(memory);
    }
    return null;
}

static Token get_token(Memory* memory, u32 i) {
    EXIT_IF_PRINT(memory->len_tokens <= i,
                  memory,
                  memory->tokens[memory->len_tokens - 1].offset);
    return memory->tokens[i];
}

static void parse_address(Memory* memory, u32* i) {
    const Token token = get_token(memory, (*i)++);
    switch (token.tag) {
    case TOKEN_U15: {
        Inst* inst = alloc_inst(memory);
        inst->tag = INST_ADDRESS;
        inst->body.as_u15 = token.body.as_u15;
        break;
    }
    case TOKEN_STR: {
        if (get_predef(memory, token.body.as_string)) {
            return;
        }
        Inst* inst = alloc_inst(memory);
        inst->tag = INST_UNRESOLVED;
        inst->body.as_string = token.body.as_string;
        break;
    }
    case TOKEN_LPAREN:
    case TOKEN_RPAREN:
    case TOKEN_AT:
    case TOKEN_EQUALS:
    case TOKEN_SCOLON:
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_BANG:
    case TOKEN_AMPERS:
    case TOKEN_PIPE:
    default: {
        EXIT_PRINT(memory, token.offset);
    }
    }
}

static SymbolDest get_dest(Memory* memory, u32* i) {
    u8 dest = 0;
    for (u32 j = *i; j < memory->len_tokens; ++j) {
        const Token token = get_token(memory, j);
        if (token.tag == TOKEN_STR) {
            for (u32 k = 0; k < token.body.as_string.len; ++k) {
                u8 bit;
                switch (token.body.as_string.chars[k]) {
                case 'M': {
                    bit = static_cast<u8>(DEST_M);
                    break;
                }
                case 'D': {
                    bit = static_cast<u8>(DEST_D);
                    break;
                }
                case 'A': {
                    bit = static_cast<u8>(DEST_A);
                    break;
                }
                default: {
                    EXIT_PRINT(memory, token.offset);
                }
                }
                if (0 < (dest & bit)) {
                    EXIT_PRINT(memory, token.offset);
                }
                dest |= bit;
            }
        } else if (token.tag == TOKEN_EQUALS) {
            *i = j;
            break;
        } else {
            EXIT_PRINT(memory, token.offset);
        }
    }
    switch (dest) {
    case 0x01: {
        return DEST_M;
    }
    case 0x02: {
        return DEST_D;
    }
    case 0x03: {
        return DEST_MD;
    }
    case 0x04: {
        return DEST_A;
    }
    case 0x05: {
        return DEST_AM;
    }
    case 0x06: {
        return DEST_AD;
    }
    case 0x07: {
        return DEST_AMD;
    }
    default: {
        EXIT();
    }
    }
}

static SymbolComp get_comp(Memory* memory, u32* i) {
    const Token left = get_token(memory, *i);
    switch (left.tag) {
    case TOKEN_U15: {
        ++(*i);
        if (left.body.as_u15 == 0) {
            return COMP_ZERO;
        }
        if (left.body.as_u15 == 1) {
            return COMP_ONE;
        }
        break;
    }
    case TOKEN_MINUS: {
        const Token token = get_token(memory, ++(*i));
        ++(*i);
        if ((token.tag == TOKEN_U15) && (token.body.as_u15 == 1)) {
            return COMP_NEGATIVE_ONE;
        }
        if (token.tag == TOKEN_STR) {
            if (token.body.as_string == TO_STR("D")) {
                return COMP_NEGATIVE_D;
            }
            if (token.body.as_string == TO_STR("A")) {
                return COMP_NEGATIVE_A;
            }
            if (token.body.as_string == TO_STR("M")) {
                return COMP_NEGATIVE_M;
            }
        }
        break;
    }
    case TOKEN_BANG: {
        const Token token = get_token(memory, ++(*i));
        ++(*i);
        if (token.tag == TOKEN_STR) {
            if (token.body.as_string == TO_STR("D")) {
                return COMP_NOT_D;
            }
            if (token.body.as_string == TO_STR("A")) {
                return COMP_NOT_A;
            }
            if (token.body.as_string == TO_STR("M")) {
                return COMP_NOT_M;
            }
        }
        break;
    }
    case TOKEN_STR: {
        if (*i == (memory->len_tokens - 1)) {
            ++(*i);
            if (left.body.as_string == TO_STR("D")) {
                return COMP_D;
            }
            if (left.body.as_string == TO_STR("A")) {
                return COMP_A;
            }
            if (left.body.as_string == TO_STR("M")) {
                return COMP_M;
            }
        } else {
            const Token op = get_token(memory, ++(*i));
            const Token right = get_token(memory, ++(*i));
            ++(*i);
            if ((op.tag == TOKEN_PLUS) && (right.tag == TOKEN_U15) &&
                (right.body.as_u15 == 1))
            {
                if (left.body.as_string == TO_STR("D")) {
                    return COMP_D_PLUS_1;
                }
                if (left.body.as_string == TO_STR("A")) {
                    return COMP_A_PLUS_1;
                }
                if (left.body.as_string == TO_STR("M")) {
                    return COMP_M_PLUS_1;
                }
            }
            if ((op.tag == TOKEN_MINUS) && (right.tag == TOKEN_U15) &&
                (right.body.as_u15 == 1))
            {
                if (left.body.as_string == TO_STR("D")) {
                    return COMP_D_MINUS_1;
                }
                if (left.body.as_string == TO_STR("A")) {
                    return COMP_A_MINUS_1;
                }
                if (left.body.as_string == TO_STR("M")) {
                    return COMP_M_MINUS_1;
                }
            }
            if ((op.tag == TOKEN_PLUS) && (right.tag == TOKEN_STR)) {
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("A")))
                {
                    return COMP_D_PLUS_A;
                }
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("M")))
                {
                    return COMP_D_PLUS_M;
                }
            }
            if ((op.tag == TOKEN_MINUS) && (right.tag == TOKEN_STR)) {
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("A")))
                {
                    return COMP_D_MINUS_A;
                }
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("M")))
                {
                    return COMP_D_MINUS_M;
                }
                if ((left.body.as_string == TO_STR("A")) &&
                    (right.body.as_string == TO_STR("D")))
                {
                    return COMP_A_MINUS_D;
                }
                if ((left.body.as_string == TO_STR("M")) &&
                    (right.body.as_string == TO_STR("D")))
                {
                    return COMP_M_MINUS_D;
                }
            }
            if ((op.tag == TOKEN_AMPERS) && (right.tag == TOKEN_STR)) {
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("A")))
                {
                    return COMP_D_AND_A;
                }
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("M")))
                {
                    return COMP_D_AND_M;
                }
            }
            if ((op.tag == TOKEN_PIPE) && (right.tag == TOKEN_STR)) {
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("A")))
                {
                    return COMP_D_OR_A;
                }
                if ((left.body.as_string == TO_STR("D")) &&
                    (right.body.as_string == TO_STR("M")))
                {
                    return COMP_D_OR_M;
                }
            }
            if (left.tag == TOKEN_STR) {
                *i -= 2;
                if (left.body.as_string == TO_STR("D")) {
                    return COMP_D;
                }
                if (left.body.as_string == TO_STR("A")) {
                    return COMP_A;
                }
                if (left.body.as_string == TO_STR("M")) {
                    return COMP_M;
                }
            }
        }
        break;
    }
    case TOKEN_LPAREN:
    case TOKEN_RPAREN:
    case TOKEN_AT:
    case TOKEN_EQUALS:
    case TOKEN_SCOLON:
    case TOKEN_PLUS:
    case TOKEN_AMPERS:
    case TOKEN_PIPE:
    default: {
    }
    }
    EXIT_PRINT(memory, left.offset);
}

static SymbolJump get_jump(Memory* memory, u32* i) {
    const Token token = get_token(memory, (*i)++);
    if (token.body.as_string == TO_STR("JGT")) {
        return JUMP_JGT;
    }
    if (token.body.as_string == TO_STR("JEQ")) {
        return JUMP_JEQ;
    }
    if (token.body.as_string == TO_STR("JGE")) {
        return JUMP_JGE;
    }
    if (token.body.as_string == TO_STR("JLT")) {
        return JUMP_JLT;
    }
    if (token.body.as_string == TO_STR("JNE")) {
        return JUMP_JNE;
    }
    if (token.body.as_string == TO_STR("JLE")) {
        return JUMP_JLE;
    }
    if (token.body.as_string == TO_STR("JMP")) {
        return JUMP_JMP;
    }
    EXIT_PRINT(memory, token.offset);
}

static void parse_compute(Memory* memory, u32* i) {
    InstCompute compute = {COMP_ZERO, DEST_NULL, JUMP_NULL};
    for (u32 j = *i; j < memory->len_tokens; ++j) {
        const Token token = get_token(memory, j);
        if (token.tag == TOKEN_EQUALS) {
            EXIT_IF_PRINT(*i == j, memory, token.offset);
            compute.dest = get_dest(memory, i);
            EXIT_IF(*i != j);
            ++(*i);
            compute.comp = get_comp(memory, i);
            if (*i == memory->len_tokens) {
                break;
            }
            if (get_token(memory, *i).tag == TOKEN_SCOLON) {
                ++(*i);
                compute.jump = get_jump(memory, i);
            }
            break;
        } else if (token.tag == TOKEN_SCOLON) {
            EXIT_IF_PRINT(*i == j, memory, token.offset);
            compute.comp = get_comp(memory, i);
            EXIT_IF(*i != j);
            ++(*i);
            compute.jump = get_jump(memory, i);
            break;
        }
    }
    Inst* inst = alloc_inst(memory);
    inst->tag = INST_COMPUTE;
    inst->body.as_compute = compute;
}

static void parse_label(Memory* memory, u32* i) {
    {
        const Token token = get_token(memory, *i);
        if (token.tag != TOKEN_STR) {
            EXIT_PRINT(memory, token.offset);
        }
        insert(&memory->labels,
               token.body.as_string,
               static_cast<u16>(memory->len_insts));
    }
    {
        const Token token = get_token(memory, ++(*i));
        EXIT_IF_PRINT(token.tag != TOKEN_RPAREN, memory, token.offset);
        ++(*i);
    }
}

static void set_insts(Memory* memory) {
    memory->len_insts = 0;
    for (u32 i = 0; i < memory->len_tokens;) {
        const Token token = get_token(memory, i);
        switch (token.tag) {
        case TOKEN_AT: {
            ++i;
            EXIT_IF_PRINT(memory->len_tokens <= i, memory, token.offset);
            parse_address(memory, &i);
            break;
        }
        case TOKEN_LPAREN: {
            ++i;
            EXIT_IF_PRINT(memory->len_tokens <= i, memory, token.offset);
            parse_label(memory, &i);
            break;
        }
        case TOKEN_STR:
        case TOKEN_U15: {
            parse_compute(memory, &i);
            break;
        }
        case TOKEN_RPAREN:
        case TOKEN_EQUALS:
        case TOKEN_SCOLON:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_BANG:
        case TOKEN_AMPERS:
        case TOKEN_PIPE:
        default: {
            EXIT_PRINT(memory, token.offset);
        }
        }
    }
}

static void resolve_labels(Memory* memory) {
    for (u32 i = 0; i < memory->len_insts; ++i) {
        Inst* inst = &memory->insts[i];
        if (inst->tag == INST_UNRESOLVED) {
            {
                const u16* address =
                    lookup(&memory->labels, inst->body.as_string);
                if (address) {
                    inst->tag = INST_ADDRESS;
                    inst->body.as_u15 = *address;
                    goto next;
                }
            }
            {
                const u16* address =
                    lookup(&memory->vars, inst->body.as_string);
                if (address) {
                    inst->tag = INST_ADDRESS;
                    inst->body.as_u15 = *address;
                    goto next;
                }
            }
            const u16 address =
                static_cast<u16>(memory->vars.len) + OFFSET_VARS;
            insert(&memory->vars, inst->body.as_string, address);
            inst->tag = INST_ADDRESS;
            inst->body.as_u15 = address;
        }
    next:;
    }
}

static void set_bytes(char* chars, u16 bytes) {
    memcpy(&chars[0], BYTES[(bytes >> 12u) & 0xFu], 4);
    memcpy(&chars[4], BYTES[(bytes >> 8u) & 0xFu], 4);
    memcpy(&chars[8], BYTES[(bytes >> 4u) & 0xFu], 4);
    memcpy(&chars[12], BYTES[bytes & 0xFu], 4);
    chars[16] = '\n';
}

static void emit(Memory* memory, const char* path) {
    const u32 n = memory->len_insts * 17;
    EXIT_IF(CAP_CHARS <= n);
    for (u32 i = 0; i < memory->len_insts; ++i) {
        const Inst inst = memory->insts[i];
        switch (inst.tag) {
        case INST_ADDRESS: {
            set_bytes(&memory->chars[i * 17], inst.body.as_u15);
            break;
        }
        case INST_COMPUTE: {
            set_bytes(&memory->chars[i * 17],
                      static_cast<u16>(
                          (7u << 13u) |
                          static_cast<u32>(inst.body.as_compute.comp) << 6u |
                          static_cast<u32>(inst.body.as_compute.dest) << 3u |
                          static_cast<u32>(inst.body.as_compute.jump)));
            break;
        }
        case INST_UNRESOLVED:
        default: {
            EXIT();
        }
        }
    }
    {
        File* file = fopen(path, "wb");
        EXIT_IF(!file);
        EXIT_IF(fwrite(memory->chars, sizeof(char), n, file) != n);
        fclose(file);
    }
}

static void* alloc(usize size) {
    void* memory = mmap(null,
                        size,
                        PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE,
                        -1,
                        0);
    EXIT_IF(memory == MAP_FAILED);
    return memory;
}

i32 main(i32 n, char** args) {
    fprintf(stderr,
            "\n"
            "sizeof(String)                         : %zu\n"
            "sizeof(Token)                          : %zu\n"
            "sizeof(SymbolComp)                     : %zu\n"
            "sizeof(SymbolPreDef)                   : %zu\n"
            "sizeof(Inst)                           : %zu\n"
            "sizeof(Table<String, u16, CAP_LABELS>) : %zu\n"
            "sizeof(Table<String, u16, CAP_VARS>)   : %zu\n"
            "sizeof(Memory)                         : %zu\n"
            "\n",
            sizeof(String),
            sizeof(Token),
            sizeof(SymbolComp),
            sizeof(SymbolPreDef),
            sizeof(Inst),
            sizeof(Table<String, u16, CAP_LABELS>),
            sizeof(Table<String, u16, CAP_VARS>),
            sizeof(Memory));
    EXIT_IF(n < 3);
    {
        Memory* memory = reinterpret_cast<Memory*>(alloc(sizeof(Memory)));
        set_chars_from_file(memory, args[1]);
        set_tokens(memory);
        set_insts(memory);
        resolve_labels(memory);
#ifdef DEBUG
        fprintf(stderr, "\n");
#endif
        fprintf(stderr,
                "memory->labels.len        : %u\n"
                "memory->labels.collisions : %u\n"
                "memory->vars.len          : %u\n"
                "memory->vars.collisions   : %u\n"
                "\n",
                memory->labels.len,
                memory->labels.collisions,
                memory->vars.len,
                memory->vars.collisions);
        emit(memory, args[2]);
    }
    fprintf(stderr, "Done!\n");
    return EXIT_SUCCESS;
}
