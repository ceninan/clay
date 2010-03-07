#include "clay.hpp"


struct LambdaContext {
    bool isBlockLambda;
    EnvPtr nonLocalEnv;
    const string &closureDataName;
    vector<string> &freeVars;
    LambdaContext(bool isBlockLambda,
                  EnvPtr nonLocalEnv,
                  const string &closureDataName,
                  vector<string> &freeVars)
        : isBlockLambda(isBlockLambda), nonLocalEnv(nonLocalEnv),
          closureDataName(closureDataName), freeVars(freeVars) {}
};

void convertFreeVars(LambdaPtr x, EnvPtr env,
                     const string &closureDataName,
                     vector<string> &freeVars);

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx);

void  convertFreeVars(ExprPtr &x, EnvPtr env, LambdaContext &ctx);



//
// initializeLambda
//

static int closureDataIndex = 0;

void initializeLambda(LambdaPtr x, EnvPtr env)
{
    assert(!x->initialized);
    x->initialized = true;

    RecordPtr r = new Record();
    r->location = x->location;
    r->name = new Identifier("LambdaFreeVars");
    x->lambdaRecord = r;

    TypePtr t = recordType(r, vector<ObjectPtr>());
    x->lambdaType = t;
    ExprPtr typeExpr = new ObjectExpr(t.ptr());

    ostringstream ostr;
    ostr << "%closureData" << closureDataIndex;
    ++closureDataIndex;
    string closureDataName = ostr.str();

    vector<string> freeVars;
    convertFreeVars(x, env, closureDataName, freeVars);

    CallPtr converted = new Call(typeExpr);
    for (unsigned i = 0; i < freeVars.size(); ++i) {
        IdentifierPtr ident = new Identifier(freeVars[i]);
        NameRefPtr nameRef = new NameRef(ident);
        if (x->isBlockLambda) {
            ExprPtr addr = new UnaryOp(ADDRESS_OF, nameRef.ptr());
            converted->args.push_back(addr);
        }
        else {
            converted->args.push_back(nameRef.ptr());
        }

        TypePtr type;
        ObjectPtr obj = lookupEnv(env, ident);
        switch (obj->objKind) {
        case PVALUE : {
            PValue *y = (PValue *)obj.ptr();
            type = y->type;
            break;
        }
        case CVALUE : {
            CValue *y = (CValue *)obj.ptr();
            type = y->type;
            break;
        }
        default :
            assert(false);
        }

        if (x->isBlockLambda)
            type = pointerType(type);

        ExprPtr fieldType = new ObjectExpr(type.ptr());
        RecordFieldPtr field = new RecordField(ident, fieldType);
        r->fields.push_back(field);
    }

    x->converted = converted.ptr();

    CodePtr code = new Code();
    code->location = x->location;
    IdentifierPtr closureDataIdent = new Identifier(closureDataName);
    ValueArgPtr closureDataArg = new ValueArg(closureDataIdent, typeExpr);
    code->formalArgs.push_back(closureDataArg.ptr());
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        ValueArgPtr y = new ValueArg(x->formalArgs[i], NULL);
        y->location = x->formalArgs[i]->location;
        code->formalArgs.push_back(y.ptr());
    }
    code->body = x->body;

    OverloadPtr overload = new Overload(kernelNameRef("call"), code);
    overload->env = env;
    overload->location = x->location;
    ObjectPtr obj = kernelName("call");
    if (obj->objKind != OVERLOADABLE)
        error("'call' operator not found!");
    Overloadable *callObj = (Overloadable *)obj.ptr();
    callObj->overloads.insert(callObj->overloads.begin(), overload);
    if (!callObj->staticFlagsInitialized)
        initIsStaticFlags(callObj);
    else
        updateIsStaticFlags(obj, overload);
}



//
// convertFreeVars
//

