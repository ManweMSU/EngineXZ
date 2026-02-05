#include "xe_sec_xaa.h"
#include "../xasm/xa_type_helper.h"

namespace Engine
{
	namespace XA
	{
		namespace Security
		{
			class XACryptographyAcceleration : public XE::Security::ICryptographyAcceleration
			{
				static uint64 _spec_word(uint operative_bit_length, uint public_exponent_bit_length) noexcept { return uint64(operative_bit_length) | (uint64(public_exponent_bit_length) << 32ULL); }
				static void _add_long_summator(Function & f, const ObjectReference & dest_ref, const ObjectReference & src_ref, const ObjectReference & modulus_ref, const ArgumentSpecification & spec)
				{
					auto nullus = TH::MakeSpec(ArgumentSemantics::Unclassified, 0, 0);
					auto add_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntAdd, ReferenceFlagInvoke));
					auto lsr_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntCmpL, ReferenceFlagInvoke));
					auto sub_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntSubt, ReferenceFlagInvoke));
					TH::AddTreeInput(add_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(add_tree, TH::MakeTree(src_ref), spec);
					TH::AddTreeOutput(add_tree, nullus);
					TH::AddTreeInput(sub_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(sub_tree, TH::MakeTree(modulus_ref), spec);
					TH::AddTreeOutput(sub_tree, nullus);
					TH::AddTreeInput(lsr_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(lsr_tree, TH::MakeTree(modulus_ref), spec);
					TH::AddTreeOutput(lsr_tree, TH::MakeSpec(1, 0));
					f.instset << TH::MakeStatementExpression(add_tree);
					f.instset << TH::MakeStatementJump(1, lsr_tree);
					f.instset << TH::MakeStatementExpression(sub_tree);
				}
				static void _add_long_duplicator(Function & f, const ObjectReference & dest_ref, const ObjectReference & modulus_ref, const ArgumentSpecification & spec)
				{
					auto nullus = TH::MakeSpec(ArgumentSemantics::Unclassified, 0, 0);
					auto shl_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntShiftL, ReferenceFlagInvoke));
					auto lsr_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntCmpL, ReferenceFlagInvoke));
					auto sub_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntSubt, ReferenceFlagInvoke));
					TH::AddTreeInput(shl_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(shl_tree, TH::MakeTree(TH::MakeRef(ReferenceLiteral)), TH::MakeSpec(1, 0));
					TH::AddTreeOutput(shl_tree, nullus);
					TH::AddTreeInput(sub_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(sub_tree, TH::MakeTree(modulus_ref), spec);
					TH::AddTreeOutput(sub_tree, nullus);
					TH::AddTreeInput(lsr_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeInput(lsr_tree, TH::MakeTree(modulus_ref), spec);
					TH::AddTreeOutput(lsr_tree, TH::MakeSpec(1, 0));
					f.instset << TH::MakeStatementExpression(shl_tree);
					f.instset << TH::MakeStatementJump(1, lsr_tree);
					f.instset << TH::MakeStatementExpression(sub_tree);
				}
				static void _add_long_nullifyer(Function & f, const ObjectReference & dest_ref, const ArgumentSpecification & spec)
				{
					auto nullus = TH::MakeSpec(ArgumentSemantics::Unclassified, 0, 0);
					auto zero_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntZero, ReferenceFlagInvoke));
					TH::AddTreeInput(zero_tree, TH::MakeTree(dest_ref), spec);
					TH::AddTreeOutput(zero_tree, nullus);
					f.instset << TH::MakeStatementExpression(zero_tree);
				}
				static void _add_long_translator(Function & f, const ObjectReference & dest_ref, const ArgumentSpecification & dest_spec, const ObjectReference & src_ref, const ArgumentSpecification & src_spec)
				{
					auto nullus = TH::MakeSpec(ArgumentSemantics::Unclassified, 0, 0);
					auto move_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntZero, ReferenceFlagInvoke));
					TH::AddTreeInput(move_tree, TH::MakeTree(dest_ref), dest_spec);
					TH::AddTreeInput(move_tree, TH::MakeTree(src_ref), src_spec);
					TH::AddTreeOutput(move_tree, nullus);
					f.instset << TH::MakeStatementExpression(move_tree);
				}
				static void _add_long_multiplicator(Function & f, int lbias, const ObjectReference & dest_ref, const ObjectReference & src_a_ref, const ObjectReference & src_b_ref, const ObjectReference & modulus_ref, const ArgumentSpecification & spec)
				{
					int dbias = f.data.Length();
					f.data.SetLength(dbias + 8);
					reinterpret_cast<uint32 *>(f.data.GetBuffer() + dbias)[0] = spec.size.num_bytes * 8;
					reinterpret_cast<uint32 *>(f.data.GetBuffer() + dbias)[1] = 1;
					// Basic evaluation trees being used
					auto init_counter_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformMove, ReferenceFlagInvoke));
					TH::AddTreeInput(init_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceInit)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(init_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceData, dbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(init_counter_tree, TH::MakeSpec(4, 0));
					auto decrement_counter_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformMove, ReferenceFlagInvoke));
					TH::AddTreeInput(decrement_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformIntegerSubt, ReferenceFlagInvoke)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceData, dbias + 4)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(decrement_counter_tree.inputs[1], TH::MakeSpec(4, 0));
					TH::AddTreeOutput(decrement_counter_tree, TH::MakeSpec(4, 0));
					auto bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntGetBit, ReferenceFlagInvoke));
					TH::AddTreeInput(bit_extract_tree, TH::MakeTree(src_b_ref), spec);
					TH::AddTreeInput(bit_extract_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(bit_extract_tree, TH::MakeSpec(4, 0));
					auto invert_bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformVectorIsZero, ReferenceFlagInvoke));
					TH::AddTreeInput(invert_bit_extract_tree, bit_extract_tree, TH::MakeSpec(4, 0));
					TH::AddTreeOutput(invert_bit_extract_tree, TH::MakeSpec(4, 0));
					auto continue_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformVectorNotZero, ReferenceFlagInvoke));
					TH::AddTreeInput(continue_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(continue_tree, TH::MakeSpec(4, 0));
					// Code synthesis
					f.instset << TH::MakeStatementOpenScope();
					_add_long_nullifyer(f, dest_ref, spec);
					f.instset << TH::MakeStatementNewLocal(TH::MakeSize(4, 0), init_counter_tree);
					f.instset << TH::MakeStatementExpression(decrement_counter_tree);
					_add_long_duplicator(f, dest_ref, modulus_ref, spec);
					f.instset << TH::MakeStatementJump(3, invert_bit_extract_tree);
					_add_long_summator(f, dest_ref, src_a_ref, modulus_ref, spec);
					f.instset << TH::MakeStatementJump(-9, continue_tree);
					f.instset << TH::MakeStatementCloseScope();
				}
				static void _add_long_exponator(Function & f, int lbias, const ObjectReference & dest_ref, const ObjectReference & base_ref, const ObjectReference & pot_ref, const ObjectReference & modulus_ref, const ObjectReference & aux_ref, const ArgumentSpecification & m_spec, const ArgumentSpecification & p_spec)
				{
					int dbias = f.data.Length();
					f.data.SetLength(dbias + 8);
					reinterpret_cast<uint32 *>(f.data.GetBuffer() + dbias)[0] = p_spec.size.num_bytes * 8;
					reinterpret_cast<uint32 *>(f.data.GetBuffer() + dbias)[1] = 1;
					// Basic evaluation trees being used
					auto init_counter_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformMove, ReferenceFlagInvoke));
					TH::AddTreeInput(init_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceInit)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(init_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceData, dbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(init_counter_tree, TH::MakeSpec(4, 0));
					auto decrement_counter_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformMove, ReferenceFlagInvoke));
					TH::AddTreeInput(decrement_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformIntegerSubt, ReferenceFlagInvoke)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(decrement_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceData, dbias + 4)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(decrement_counter_tree.inputs[1], TH::MakeSpec(4, 0));
					TH::AddTreeOutput(decrement_counter_tree, TH::MakeSpec(4, 0));
					auto increment_counter_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformMove, ReferenceFlagInvoke));
					TH::AddTreeInput(increment_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(increment_counter_tree, TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformIntegerAdd, ReferenceFlagInvoke)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(increment_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeInput(increment_counter_tree.inputs[1], TH::MakeTree(TH::MakeRef(ReferenceData, dbias + 4)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(increment_counter_tree.inputs[1], TH::MakeSpec(4, 0));
					TH::AddTreeOutput(increment_counter_tree, TH::MakeSpec(4, 0));
					auto bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntGetBit, ReferenceFlagInvoke));
					TH::AddTreeInput(bit_extract_tree, TH::MakeTree(pot_ref), p_spec);
					TH::AddTreeInput(bit_extract_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(bit_extract_tree, TH::MakeSpec(4, 0));
					auto fused_decrement_bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLongIntGetBit, ReferenceFlagInvoke));
					TH::AddTreeInput(fused_decrement_bit_extract_tree, TH::MakeTree(pot_ref), p_spec);
					TH::AddTreeInput(fused_decrement_bit_extract_tree, decrement_counter_tree, TH::MakeSpec(4, 0));
					TH::AddTreeOutput(fused_decrement_bit_extract_tree, TH::MakeSpec(4, 0));
					auto fused_decrement_invert_bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformVectorIsZero, ReferenceFlagInvoke));
					TH::AddTreeInput(fused_decrement_invert_bit_extract_tree, fused_decrement_bit_extract_tree, TH::MakeSpec(4, 0));
					TH::AddTreeOutput(fused_decrement_invert_bit_extract_tree, TH::MakeSpec(4, 0));
					auto continue_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformVectorNotZero, ReferenceFlagInvoke));
					TH::AddTreeInput(continue_tree, TH::MakeTree(TH::MakeRef(ReferenceLocal, lbias)), TH::MakeSpec(4, 0));
					TH::AddTreeOutput(continue_tree, TH::MakeSpec(4, 0));
					auto idle_loop_condition_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformLogicalAnd, ReferenceFlagInvoke));
					TH::AddTreeInput(idle_loop_condition_tree, fused_decrement_invert_bit_extract_tree, TH::MakeSpec(4, 0));
					TH::AddTreeInput(idle_loop_condition_tree, continue_tree, TH::MakeSpec(4, 0));
					TH::AddTreeOutput(idle_loop_condition_tree, TH::MakeSpec(4, 0));
					auto invert_bit_extract_tree = TH::MakeTree(TH::MakeRef(ReferenceTransform, TransformVectorIsZero, ReferenceFlagInvoke));
					TH::AddTreeInput(invert_bit_extract_tree, bit_extract_tree, TH::MakeSpec(4, 0));
					TH::AddTreeOutput(invert_bit_extract_tree, TH::MakeSpec(4, 0));
					// Code synthesis
					f.instset << TH::MakeStatementOpenScope();
					_add_long_translator(f, dest_ref, m_spec, TH::MakeRef(ReferenceData, dbias + 4), TH::MakeSpec(4, 0));
					f.instset << TH::MakeStatementNewLocal(TH::MakeSize(4, 0), init_counter_tree);
					f.instset << TH::MakeStatementJump(-1, idle_loop_condition_tree);
					f.instset << TH::MakeStatementExpression(increment_counter_tree);
					int pos_a = f.instset.Length();
					f.instset << TH::MakeStatementExpression(decrement_counter_tree);
					_add_long_multiplicator(f, lbias + 1, aux_ref, dest_ref, dest_ref, modulus_ref, m_spec);
					_add_long_translator(f, dest_ref, m_spec, aux_ref, m_spec);
					f.instset << TH::MakeStatementJump(0, invert_bit_extract_tree);
					int pos_b = f.instset.Length();
					_add_long_multiplicator(f, lbias + 1, aux_ref, dest_ref, base_ref, modulus_ref, m_spec);
					_add_long_translator(f, dest_ref, m_spec, aux_ref, m_spec);
					int pos_c = f.instset.Length();
					f.instset << TH::MakeStatementJump(0, continue_tree);
					f.instset[pos_c].attachment.num_bytes = -int(pos_c + 1 - pos_a);
					f.instset[pos_b - 1].attachment.num_bytes = pos_c - pos_b;
					f.instset << TH::MakeStatementCloseScope();
				}
				void _create_rsa_acceleration(uint operative_bit_length, uint public_exponent_bit_length, IExecutable ** exec)
				{
					Function f;
					uint m_byte_width = operative_bit_length / 8;
					uint p_byte_width = public_exponent_bit_length / 8;
					auto m_spec = TH::MakeSpec(ArgumentSemantics::Object, m_byte_width, 0);
					auto p_spec = TH::MakeSpec(ArgumentSemantics::Object, p_byte_width, 0);
					f.retval = TH::MakeSpec(ArgumentSemantics::Unclassified, 0, 0);
					f.inputs << m_spec << m_spec << p_spec << m_spec << m_spec;
					_add_long_exponator(f, 0, TH::MakeRef(ReferenceArgument, 0), TH::MakeRef(ReferenceArgument, 1), TH::MakeRef(ReferenceArgument, 2), TH::MakeRef(ReferenceArgument, 3), TH::MakeRef(ReferenceArgument, 4), m_spec, p_spec);
					f.instset << TH::MakeStatementReturn();
					TranslatedFunction tf;
					if (!_translator->Translate(tf, f)) throw InvalidStateException();
					Volumes::Dictionary<string, TranslatedFunction *> dict;
					dict.Append("_", &tf);
					SafePointer<IExecutable> executable = _linker->LinkFunctions(dict);
					if (!executable) throw InvalidStateException();
					*exec = executable.Inner();
					executable->Retain();
				}
				struct _cache_record {
					SafePointer<IExecutable> exec;
					uint uses;
				};
			private:
				SafePointer<IAssemblyTranslator> _translator;
				SafePointer<IExecutableLinker> _linker;
				Volumes::Dictionary<uint64, _cache_record> _cache;
			public:
				XACryptographyAcceleration(IAssemblyTranslator * translator, IExecutableLinker * linker) { _translator.SetRetain(translator); _linker.SetRetain(linker); }
				virtual ~XACryptographyAcceleration(void) override {}
				virtual void CreateRSAAcceleration(uint operative_bit_length, uint public_exponent_bit_length, XE::Security::RSAAccelerationPowerFunction * func, Object ** retainer) noexcept override
				{
					try {
						if (operative_bit_length >= 0x100000 || public_exponent_bit_length >= 0x100000) throw Exception();
						auto word = _spec_word(operative_bit_length, public_exponent_bit_length);
						auto obj = _cache[word];
						if (obj) {
							auto f = reinterpret_cast<XE::Security::RSAAccelerationPowerFunction>(obj->exec->GetEntryPoint(L"_"));
							if (!f) throw Exception();
							if (obj->uses < 0x1000000U) obj->uses++;
							*func = f;
							*retainer = obj->exec;
							obj->exec->Retain();
						} else {
							SafePointer<IExecutable> exec;
							_create_rsa_acceleration(operative_bit_length, public_exponent_bit_length, exec.InnerRef());
							if (_cache.Count() >= 8) {
								uint64 least_used = 0;
								uint minimal_uses = 0xFFFFFFFF;
								for (auto & r : _cache) if (r.value.uses < minimal_uses) { least_used = r.key; minimal_uses = r.value.uses; }
								_cache.Remove(least_used);
							}
							_cache_record rec;
							rec.exec.SetRetain(exec);
							rec.uses = 1;
							try { _cache.Append(word, rec); } catch (...) {}
							auto f = reinterpret_cast<XE::Security::RSAAccelerationPowerFunction>(exec->GetEntryPoint(L"_"));
							if (!f) throw Exception();
							*func = f;
							*retainer = exec.Inner();
							exec->Retain();
						}
					} catch (...) { *func = 0; *retainer = 0; }
				}
			};

			XE::Security::ICryptographyAcceleration * CreateSyntheticCryptographyAcceleration(IAssemblyTranslator * translator, IExecutableLinker * linker) noexcept { try { return new XACryptographyAcceleration(translator, linker); } catch (...) { return 0; } }
		}
	}
}