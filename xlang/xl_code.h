#pragma once

#include "xl_lal.h"

namespace Engine
{
	namespace XL
	{
		enum class BlockClass { Regular, ThrowCatch };

		class LFunctionBlock : public Object
		{
		public:
			virtual BlockClass GetClass(void) = 0;
			virtual int GetLocalBase(void) = 0;
		};

		class LFunctionContext : public Object
		{
			LContext & _ctx;
			SafePointer<LObject> _root, _func, _retval;
			uint _flags;
			XA::Function _subj;
			Volumes::Stack< SafePointer<LFunctionBlock> > _blocks;
			Array<int> _acorr;
			LFunctionBlock * _current_try_block;
			int _local_counter, _current_catch_serial;
			bool _void_retval;
		public:
			LFunctionContext(LContext & ctx, LObject * dest, uint dest_flags, LObject * retval, int argc, LObject ** argvt, const string * argvn);
			virtual ~LFunctionContext(void) override;

			LObject * GetRootScope(void);

			void OpenRegularBlock(LObject ** scope);
			void CloseRegularBlock(void);
			void OpenTryBlock(LObject ** scope);
			void OpenCatchBlock(LObject ** scope, const string & var_error_code, const string & var_error_subcode, LObject * err_type, LObject * ser_type);
			void CloseCatchBlock(void);

			void EncodeExpression(LObject * expression);
			void EncodeThrow(LObject * error_code);
			void EncodeThrow(LObject * error_code, LObject * error_subcode);

			void EndEncoding(void);
		};
	}
}