#include <stdio.h> // DEBUG

#include "nasl.h"
#include "code.h"

// FIXME: need to store a list of all contexts
struct Context globalContext;

char* opStringDEBUG(int op)
{
    switch(op) {
    case OP_AND: return "AND";
    case OP_OR: return "OR";
    case OP_NOT: return "NOT";
    case OP_MUL: return "MUL";
    case OP_PLUS: return "PLUS";
    case OP_MINUS: return "MINUS";
    case OP_DIV: return "DIV";
    case OP_CAT: return "CAT";
    case OP_LT: return "LT";
    case OP_LTE: return "LTE";
    case OP_EQ: return "EQ";
    case OP_NEQ: return "NEQ";
    case OP_GT: return "GT";
    case OP_GTE: return "GTE";
    case OP_RETURN: return "RETURN";
    case OP_ASSIGN: return "ASSIGN";
    case OP_DUP: return "DUP";
    case OP_PUSHCONST: return "PUSHCONST";
    case OP_PUSHNIL: return "PUSHNIL";
    case OP_INDEX: return "INDEX";
    case OP_MEMBER: return "MEMBER";
    case OP_POP: return "POP";
    case OP_SYMBOL: return "SYMBOL";
    }
    return "<bad opcode>";
}

static void initContext(struct Context* c)
{
    int i;
    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_init(&(c->pools[i]), naTypeSize(i));

    c->fTop = c->opTop = 0;

    BZERO(c->fStack, MAX_RECURSION * sizeof(struct Frame));
    BZERO(c->opStack, MAX_STACK_SIZE * sizeof(naRef));
}

struct Context* naNewContext()
{
    // FIXME: need more than one!
    struct Context* c = &globalContext;
    initContext(c);
    return c;
}

void naGarbageCollect()
{
    int i;
    struct Context* c = &globalContext; // FIXME: more than one!
    for(i=0; i <= c->fTop; i++) {
        struct Frame* f = &(c->fStack[i]);
        naGC_mark(f->code);
        naGC_mark(f->namespace);
        naGC_mark(f->locals);
    }
    for(i=0; i <= c->opTop; i++)
        naGC_mark(c->opStack[i]);

    for(i=0; i<NUM_NASL_TYPES; i++)
        naGC_reap(&(c->pools[i]));

    // FIXME: need to include constants for Parser's during
    // compilation, they aren't referenced anywhere yet!
}

void setupFuncall(struct Context* ctx, naRef closure)
{
    struct Frame* f;

    if(ctx->fTop >= MAX_RECURSION) ERR("recursion too deep");

    f = &(ctx->fStack[ctx->fTop++]);
    f->code = closure.ref.ptr.closure->code;
    f->namespace = closure.ref.ptr.closure->namespace;
    f->locals = naNewHash(ctx);
    f->ip = 0;
    f->bp = ctx->opTop;
}

static double numify(naRef o)
{
    if(IS_NUM(o)) return o.num;
    else if(!IS_STR(o)) ERR("non-scalar in numeric context");
    return (naStr_tonum(o)).num;   
}

static naRef stringify(naRef r)
{
    naRef result;
    if(IS_STR(r)) return r;
    if(IS_NUM(r)) {
        naStr_fromnum(result, r.num);
        return result;
    }
    ERR("non-scalar in string context");
    return naNil();
}

static naRef evalAndOr(int op, naRef ra, naRef rb)
{
    int a = naTrue(ra);
    int b = naTrue(rb);
    int result;
    if(op == OP_AND) result = a && b ? 1 : 0;
    else             result = a || b ? 1 : 0;
    return naNum(result);
}

static naRef evalBinaryNumeric(int op, naRef ra, naRef rb)
{
    double a = numify(ra), b = numify(rb);
    switch(op) {
    case OP_PLUS:  return naNum(a + b);
    case OP_MINUS: return naNum(a - b);
    case OP_MUL:   return naNum(a * b);
    case OP_DIV:   return naNum(a / b);
    case OP_LT:    return naNum(a < b ? 1 : 0);
    case OP_LTE:   return naNum(a <= b ? 1 : 0);
    case OP_GT:    return naNum(a > b ? 1 : 0);
    case OP_GTE:   return naNum(a >= b ? 1 : 0);
    }
    return naNil();
}

