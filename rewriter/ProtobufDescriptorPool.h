#ifndef SORBET_REWRITER_PROTOBUF_DESCRIPTOR_POOL_H
#define SORBET_REWRITER_PROTOBUF_DESCRIPTOR_POOL_H
#include "ast/ast.h"

namespace sorbet::rewriter {

/**
 * This class desugars things of the form
 *
 *   A = ::Google::Protobuf::DescriptorPool.generated_pool.lookup("...").msgclass
 *   A::B = ::Google::Protobuf::DescriptorPool.generated_pool.lookup("...").enummodule
 *
 * into
 *
 *   class A
 *     ::Google::Protobuf::DescriptorPool.generated_pool.lookup("...").msgclass
 *   end
 *
 *   module A::B
 *     ::Google::Protobuf::DescriptorPool.generated_pool.lookup("...").enummodule
 *   end
 *
 * The idea is that the this way Sorbet will statically see `A` and `B` for what they are:
 * class symbols, not static field symbols.
 *
 * The missing method script will then fill in any methods that these classes have.
 */
class ProtobufDescriptorPool final {
public:
    static std::vector<ast::TreePtr> run(core::MutableContext ctx, ast::Assign *asgn);

    ProtobufDescriptorPool() = delete;
};

} // namespace sorbet::rewriter

#endif
