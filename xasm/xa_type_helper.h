#pragma once

#include "xa_types.h"

namespace Engine
{
	namespace XA
	{
		namespace TH {
			ObjectSize MakeSize(void);
			ObjectSize MakeSize(int bytes, int words);
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, int bytes, int words);
			ArgumentSpecification MakeSpec(ArgumentSemantics semantics, const ObjectSize & size);
			ArgumentSpecification MakeSpec(int bytes, int words);
			ArgumentSpecification MakeSpec(const ObjectSize & size);
			ArgumentSpecification MakeSpec(void);
			ObjectReference MakeRef(void);
			ObjectReference MakeRef(uint8 cls);
			ObjectReference MakeRef(uint8 cls, int index, uint8 flags = ReferenceFlagRefer);
			FinalizerReference MakeFinal(void);
			FinalizerReference MakeFinal(const ObjectReference & final);
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1);
			FinalizerReference MakeFinal(const ObjectReference & final, const ObjectReference & xa1, const ObjectReference & xa2);
			FinalizerReference MakeFinal(const ObjectReference & final, const Array<ObjectReference> & xa);
			ExpressionTree MakeTree(void);
			ExpressionTree MakeTree(const ObjectReference & self);
			ExpressionTree MakeTreeLiteral(const ObjectSize & literal);
			ExpressionTree & AddTreeInput(ExpressionTree & tree, const ExpressionTree & input, const ArgumentSpecification & input_spec);
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec);
			ExpressionTree & AddTreeOutput(ExpressionTree & tree, const ArgumentSpecification & output_spec, const FinalizerReference & final);
			Statement MakeStatementNOP(void);
			Statement MakeStatementTrap(void);
			Statement MakeStatementOpenScope(void);
			Statement MakeStatementCloseScope(void);
			Statement MakeStatementExpression(const ExpressionTree & tree);
			Statement MakeStatementNewLocal(const ObjectSize & size);
			Statement MakeStatementNewLocal(const ObjectSize & size, const FinalizerReference & final);
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init);
			Statement MakeStatementNewLocal(const ObjectSize & size, const ExpressionTree & init, const FinalizerReference & final);
			Statement MakeStatementJump(int dist_rel_next_inst);
			Statement MakeStatementJump(int dist_rel_next_inst, const ExpressionTree & condition);
			Statement MakeStatementReturn(void);
			Statement MakeStatementReturn(const ExpressionTree & retval_init);
		}
	}
}