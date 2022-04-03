/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/sksl/ir/SkSLPrefixExpression.h"

#include "include/sksl/SkSLErrorReporter.h"
#include "src/sksl/SkSLAnalysis.h"
#include "src/sksl/SkSLConstantFolder.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/ir/SkSLConstructor.h"
#include "src/sksl/ir/SkSLConstructorArray.h"
#include "src/sksl/ir/SkSLConstructorCompound.h"
#include "src/sksl/ir/SkSLConstructorDiagonalMatrix.h"
#include "src/sksl/ir/SkSLConstructorSplat.h"
#include "src/sksl/ir/SkSLLiteral.h"
#include "src/sksl/ir/SkSLVariableReference.h"

namespace SkSL {

static ExpressionArray negate_operands(const Context& context, const ExpressionArray& operands);

static std::unique_ptr<Expression> simplify_negation(const Context& context,
                                                     const Expression& originalExpr) {
    const Expression* value = ConstantFolder::GetConstantValueForVariable(originalExpr);
    switch (value->kind()) {
        case Expression::Kind::kLiteral: {
            // Convert -literal(1) to literal(-1).
            double negated = -value->as<Literal>().value();
            // Don't simplify the expression if the type can't hold the negated value.
            const Type& type = value->type();
            if (type.checkForOutOfRangeLiteral(context, negated, value->fPosition)) {
                return nullptr;
            }
            return Literal::Make(originalExpr.fPosition, negated, &type);
        }
        case Expression::Kind::kPrefix: {
            // Convert `-(-expression)` into `expression`.
            const PrefixExpression& prefix = value->as<PrefixExpression>();
            if (prefix.getOperator().kind() == Operator::Kind::MINUS) {
                return prefix.operand()->clone();
            }
            break;
        }
        case Expression::Kind::kConstructorArray:
            // Convert `-array[N](literal, ...)` into `array[N](-literal, ...)`.
            if (value->isCompileTimeConstant()) {
                const ConstructorArray& ctor = value->as<ConstructorArray>();
                return ConstructorArray::Make(context, originalExpr.fPosition, ctor.type(),
                                              negate_operands(context, ctor.arguments()));
            }
            break;

        case Expression::Kind::kConstructorDiagonalMatrix:
            // Convert `-matrix(literal)` into `matrix(-literal)`.
            if (value->isCompileTimeConstant()) {
                const ConstructorDiagonalMatrix& ctor = value->as<ConstructorDiagonalMatrix>();
                if (std::unique_ptr<Expression> simplified = simplify_negation(context,
                                                                               *ctor.argument())) {
                    return ConstructorDiagonalMatrix::Make(context, originalExpr.fPosition,
                            ctor.type(), std::move(simplified));
                }
            }
            break;

        case Expression::Kind::kConstructorSplat:
            // Convert `-vector(literal)` into `vector(-literal)`.
            if (value->isCompileTimeConstant()) {
                const ConstructorSplat& ctor = value->as<ConstructorSplat>();
                if (std::unique_ptr<Expression> simplified = simplify_negation(context,
                                                                               *ctor.argument())) {
                    return ConstructorSplat::Make(context, originalExpr.fPosition, ctor.type(),
                                                  std::move(simplified));
                }
            }
            break;

        case Expression::Kind::kConstructorCompound:
            // Convert `-vecN(literal, ...)` into `vecN(-literal, ...)`.
            if (value->isCompileTimeConstant()) {
                const ConstructorCompound& ctor = value->as<ConstructorCompound>();
                return ConstructorCompound::Make(context, originalExpr.fPosition, ctor.type(),
                                                 negate_operands(context, ctor.arguments()));
            }
            break;

        default:
            break;
    }
    return nullptr;
}

static ExpressionArray negate_operands(const Context& context, const ExpressionArray& array) {
    ExpressionArray replacement;
    replacement.reserve_back(array.size());
    for (const std::unique_ptr<Expression>& expr : array) {
        // The logic below is very similar to `negate_operand`, but with different ownership rules.
        if (std::unique_ptr<Expression> simplified = simplify_negation(context, *expr)) {
            replacement.push_back(std::move(simplified));
        } else {
            replacement.push_back(std::make_unique<PrefixExpression>(Operator::Kind::MINUS,
                                                                     expr->clone()));
        }
    }
    return replacement;
}

static std::unique_ptr<Expression> negate_operand(const Context& context,
                                                  std::unique_ptr<Expression> value) {
    // Attempt to simplify this negation (e.g. eliminate double negation, literal values)
    if (std::unique_ptr<Expression> simplified = simplify_negation(context, *value)) {
        return simplified;
    }

    // No simplified form; convert expression to Prefix(TK_MINUS, expression).
    return std::make_unique<PrefixExpression>(Operator::Kind::MINUS, std::move(value));
}

static std::unique_ptr<Expression> logical_not_operand(const Context& context,
                                                       std::unique_ptr<Expression> operand) {
    const Expression* value = ConstantFolder::GetConstantValueForVariable(*operand);
    switch (value->kind()) {
        case Expression::Kind::kLiteral: {
            // Convert !boolLiteral(true) to boolLiteral(false).
            SkASSERT(value->type().isBoolean());
            const Literal& b = value->as<Literal>();
            return Literal::MakeBool(operand->fPosition, !b.boolValue(), &operand->type());
        }
        case Expression::Kind::kPrefix: {
            // Convert `!(!expression)` into `expression`.
            PrefixExpression& prefix = operand->as<PrefixExpression>();
            if (prefix.getOperator().kind() == Operator::Kind::LOGICALNOT) {
                return std::move(prefix.operand());
            }
            break;
        }
        default:
            break;
    }

    // No simplified form; convert expression to Prefix(TK_LOGICALNOT, expression).
    return std::make_unique<PrefixExpression>(Operator::Kind::LOGICALNOT, std::move(operand));
}

std::unique_ptr<Expression> PrefixExpression::Convert(const Context& context,
                                                      Operator op,
                                                      std::unique_ptr<Expression> base) {
    const Type& baseType = base->type();
    switch (op.kind()) {
        case Operator::Kind::PLUS:
            if (baseType.isArray() || !baseType.componentType().isNumber()) {
                context.fErrors->error(base->fPosition,
                                       "'+' cannot operate on '" + baseType.displayName() + "'");
                return nullptr;
            }
            break;

        case Operator::Kind::MINUS:
            if (baseType.isArray() || !baseType.componentType().isNumber()) {
                context.fErrors->error(base->fPosition,
                                       "'-' cannot operate on '" + baseType.displayName() + "'");
                return nullptr;
            }
            break;

        case Operator::Kind::PLUSPLUS:
        case Operator::Kind::MINUSMINUS:
            if (!baseType.isNumber()) {
                context.fErrors->error(base->fPosition,
                                       "'" + std::string(op.tightOperatorName()) +
                                       "' cannot operate on '" + baseType.displayName() + "'");
                return nullptr;
            }
            if (!Analysis::UpdateVariableRefKind(base.get(), VariableReference::RefKind::kReadWrite,
                                                 context.fErrors)) {
                return nullptr;
            }
            break;

        case Operator::Kind::LOGICALNOT:
            if (!baseType.isBoolean()) {
                context.fErrors->error(base->fPosition,
                                       "'" + std::string(op.tightOperatorName()) +
                                       "' cannot operate on '" + baseType.displayName() + "'");
                return nullptr;
            }
            break;

        case Operator::Kind::BITWISENOT:
            if (context.fConfig->strictES2Mode()) {
                // GLSL ES 1.00, Section 5.1
                context.fErrors->error(
                        base->fPosition,
                        "operator '" + std::string(op.tightOperatorName()) + "' is not allowed");
                return nullptr;
            }
            if (baseType.isArray() || !baseType.componentType().isInteger()) {
                context.fErrors->error(base->fPosition,
                                       "'" + std::string(op.tightOperatorName()) +
                                       "' cannot operate on '" + baseType.displayName() + "'");
                return nullptr;
            }
            if (baseType.isLiteral()) {
                // The expression `~123` is no longer a literal; coerce to the actual type.
                base = baseType.scalarTypeForLiteral().coerceExpression(std::move(base), context);
                if (!base) {
                    return nullptr;
                }
            }
            break;

        default:
            SK_ABORT("unsupported prefix operator");
    }

    return PrefixExpression::Make(context, op, std::move(base));
}

std::unique_ptr<Expression> PrefixExpression::Make(const Context& context, Operator op,
                                                   std::unique_ptr<Expression> base) {
    switch (op.kind()) {
        case Operator::Kind::PLUS:
            SkASSERT(!base->type().isArray());
            SkASSERT(base->type().componentType().isNumber());
            return base;

        case Operator::Kind::MINUS:
            SkASSERT(!base->type().isArray());
            SkASSERT(base->type().componentType().isNumber());
            return negate_operand(context, std::move(base));

        case Operator::Kind::LOGICALNOT:
            SkASSERT(base->type().isBoolean());
            return logical_not_operand(context, std::move(base));

        case Operator::Kind::PLUSPLUS:
        case Operator::Kind::MINUSMINUS:
            SkASSERT(base->type().isNumber());
            SkASSERT(Analysis::IsAssignable(*base));
            break;

        case Operator::Kind::BITWISENOT:
            SkASSERT(!context.fConfig->strictES2Mode());
            SkASSERT(!base->type().isArray());
            SkASSERT(base->type().componentType().isInteger());
            SkASSERT(!base->type().isLiteral());
            break;

        default:
            SkDEBUGFAILF("unsupported prefix operator: %s", op.operatorName());
    }

    return std::make_unique<PrefixExpression>(op, std::move(base));
}

std::unique_ptr<Expression> PrefixExpression::Make(const Context& context, Position pos,
                                                   Operator op, std::unique_ptr<Expression> base) {
    std::unique_ptr<Expression> result = PrefixExpression::Make(context, op, std::move(base));
    // TODO(ethannicholas): create it with the right position in the first place
    result->fPosition = pos;
    return result;
}

}  // namespace SkSL
