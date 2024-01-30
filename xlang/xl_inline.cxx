#include "xl_inline.h"

#include "xl_func.h"
#include "xl_types.h"
#include "xl_var.h"

namespace Engine
{
	namespace XL
	{
		class InlineComputable : public Object, public IComputableProvider
		{
		public:
			LContext * ctx;
			string retval_cn;
			XA::ExpressionTree tree;
			ObjectArray<LObject> inputs;
		public:
			InlineComputable(LContext * _ctx, const string & rv) : ctx(_ctx), inputs(2), retval_cn(XI::Module::TypeReference::MakeClassReference(rv))
			{
				tree.retval_final = XA::TH::MakeFinal();
			}
			virtual ~InlineComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { return CreateType(retval_cn, *ctx); }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				tree.inputs.Clear();
				for (auto & i : inputs) tree.inputs << i.Evaluate(func, error_ctx);
				return tree;
			}
		};
		class InlineAssignComputable : public Object, public IComputableProvider
		{
		public:
			LContext * ctx;
			string retval_cn, op;
			XA::ArgumentSpecification spec;
			ObjectArray<LObject> inputs;
		public:
			InlineAssignComputable(LContext * _ctx, const string & cls, const string & o) : ctx(_ctx), inputs(2),
				retval_cn(XI::Module::TypeReference::MakeReference(XI::Module::TypeReference::MakeClassReference(cls))), op(o) {}
			virtual ~InlineAssignComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { return CreateType(retval_cn, *ctx); }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (op == OperatorAssign) {
					return MakeAddressOf(MakeBlt(inputs[0].Evaluate(func, error_ctx), inputs[1].Evaluate(func, error_ctx), spec.size), spec.size);
				} else if (inputs.Length() == 2) {
					auto split = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformSplit, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(split, inputs[0].Evaluate(func, error_ctx), spec);
					XA::TH::AddTreeOutput(split, spec);
					auto trs = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformNull, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(trs, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceSplitter)), spec);
					XA::TH::AddTreeInput(trs, inputs[1].Evaluate(func, error_ctx), spec);
					XA::TH::AddTreeOutput(trs, spec);
					if (op == OperatorAOr) trs.self.index = XA::TransformVectorOr;
					else if (op == OperatorAXor) trs.self.index = XA::TransformVectorXor;
					else if (op == OperatorAAnd) trs.self.index = XA::TransformVectorAnd;
					else if (op == OperatorAAdd) trs.self.index = XA::TransformIntegerAdd;
					else if (op == OperatorASubtract) trs.self.index = XA::TransformIntegerSubt;
					else if (op == OperatorAMultiply) {
						if (spec.semantics == XA::ArgumentSemantics::SignedInteger) trs.self.index = XA::TransformIntegerSMul;
						else trs.self.index = XA::TransformIntegerUMul;
					} else if (op == OperatorADivide) {
						if (spec.semantics == XA::ArgumentSemantics::SignedInteger) trs.self.index = XA::TransformIntegerSDiv;
						else trs.self.index = XA::TransformIntegerUDiv;
					} else if (op == OperatorAResidual) {
						if (spec.semantics == XA::ArgumentSemantics::SignedInteger) trs.self.index = XA::TransformIntegerSMod;
						else trs.self.index = XA::TransformIntegerUMod;
					} else if (op == OperatorAShiftLeft) {
						if (spec.semantics == XA::ArgumentSemantics::SignedInteger) trs.self.index = XA::TransformVectorShiftAL;
						else trs.self.index = XA::TransformVectorShiftL;
					} else if (op == OperatorAShiftRight) {
						if (spec.semantics == XA::ArgumentSemantics::SignedInteger) trs.self.index = XA::TransformVectorShiftAR;
						else trs.self.index = XA::TransformVectorShiftR;
					} else throw InvalidStateException();
					return MakeAddressOf(MakeBlt(split, trs, spec.size), spec.size);
				} else {
					uint64 one = 1;
					auto size = spec.size.num_bytes + 8 * spec.size.num_words;
					auto split = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformSplit, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(split, inputs[0].Evaluate(func, error_ctx), spec);
					XA::TH::AddTreeOutput(split, spec);
					auto trs = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformNull, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(trs, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceSplitter)), spec);
					XA::TH::AddTreeInput(trs, MakeConstant(func, &one, size, size), spec);
					XA::TH::AddTreeOutput(trs, spec);
					if (op == OperatorIncrement) trs.self.index = XA::TransformIntegerAdd;
					else if (op == OperatorDecrement) trs.self.index = XA::TransformIntegerSubt;
					else throw InvalidStateException();
					return MakeAddressOf(MakeBlt(split, trs, spec.size), spec.size);
				}
			}
		};

		LObject * CheckInlinePossibility(LObject * function, LObject * instance, LObject * arg1, LObject * arg2)
		{
			try {
				LContext * ctx;
				if (function->GetClass() == Class::FunctionOverload) {
					ctx = &static_cast<XFunctionOverload *>(function)->GetContext();
				} else if (function->GetClass() == Class::MethodOverload) {
					ctx = &static_cast<XMethodOverload *>(function)->GetContext();
				} else return 0;
				auto path = function->GetFullName();
				auto del = path.FindFirst(L':');
				auto path_pure = path.Fragment(0, del);
				auto cn = path.Fragment(del + 1, -1);
				del = path_pure.FindLast(L'.');
				auto cls = del >= 0 ? path_pure.Fragment(0, del) : L"";
				auto op = path_pure.Fragment(del + 1, -1);
				SafePointer< Array<XI::Module::TypeReference> > sign = XI::Module::TypeReference(cn).GetFunctionSignature();
				if (cls.Length()) {
					if (op == NameConstructor) {
						if (sign->Length() == 1) {
							SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							return CreateComputable(*ctx, xtype, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)));
						} else if (sign->Length() == 2 && sign->ElementAt(1).GetClassName() == cls) {
							SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							SafePointer<InlineComputable> inl = new InlineComputable(ctx, NameVoid);
							inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke);
							inl->tree.retval_spec = xtype->GetArgumentSpecification();
							inl->tree.input_specs << xtype->GetArgumentSpecification();
							inl->tree.input_specs << xtype->GetArgumentSpecification();
							inl->inputs.Append(instance);
							inl->inputs.Append(arg1);
							return CreateComputable(*ctx, inl);
						}
					} else if (op == NameConverter) {
						if (sign->Length() == 1) {
							SafePointer<XType> xsrc = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							SafePointer<XType> xdest = CreateType(sign->ElementAt(0).QueryCanonicalName(), *ctx);
							auto src_spec = xsrc->GetArgumentSpecification();
							auto dest_spec = xdest->GetArgumentSpecification();
							if (dest_spec.semantics != XA::ArgumentSemantics::Integer && dest_spec.semantics != XA::ArgumentSemantics::SignedInteger) return 0;
							SafePointer<InlineComputable> inl = new InlineComputable(ctx, xdest->GetFullName());
							if (xdest->GetFullName() == NameBoolean) {
								inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformVectorNotZero, XA::ReferenceFlagInvoke);
								inl->tree.retval_spec = src_spec;
								inl->tree.input_specs << src_spec;
							} else {
								inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, src_spec.semantics == XA::ArgumentSemantics::SignedInteger ?
									XA::TransformIntegerSResize : XA::TransformIntegerUResize, XA::ReferenceFlagInvoke);
								inl->tree.retval_spec = dest_spec;
								inl->tree.input_specs << src_spec;
							}
							inl->inputs.Append(instance);
							return CreateComputable(*ctx, inl);
						}
					} else if (op == OperatorReferInvert) {
						if (sign->Length() == 1 && sign->ElementAt(0).GetClassName() == cls) {
							SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							SafePointer<InlineComputable> inl = new InlineComputable(ctx, cls);
							if (cls == NameBoolean) inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke);
							else inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformVectorInverse, XA::ReferenceFlagInvoke);
							inl->tree.retval_spec = xtype->GetArgumentSpecification();
							inl->tree.input_specs << xtype->GetArgumentSpecification();
							inl->inputs.Append(instance);
							return CreateComputable(*ctx, inl);
						}
					} else if (op == OperatorNot) {
						if (sign->Length() == 1 && sign->ElementAt(0).GetClassName() == NameBoolean) {
							SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							SafePointer<InlineComputable> inl = new InlineComputable(ctx, NameBoolean);
							inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke);
							inl->tree.retval_spec = xtype->GetArgumentSpecification();
							inl->tree.input_specs << xtype->GetArgumentSpecification();
							inl->inputs.Append(instance);
							return CreateComputable(*ctx, inl);
						}
					} else if (op == OperatorNegative) {
						if (sign->Length() == 1 && sign->ElementAt(0).GetClassName() == cls) {
							SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
							SafePointer<InlineComputable> inl = new InlineComputable(ctx, cls);
							if (cls == NameBoolean) inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalNot, XA::ReferenceFlagInvoke);
							else inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformIntegerInverse, XA::ReferenceFlagInvoke);
							inl->tree.retval_spec = xtype->GetArgumentSpecification();
							inl->tree.input_specs << xtype->GetArgumentSpecification();
							inl->inputs.Append(instance);
							return CreateComputable(*ctx, inl);
						}
					} else {
						if (sign->ElementAt(0).GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
							sign->ElementAt(0).GetReferenceDestination().GetClassName() == cls) {
							if (sign->Length() == 2) {
								if (sign->ElementAt(1).GetClassName() != cls) return 0;
								if (op == OperatorAssign || op == OperatorAOr || op == OperatorAXor || op == OperatorAAnd ||
									op == OperatorAAdd || op == OperatorASubtract || op == OperatorAMultiply || op == OperatorADivide ||
									op == OperatorAResidual || op == OperatorAShiftLeft || op == OperatorAShiftRight) {
									SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
									SafePointer<InlineAssignComputable> inl = new InlineAssignComputable(ctx, cls, op);
									inl->spec = xtype->GetArgumentSpecification();
									inl->inputs.Append(instance);
									inl->inputs.Append(arg1);
									return CreateComputable(*ctx, inl);
								}
							} else if (sign->Length() == 1) {
								if (op == OperatorIncrement || op == OperatorDecrement) {
									SafePointer<XType> xtype = CreateType(XI::Module::TypeReference::MakeClassReference(cls), *ctx);
									SafePointer<InlineAssignComputable> inl = new InlineAssignComputable(ctx, cls, op);
									inl->spec = xtype->GetArgumentSpecification();
									inl->inputs.Append(instance);
									return CreateComputable(*ctx, inl);
								}
							}
						}
					}
				} else {
					SafePointer<XType> xtype = CreateType(sign->ElementAt(1).QueryCanonicalName(), *ctx);
					SafePointer<XType> xrv = CreateType(sign->ElementAt(0).QueryCanonicalName(), *ctx);
					SafePointer<InlineComputable> inl = new InlineComputable(ctx, xrv->GetFullName());
					bool use_signed = xtype->GetArgumentSpecification().semantics == XA::ArgumentSemantics::SignedInteger;
					bool use_logical = xtype->GetFullName() == NameBoolean;
					inl->tree.self = XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformNull, XA::ReferenceFlagInvoke);
					if (op == OperatorOr) inl->tree.self.index = XA::TransformVectorOr;
					else if (op == OperatorXor) inl->tree.self.index = XA::TransformVectorXor;
					else if (op == OperatorAnd) inl->tree.self.index = XA::TransformVectorAnd;
					else if (op == OperatorAdd) inl->tree.self.index = XA::TransformIntegerAdd;
					else if (op == OperatorSubtract) inl->tree.self.index = XA::TransformIntegerSubt;
					else if (op == OperatorMultiply) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSMul;
						else inl->tree.self.index = XA::TransformIntegerUMul;
					} else if (op == OperatorDivide) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSDiv;
						else inl->tree.self.index = XA::TransformIntegerUDiv;
					} else if (op == OperatorResidual) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSMod;
						else inl->tree.self.index = XA::TransformIntegerUMod;
					} else if (op == OperatorEqual) {
						if (use_logical) inl->tree.self.index = XA::TransformLogicalSame;
						else inl->tree.self.index = XA::TransformIntegerEQ;
					} else if (op == OperatorNotEqual) {
						if (use_logical) inl->tree.self.index = XA::TransformLogicalNotSame;
						else inl->tree.self.index = XA::TransformIntegerNEQ;
					} else if (op == OperatorLesser) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSL;
						else inl->tree.self.index = XA::TransformIntegerUL;
					} else if (op == OperatorGreater) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSG;
						else inl->tree.self.index = XA::TransformIntegerUG;
					} else if (op == OperatorLesserEqual) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSLE;
						else inl->tree.self.index = XA::TransformIntegerULE;
					} else if (op == OperatorGreaterEqual) {
						if (use_signed) inl->tree.self.index = XA::TransformIntegerSGE;
						else inl->tree.self.index = XA::TransformIntegerUGE;
					} else if (op == OperatorShiftLeft) {
						if (use_signed) inl->tree.self.index = XA::TransformVectorShiftAL;
						else inl->tree.self.index = XA::TransformVectorShiftL;
					} else if (op == OperatorShiftRight) {
						if (use_signed) inl->tree.self.index = XA::TransformVectorShiftAR;
						else inl->tree.self.index = XA::TransformVectorShiftR;
					} else return 0;
					inl->tree.retval_spec = xtype->GetArgumentSpecification();
					inl->tree.input_specs << xtype->GetArgumentSpecification();
					inl->tree.input_specs << xtype->GetArgumentSpecification();
					inl->inputs.Append(arg1);
					inl->inputs.Append(arg2);
					return CreateComputable(*ctx, inl);
				}
				return 0;
			} catch (...) { return 0; }
		}
		bool MayHaveInline(const string & name, const string & cls)
		{
			try {
				if (cls == NameBoolean) return true;
				else if (cls == NameInt8) return true;
				else if (cls == NameUInt8) return true;
				else if (cls == NameInt16) return true;
				else if (cls == NameUInt16) return true;
				else if (cls == NameInt32) return true;
				else if (cls == NameUInt32) return true;
				else if (cls == NameInt64) return true;
				else if (cls == NameUInt64) return true;
				else if (cls == NameIntPtr) return true;
				else if (cls == NameUIntPtr) return true;
				else if (!cls.Length()) {
					auto del = name.FindFirst(L':');
					auto op = name.Fragment(0, del);
					auto cn = name.Fragment(del + 1, -1);
					if (op == OperatorOr || op == OperatorXor || op == OperatorAnd || op == OperatorAdd ||
						op == OperatorSubtract || op == OperatorMultiply || op == OperatorDivide || op == OperatorResidual ||
						op == OperatorLesser || op == OperatorGreater || op == OperatorEqual || op == OperatorNotEqual ||
						op == OperatorLesserEqual || op == OperatorGreaterEqual || op == OperatorShiftLeft || op == OperatorShiftRight) {
						SafePointer< Array<XI::Module::TypeReference> > sign = XI::Module::TypeReference(cn).GetFunctionSignature();
						if (sign->Length() != 3) return false;
						if (sign->ElementAt(1).QueryCanonicalName() != sign->ElementAt(2).QueryCanonicalName()) return false;
						if (op == OperatorLesser || op == OperatorGreater || op == OperatorEqual || op == OperatorNotEqual ||
							op == OperatorLesserEqual || op == OperatorGreaterEqual) {
							if (sign->ElementAt(0).GetClassName() != NameBoolean) return false;
						} else {
							if (sign->ElementAt(0).QueryCanonicalName() != sign->ElementAt(1).QueryCanonicalName()) return false;
						}
						auto & type_ref = sign->ElementAt(1);
						if (type_ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer &&
							type_ref.GetPointerDestination().GetClassName() == NameVoid) return true;
						auto type = type_ref.GetClassName();
						if (type == NameBoolean) return true;
						else if (type == NameInt8) return true;
						else if (type == NameUInt8) return true;
						else if (type == NameInt16) return true;
						else if (type == NameUInt16) return true;
						else if (type == NameInt32) return true;
						else if (type == NameUInt32) return true;
						else if (type == NameInt64) return true;
						else if (type == NameUInt64) return true;
						else if (type == NameIntPtr) return true;
						else if (type == NameUIntPtr) return true;
						else return false;
					} else return false;
				} else return false;
			} catch (...) { return false; }
		}
		bool InlineRegisterCorrect(XA::Function & func, XA::ObjectReference & ref, const XA::Function & org)
		{
			if (ref.ref_class == XA::ReferenceNull) return true;
			else if (ref.ref_class == XA::ReferenceInit) return true;
			else if (ref.ref_class == XA::ReferenceSplitter) return true;
			else if (ref.ref_class == XA::ReferenceLiteral) return true;
			else if (ref.ref_class == XA::ReferenceCode) return false;
			else if (ref.ref_class == XA::ReferenceArgument) return false;
			else if (ref.ref_class == XA::ReferenceRetVal) return false;
			else if (ref.ref_class == XA::ReferenceLocal) return false;
			else if (ref.ref_class == XA::ReferenceExternal) {
				if (ref.index < 0 || ref.index >= org.extrefs.Length()) return false;
				int reindex = -1;
				for (int i = 0; i < func.extrefs.Length(); i++) if (func.extrefs[i] == org.extrefs[ref.index]) { reindex = i; break; }
				if (reindex < 0) { reindex = func.extrefs.Length(); func.extrefs << org.extrefs[ref.index]; }
				ref.index = reindex;
				return true;
			} else if (ref.ref_class == XA::ReferenceData) {
				auto dconst = MakeConstant(func, org.data.GetBuffer(), org.data.Length(), 8);
				ref.index += dconst.self.index;
				return true;
			} else if (ref.ref_class == XA::ReferenceTransform) {
				if (ref.index == XA::TransformBreakIf) return false;
				else return true;
			} else return false;
		}
		bool InlineNodeCorrect(XA::Function & func, XA::ExpressionTree & corr, XA::ExpressionTree & src, const XA::Function & org)
		{
			if (corr.self.ref_class == XA::ReferenceArgument) {
				if (corr.self.ref_flags & XA::ReferenceFlagInvoke) return false;
				if (corr.self.index < 0 || corr.self.index >= src.inputs.Length()) return false;
				auto index = corr.self.index;
				corr = src.inputs[index];
			} else {
				if (!InlineRegisterCorrect(func, corr.self, org)) return false;
				for (auto & i : corr.inputs) if (!InlineNodeCorrect(func, i, src, org)) return false;
				if (!InlineRegisterCorrect(func, corr.retval_final.final, org)) return false;
				for (auto & i : corr.retval_final.final_args) if (!InlineRegisterCorrect(func, i, org)) return false;
			}
			return true;
		}
		bool TryForCanonicalInline(XA::Function & func, XA::ExpressionTree & node, LObject * fver)
		{
			if (fver->GetClass() != Class::FunctionOverload) return false;
			auto fx = static_cast<XFunctionOverload *>(fver);
			if (fx->GetFlags() & XI::Module::Function::FunctionThrows) return false;
			auto & src_xa = fx->GetImplementationDesc()._xa;
			if (src_xa.instset.Length() == 0) return false;
			if (src_xa.instset[0].opcode != XA::OpcodeOpenScope && src_xa.instset[0].opcode != XA::OpcodeControlReturn) return false;
			if (src_xa.instset[0].opcode == XA::OpcodeOpenScope && src_xa.instset.Length() == 1) return false;
			if (src_xa.instset[0].opcode == XA::OpcodeOpenScope && src_xa.instset[1].opcode != XA::OpcodeControlReturn) return false;
			auto & src_node = src_xa.instset[0].opcode == XA::OpcodeOpenScope ? src_xa.instset[1].tree : src_xa.instset[0].tree;
			if (src_node.self.qword != XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke).qword) return false;
			if (src_node.inputs.Length() != 2) return false;
			if (src_node.inputs[0].self.qword != XA::TH::MakeRef(XA::ReferenceRetVal).qword) return false;
			auto substitute = src_node.inputs[1];
			if (!InlineNodeCorrect(func, substitute, node, src_xa)) return false;
			node = substitute;
			return true;
		}
	}
}