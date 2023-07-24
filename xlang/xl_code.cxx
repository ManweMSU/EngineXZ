#include "xl_code.h"

#include "xl_types.h"
#include "xl_var.h"
#include "../xasm/xa_type_helper.h"

// TODO: REMOVE
#include "../xasm/xa_dasm.h"
// TODO: END REMOVE

namespace Engine
{
	namespace XL
	{
		// TODO: ADD SIZES

		XA::ExpressionTree MakeConstant(XA::Function & hdlr, const void * pdata, int size, int align = 1)
		{
			int offset = -1;
			for (int i = 0; i <= hdlr.data.Length() - size; i += align) {
				if (pdata) {
					if (MemoryCompare(hdlr.data.GetBuffer() + i, pdata, size) == 0) { offset = i; break; }
				} else {
					bool zero = true;
					for (int j = 0; j < size; j++) if (hdlr.data[i + j]) { zero = false; break; }
					if (zero) { offset = i; break; }
				}
			}
			if (offset < 0) {
				while (hdlr.data.Length() % align) hdlr.data << 0;
				offset = hdlr.data.Length();
				hdlr.data.SetLength(offset + size);
				if (pdata) {
					MemoryCopy(hdlr.data.GetBuffer() + offset, pdata, size);
				} else {
					for (int j = 0; j < size; j++) hdlr.data[offset + j] = 0;
				}
			}
			return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset));
		}
		XA::ExpressionTree MakeBlt(const XA::ExpressionTree & dest, const XA::ExpressionTree & src, const XA::ObjectSize & size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, dest, XA::TH::MakeSpec(size));
			XA::TH::AddTreeInput(result, src, XA::TH::MakeSpec(size));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(size));
			return result;
		}
		XA::ExpressionTree MakePointer(const XA::ExpressionTree & obj, const XA::ObjectSize & size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTakePointer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, obj, XA::TH::MakeSpec(size));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(0, 1));
			return result;
		}
		XA::ExpressionTree FollowPointer(const XA::ExpressionTree & obj, const XA::ObjectSize & size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, obj, XA::TH::MakeSpec(0, 1));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(size));
			return result;
		}
		XA::ExpressionTree MakeOffset(const XA::ExpressionTree & obj, const XA::ObjectSize & by, const XA::ObjectSize & obj_size, const XA::ObjectSize & new_size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, obj, XA::TH::MakeSpec(obj_size));
			XA::TH::AddTreeInput(result, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(by));
			XA::TH::AddTreeInput(result, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(1, 0));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(new_size));
			return result;
		}

		void ThrowAddressCorrect(XA::ExpressionTree & at, const Array<int> acorr, int self)
		{
			if (at.self.ref_class == XA::ReferenceTransform && at.self.index == XA::TransformBreakIf) {
				ThrowAddressCorrect(at.inputs[0], acorr, self);
				int abs_offs = acorr[at.input_specs[2].size.num_bytes];
				at.input_specs[2].size = XA::TH::MakeSize(abs_offs - self - 1, 0);
			} else for (auto & s : at.inputs) ThrowAddressCorrect(s, acorr, self);
		}
		void ThrowSerialSet(XA::ExpressionTree & at, int number)
		{
			if (at.self.ref_class == XA::ReferenceTransform && at.self.index == XA::TransformBreakIf) {
				ThrowSerialSet(at.inputs[0], number);
				at.input_specs[2].size = XA::TH::MakeSize(number, 0);
			} else for (auto & s : at.inputs) ThrowSerialSet(s, number);
		}

		class RegularBlock : public LFunctionBlock
		{
			int _base;
			SafePointer<LObject> _scope;
		public:
			RegularBlock(int base, LObject * scope) : _base(base) { _scope.SetRetain(scope); }
			virtual BlockClass GetClass(void) override { return BlockClass::Regular; }
			virtual int GetLocalBase(void) override { return _base; }
		};
		class CatchThrowBlock : public LFunctionBlock
		{
			int _base;
			SafePointer<LObject> _scope;
			XA::ExpressionTree _tree;
			int _catch_serial, _post_catch_serial;
		public:
			CatchThrowBlock(int base) : _base(base), _post_catch_serial(-1) {}
			virtual BlockClass GetClass(void) override { return BlockClass::ThrowCatch; }
			virtual int GetLocalBase(void) override { return _base; }
			XA::ExpressionTree & GetErrorContext(void) { return _tree; }
			int & GetCatchSerial(void) { return _catch_serial; }
			int & GetPostCatchSerial(void) { return _post_catch_serial; }
			void SetScope(LObject * scope) { _scope.SetRetain(scope); }
			bool Active(void) { return _catch_serial >= 0; }
		};

		LFunctionContext::LFunctionContext(LContext & ctx, LObject * dest, uint dest_flags, LObject * retval, int argc, LObject ** argvt, const string * argvn) :
			_ctx(ctx), _flags(dest_flags), _acorr(0x1000), _local_counter(0), _current_catch_serial(-1)
		{
			// TODO: IMPLEMENT METHOD CASES
			//   1. NON-ZERO BASE
			//   2. CONSTRUCTOR AND DESTRUCTOR
			_func.SetRetain(dest);
			_root = _ctx.QueryScope();
			_ctx.QueryFunctionImplementation(_func, _subj);
			_retval = _ctx.QueryComputable(retval, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceRetVal)));
			for (int i = 0; i < argc; i++) if (argvn[i].Length()) {
				SafePointer<LObject> arg = _ctx.QueryComputable(argvt[i], XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, i)));
				try { _root->AddMember(argvn[i], arg); } catch (...) {}
			}
			auto retval_sem = _ctx.GetClassInstanceSize(retval);
			_void_retval = retval_sem.num_bytes == 0 && retval_sem.num_words == 0;
			if (_flags & FunctionThrows) {
				SafePointer<LFunctionBlock> block = new CatchThrowBlock(_local_counter);
				auto block_try = static_cast<CatchThrowBlock *>(block.Inner());
				_blocks.InsertLast(block);
				_current_catch_serial = 0;
				block_try->SetScope(_root);
				block_try->GetErrorContext() = FollowPointer(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, argc)), XA::TH::MakeSize(0, 2));
				block_try->GetCatchSerial() = _current_catch_serial;
				_acorr << -1;
				_subj.instset << XA::TH::MakeStatementOpenScope();
				_current_try_block = block_try;
			} else {
				_current_try_block = 0;
				SafePointer<LFunctionBlock> block = new RegularBlock(_local_counter, _root);
				_blocks.InsertLast(block);
				_subj.instset << XA::TH::MakeStatementOpenScope();
			}
		}
		LFunctionContext::LFunctionContext(LContext & ctx, LObject * dest, uint dest_flags, const Array<LObject *> & perform, const Array<LObject *> & revert) :
			_ctx(ctx), _flags(dest_flags), _acorr(0x1000), _local_counter(0), _current_catch_serial(-1)
		{
			if (perform.Length() != revert.Length()) throw InvalidArgumentException();
			_func.SetRetain(dest);
			_ctx.QueryFunctionImplementation(_func, _subj);
			_void_retval = true;
			auto error_ctx = FollowPointer(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), XA::TH::MakeSize(0, 2));
			auto throws = (_flags & FunctionThrows) != 0;
			for (int i = 0; i < perform.Length(); i++) {
				_current_catch_serial++;
				_acorr << -1;
				auto tree = perform[i]->Evaluate(_subj, throws ? &error_ctx : 0);
				ThrowSerialSet(tree, _current_catch_serial);
				_subj.instset << XA::TH::MakeStatementExpression(tree);
			}
			_subj.instset << XA::TH::MakeStatementReturn();
			for (int i = perform.Length() - 1; i >= 0; i--) {
				_acorr[_current_catch_serial] = _subj.instset.Length();
				_current_catch_serial--;
				if (revert[i]) {
					auto tree = revert[i]->Evaluate(_subj, 0);
					_subj.instset << XA::TH::MakeStatementExpression(tree);
				}
			}
			_subj.instset << XA::TH::MakeStatementReturn();
			for (int index = 0; index < _subj.instset.Length(); index++) {
				auto & i = _subj.instset[index];
				if (i.opcode == XA::OpcodeUnconditionalJump || i.opcode == XA::OpcodeConditionalJump) {
					int abs_offs = _acorr[i.attachment.num_bytes];
					i.attachment = XA::TH::MakeSize(abs_offs - index - 1, 0);
				}
				ThrowAddressCorrect(i.tree, _acorr, index);
			}

			// TODO: REMOVE
			IO::Console console;
			XA::DisassemblyFunction(_subj, &console);
			// TODO: END REMOVE

			_ctx.SupplyFunctionImplementation(_func, _subj);
		}
		LFunctionContext::~LFunctionContext(void) {}
		LObject * LFunctionContext::GetRootScope(void) { return _root; }
		bool LFunctionContext::IsZeroReturn(void) { return _void_retval; }
		void LFunctionContext::OpenRegularBlock(LObject ** scope)
		{
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			SafePointer<LFunctionBlock> block = new RegularBlock(_local_counter, block_scope);
			_blocks.InsertLast(block);
			_subj.instset << XA::TH::MakeStatementOpenScope();
			*scope = block_scope;
		}
		void LFunctionContext::CloseRegularBlock(void)
		{
			auto last = _blocks.GetLast();
			if (!last) throw InvalidStateException();
			auto block = last->GetValue();
			if (block->GetClass() != BlockClass::Regular) throw InvalidStateException();
			_local_counter = block->GetLocalBase();
			_blocks.RemoveLast();
			_subj.instset << XA::TH::MakeStatementCloseScope();
		}
		void LFunctionContext::OpenTryBlock(LObject ** scope)
		{
			SafePointer<LFunctionBlock> block = new CatchThrowBlock(_local_counter);
			auto block_try = static_cast<CatchThrowBlock *>(block.Inner());
			_blocks.InsertLast(block);
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			_current_catch_serial = _acorr.Length();
			_current_try_block = block_try;
			block_try->SetScope(block_scope);
			block_try->GetErrorContext() = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLocal, _local_counter));
			block_try->GetCatchSerial() = _current_catch_serial;
			_acorr << -1;
			_local_counter++;
			_subj.instset << XA::TH::MakeStatementOpenScope();
			_subj.instset << XA::TH::MakeStatementNewLocal(XA::TH::MakeSize(0, 2), MakeBlt(
				XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)),
				MakeConstant(_subj, 0, 16, 8), XA::TH::MakeSize(0, 2)
			));
			_subj.instset << XA::TH::MakeStatementOpenScope();
		}
		void LFunctionContext::OpenCatchBlock(LObject ** scope, const string & var_error_code, const string & var_error_subcode, LObject * err_type, LObject * ser_type)
		{
			auto last = _blocks.GetLast();
			if (!last) throw InvalidStateException();
			if (last->GetValue()->GetClass() != BlockClass::ThrowCatch) throw InvalidStateException();
			auto block = static_cast<CatchThrowBlock *>(last->GetValue().Inner());
			if (!block->Active()) throw InvalidStateException();
			_local_counter = block->GetLocalBase() + 1;
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			block->SetScope(block_scope);
			block->GetPostCatchSerial() = _acorr.Length();
			_acorr << -1;
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_subj.instset << XA::TH::MakeStatementJump(block->GetPostCatchSerial());
			_acorr[block->GetCatchSerial()] = _subj.instset.Length();
			_subj.instset << XA::TH::MakeStatementOpenScope();
			block->GetCatchSerial() = -1;
			if (err_type) {
				SafePointer<LObject> var_error = _ctx.QueryComputable(err_type, block->GetErrorContext());
				try { if (var_error_code.Length()) block_scope->AddMember(var_error_code, var_error); } catch (...) {}
			}
			if (ser_type) {
				SafePointer<LObject> var_suberror = _ctx.QueryComputable(ser_type, MakeOffset(block->GetErrorContext(), XA::TH::MakeSize(0, 1), XA::TH::MakeSize(0, 2), XA::TH::MakeSize(0, 1)));
				try { if (var_error_subcode.Length()) block_scope->AddMember(var_error_subcode, var_suberror); } catch (...) {}
			}
			_current_catch_serial = -1;
			_current_try_block = 0;
			for (auto & b : _blocks.InversedElements()) if (b->GetClass() == BlockClass::ThrowCatch) {
				if (static_cast<CatchThrowBlock *>(b.Inner())->Active()) {
					_current_catch_serial = static_cast<CatchThrowBlock *>(b.Inner())->GetCatchSerial();
					_current_try_block = b.Inner();
					break;
				}
			}
		}
		void LFunctionContext::CloseCatchBlock(void)
		{
			auto last = _blocks.GetLast();
			if (!last) throw InvalidStateException();
			if (last->GetValue()->GetClass() != BlockClass::ThrowCatch) throw InvalidStateException();
			auto block = static_cast<CatchThrowBlock *>(last->GetValue().Inner());
			if (block->Active()) throw InvalidStateException();
			_local_counter = block->GetLocalBase();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_acorr[block->GetPostCatchSerial()] = _subj.instset.Length();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_blocks.RemoveLast();
		}
		void LFunctionContext::EncodeExpression(LObject * expression)
		{
			auto tree = expression->Evaluate(_subj, _current_try_block ?
				&static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
			ThrowSerialSet(tree, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementExpression(tree);
		}
		void LFunctionContext::EncodeReturn(LObject * expression)
		{
			if (_void_retval) {
				_subj.instset << XA::TH::MakeStatementReturn();
			} else {
				SafePointer<LObject> rv_init = InitInstance(_retval, expression);
				auto tree = rv_init->Evaluate(_subj, _current_try_block ? &static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
				ThrowSerialSet(tree, _current_catch_serial);
				_subj.instset << XA::TH::MakeStatementReturn(tree);
			}
		}
		LObject * LFunctionContext::EncodeCreateVariable(LObject * of_type)
		{
			if (of_type->GetClass() != Class::Type) throw InvalidArgumentException();
			SafePointer<LObject> init_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)));
			SafePointer<LObject> dtor = static_cast<XType *>(of_type)->GetDestructor();
			SafePointer<LObject> init_object = InitInstance(init_instance, 0, 0);
			auto tree = init_object->Evaluate(_subj, _current_try_block ? &static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
			ThrowSerialSet(tree, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementNewLocal(static_cast<XType *>(of_type)->GetArgumentSpecification().size,
				tree, XA::TH::MakeFinal(dtor->GetClass() == Class::Null ? XA::TH::MakeRef() : MakeSymbolReferenceL(_subj, dtor->GetFullName())));
			SafePointer<LObject> var_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLocal, _local_counter)));
			_local_counter++;
			var_instance->Retain();
			return var_instance;
		}
		LObject * LFunctionContext::EncodeCreateVariable(LObject * of_type, LObject * init_expr)
		{
			if (of_type->GetClass() != Class::Type) throw InvalidArgumentException();
			SafePointer<LObject> init_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)));
			SafePointer<LObject> dtor = static_cast<XType *>(of_type)->GetDestructor();
			SafePointer<LObject> init_object = InitInstance(init_instance, init_expr);
			auto tree = init_object->Evaluate(_subj, _current_try_block ? &static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
			ThrowSerialSet(tree, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementNewLocal(static_cast<XType *>(of_type)->GetArgumentSpecification().size,
				tree, XA::TH::MakeFinal(dtor->GetClass() == Class::Null ? XA::TH::MakeRef() : MakeSymbolReferenceL(_subj, dtor->GetFullName())));
			SafePointer<LObject> var_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLocal, _local_counter)));
			_local_counter++;
			var_instance->Retain();
			return var_instance;
		}
		LObject * LFunctionContext::EncodeCreateVariable(LObject * of_type, int argc, LObject ** argv)
		{
			if (of_type->GetClass() != Class::Type) throw InvalidArgumentException();
			SafePointer<LObject> init_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)));
			SafePointer<LObject> dtor = static_cast<XType *>(of_type)->GetDestructor();
			SafePointer<LObject> init_object = InitInstance(init_instance, argc, argv);
			auto tree = init_object->Evaluate(_subj, _current_try_block ? &static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
			ThrowSerialSet(tree, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementNewLocal(static_cast<XType *>(of_type)->GetArgumentSpecification().size,
				tree, XA::TH::MakeFinal(dtor->GetClass() == Class::Null ? XA::TH::MakeRef() : MakeSymbolReferenceL(_subj, dtor->GetFullName())));
			SafePointer<LObject> var_instance = _ctx.QueryComputable(of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLocal, _local_counter)));
			_local_counter++;
			var_instance->Retain();
			return var_instance;
		}
		void LFunctionContext::EncodeThrow(LObject * error_code) { EncodeThrow(error_code, 0); }
		void LFunctionContext::EncodeThrow(LObject * error_code, LObject * error_subcode)
		{
			if (!_current_try_block) throw InvalidStateException();
			auto block_try = static_cast<CatchThrowBlock *>(_current_try_block);
			auto error_context = block_try->GetErrorContext();
			SafePointer<LObject> type_ec = error_code->GetType();
			auto type_size = _ctx.GetClassInstanceSize(type_ec);
			if (type_size.num_words > 1 || (type_size.num_words == 1 && type_size.num_bytes) || type_size.num_bytes > 4) throw InvalidArgumentException();
			_subj.instset << XA::TH::MakeStatementExpression(MakeBlt(error_context, error_code->Evaluate(_subj, &error_context), type_size));
			if (error_subcode) {
				type_ec = error_subcode->GetType();
				type_size = _ctx.GetClassInstanceSize(type_ec);
				if (type_size.num_words > 1 || (type_size.num_words == 1 && type_size.num_bytes) || type_size.num_bytes > 4) throw InvalidArgumentException();
				_subj.instset << XA::TH::MakeStatementExpression(MakeBlt(MakeOffset(error_context, XA::TH::MakeSize(0, 1), XA::TH::MakeSize(0, 2), XA::TH::MakeSize(0, 1)),
					error_subcode->Evaluate(_subj, &error_context), type_size));
			}
			_subj.instset << XA::TH::MakeStatementJump(_current_catch_serial);
		}
		void LFunctionContext::EndEncoding(void)
		{
			if (_flags & FunctionThrows) {
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn();
				auto last = _blocks.GetLast();
				if (!last) throw InvalidStateException();
				if (last->GetValue()->GetClass() != BlockClass::ThrowCatch) throw InvalidStateException();
				auto block = static_cast<CatchThrowBlock *>(last->GetValue().Inner());
				if (!block->Active()) throw InvalidStateException();
				_subj.instset << XA::TH::MakeStatementCloseScope();
				_acorr[block->GetCatchSerial()] = _subj.instset.Length();
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn(); else {
					SafePointer<LObject> rv_init = ZeroInstance(_retval);
					auto tree = rv_init->Evaluate(_subj, 0);
					_subj.instset << XA::TH::MakeStatementReturn(tree);
				}
			} else {
				CloseRegularBlock();
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn();
			}
			for (int index = 0; index < _subj.instset.Length(); index++) {
				auto & i = _subj.instset[index];
				if (i.opcode == XA::OpcodeUnconditionalJump || i.opcode == XA::OpcodeConditionalJump) {
					int abs_offs = _acorr[i.attachment.num_bytes];
					i.attachment = XA::TH::MakeSize(abs_offs - index - 1, 0);
				}
				ThrowAddressCorrect(i.tree, _acorr, index);
			}

			// TODO: REMOVE
			IO::Console console;
			XA::DisassemblyFunction(_subj, &console);
			// TODO: END REMOVE

			_ctx.SupplyFunctionImplementation(_func, _subj);
		}
	}
}