void convertFreeVars(LambdaPtr x, EnvPtr env,
                     const string &closureDataName,
                     vector<string> &freeVars)
{
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        IdentifierPtr name = x->formalArgs[i];
        addLocal(env2, name, name.ptr());
    }
    LambdaContext ctx(x->isBlockLambda, env, closureDataName, freeVars);
    convertFreeVars(x->body, env2, ctx);
}

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->objKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        for (unsigned i = 0; i < y->statements.size(); ++i) {
            StatementPtr z = y->statements[i];
            if (z->objKind == BINDING) {
                Binding *a = (Binding *)z.ptr();
                convertFreeVars(a->expr, env, ctx);
                EnvPtr env2 = new Env(env);
                addLocal(env2, a->name, a->name.ptr());
                env = env2;
            }
            else {
                convertFreeVars(z, env, ctx);
            }
        }
        break;
    }

    case LABEL :
    case BINDING : {
        break;
    }

    case ASSIGNMENT : {
        Assignment *y = (Assignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *y = (InitAssignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *y = (UpdateAssignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case GOTO : {
        break;
    }

    case RETURN : {
        Return *y = (Return *)x.ptr();
        if (y->expr.ptr())
            convertFreeVars(y->expr, env, ctx);
        break;
    }

    case RETURN_REF : {
        ReturnRef *y = (ReturnRef *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case IF : {
        If *y = (If *)x.ptr();
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->thenPart, env, ctx);
        if (y->elsePart.ptr())
            convertFreeVars(y->elsePart, env, ctx);
        break;
    }

    case EXPR_STATEMENT : {
        ExprStatement *y = (ExprStatement *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case WHILE : {
        While *y = (While *)x.ptr();
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->body, env, ctx);
        break;
    }

    case BREAK :
    case CONTINUE : {
        break;
    }

    case FOR : {
        For *y = (For *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        EnvPtr env2 = new Env(env);
        addLocal(env2, y->variable, y->variable.ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    default :
        assert(false);

    }
}

void convertFreeVars(ExprPtr &x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->objKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL :
    case CHAR_LITERAL :
    case STRING_LITERAL :
        break;

    case NAME_REF : {
        NameRef *y = (NameRef *)x.ptr();
        bool isNonLocal = false;
        bool isGlobal = false;
        ObjectPtr z = lookupEnvEx(env, y->name, ctx.nonLocalEnv,
                                  isNonLocal, isGlobal);
        if (isNonLocal && !isGlobal) {
            if ((z->objKind == PVALUE) || (z->objKind == CVALUE)) {
                vector<string>::iterator i =
                    std::find(ctx.freeVars.begin(), ctx.freeVars.end(),
                              y->name->str);
                if (i == ctx.freeVars.end()) {
                    ctx.freeVars.push_back(y->name->str);
                }
                IdentifierPtr a = new Identifier(ctx.closureDataName);
                a->location = y->location;
                NameRefPtr b = new NameRef(a);
                b->location = y->location;
                FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                c->location = y->location;
                if (ctx.isBlockLambda) {
                    ExprPtr d = new UnaryOp(DEREFERENCE, c.ptr());
                    d->location = y->location;
                    x = d.ptr();
                }
                else {
                    x = c.ptr();
                }
            }
        }
        break;
    }

    case RETURNED :
        break;

    case TUPLE : {
        Tuple *y = (Tuple *)x.ptr();
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertFreeVars(y->args[i], env, ctx);
        break;
    }

    case ARRAY : {
        Array *y = (Array *)x.ptr();
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertFreeVars(y->args[i], env, ctx);
        break;
    }

    case INDEXING : {
        Indexing *y = (Indexing *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertFreeVars(y->args[i], env, ctx);
        break;
    }

    case CALL : {
        Call *y = (Call *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertFreeVars(y->args[i], env, ctx);
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case TUPLE_REF : {
        TupleRef *y = (TupleRef *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case UNARY_OP : {
        UnaryOp *y = (UnaryOp *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case BINARY_OP : {
        BinaryOp *y = (BinaryOp *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case AND : {
        And *y = (And *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case OR : {
        Or *y = (Or *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < y->formalArgs.size(); ++i)
            addLocal(env2, y->formalArgs[i], y->formalArgs[i].ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    case SC_EXPR :
    case OBJECT_EXPR :
        break;

    default :
        assert(false);

    }
}