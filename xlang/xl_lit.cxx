﻿#include "xl_lit.h"

namespace Engine
{
	namespace XL
	{
		XLiteral * ProcessLiteralTransform(LContext & ctx, const string & op, XLiteral * a, XLiteral * b)
		{
			if (b) {
				auto & la = a->Expose();
				auto & lb = b->Expose();
				if (la.contents != lb.contents || la.length != lb.length) return 0;
				XI::Module::Literal lr;
				if (op == OperatorOr) {
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger ||
						la.contents == XI::Module::Literal::Class::Boolean) {
						lr = la;
						lr.data_uint64 |= lb.data_uint64;
					} else return 0;
				} else if (op == OperatorXor) {
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger ||
						la.contents == XI::Module::Literal::Class::Boolean) {
						lr = la;
						lr.data_uint64 ^= lb.data_uint64;
					} else return 0;
				} else if (op == OperatorAnd) {
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger ||
						la.contents == XI::Module::Literal::Class::Boolean) {
						lr = la;
						lr.data_uint64 &= lb.data_uint64;
					} else return 0;
				} else if (op == OperatorAdd) {
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						lr = la;
						lr.data_uint64 += lb.data_uint64;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						lr = la;
						if (lr.length == 8) lr.data_double += lb.data_double;
						else if (lr.length == 4) lr.data_float += lb.data_float;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::String) {
						lr.contents = XI::Module::Literal::Class::String;
						lr.length = 0;
						lr.data_uint64 = 0;
						lr.data_string = la.data_string + lb.data_string;
					} else return 0;
				} else if (op == OperatorSubtract || op == OperatorCompare) {
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						lr = la;
						lr.data_uint64 -= lb.data_uint64;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						lr = la;
						if (lr.length == 8) lr.data_double -= lb.data_double;
						else if (lr.length == 4) lr.data_float -= lb.data_float;
						else return 0;
					} else return 0;
				} else if (op == OperatorMultiply) {
					lr = la;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_sint8 = la.data_sint8 * lb.data_sint8;
						else if (lr.length == 2) lr.data_sint16 = la.data_sint16 * lb.data_sint16;
						else if (lr.length == 4) lr.data_sint32 = la.data_sint32 * lb.data_sint32;
						else if (lr.length == 8) lr.data_sint64 = la.data_sint64 * lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 * lb.data_uint8;
						else if (lr.length == 2) lr.data_uint16 = la.data_uint16 * lb.data_uint16;
						else if (lr.length == 4) lr.data_uint32 = la.data_uint32 * lb.data_uint32;
						else if (lr.length == 8) lr.data_uint64 = la.data_uint64 * lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_float = la.data_float * lb.data_float;
						else if (lr.length == 8) lr.data_double = la.data_double * lb.data_double;
						else return 0;
					} else return 0;
				} else if (op == OperatorDivide) {
					lr = la;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (!lb.data_uint64) return 0;
						if (lr.length == 1) lr.data_sint8 = la.data_sint8 / lb.data_sint8;
						else if (lr.length == 2) lr.data_sint16 = la.data_sint16 / lb.data_sint16;
						else if (lr.length == 4) lr.data_sint32 = la.data_sint32 / lb.data_sint32;
						else if (lr.length == 8) lr.data_sint64 = la.data_sint64 / lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (!lb.data_uint64) return 0;
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 / lb.data_uint8;
						else if (lr.length == 2) lr.data_uint16 = la.data_uint16 / lb.data_uint16;
						else if (lr.length == 4) lr.data_uint32 = la.data_uint32 / lb.data_uint32;
						else if (lr.length == 8) lr.data_uint64 = la.data_uint64 / lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_float = la.data_float / lb.data_float;
						else if (lr.length == 8) lr.data_double = la.data_double / lb.data_double;
						else return 0;
					} else return 0;
				} else if (op == OperatorResidual) {
					lr = la;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (!lb.data_uint64) return 0;
						if (lr.length == 1) lr.data_sint8 = la.data_sint8 % lb.data_sint8;
						else if (lr.length == 2) lr.data_sint16 = la.data_sint16 % lb.data_sint16;
						else if (lr.length == 4) lr.data_sint32 = la.data_sint32 % lb.data_sint32;
						else if (lr.length == 8) lr.data_sint64 = la.data_sint64 % lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (!lb.data_uint64) return 0;
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 % lb.data_uint8;
						else if (lr.length == 2) lr.data_uint16 = la.data_uint16 % lb.data_uint16;
						else if (lr.length == 4) lr.data_uint32 = la.data_uint32 % lb.data_uint32;
						else if (lr.length == 8) lr.data_uint64 = la.data_uint64 % lb.data_uint64;
						else return 0;
					} else return 0;
				} else if (op == OperatorLesser) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_sint8 < lb.data_sint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_sint16 < lb.data_sint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_sint32 < lb.data_sint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_sint64 < lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 < lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 < lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 < lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 < lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float < lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double < lb.data_double;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 < lb.data_uint8;
						else return 0;
					} else return 0;
				} else if (op == OperatorGreater) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_sint8 > lb.data_sint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_sint16 > lb.data_sint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_sint32 > lb.data_sint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_sint64 > lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 > lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 > lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 > lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 > lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float > lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double > lb.data_double;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 > lb.data_uint8;
						else return 0;
					} else return 0;
				} else if (op == OperatorEqual) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger ||
						la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 == lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 == lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 == lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 == lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float == lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double == lb.data_double;
						else return 0;
					} else return 0;
				} else if (op == OperatorNotEqual) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger || la.contents == XI::Module::Literal::Class::UnsignedInteger ||
						la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 != lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 != lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 != lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 != lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float != lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double != lb.data_double;
						else return 0;
					} else return 0;
				} else if (op == OperatorLesserEqual) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_sint8 <= lb.data_sint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_sint16 <= lb.data_sint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_sint32 <= lb.data_sint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_sint64 <= lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 <= lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 <= lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 <= lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 <= lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float <= lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double <= lb.data_double;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 <= lb.data_uint8;
						else return 0;
					} else return 0;
				} else if (op == OperatorGreaterEqual) {
					lr.contents = XI::Module::Literal::Class::Boolean;
					lr.length = 1;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_sint8 >= lb.data_sint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_sint16 >= lb.data_sint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_sint32 >= lb.data_sint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_sint64 >= lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 >= lb.data_uint8;
						else if (lr.length == 2) lr.data_uint8 = la.data_uint16 >= lb.data_uint16;
						else if (lr.length == 4) lr.data_uint8 = la.data_uint32 >= lb.data_uint32;
						else if (lr.length == 8) lr.data_uint8 = la.data_uint64 >= lb.data_uint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (lr.length == 4) lr.data_uint8 = la.data_float >= lb.data_float;
						else if (lr.length == 8) lr.data_uint8 = la.data_double >= lb.data_double;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::Boolean) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 >= lb.data_uint8;
						else return 0;
					} else return 0;
				} else if (op == OperatorShiftLeft) {
					lr = la;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_sint8 = la.data_sint8 << lb.data_sint8;
						else if (lr.length == 2) lr.data_sint16 = la.data_sint16 << lb.data_sint16;
						else if (lr.length == 4) lr.data_sint32 = la.data_sint32 << lb.data_sint32;
						else if (lr.length == 8) lr.data_sint64 = la.data_sint64 << lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 << lb.data_uint8;
						else if (lr.length == 2) lr.data_uint16 = la.data_uint16 << lb.data_uint16;
						else if (lr.length == 4) lr.data_uint32 = la.data_uint32 << lb.data_uint32;
						else if (lr.length == 8) lr.data_uint64 = la.data_uint64 << lb.data_uint64;
						else return 0;
					} else return 0;
				} else if (op == OperatorShiftRight) {
					lr = la;
					if (la.contents == XI::Module::Literal::Class::SignedInteger) {
						if (lr.length == 1) lr.data_sint8 = la.data_sint8 >> lb.data_sint8;
						else if (lr.length == 2) lr.data_sint16 = la.data_sint16 >> lb.data_sint16;
						else if (lr.length == 4) lr.data_sint32 = la.data_sint32 >> lb.data_sint32;
						else if (lr.length == 8) lr.data_sint64 = la.data_sint64 >> lb.data_sint64;
						else return 0;
					} else if (la.contents == XI::Module::Literal::Class::UnsignedInteger) {
						if (lr.length == 1) lr.data_uint8 = la.data_uint8 >> lb.data_uint8;
						else if (lr.length == 2) lr.data_uint16 = la.data_uint16 >> lb.data_uint16;
						else if (lr.length == 4) lr.data_uint32 = la.data_uint32 >> lb.data_uint32;
						else if (lr.length == 8) lr.data_uint64 = la.data_uint64 >> lb.data_uint64;
						else return 0;
					} else return 0;
				} else return 0;
				if (lr.length == 4) lr.data_uint64 &= 0xFFFFFFFF;
				else if (lr.length == 2) lr.data_uint64 &= 0xFFFF;
				else if (lr.length == 1) lr.data_uint64 &= 0xFF;
				else if (lr.length == 0) lr.data_uint64 = 0;
				return CreateLiteral(ctx, lr);
			} else {
				auto & s = a->Expose();
				auto r = s;
				if (op == OperatorReferInvert) {
					if (r.contents == XI::Module::Literal::Class::SignedInteger) {
						r.data_uint64 = ~r.data_uint64;
					} else if (r.contents == XI::Module::Literal::Class::UnsignedInteger) {
						r.data_uint64 = ~r.data_uint64;
					} else if (r.contents == XI::Module::Literal::Class::Boolean) {
						r.data_uint8 = 1 - r.data_uint8;
					} else return 0;
				} else if (op == OperatorNot) {
					if (r.contents == XI::Module::Literal::Class::String) return 0;
					r.contents = XI::Module::Literal::Class::Boolean;
					r.length = 1;
					if (r.data_uint64) r.data_uint64 = 0; else r.data_uint64 = 1;
				} else if (op == OperatorNegative) {
					if (r.contents == XI::Module::Literal::Class::UnsignedInteger) r.contents = XI::Module::Literal::Class::SignedInteger;
					if (r.contents == XI::Module::Literal::Class::SignedInteger) {
						r.data_sint64 = -r.data_sint64;
					} else if (r.contents == XI::Module::Literal::Class::FloatingPoint) {
						if (r.length == 8) {
							r.data_double = -r.data_double;
						} else if (r.length == 4) {
							r.data_float = -r.data_float;
						} else return 0;
					} else if (r.contents == XI::Module::Literal::Class::Boolean) {
						r.data_uint8 = 1 - r.data_uint8;
					} else return 0;
				} else return 0;
				if (r.length == 4) r.data_uint64 &= 0xFFFFFFFF;
				else if (r.length == 2) r.data_uint64 &= 0xFFFF;
				else if (r.length == 1) r.data_uint64 &= 0xFF;
				else if (r.length == 0) r.data_uint64 = 0;
				return CreateLiteral(ctx, r);
			}
		}
		XLiteral * ProcessLiteralConvert(LContext & ctx, XLiteral * value, XType * type)
		{
			auto & lv = value->Expose();
			XI::Module::Literal r;
			r.data_uint64 = 0;
			auto ts = type->GetArgumentSpecification();
			if (ts.semantics != XA::ArgumentSemantics::Integer && ts.semantics != XA::ArgumentSemantics::SignedInteger &&
				ts.semantics != XA::ArgumentSemantics::FloatingPoint) return 0;
			auto tn = type->GetFullName();
			if (tn == NameBoolean) {
				r.contents = XI::Module::Literal::Class::Boolean;
				r.length = 1;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_uint8 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger || lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_uint8 = lv.data_uint8 != 0;
					else if (lv.length == 2) r.data_uint8 = lv.data_uint16 != 0;
					else if (lv.length == 4) r.data_uint8 = lv.data_uint32 != 0;
					else if (lv.length == 8) r.data_uint8 = lv.data_uint64 != 0;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_uint8 = lv.data_float != 0;
					else if (lv.length == 8) r.data_uint8 = lv.data_double != 0;
					else return 0;
				} else return 0;
			} else if (tn == NameInt8) {
				r.contents = XI::Module::Literal::Class::SignedInteger;
				r.length = 1;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_sint8 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_sint8 = lv.data_uint8;
					else if (lv.length == 2) r.data_sint8 = lv.data_uint16;
					else if (lv.length == 4) r.data_sint8 = lv.data_uint32;
					else if (lv.length == 8) r.data_sint8 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_sint8 = lv.data_sint8;
					else if (lv.length == 2) r.data_sint8 = lv.data_sint16;
					else if (lv.length == 4) r.data_sint8 = lv.data_sint32;
					else if (lv.length == 8) r.data_sint8 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_sint8 = int8(lv.data_float);
					else if (lv.length == 8) r.data_sint8 = int8(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameInt16) {
				r.contents = XI::Module::Literal::Class::SignedInteger;
				r.length = 2;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_sint16 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_sint16 = lv.data_uint8;
					else if (lv.length == 2) r.data_sint16 = lv.data_uint16;
					else if (lv.length == 4) r.data_sint16 = lv.data_uint32;
					else if (lv.length == 8) r.data_sint16 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_sint16 = lv.data_sint8;
					else if (lv.length == 2) r.data_sint16 = lv.data_sint16;
					else if (lv.length == 4) r.data_sint16 = lv.data_sint32;
					else if (lv.length == 8) r.data_sint16 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_sint16 = int16(lv.data_float);
					else if (lv.length == 8) r.data_sint16 = int16(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameInt32) {
				r.contents = XI::Module::Literal::Class::SignedInteger;
				r.length = 4;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_sint32 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_sint32 = lv.data_uint8;
					else if (lv.length == 2) r.data_sint32 = lv.data_uint16;
					else if (lv.length == 4) r.data_sint32 = lv.data_uint32;
					else if (lv.length == 8) r.data_sint32 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_sint32 = lv.data_sint8;
					else if (lv.length == 2) r.data_sint32 = lv.data_sint16;
					else if (lv.length == 4) r.data_sint32 = lv.data_sint32;
					else if (lv.length == 8) r.data_sint32 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_sint32 = int32(lv.data_float);
					else if (lv.length == 8) r.data_sint32 = int32(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameInt64) {
				r.contents = XI::Module::Literal::Class::SignedInteger;
				r.length = 8;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_sint64 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_sint64 = lv.data_uint8;
					else if (lv.length == 2) r.data_sint64 = lv.data_uint16;
					else if (lv.length == 4) r.data_sint64 = lv.data_uint32;
					else if (lv.length == 8) r.data_sint64 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_sint64 = lv.data_sint8;
					else if (lv.length == 2) r.data_sint64 = lv.data_sint16;
					else if (lv.length == 4) r.data_sint64 = lv.data_sint32;
					else if (lv.length == 8) r.data_sint64 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_sint64 = int64(lv.data_float);
					else if (lv.length == 8) r.data_sint64 = int64(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameUInt8) {
				r.contents = XI::Module::Literal::Class::UnsignedInteger;
				r.length = 1;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_uint8 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_uint8 = lv.data_uint8;
					else if (lv.length == 2) r.data_uint8 = lv.data_uint16;
					else if (lv.length == 4) r.data_uint8 = lv.data_uint32;
					else if (lv.length == 8) r.data_uint8 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_uint8 = lv.data_sint8;
					else if (lv.length == 2) r.data_uint8 = lv.data_sint16;
					else if (lv.length == 4) r.data_uint8 = lv.data_sint32;
					else if (lv.length == 8) r.data_uint8 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_uint8 = uint8(lv.data_float);
					else if (lv.length == 8) r.data_uint8 = uint8(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameUInt16) {
				r.contents = XI::Module::Literal::Class::UnsignedInteger;
				r.length = 2;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_uint16 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_uint16 = lv.data_uint8;
					else if (lv.length == 2) r.data_uint16 = lv.data_uint16;
					else if (lv.length == 4) r.data_uint16 = lv.data_uint32;
					else if (lv.length == 8) r.data_uint16 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_uint16 = lv.data_sint8;
					else if (lv.length == 2) r.data_uint16 = lv.data_sint16;
					else if (lv.length == 4) r.data_uint16 = lv.data_sint32;
					else if (lv.length == 8) r.data_uint16 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_uint16 = uint16(lv.data_float);
					else if (lv.length == 8) r.data_uint16 = uint16(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameUInt32) {
				r.contents = XI::Module::Literal::Class::UnsignedInteger;
				r.length = 4;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_uint32 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_uint32 = lv.data_uint8;
					else if (lv.length == 2) r.data_uint32 = lv.data_uint16;
					else if (lv.length == 4) r.data_uint32 = lv.data_uint32;
					else if (lv.length == 8) r.data_uint32 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_uint32 = lv.data_sint8;
					else if (lv.length == 2) r.data_uint32 = lv.data_sint16;
					else if (lv.length == 4) r.data_uint32 = lv.data_sint32;
					else if (lv.length == 8) r.data_uint32 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_uint32 = uint32(lv.data_float);
					else if (lv.length == 8) r.data_uint32 = uint32(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameUInt64) {
				r.contents = XI::Module::Literal::Class::UnsignedInteger;
				r.length = 8;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_uint64 = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_uint64 = lv.data_uint8;
					else if (lv.length == 2) r.data_uint64 = lv.data_uint16;
					else if (lv.length == 4) r.data_uint64 = lv.data_uint32;
					else if (lv.length == 8) r.data_uint64 = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_uint64 = lv.data_sint8;
					else if (lv.length == 2) r.data_uint64 = lv.data_sint16;
					else if (lv.length == 4) r.data_uint64 = lv.data_sint32;
					else if (lv.length == 8) r.data_uint64 = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_uint64 = uint64(lv.data_float);
					else if (lv.length == 8) r.data_uint64 = uint64(lv.data_double);
					else return 0;
				} else return 0;
			} else if (tn == NameFloat32) {
				r.contents = XI::Module::Literal::Class::FloatingPoint;
				r.length = 4;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_float = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_float = lv.data_uint8;
					else if (lv.length == 2) r.data_float = lv.data_uint16;
					else if (lv.length == 4) r.data_float = lv.data_uint32;
					else if (lv.length == 8) r.data_float = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_float = lv.data_sint8;
					else if (lv.length == 2) r.data_float = lv.data_sint16;
					else if (lv.length == 4) r.data_float = lv.data_sint32;
					else if (lv.length == 8) r.data_float = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_float = lv.data_float;
					else if (lv.length == 8) r.data_float = lv.data_double;
					else return 0;
				} else return 0;
			} else if (tn == NameFloat64) {
				r.contents = XI::Module::Literal::Class::FloatingPoint;
				r.length = 8;
				if (lv.contents == XI::Module::Literal::Class::Boolean) {
					r.data_double = lv.data_uint8;
				} else if (lv.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (lv.length == 1) r.data_double = lv.data_uint8;
					else if (lv.length == 2) r.data_double = lv.data_uint16;
					else if (lv.length == 4) r.data_double = lv.data_uint32;
					else if (lv.length == 8) r.data_double = lv.data_uint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::SignedInteger) {
					if (lv.length == 1) r.data_double = lv.data_sint8;
					else if (lv.length == 2) r.data_double = lv.data_sint16;
					else if (lv.length == 4) r.data_double = lv.data_sint32;
					else if (lv.length == 8) r.data_double = lv.data_sint64;
					else return 0;
				} else if (lv.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (lv.length == 4) r.data_double = lv.data_float;
					else if (lv.length == 8) r.data_double = lv.data_double;
					else return 0;
				} else return 0;
			} else return 0;
			if (r.length == 4) r.data_uint64 &= 0xFFFFFFFF;
			else if (r.length == 2) r.data_uint64 &= 0xFFFF;
			else if (r.length == 1) r.data_uint64 &= 0xFF;
			else if (r.length == 0) r.data_uint64 = 0;
			return CreateLiteral(ctx, r);
		}
	}
}