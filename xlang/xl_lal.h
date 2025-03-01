#pragma once

#include "../xasm/xa_types.h"
#include "../ximg/xi_resources.h"
#include "xl_names.h"

namespace Engine
{
	namespace XL
	{
		enum FunctionFlags {
			FunctionVirtual		= 0x0001,
			FunctionInitializer	= 0x0002,
			FunctionFinalizer	= 0x0004,
			FunctionMain		= 0x0008,
			FunctionMethod		= 0x0010,
			FunctionThrows		= 0x0020,
			FunctionThisCall	= 0x0040,
			FunctionPureCall	= 0x0080,
			FunctionOverride	= 0x0100,
			FunctionInline		= 0x0200,
			FunctionCExpanding	= 0x1000,
			FunctionCNarrowing	= 0x2000,
			FunctionCExpensive	= 0x4000,
			FunctionCSimilar	= 0x8000,
		};
		enum CreateMethodsFlags {
			CreateMethodConstructorInit	= 0x001,
			CreateMethodConstructorCopy	= 0x002,
			CreateMethodConstructorZero	= 0x004,
			CreateMethodConstructorMove	= 0x008,
			CreateMethodDestructor		= 0x010,
			CreateMethodAssign			= 0x020,
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
			Type, Prototype,
			Function, FunctionOverload, Method, MethodOverload,
			Literal, NullLiteral, Variable, Field,
			Property, InstancedProperty,
			Internal
		};
		struct InvokationDesc {
			string path;
			Volumes::List< Volumes::KeyValuePair< SafePointer<LObject>, Class > > arglist;
		};

		class IModuleLoadCallback
		{
		public:
			virtual Streaming::Stream * GetModuleStream(const string & name) = 0;
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) = 0;
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual = 0) = 0;
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) = 0;
			virtual void AddMember(const string & name, LObject * child) = 0;
			virtual void AddAttribute(const string & key, const string & value) = 0;
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) = 0;
		};
		class LPrototypeHandler : public Object
		{
		public:
			virtual void HandlePrototype(const string & ident, const string & lang_spec, const DataBlock & data, const Volumes::Dictionary<string, string> & attributes) = 0;
		};
		class LContext : public Object
		{
			bool _idle_mode, _perform_version_control, _write_version_info, _built_in_inlines;
			string _module_name;
			uint _subsystem, _private_counter;
			SafePointer<LObject> _root_ns, _private_ns;
			SafePointer<LPrototypeHandler> _prot_hdlr;
			Array<string> _import_list;
			XI::AssemblyVersionInformation _verinfo;
			SafePointer<DataBlock> _data;
			Volumes::ObjectDictionary<uint64, DataBlock> _rsrc;
		public:
			LContext(const string & module);
			virtual ~LContext(void) override;

			bool IsIdle(void);
			void SetIdleMode(bool set);
			bool IsVersionControlOn(void);
			bool IsVersionInfoOn(void);
			void SetVersionInfoMode(bool control, bool write);
			void SetVersionInfoVersion(uint32 version);
			void SetVersionInfoSubstitute(uint32 version, uint32 with_mask);
			void MakeSubsystemConsole(void);
			void MakeSubsystemGUI(void);
			void MakeSubsystemNone(void);
			void MakeSubsystemLibrary(void);
			void MakeSubsystemXW(void);
			uint GetSubsystem(void);
			bool BuiltInInlinesAllowed(void);
			void AllowBuiltInInlines(bool allow);

			bool IncludeModule(const string & name, IModuleLoadCallback * callback, bool embed);
			void SetPrototypeHandler(LPrototypeHandler * hdlr);
			LPrototypeHandler * GetPrototypeHandler(void);
			LObject * GetRootNamespace(void);
			LObject * GetPrivateNamespace(void);
			LObject * CreateNamespace(LObject * create_under, const string & name);
			LObject * CreateAlias(LObject * create_under, const string & name, LObject * destination);
			LObject * CreateClass(LObject * create_under, const string & name);
			LObject * CreateFunction(LObject * create_under, const string & name);
			LObject * CreateFunctionOverload(LObject * create_under, LObject * retval, int argc, LObject ** argv, uint flags);
			LObject * CreateVariable(LObject * create_under, const string & name, LObject * type);
			LObject * CreateVariable(LObject * create_under, const string & name, LObject * type, XA::ObjectSize size);
			LObject * CreateField(LObject * create_under, const string & name, LObject * type, XA::ObjectSize offs_override);
			LObject * CreateField(LObject * create_under, const string & name, LObject * type, bool align_mode);
			LObject * CreateProperty(LObject * create_under, const string & name, LObject * type);
			LObject * CreatePropertySetter(LObject * prop, uint flags);
			LObject * CreatePropertyGetter(LObject * prop, uint flags);
			LObject * CreatePrivateFunction(uint flags);
			LObject * CreatePrivateClass(void);
			void InstallObject(LObject * object, const string & path);
			bool IsInterface(LObject * cls);
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
			void CreateClassDefaultMethods(LObject * cls, uint methods, ObjectArray<LObject> & vft_init);
			void CreateClassVFT(LObject * cls);
			void AdoptParentClass(LObject * cls, LObject * parent);
			void AdoptInterface(LObject * cls, LObject * interface);
			void LockClass(LObject * cls, bool lock);
			LObject * QueryObject(const string & path);
			LObject * QueryScope(void);
			LObject * QueryStaticArray(LObject * type, int volume);
			LObject * QueryTypePointer(LObject * type);
			LObject * QueryTypeReference(LObject * type);
			LObject * QueryFunctionPointer(LObject * retval, int argc, LObject ** argv);
			LObject * QueryTernaryResult(LObject * cond, LObject * if_true, LObject * if_false);
			LObject * QueryTypeOfOperator(void);
			LObject * QuerySizeOfOperator(bool max_size = false);
			LObject * QueryModuleOperator(void);
			LObject * QueryModuleOperator(const string & name);
			LObject * QueryInterfaceOperator(const string & name);
			LObject * QueryAddressOfOperator(void);
			LObject * QueryLogicalAndOperator(void);
			LObject * QueryLogicalOrOperator(void);
			LObject * QueryLiteral(bool value);
			LObject * QueryLiteral(uint64 value);
			LObject * QueryLiteral(uint64 value, int quant, bool sgn);
			LObject * QueryLiteral(double value);
			LObject * QueryLiteral(double value, int quant);
			LObject * QueryLiteral(const string & value);
			LObject * QueryDetachedLiteral(LObject * base);
			LObject * QueryNullLiteral(void);
			LObject * QueryComputable(LObject * of_type, const XA::ExpressionTree & with_tree);
			LObject * InitInstance(LObject * instance, LObject * expression);
			LObject * InitInstance(LObject * instance, int argc, LObject ** argv);
			LObject * DestroyInstance(LObject * instance);
			LObject * SetPropertyValue(LObject * prop, LObject * value);
			void AttachLiteral(LObject * literal, LObject * attach_under, const string & name);
			int QueryLiteralValue(LObject * literal);
			string QueryLiteralString(LObject * literal);
			Volumes::ObjectDictionary<uint64, DataBlock> & QueryResources(void);
			void ProduceModule(const string & asm_name, int v1, int v2, int v3, int v4, Streaming::Stream * dest);
		};
	}
}