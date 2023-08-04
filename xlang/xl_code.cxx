#include "xl_code.h"

#include "xl_func.h"
#include "xl_types.h"
#include "xl_var.h"
#include "xl_prop.h"
#include "../xasm/xa_type_helper.h"

namespace Engine
{
	namespace XL
	{
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
		bool GetUnifiedOffset(XA::ExpressionTree & node, XA::ObjectSize & offs)
		{
			if (node.inputs.Length() == 2) {
				if (node.inputs[1].self.ref_class == XA::ReferenceLiteral) {
					offs = node.input_specs[1].size;
					return true;
				} else return false;
			} else if (node.inputs.Length() == 3) {
				if (node.inputs[1].self.ref_class == XA::ReferenceLiteral && node.inputs[2].self.ref_class == XA::ReferenceLiteral) {
					auto s1 = node.input_specs[1].size;
					auto s2 = node.input_specs[2].size;
					if (s1.num_words && s2.num_words) return false;
					if (s1.num_bytes == 0xFFFFFFFF && s1.num_words == 0) {
						offs.num_bytes = -s2.num_bytes;
						offs.num_words = -s2.num_words;
						return true;
					} else if (s2.num_bytes == 0xFFFFFFFF && s2.num_words == 0) {
						offs.num_bytes = -s1.num_bytes;
						offs.num_words = -s1.num_words;
						return true;
					} else if (s1.num_words == 0) {
						offs.num_bytes = s2.num_bytes * s1.num_bytes;
						offs.num_words = s2.num_words * s1.num_bytes;
						return true;
					} else {
						offs.num_bytes = s1.num_bytes * s2.num_bytes;
						offs.num_words = s1.num_words * s2.num_bytes;
						return true;
					}
				} else return false;
			} else return false;
		}
		void OptimizeNode(XA::ExpressionTree & node)
		{
			for (auto & i : node.inputs) OptimizeNode(i);
			if (node.self.ref_class == XA::ReferenceTransform && (node.self.ref_flags & XA::ReferenceFlagInvoke)) {
				if (node.inputs.Length() && node.inputs[0].self.ref_class == XA::ReferenceTransform && (node.inputs[0].self.ref_flags & XA::ReferenceFlagInvoke)) {
					if (node.self.index == XA::TransformFollowPointer) {
						if (node.inputs[0].inputs.Length() && node.inputs[0].self.index == XA::TransformTakePointer) {
							auto inter = node.inputs[0].inputs[0];
							node = inter;
						}
					} else if (node.self.index == XA::TransformTakePointer) {
						if (node.inputs[0].inputs.Length() && node.inputs[0].self.index == XA::TransformFollowPointer) {
							auto inter = node.inputs[0].inputs[0];
							node = inter;
						}
					} else if (node.self.index == XA::TransformAddressOffset) {
						if (node.inputs[0].inputs.Length() && node.inputs[0].self.index == XA::TransformAddressOffset) {
							XA::ObjectSize o1, o2;
							if (GetUnifiedOffset(node, o1) && GetUnifiedOffset(node.inputs[0], o2)) {
								int bytes = o1.num_bytes + o2.num_bytes;
								int words = o1.num_words + o2.num_words;
								if ((bytes >= 0 && words >= 0) || (bytes <= 0 && words <= 0)) {
									int sign = 1;
									if (bytes < 0) { sign = -1; bytes = -bytes; words = -words; }
									XA::ExpressionTree ref;
									ref.self = node.self;
									ref.retval_spec = node.retval_spec;
									ref.retval_final = node.retval_final;
									ref.inputs << node.inputs[0].inputs[0];
									ref.input_specs << node.inputs[0].input_specs[0];
									ref.inputs << XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral));
									ref.input_specs << XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, bytes, words);
									if (sign == -1) {
										ref.inputs << XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral));
										ref.input_specs << XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, -1, 0);
									}
									node = ref;
								}
							}
						}
					} else if (node.self.index == XA::TransformVectorIsZero) {
						if (node.inputs[0].self.index == XA::TransformVectorIsZero) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformVectorNotZero;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformVectorNotZero) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformVectorIsZero;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerEQ) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerNEQ;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerNEQ) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerEQ;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerULE) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerUG;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerUG) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerULE;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerUGE) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerUL;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerUL) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerUGE;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerSLE) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerSG;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerSG) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerSLE;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerSGE) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerSL;
							node = inter;
						} else if (node.inputs[0].self.index == XA::TransformIntegerSL) {
							auto inter = node.inputs[0];
							inter.self.index = XA::TransformIntegerSGE;
							node = inter;
						}
					} else if (node.self.index == XA::TransformVectorInverse) {
						if (node.inputs[0].inputs.Length() && node.inputs[0].self.index == XA::TransformVectorInverse) {
							auto inter = node.inputs[0].inputs[0];
							node = inter;
						}
					} else if (node.self.index == XA::TransformIntegerInverse) {
						if (node.inputs[0].inputs.Length() && node.inputs[0].self.index == XA::TransformIntegerInverse) {
							auto inter = node.inputs[0].inputs[0];
							node = inter;
						}
					}
				}
			}
		}
		void OptimizeCode(XA::Function & func, Array<int> & acorr)
		{
			int p = 0;
			while (p < func.instset.Length()) {
				auto & i = func.instset[p];
				OptimizeNode(i.tree);
				if (i.opcode == XA::OpcodeExpression && !(i.tree.self.ref_flags & XA::ReferenceFlagInvoke)) {
					for (auto & a : acorr) if (a > p) a--;
					func.instset.Remove(p);
				} else p++;
			}
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

		struct InitShutdownInfo
		{
			int sdwn_serial;
			SafePointer<LObject> init, sdwn;
		};
		typedef Array<InitShutdownInfo> InitShutdownVolume;

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
			if (desc.create_init_sequence) {
				SafePointer<InitShutdownVolume> list = new InitShutdownVolume(0x80);
				ObjectArray<LObject> to_init(0x40);
				auto cls = static_cast<XClass *>(desc.instance);
				cls->ListFields(to_init);
				if (cls->GetParentClass()) to_init.Append(cls->GetParentClass());
				if (desc.init_callback) {
					FunctionInitDesc init_desc;
					while (true) {
						desc.init_callback->GetNextInit(_root, init_desc);
						if (init_desc.subject) {
							if (init_desc.subject == desc.instance) init_desc.subject = cls->GetParentClass();
							bool found = false;
							for (int i = 0; i < to_init.Length(); i++) if (to_init.ElementAt(i) == init_desc.subject) { found = true; to_init.Remove(i); break; }
							if (!found) throw InvalidArgumentException();
							SafePointer<LObject> real_subject;
							if (init_desc.subject->GetClass() == Class::Field) {
								real_subject = static_cast<XField *>(init_desc.subject)->SetInstance(_self);
							} else if (init_desc.subject == cls->GetParentClass()) {
								real_subject = cls->TransformTo(_self, cls->GetParentClass(), false);
							} else throw InvalidArgumentException();
							Array<LObject *> argv(0x10);
							for (auto & l : init_desc.init) argv.Append(l);
							InitShutdownInfo info;
							info.sdwn_serial = _acorr.Length(); _acorr << -1;
							info.init = InitInstance(real_subject, argv.Length(), argv.GetBuffer());
							info.sdwn = DestroyInstance(real_subject);
							list->Append(info);
						} else break;
					}
				}
				for (auto & i : to_init) {
					SafePointer<LObject> real_subject;
					if (i.GetClass() == Class::Field) {
						real_subject = static_cast<XField *>(&i)->SetInstance(_self);
					} else if (&i == cls->GetParentClass()) {
						real_subject = cls->TransformTo(_self, cls->GetParentClass(), false);
					} else throw InvalidArgumentException();
					InitShutdownInfo info;
					info.sdwn_serial = _acorr.Length(); _acorr << -1;
					info.init = InitInstance(real_subject, 0, 0);
					info.sdwn = DestroyInstance(real_subject);
					list->Append(info);
				}
				to_init.Clear();
				for (auto & l : *list) {
					auto tree = l.init->Evaluate(_subj, _current_try_block ?
						&static_cast<CatchThrowBlock *>(_current_try_block)->GetErrorContext() : 0);
					ThrowSerialSet(tree, l.sdwn_serial);
					_subj.instset << XA::TH::MakeStatementExpression(tree);
					l.init.SetReference(0);
				}
				_shutdown_info.SetRetain(list);
			}
			if (desc.create_shutdown_sequence) {
				_shutdown_on_error = false;
				SafePointer<InitShutdownVolume> list = new InitShutdownVolume(0x80);
				ObjectArray<LObject> to_shutdown(0x40);
				auto cls = static_cast<XClass *>(desc.instance);
				cls->ListFields(to_shutdown);
				if (cls->GetParentClass()) to_shutdown.Append(cls->GetParentClass());
				for (auto & i : to_shutdown) {
					SafePointer<LObject> real_subject;
					if (i.GetClass() == Class::Field) {
						real_subject = static_cast<XField *>(&i)->SetInstance(_self);
					} else if (&i == cls->GetParentClass()) {
						real_subject = cls->TransformTo(_self, cls->GetParentClass(), false);
					} else throw InvalidArgumentException();
					InitShutdownInfo info;
					info.sdwn_serial = -1;
					info.init = 0;
					info.sdwn = DestroyInstance(real_subject);
					list->Append(info);
				}
				to_shutdown.Clear();
				_shutdown_info.SetRetain(list);
			} else _shutdown_on_error = true;
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
				if (revert[i]) {
					auto tree = revert[i]->Evaluate(_subj, 0);
					_subj.instset << XA::TH::MakeStatementExpression(tree);
				}
				_acorr[_current_catch_serial] = _subj.instset.Length();
				_current_catch_serial--;
			}
			_subj.instset << XA::TH::MakeStatementReturn();
			OptimizeCode(_subj, _acorr);
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
		LFunctionContext::LFunctionContext(LContext & ctx, LObject * dest, const SimpifiedFunctionContextDesc & desc) :
			_ctx(ctx), _acorr(0x1000), _flags(desc.flags), _local_counter(0), _current_catch_serial(-1)
		{
			_func.SetRetain(dest);
			_ctx.QueryFunctionImplementation(_func, _subj);
			_void_retval = true;
			SafePointer<XComputable> self = CreateComputable(_ctx, static_cast<XType *>(desc.instance),
				MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)),
					static_cast<XType *>(desc.instance)->GetArgumentSpecification().size));
			SafePointer<XComputable> argument = desc.argument ? CreateComputable(_ctx, static_cast<XType *>(desc.argument),
				XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 1))) : 0;
			auto error_ctx = FollowPointer(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, argument ? 2 : 1)), XA::TH::MakeSize(0, 2));
			auto throws = (_flags & FunctionThrows) != 0;
			Volumes::Stack< SafePointer<LObject> > rev_list;
			while (true) {
				SafePointer<LObject> init, revert;
				desc.init_callback->GetNextInit(self, argument, init.InnerRef(), revert.InnerRef());
				if (init) {
					_current_catch_serial++;
					_acorr << -1;
					auto tree = init->Evaluate(_subj, throws ? &error_ctx : 0);
					ThrowSerialSet(tree, _current_catch_serial);
					_subj.instset << XA::TH::MakeStatementExpression(tree);
					rev_list.Push(revert);
				} else break;
			}
			_subj.instset << XA::TH::MakeStatementReturn();
			while (!rev_list.IsEmpty()) {
				auto revert = rev_list.Pop();
				if (revert) {
					auto tree = revert->Evaluate(_subj, 0);
					_subj.instset << XA::TH::MakeStatementExpression(tree);
				}
				_acorr[_current_catch_serial] = _subj.instset.Length();
				_current_catch_serial--;
			}
			_subj.instset << XA::TH::MakeStatementReturn();
			OptimizeCode(_subj, _acorr);
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
			if (!_shutdown_on_error && _shutdown_info) {
				auto list = static_cast<InitShutdownVolume *>(_shutdown_info.Inner());
				for (auto & l : *list) if (l.sdwn) _subj.instset << XA::TH::MakeStatementExpression(l.sdwn->Evaluate(_subj, 0));
			}
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
				if (!_shutdown_on_error && _shutdown_info) {
					auto list = static_cast<InitShutdownVolume *>(_shutdown_info.Inner());
					for (auto & l : *list) if (l.sdwn) _subj.instset << XA::TH::MakeStatementExpression(l.sdwn->Evaluate(_subj, 0));
				}
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn();
				auto last = _blocks.GetLast();
				if (!last) throw InvalidStateException();
				if (last->GetValue()->GetClass() != BlockClass::ThrowCatch) throw InvalidStateException();
				auto block = static_cast<CatchThrowBlock *>(last->GetValue().Inner());
				if (!block->Active()) throw InvalidStateException();
				_subj.instset << XA::TH::MakeStatementCloseScope();
				_acorr[block->GetCatchSerial()] = _subj.instset.Length();
				if (_shutdown_on_error && _shutdown_info) {
					auto list = static_cast<InitShutdownVolume *>(_shutdown_info.Inner());
					for (auto & l : list->InversedElements()) {
						if (l.sdwn) _subj.instset << XA::TH::MakeStatementExpression(l.sdwn->Evaluate(_subj, 0));
						_acorr[l.sdwn_serial] = _subj.instset.Length();
					}
				}
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn(); else {
					SafePointer<LObject> rv_init = ZeroInstance(_retval);
					auto tree = rv_init->Evaluate(_subj, 0);
					_subj.instset << XA::TH::MakeStatementReturn(tree);
				}
			} else {
				CloseRegularBlock();
				if (!_shutdown_on_error && _shutdown_info) {
					auto list = static_cast<InitShutdownVolume *>(_shutdown_info.Inner());
					for (auto & l : *list) if (l.sdwn) _subj.instset << XA::TH::MakeStatementExpression(l.sdwn->Evaluate(_subj, 0));
				}
				if (_void_retval) _subj.instset << XA::TH::MakeStatementReturn();
			}
			OptimizeCode(_subj, _acorr);
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
	}
}