static naRef getSymbol(struct Frame* f, naRef sym)
{
    // Locals first, then the function closure
    naRef result = naHash_get(f->locals, sym);
    if(IS_NIL(result)) result = naHash_get(f->namespace, sym);
    if(IS_NIL(result)) ERR("undefined symbol");
    return result;
}

static inline void PUSH(struct Context* ctx, naRef r)
{
    ctx->opStack[ctx->opTop++] = r;
}

static inline naRef POP(struct Context* ctx)
{
    return ctx->opStack[--ctx->opTop];
}

static inline int ARG16(unsigned char* byteCode, struct Frame* f)
{
    int arg = byteCode[f->ip]<<8 | byteCode[f->ip+1];
    f->ip += 2;
    return arg;
}

static void run1(struct Context* ctx)
{
    naRef a, b, c;
    struct Frame* f = &(ctx->fStack[ctx->fTop-1]);
    struct naCode* cd = f->code.ref.ptr.code;
    int i, op = cd->byteCode[f->ip++];

    printf("OP: %s\n", opStringDEBUG(op));
    switch(op) {
    case OP_POP:
        POP(ctx);
        break;
    case OP_PLUS: case OP_MINUS: case OP_MUL: case OP_DIV:
    case OP_LT: case OP_LTE: case OP_GT: case OP_GTE:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalBinaryNumeric(op, b, a));
        break;
    case OP_AND: case OP_OR:
        a = POP(ctx); b = POP(ctx);
        PUSH(ctx, evalAndOr(op, a, b));
        break;
    case OP_NOT:
        a = POP(ctx);
        PUSH(ctx, naNum(naTrue(a) ? 0 : 1));
        break;
    case OP_PUSHCONST:
        i = ARG16(cd->byteCode, f);
        PUSH(ctx, cd->constants[i]);
        break;
    case OP_PUSHNIL:
        PUSH(ctx, naNil());
        break;
    case OP_MEMBER:
        a = POP(ctx); b = POP(ctx);
        if(!IS_HASH(b)) ERR("non-objects have no members");
        c = naHash_get(b, a);
        if(IS_NIL(c)) ERR("no such member");
        PUSH(ctx, c);
        break;
    case OP_CAT:
        a = stringify(POP(ctx)); b = stringify(POP(ctx));
        c = naNewString(ctx);
        naStr_concat(c, b, a);
        PUSH(ctx, c);
        break;
    case OP_SYMBOL:
        a = getSymbol(f, POP(ctx));
        PUSH(ctx, a);
        break;
    }

    // Are we done now?
    if(f->ip >= cd->nBytes)
        ctx->done = 1;
}

void printRefDEBUG(naRef r)
{
    if(IS_NUM(r)) {
        printf("%f\n", r.num);
    } else if(IS_NIL(r)) {
        printf("<nil>\n");
    } else if(IS_STR(r)) {
        int j;
        printf("\"");
        for(j=0; j<r.ref.ptr.str->len; j++)
            printf("%c", r.ref.ptr.str->data[j]);
        printf("\"\n");
    } else *(int*)0=0;
}

void printStack(struct Context* ctx)
{
    int i;
    printf("Stack:\n");
    for(i=ctx->opTop-1; i>=0; i--)
        printRefDEBUG(ctx->opStack[i]);
    printf("--\n");
}


void naRun(struct Context* ctx, naRef code)
{
    naRef namespace, closure;

    namespace = naNewHash(ctx);
    
    closure = naNewClosure(ctx);
    closure.ref.ptr.closure->code = code;
    closure.ref.ptr.closure->namespace = namespace;

    { // DEBUG
        int i;
        struct naCode* c = code.ref.ptr.code;
        printf("Constants:\n");
        for(i=0; i<c->nConstants; i++) {
            printf("%d ", i);
            printRefDEBUG(c->constants[i]);
        }
        printf("--\n");
    } // DEBUG

    setupFuncall(ctx, closure);

    ctx->done = 0;
    while(!ctx->done) {
        printStack(ctx); // DEBUG
        run1(ctx);
    }

    printf("DONE:\n"); // DEBUG 
    printStack(ctx); // DEBUG
}
