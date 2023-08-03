#pragma once

#include "xl_lal.h"

namespace Engine
{
	namespace XL
	{
		enum class BlockClass { Regular, ThrowCatch, Conditional, Loop };

		class LFunctionBlock : public Object
		{
		public:
			virtual BlockClass GetClass(void) = 0;
			virtual int GetLocalBase(void) = 0;
		};
		struct FunctionInitDesc
		{
			LObject * subject;
			Volumes::List<LObject *> init;
		};
		class IFunctionInitCallback
		{
		public:
			virtual void GetNextInit(LObject * arguments_scope, FunctionInitDesc & desc) = 0;
		};
		class ISimpifiedFunctionInitCallback
		{
		public:
			virtual void GetNextInit(LObject * self, LObject * argument, LObject ** init, LObject ** revert) = 0;
		};
		struct FunctionContextDesc
		{
			LObject * retval, * instance;
			int argc;
			LObject ** argvt;
			string * argvn;
			string this_name;
			uint flags;
			LObject * vft_init;
			ObjectArray<LObject> * vft_init_seq;
			bool create_init_sequence, create_shutdown_sequence;
			IFunctionInitCallback * init_callback;
		};
		struct SimpifiedFunctionContextDesc
		{
			LObject * instance, * argument;
			uint flags;
			ISimpifiedFunctionInitCallback * init_callback;
		};
		
		class LFunctionContext : public Object
		{
			LContext & _ctx;
			SafePointer<LObject> _root, _func, _retval, _self, _self_ptr;
			SafePointer<Object> _shutdown_info;
			uint _flags;
			XA::Function _subj;
			Volumes::Stack< SafePointer<LFunctionBlock> > _blocks;
			Array<int> _acorr;
			LFunctionBlock * _current_try_block;
			int _local_counter, _current_catch_serial;
			bool _void_retval, _shutdown_on_error;
		public:
			LFunctionContext(LContext & ctx, LObject * dest, const FunctionContextDesc & desc);
			LFunctionContext(LContext & ctx, LObject * dest, uint flags, const Array<LObject *> & perform, const Array<LObject *> & revert);
			LFunctionContext(LContext & ctx, LObject * dest, const SimpifiedFunctionContextDesc & desc);
			virtual ~LFunctionContext(void) override;

			LObject * GetRootScope(void);
			LObject * GetInstance(void);
			bool IsZeroReturn(void);

			void OpenRegularBlock(LObject ** scope);
			void CloseRegularBlock(void);
			void OpenIfBlock(LObject * condition, LObject ** scope);
			void OpenElseBlock(LObject ** scope);
			void CloseIfElseBlock(void);
			void OpenForBlock(LObject * condition, LObject * step, LObject ** scope);
			void CloseForBlock(void);
			void OpenWhileBlock(LObject * condition, LObject ** scope);
			void CloseWhileBlock(void);
			void OpenDoWhileBlock(LObject ** scope);
			void CloseDoWhileBlock(LObject * condition);
			void OpenTryBlock(LObject ** scope);
			void OpenCatchBlock(LObject ** scope, const string & var_error_code, const string & var_error_subcode, LObject * err_type, LObject * ser_type);
			void CloseCatchBlock(void);

			void EncodeExpression(LObject * expression);
			LObject * EncodeCreateVariable(LObject * of_type);
			LObject * EncodeCreateVariable(LObject * of_type, LObject * init_expr);
			LObject * EncodeCreateVariable(LObject * of_type, int argc, LObject ** argv);
			void EncodeReturn(LObject * expression);
			void EncodeBreak(void);
			void EncodeBreak(LObject * level);
			void EncodeBreak(int level);
			void EncodeContinue(void);
			void EncodeContinue(LObject * level);
			void EncodeContinue(int level);
			void EncodeThrow(LObject * error_code);
			void EncodeThrow(LObject * error_code, LObject * error_subcode);

			void EndEncoding(void);
		};
	}
}