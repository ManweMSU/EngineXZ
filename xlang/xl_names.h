#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XL
	{
		constexpr const widechar * NameConstructor		= L"@crea";
		constexpr const widechar * NameConstructorZero	= L"@crea_vacui";
		constexpr const widechar * NameConstructorMove	= L"@crea_move";
		constexpr const widechar * NameDestructor		= L"@perde";
		constexpr const widechar * NameConverter		= L"@converte";
		constexpr const widechar * NameVFT				= L"@ofv";

		constexpr const widechar * NameVoid		= L"nihil";
		constexpr const widechar * NameBoolean	= L"logicum";
		constexpr const widechar * NameInt		= L"int";
		constexpr const widechar * NameUInt		= L"nint";
		constexpr const widechar * NameInt8		= L"int8";
		constexpr const widechar * NameUInt8	= L"nint8";
		constexpr const widechar * NameInt16	= L"int16";
		constexpr const widechar * NameUInt16	= L"nint16";
		constexpr const widechar * NameInt32	= L"int32";
		constexpr const widechar * NameUInt32	= L"nint32";
		constexpr const widechar * NameInt64	= L"int64";
		constexpr const widechar * NameUInt64	= L"nint64";
		constexpr const widechar * NameIntPtr	= L"intadl";
		constexpr const widechar * NameUIntPtr	= L"nintadl";
		constexpr const widechar * NameFloat32	= L"frac";
		constexpr const widechar * NameFloat64	= L"dfrac";

		// Instance operators
		constexpr const widechar * OperatorInvoke		= L"()";
		constexpr const widechar * OperatorSubscript	= L"[]";
		constexpr const widechar * OperatorTakeAddress	= L"@";
		constexpr const widechar * OperatorReferInvert	= L"~";
		constexpr const widechar * OperatorFollow		= L"^";
		constexpr const widechar * OperatorNot			= L"!";
		constexpr const widechar * OperatorNegative		= L"-";
		constexpr const widechar * OperatorAssign		= L"=";
		constexpr const widechar * OperatorAOr			= L"|=";
		constexpr const widechar * OperatorAXor			= L"#=";
		constexpr const widechar * OperatorAAnd			= L"&=";
		constexpr const widechar * OperatorAAdd			= L"+=";
		constexpr const widechar * OperatorASubtract	= L"-=";
		constexpr const widechar * OperatorAMultiply	= L"*=";
		constexpr const widechar * OperatorADivide		= L"/=";
		constexpr const widechar * OperatorAResidual	= L"%=";
		constexpr const widechar * OperatorAShiftLeft	= L"<<=";
		constexpr const widechar * OperatorAShiftRight	= L">>=";
		constexpr const widechar * OperatorIncrement	= L"++";
		constexpr const widechar * OperatorDecrement	= L"--";

		// Function operators
		constexpr const widechar * OperatorOr			= L"|";
		constexpr const widechar * OperatorXor			= L"#";
		constexpr const widechar * OperatorAnd			= L"&";
		constexpr const widechar * OperatorAdd			= L"+";
		constexpr const widechar * OperatorSubtract		= L"-";
		constexpr const widechar * OperatorMultiply		= L"*";
		constexpr const widechar * OperatorDivide		= L"/";
		constexpr const widechar * OperatorResidual		= L"%";
		constexpr const widechar * OperatorLesser		= L"<";
		constexpr const widechar * OperatorGreater		= L">";
		constexpr const widechar * OperatorEqual		= L"==";
		constexpr const widechar * OperatorNotEqual		= L"!=";
		constexpr const widechar * OperatorLesserEqual	= L"<=";
		constexpr const widechar * OperatorGreaterEqual	= L">=";
		constexpr const widechar * OperatorCompare		= L"<=>";
		constexpr const widechar * OperatorShiftLeft	= L"<<";
		constexpr const widechar * OperatorShiftRight	= L">>";

		constexpr const widechar * IteratorBegin	= L"initus";
		constexpr const widechar * IteratorEnd		= L"finis";
		constexpr const widechar * IteratorPreBegin	= L"prae_initus";
		constexpr const widechar * IteratorPostEnd	= L"post_finis";

		constexpr int CastPriorityNoCast	= -1;
		constexpr int CastPriorityExplicit	= 0;
		constexpr int CastPriorityConverter	= 1;
		constexpr int CastPrioritySimilar	= 2; // Parent class/interface or type to reference
		constexpr int CastPriorityIdentity	= 3;
	}
}