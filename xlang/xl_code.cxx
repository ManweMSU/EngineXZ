#include "xl_code.h"

#include "xl_func.h"
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
		class IfElseBlock : public LFunctionBlock
		{
			int _base;
			bool _else_mode;
			SafePointer<LObject> _scope;
			int _bypass_main_branch_serial;
			int _bypass_secondary_branch_serial;
		public:
			IfElseBlock(int base) : _base(base), _else_mode(false), _bypass_main_branch_serial(-1), _bypass_secondary_branch_serial(-1) {}
			virtual BlockClass GetClass(void) override { return BlockClass::Conditional; }
			virtual int GetLocalBase(void) override { return _base; }
			bool & GetElseMode(void) { return _else_mode; }
			int & GetBypassMainSerial(void) { return _bypass_main_branch_serial; }
			int & GetBypassSecondarySerial(void) { return _bypass_secondary_branch_serial; }
			void SetScope(LObject * scope) { _scope.SetRetain(scope); }
		};
		class LoopBlock : public LFunctionBlock
		{
			int _base, _type;
			SafePointer<LObject> _scope;
			SafePointer<LObject> _step;
			int _break_serial, _continue_serial, _bypass_serial, _loop_serial;
		public:
			LoopBlock(int base, int type) : _base(base), _type(type), _break_serial(-1), _continue_serial(-1), _bypass_serial(-1), _loop_serial(-1) {}
			virtual BlockClass GetClass(void) override { return BlockClass::Loop; }
			virtual int GetLocalBase(void) override { return _base; }
			int GetLoopType(void) { return _type; }
			int & GetBreakSerial(void) { return _break_serial; }
			int & GetContinueSerial(void) { return _continue_serial; }
			int & GetBypassSerial(void) { return _bypass_serial; }
			int & GetLoopSerial(void) { return _loop_serial; }
			void SetScope(LObject * scope) { _scope.SetRetain(scope); }
			SafePointer<LObject> & GetStep(void) { return _step; }
		};

		LFunctionContext::LFunctionContext(LContext & ctx, LObject * dest, const FunctionContextDesc & desc) :
			_ctx(ctx), _acorr(0x1000), _local_counter(0), _current_catch_serial(-1), _current_try_block(0)
		{
			_func.SetRetain(dest);
			_root = _ctx.QueryScope();
			_ctx.QueryFunctionImplementation(_func, _subj);
			_retval = _ctx.QueryComputable(desc.retval, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceRetVal)));
			_flags = desc.flags;
			auto & vfd = static_cast<XFunctionOverload *>(dest)->GetVFDesc();
			int argb = 0;
			if (desc.instance) {
				argb = 1;
				XA::ExpressionTree self_ptr = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0));
				if (vfd.base_offset.num_bytes || vfd.base_offset.num_words) {
					auto offs = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(offs, MakeAddressFollow(self_ptr, static_cast<XType *>(desc.instance)->GetArgumentSpecification().size), XA::TH::MakeSpec(0, 1));
					XA::TH::AddTreeInput(offs, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(vfd.base_offset));
					XA::TH::AddTreeInput(offs, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, -1, 0));
					XA::TH::AddTreeOutput(offs, static_cast<XType *>(desc.instance)->GetArgumentSpecification());
					self_ptr = MakeAddressOf(offs, static_cast<XType *>(desc.instance)->GetArgumentSpecification().size);
				}
				SafePointer<XType> ptr_type = CreateType(XI::Module::TypeReference::MakePointer(static_cast<XType *>(desc.instance)->GetCanonicalType()), _ctx);
				_self_ptr = _ctx.QueryComputable(ptr_type, self_ptr);
				SafePointer<LObject> method = _self_ptr->GetMember(OperatorFollow);
				_self = method->Invoke(0, 0);
				if (desc.this_name.Length()) try { _root->AddMember(desc.this_name, _self_ptr); } catch (...) {}
			}
			for (int i = 0; i < desc.argc; i++) if (desc.argvn[i].Length()) {
				SafePointer<LObject> arg = _ctx.QueryComputable(desc.argvt[i], XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, argb + i)));
				try { _root->AddMember(desc.argvn[i], arg); } catch (...) {}
			}
			auto retval_sem = _ctx.GetClassInstanceSize(desc.retval);
			_void_retval = retval_sem.num_bytes == 0 && retval_sem.num_words == 0;

			// TODO: IMPLEMENT INIT SEQUENCES

			if (desc.vft_init) {
				int vft_min, vft_max;
				auto cls = static_cast<XClass *>(desc.vft_init);
				cls->GetRangeVFT(vft_min, vft_max);
				if (vft_min >= 0) for (int vft = vft_min; vft <= vft_max; vft++) {
					auto vft_offs = cls->GetOffsetVFT(vft);
					auto vft_var = cls->CreateVFT(vft, *desc.vft_init_seq);
					auto init = MakeBlt(
						MakeOffset(_self->Evaluate(_subj, 0), vft_offs, XA::TH::MakeSize(vft_offs.num_bytes, vft_offs.num_words + 1), XA::TH::MakeSize(0, 1)),
						MakeAddressOf(vft_var->Evaluate(_subj, 0), XA::TH::MakeSize(0, 1)), XA::TH::MakeSize(0, 1)
					);
					_subj.instset << XA::TH::MakeStatementExpression(init);
				}
			}
			if (_flags & FunctionThrows) {
				SafePointer<LFunctionBlock> block = new CatchThrowBlock(_local_counter);
				auto block_try = static_cast<CatchThrowBlock *>(block.Inner());
				_blocks.InsertLast(block);
				_current_catch_serial = 0;
				block_try->SetScope(_root);
				block_try->GetErrorContext() = FollowPointer(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, argb + desc.argc)), XA::TH::MakeSize(0, 2));
				block_try->GetCatchSerial() = _current_catch_serial;
				_acorr << -1;
				_subj.instset << XA::TH::MakeStatementOpenScope();
				_current_try_block = block_try;
			} else {
				SafePointer<LFunctionBlock> block = new RegularBlock(_local_counter, _root);
				_blocks.InsertLast(block);
				_subj.instset << XA::TH::MakeStatementOpenScope();
			}
		}
		LFunctionContext::LFunctionContext(LContext & ctx, LObject * dest, uint flags, const Array<LObject *> & perform, const Array<LObject *> & revert) :
			_ctx(ctx), _acorr(0x1000), _flags(flags), _local_counter(0), _current_catch_serial(-1)
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
			_ctx.SupplyFunctionImplementation(_func, _subj);
		}
		LFunctionContext::~LFunctionContext(void) {}
		LObject * LFunctionContext::GetRootScope(void) { return _root; }
		LObject * LFunctionContext::GetInstance(void) { return _self; }
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
		void LFunctionContext::OpenIfBlock(LObject * condition, LObject ** scope)
		{
			SafePointer<LFunctionBlock> block = new IfElseBlock(_local_counter);
			_blocks.InsertLast(block);
			auto if_else = static_cast<IfElseBlock *>(block.Inner());
			if_else->GetBypassMainSerial() = _acorr.Length();
			_acorr << -1;
			SafePointer<XType> boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
			SafePointer<LObject> cond_cast = PerformTypeCast(boolean, condition, CastPriorityConverter);
			auto inverter = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(inverter, cond_cast->Evaluate(_subj, _current_try_block ?
				&static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0), boolean->GetArgumentSpecification());
			XA::TH::AddTreeOutput(inverter, boolean->GetArgumentSpecification());
			ThrowSerialSet(inverter, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementJump(if_else->GetBypassMainSerial(), inverter);
			_subj.instset << XA::TH::MakeStatementOpenScope();
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			if_else->SetScope(block_scope);
		}
		void LFunctionContext::OpenElseBlock(LObject ** scope)
		{
			auto last = _blocks.GetLast();
			if (!last || last->GetValue()->GetClass() != BlockClass::Conditional) throw InvalidStateException();
			auto data = static_cast<IfElseBlock *>(last->GetValue().Inner());
			if (data->GetElseMode()) throw InvalidStateException();
			data->GetElseMode() = true;
			_local_counter = data->GetLocalBase();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			data->GetBypassSecondarySerial() = _acorr.Length();
			_acorr << -1;
			_subj.instset << XA::TH::MakeStatementJump(data->GetBypassSecondarySerial());
			_acorr[data->GetBypassMainSerial()] = _subj.instset.Length();
			_subj.instset << XA::TH::MakeStatementOpenScope();
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			data->SetScope(block_scope);
		}
		void LFunctionContext::CloseIfElseBlock(void)
		{
			auto last = _blocks.GetLast();
			if (!last || last->GetValue()->GetClass() != BlockClass::Conditional) throw InvalidStateException();
			auto ptr = last->GetValue();
			auto data = static_cast<IfElseBlock *>(ptr.Inner());
			_blocks.RemoveLast();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			if (data->GetElseMode()) _acorr[data->GetBypassSecondarySerial()] = _subj.instset.Length();
			else _acorr[data->GetBypassMainSerial()] = _subj.instset.Length();
			_local_counter = data->GetLocalBase();
		}
		void LFunctionContext::OpenForBlock(LObject * condition, LObject * step, LObject ** scope)
		{
			SafePointer<LFunctionBlock> block = new LoopBlock(_local_counter, 2);
			_blocks.InsertLast(block);
			auto loop = static_cast<LoopBlock *>(block.Inner());
			auto end = _acorr.Length(); _acorr << -1;
			loop->GetBreakSerial() = end;
			loop->GetContinueSerial() = _acorr.Length(); _acorr << -1;
			loop->GetBypassSerial() = end;
			loop->GetLoopSerial() = _acorr.Length(); _acorr << _subj.instset.Length();
			loop->GetStep().SetRetain(step);
			SafePointer<XType> boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
			SafePointer<LObject> cond_cast = PerformTypeCast(boolean, condition, CastPriorityConverter);
			auto inverter = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(inverter, cond_cast->Evaluate(_subj, _current_try_block ?
				&static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0), boolean->GetArgumentSpecification());
			XA::TH::AddTreeOutput(inverter, boolean->GetArgumentSpecification());
			ThrowSerialSet(inverter, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementJump(loop->GetBypassSerial(), inverter);
			_subj.instset << XA::TH::MakeStatementOpenScope();
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			loop->SetScope(block_scope);
		}
		void LFunctionContext::CloseForBlock(void)
		{
			auto last = _blocks.GetLast();
			if (!last || last->GetValue()->GetClass() != BlockClass::Loop) throw InvalidStateException();
			auto ptr = last->GetValue();
			auto data = static_cast<LoopBlock *>(ptr.Inner());
			if (data->GetLoopType() != 2) throw InvalidStateException();
			_blocks.RemoveLast();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_acorr[data->GetContinueSerial()] = _subj.instset.Length();
			EncodeExpression(data->GetStep());
			_subj.instset << XA::TH::MakeStatementJump(data->GetLoopSerial());
			_acorr[data->GetBypassSerial()] = _subj.instset.Length();
			_local_counter = data->GetLocalBase();
		}
		void LFunctionContext::OpenWhileBlock(LObject * condition, LObject ** scope)
		{
			SafePointer<LFunctionBlock> block = new LoopBlock(_local_counter, 0);
			_blocks.InsertLast(block);
			auto loop = static_cast<LoopBlock *>(block.Inner());
			auto begin = _acorr.Length(); _acorr << _subj.instset.Length();
			auto end = _acorr.Length(); _acorr << -1;
			loop->GetBreakSerial() = end;
			loop->GetContinueSerial() = begin;
			loop->GetBypassSerial() = end;
			loop->GetLoopSerial() = begin;
			SafePointer<XType> boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
			SafePointer<LObject> cond_cast = PerformTypeCast(boolean, condition, CastPriorityConverter);
			auto inverter = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(inverter, cond_cast->Evaluate(_subj, _current_try_block ?
				&static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0), boolean->GetArgumentSpecification());
			XA::TH::AddTreeOutput(inverter, boolean->GetArgumentSpecification());
			ThrowSerialSet(inverter, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementJump(loop->GetBypassSerial(), inverter);
			_subj.instset << XA::TH::MakeStatementOpenScope();
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			loop->SetScope(block_scope);
		}
		void LFunctionContext::CloseWhileBlock(void)
		{
			auto last = _blocks.GetLast();
			if (!last || last->GetValue()->GetClass() != BlockClass::Loop) throw InvalidStateException();
			auto ptr = last->GetValue();
			auto data = static_cast<LoopBlock *>(ptr.Inner());
			if (data->GetLoopType() != 0) throw InvalidStateException();
			_blocks.RemoveLast();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_subj.instset << XA::TH::MakeStatementJump(data->GetLoopSerial());
			_acorr[data->GetBypassSerial()] = _subj.instset.Length();
			_local_counter = data->GetLocalBase();
		}
		void LFunctionContext::OpenDoWhileBlock(LObject ** scope)
		{
			SafePointer<LFunctionBlock> block = new LoopBlock(_local_counter, 1);
			_blocks.InsertLast(block);
			auto loop = static_cast<LoopBlock *>(block.Inner());
			loop->GetBypassSerial() = -1;
			loop->GetLoopSerial() = _acorr.Length(); _acorr << _subj.instset.Length();
			loop->GetBreakSerial() = _acorr.Length(); _acorr << -1;
			loop->GetContinueSerial() = _acorr.Length(); _acorr << -1;
			_subj.instset << XA::TH::MakeStatementOpenScope();
			SafePointer<LObject> block_scope = _ctx.QueryScope();
			*scope = block_scope;
			loop->SetScope(block_scope);
		}
		void LFunctionContext::CloseDoWhileBlock(LObject * condition)
		{
			auto last = _blocks.GetLast();
			if (!last || last->GetValue()->GetClass() != BlockClass::Loop) throw InvalidStateException();
			auto ptr = last->GetValue();
			auto data = static_cast<LoopBlock *>(ptr.Inner());
			if (data->GetLoopType() != 1) throw InvalidStateException();
			_blocks.RemoveLast();
			_subj.instset << XA::TH::MakeStatementCloseScope();
			_acorr[data->GetContinueSerial()] = _subj.instset.Length();
			SafePointer<XType> boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
			SafePointer<LObject> cond_cast = PerformTypeCast(boolean, condition, CastPriorityConverter);
			auto cond_tree = cond_cast->Evaluate(_subj, _current_try_block ? &static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
			ThrowSerialSet(cond_tree, _current_catch_serial);
			_subj.instset << XA::TH::MakeStatementJump(data->GetLoopSerial(), cond_tree);
			_acorr[data->GetBreakSerial()] = _subj.instset.Length();
			_local_counter = data->GetLocalBase();
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
		void LFunctionContext::EncodeBreak(void) { EncodeBreak(0); }
		void LFunctionContext::EncodeBreak(LObject * level)
		{
			if (level->GetClass() != Class::Literal) throw InvalidArgumentException();
			EncodeBreak(_ctx.QueryLiteralValue(level));
		}
		void LFunctionContext::EncodeBreak(int level)
		{
			if (level < 0) throw InvalidArgumentException();
			int current = 0;
			for (auto & b : _blocks.InversedElements()) if (b->GetClass() == BlockClass::Loop) {
				if (current == level) {
					auto loop = static_cast<LoopBlock *>(b.Inner());
					_subj.instset << XA::TH::MakeStatementJump(loop->GetBreakSerial());
					return;
				}
				current++;
			}
			throw InvalidArgumentException();
		}
		void LFunctionContext::EncodeContinue(void) { EncodeContinue(0); }
		void LFunctionContext::EncodeContinue(LObject * level)
		{
			if (level->GetClass() != Class::Literal) throw InvalidArgumentException();
			EncodeContinue(_ctx.QueryLiteralValue(level));
		}
		void LFunctionContext::EncodeContinue(int level)
		{
			if (level < 0) throw InvalidArgumentException();
			int current = 0;
			for (auto & b : _blocks.InversedElements()) if (b->GetClass() == BlockClass::Loop) {
				if (current == level) {
					auto loop = static_cast<LoopBlock *>(b.Inner());
					_subj.instset << XA::TH::MakeStatementJump(loop->GetContinueSerial());
					return;
				}
				current++;
			}
			throw InvalidArgumentException();
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
			ThrowSerialSet(_subj.instset.LastElement().tree, _current_catch_serial);
			if (error_subcode) {
				type_ec = error_subcode->GetType();
				type_size = _ctx.GetClassInstanceSize(type_ec);
				if (type_size.num_words > 1 || (type_size.num_words == 1 && type_size.num_bytes) || type_size.num_bytes > 4) throw InvalidArgumentException();
				_subj.instset << XA::TH::MakeStatementExpression(MakeBlt(MakeOffset(error_context, XA::TH::MakeSize(0, 1), XA::TH::MakeSize(0, 2), XA::TH::MakeSize(0, 1)),
					error_subcode->Evaluate(_subj, &error_context), type_size));
				ThrowSerialSet(_subj.instset.LastElement().tree, _current_catch_serial);
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