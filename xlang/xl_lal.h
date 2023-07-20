#pragma once

#include "../xasm/xa_types.h"
#include "xl_names.h"

namespace Engine
{
	namespace XL
	{
		enum FunctionFlags {
			FunctionVirtual = 0x01,
			FunctionInitializer = 0x02,
			FunctionFinalizer = 0x04,
			FunctionMain = 0x08,
			FunctionMethod = 0x10,
			FunctionThrows = 0x20,
			FunctionThisCall = 0x40,
			FunctionPureCall = 0x80,
		};

		class LObject;
		class LException : public Exception
		{
		public:
			SafePointer<LObject> source;
			LException(LObject * reason);
		};
		class ObjectHasNoTypeException : public LException
		{
		public:
			ObjectHasNoTypeException(LObject * reason);
			virtual string ToString(void) const override;
		};
		class ObjectHasNoSuchMemberException : public LException
		{
		public:
			string member;
			ObjectHasNoSuchMemberException(LObject * reason, const string & member_name);
			virtual string ToString(void) const override;
		};
		class ObjectMemberRedefinitionException : public LException
		{
		public:
			string member;
			ObjectMemberRedefinitionException(LObject * reason, const string & member_name);
			virtual string ToString(void) const override;
		};
		class ObjectHasNoSuchOverloadException : public LException
		{
		public:
			ObjectArray<LObject> arguments;
			ObjectHasNoSuchOverloadException(LObject * reason, int argc, LObject ** argv);
			virtual string ToString(void) const override;
		};
		class ObjectIsNotEvaluatableException : public LException
		{
		public:
			ObjectIsNotEvaluatableException(LObject * reason);
			virtual string ToString(void) const override;
		};
		class ObjectHasNoAttributesException : public LException
		{
		public:
			ObjectHasNoAttributesException(LObject * reason);
			virtual string ToString(void) const override;
		};
		class ObjectMayThrow : public LException
		{
		public:
			ObjectMayThrow(LObject * reason);
			virtual string ToString(void) const override;
		};
		class ObjectHasNoSuitableCast : public LException
		{
			SafePointer<LObject> type_from, type_to;
		public:
			ObjectHasNoSuitableCast(LObject * reason, LObject * from, LObject * to);
			virtual string ToString(void) const override;
		};

		enum class Class {
			Null, Namespace, Scope, Alias,
			Type,
			Function, FunctionOverload, Method, MethodOverload,
			Literal, Variable, Field,
			Property,
			Internal
		};

		class LObject : public Object
		{
		public:
			// Object information
			virtual Class GetClass(void) = 0;
			virtual string GetName(void) = 0;
			virtual string GetFullName(void) = 0;
			virtual bool IsDefinedLocally(void) = 0;
			// Working with
			virtual LObject * GetType(void) = 0;
			virtual LObject * GetMember(const string & name) = 0;
			virtual LObject * Invoke(int argc, LObject ** argv) = 0;
			virtual void AddMember(const string & name, LObject * child) = 0;
			virtual void AddAttribute(const string & key, const string & value) = 0;
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) = 0;
		};
		class LContext : public Object
		{
			string _module_name;
			uint _subsystem;
			SafePointer<LObject> _root_ns;
			Volumes::ObjectDictionary<string, DataBlock> _rsrc;
		public:
			LContext(const string & module);
			virtual ~LContext(void) override;

			void MakeSubsystemConsole(void);
			void MakeSubsystemGUI(void);
			void MakeSubsystemNone(void);
			void MakeSubsystemLibrary(void);

			bool IncludeModule(const string & name, Streaming::Stream * module_data);
			LObject * GetRootNamespace(void);
			LObject * CreateNamespace(LObject * create_under, const string & name);
			LObject * CreateAlias(LObject * create_under, const string & name, LObject * destination);
			LObject * CreateClass(LObject * create_under, const string & name);
			LObject * CreateFunction(LObject * create_under, const string & name);
			LObject * CreateFunctionOverload(LObject * create_under, LObject * retval, int argc, LObject ** argv, uint flags);

			// TODO: IMPLEMENT

			XA::ArgumentSemantics GetClassSemantics(LObject * cls);
			XA::ObjectSize GetClassInstanceSize(LObject * cls);
			void SetClassSemantics(LObject * cls, XA::ArgumentSemantics value);
			void SetClassInstanceSize(LObject * cls, XA::ObjectSize value);
			void MarkClassAsCore(LObject * cls);
			void MarkClassAsStandard(LObject * cls);
			void MarkClassAsInterface(LObject * cls);
			void QueryFunctionImplementation(LObject * func, XA::Function & code);
			void SupplyFunctionImplementation(LObject * func, const XA::Function & code);
			void SupplyFunctionImplementation(LObject * func, const string & name, const string & lib);

			// TODO: IMPLEMENT

			LObject * QueryObject(const string & path);
			LObject * QueryScope(void);
			LObject * QueryStaticArray(LObject * type, int volume);
			LObject * QueryTypePointer(LObject * type);
			LObject * QueryTypeReference(LObject * type);
			LObject * QueryFunctionPointer(LObject * retval, int argc, LObject ** argv);
			LObject * QueryTypeOfOperator(void);
			LObject * QuerySizeOfOperator(void);
			LObject * QueryModuleOperator(void);
			LObject * QueryModuleOperator(const string & name);
			LObject * QueryInterfaceOperator(const string & name);
			LObject * QueryLiteral(bool value);
			LObject * QueryLiteral(uint64 value);
			LObject * QueryLiteral(double value);
			LObject * QueryLiteral(const string & value);
			LObject * QueryDetachedLiteral(LObject * base);
			LObject * QueryComputable(LObject * of_type, const XA::ExpressionTree & with_tree);
			void AttachLiteral(LObject * literal, LObject * attach_under, const string & name);
			int QueryLiteralValue(LObject * literal);
			string QueryLiteralString(LObject * literal);
			Volumes::ObjectDictionary<string, DataBlock> & QueryResources(void);
			void ProduceModule(const string & asm_name, int v1, int v2, int v3, int v4, Streaming::Stream * dest);
		};
	}
}