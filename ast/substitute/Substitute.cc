#include "ast/substitute/substitute.h"
#include "ast/Helpers.h"
#include "ast/treemap/treemap.h"
#include "common/typecase.h"
#include "core/GlobalSubstitution.h"
namespace sorbet::ast {

namespace {
class SubstWalk {
private:
    const core::GlobalSubstitution &subst;

    ExprPtr substClassName(core::MutableContext ctx, ExprPtr node) {
        auto constLit = cast_tree<UnresolvedConstantLit>(node.get());
        if (constLit == nullptr) { // uncommon case. something is strange
            if (isa_tree<EmptyTree>(node.get())) {
                return node;
            }
            return TreeMap::apply(ctx, *this, move(node));
        }

        auto scope = substClassName(ctx, move(constLit->scope));
        auto cnst = subst.substitute(constLit->cnst);

        return make_unique<UnresolvedConstantLit>(constLit->loc, move(scope), cnst);
    }

    ExprPtr substArg(core::MutableContext ctx, ExprPtr argp) {
        Expression *arg = argp.get();
        while (arg != nullptr) {
            typecase(
                arg, [&](RestArg *rest) { arg = rest->expr.get(); }, [&](KeywordArg *kw) { arg = kw->expr.get(); },
                [&](OptionalArg *opt) { arg = opt->expr.get(); }, [&](BlockArg *opt) { arg = opt->expr.get(); },
                [&](ShadowArg *opt) { arg = opt->expr.get(); },
                [&](Local *local) {
                    local->localVariable._name = subst.substitute(local->localVariable._name);
                    arg = nullptr;
                },
                [&](UnresolvedIdent *nm) { Exception::raise("UnresolvedIdent remaining after local_vars"); });
        }
        return argp;
    }

public:
    SubstWalk(const core::GlobalSubstitution &subst) : subst(subst) {}

    unique_ptr<ClassDef> preTransformClassDef(core::MutableContext ctx, unique_ptr<ClassDef> original) {
        original->name = substClassName(ctx, move(original->name));
        for (auto &anc : original->ancestors) {
            anc = substClassName(ctx, move(anc));
        }
        return original;
    }

    unique_ptr<MethodDef> preTransformMethodDef(core::MutableContext ctx, unique_ptr<MethodDef> original) {
        original->name = subst.substitute(original->name);
        for (auto &arg : original->args) {
            arg = substArg(ctx, move(arg));
        }
        return original;
    }

    unique_ptr<Block> preTransformBlock(core::MutableContext ctx, unique_ptr<Block> original) {
        for (auto &arg : original->args) {
            arg = substArg(ctx, move(arg));
        }
        return original;
    }

    ExprPtr postTransformUnresolvedIdent(core::MutableContext ctx,
                                                        unique_ptr<UnresolvedIdent> original) {
        original->name = subst.substitute(original->name);
        return original;
    }

    ExprPtr postTransformLocal(core::MutableContext ctx, unique_ptr<Local> local) {
        local->localVariable._name = subst.substitute(local->localVariable._name);
        return local;
    }

    unique_ptr<Send> preTransformSend(core::MutableContext ctx, unique_ptr<Send> original) {
        original->fun = subst.substitute(original->fun);
        return original;
    }

    ExprPtr postTransformLiteral(core::MutableContext ctx, unique_ptr<Literal> original) {
        if (original->isString(ctx)) {
            auto nameRef = original->asString(ctx);
            // The 'from' and 'to' GlobalState in this substitution will always be the same,
            // because the newly created nameRef reuses our current GlobalState id
            bool allowSameFromTo = true;
            auto newName = subst.substitute(nameRef, allowSameFromTo);
            if (newName == nameRef) {
                return original;
            }
            return MK::String(original->loc, newName);
        }
        if (original->isSymbol(ctx)) {
            auto nameRef = original->asSymbol(ctx);
            // The 'from' and 'to' GlobalState in this substitution will always be the same,
            // because the newly created nameRef reuses our current GlobalState id
            bool allowSameFromTo = true;
            auto newName = subst.substitute(nameRef, allowSameFromTo);
            if (newName == nameRef) {
                return original;
            }
            return MK::Symbol(original->loc, newName);
        }
        return original;
    }

    ExprPtr postTransformUnresolvedConstantLit(core::MutableContext ctx,
                                                              unique_ptr<UnresolvedConstantLit> original) {
        original->cnst = subst.substitute(original->cnst);
        original->scope = substClassName(ctx, move(original->scope));
        return original;
    }
};
} // namespace

ExprPtr Substitute::run(core::MutableContext ctx, const core::GlobalSubstitution &subst,
                                       ExprPtr what) {
    if (subst.useFastPath()) {
        return what;
    }
    SubstWalk walk(subst);
    what = TreeMap::apply(ctx, walk, move(what));
    return what;
}

} // namespace sorbet::ast
