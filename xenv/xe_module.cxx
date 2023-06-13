#include "xe_module.h"

namespace Engine
{
	namespace XE
	{
		Module::TypeReference::TypeReference(const string & src) : _desc(src), _from(0) { _length = src.Length(); }
		Module::TypeReference::TypeReference(const string & src, int from, int length) : _desc(src), _from(from), _length(length) {}
		Module::TypeReference::Class Module::TypeReference::GetReferenceClass(void) const
		{
			if (_length <= 0) return Class::Unknown;
			if (_desc[_from] == L'C') return Class::Class;
			else if (_desc[_from] == L'?') return Class::AbstractPlaceholder;
			else if (_desc[_from] == L'A') return Class::Array;
			else if (_desc[_from] == L'P') return Class::Pointer;
			else if (_desc[_from] == L'R') return Class::Reference;
			else if (_desc[_from] == L'F') return Class::Function;
			else if (_desc[_from] == L'I') return Class::AbstractInstance;
			else return Class::Unknown;
		}
		string Module::TypeReference::GetClassName(void) const
		{
			if (GetReferenceClass() == Class::Class) return _desc.Fragment(_from + 1, _length - 1);
			else throw InvalidStateException();
		}
		int Module::TypeReference::GetAbstractPlaceholderIndex(void) const
		{
			if (GetReferenceClass() == Class::AbstractPlaceholder) return _desc.Fragment(_from + 1, _length - 1).ToInt32();
			else throw InvalidStateException();
		}
		int Module::TypeReference::GetArrayVolume(void) const
		{
			if (GetReferenceClass() == Class::Array) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L',') i++;
				return _desc.Fragment(_from + 1, i - 1).ToInt32();
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetArrayElement(void) const
		{
			if (GetReferenceClass() == Class::Array) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L',') i++;
				if (i >= _length) throw InvalidFormatException();
				return TypeReference(_desc, _from + i + 1, _length - i - 1);
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetPointerDestination(void) const
		{
			if (GetReferenceClass() == Class::Pointer) return TypeReference(_desc, _from + 1, _length - 1);
			else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetReferenceDestination(void) const
		{
			if (GetReferenceClass() == Class::Reference) return TypeReference(_desc, _from + 1, _length - 1);
			else throw InvalidStateException();
		}
		Array<Module::TypeReference> * Module::TypeReference::GetFunctionSignature(void) const
		{
			if (GetReferenceClass() == Class::Function) {
				SafePointer< Array<TypeReference> > result = new Array<TypeReference>(0x10);
				int i = 1;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					result->Append(TypeReference(_desc, _from + s, i - s));
					i++;
				}
				if (result->Length() == 0) throw InvalidFormatException();
				result->Retain();
				return result;
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetAbstractInstanceBase(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1, n = 0;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++; n++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					if (n == 1) return TypeReference(_desc, _from + s, i - s);
					i++;
				}
				throw InvalidFormatException();
			} else throw InvalidStateException();
		}
		int Module::TypeReference::GetAbstractInstanceParameterIndex(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1;
				while (i < _length && _desc[_from + i] != L'(') i++;
				return _desc.Fragment(_from + 1, i - 1).ToInt32();
			} else throw InvalidStateException();
		}
		Module::TypeReference Module::TypeReference::GetAbstractInstanceParameterType(void) const
		{
			if (GetReferenceClass() == Class::AbstractInstance) {
				int i = 1, n = 0;
				while (i < _length) {
					while (i < _length && _desc[_from + i] != L'(') i++;
					if (i >= _length) break;
					i++; n++;
					int s = i, l = 1;
					while (i < _length) {
						if (_desc[_from + i] == L'(') l++; else if (_desc[_from + i] == L')') { l--; if (!l) break; }
						i++;
					}
					if (l) throw InvalidFormatException();
					if (n == 2) return TypeReference(_desc, _from + s, i - s);
					i++;
				}
				throw InvalidFormatException();
			} else throw InvalidStateException();
		}
		string Module::TypeReference::MakeClassReference(const string & class_name) { return L"C" + class_name; }
		string Module::TypeReference::MakeAbstractPlaceholder(int param_index) { return L"?" + string(param_index); }
		string Module::TypeReference::MakeArray(const string & cn, int volume) { return L"A" + string(volume) + L"," + cn; }
		string Module::TypeReference::MakePointer(const string & cn) { return L"P" + cn; }
		string Module::TypeReference::MakeReference(const string & cn) { return L"R" + cn; }
		string Module::TypeReference::MakeFunction(const string & rv_cn, const Array<string> * args_cn)
		{
			DynamicString result;
			result << L"F(" << rv_cn << L")";
			if (args_cn) for (auto & a : *args_cn) result << L"(" << a << L")";
			return result.ToString();
		}
		string Module::TypeReference::MakeInstance(const string & cn_of, int param_index, const string & cn_with) { return L"I" + string(param_index) + L"(" + cn_of + L")(" + cn_with + L")"; }
		string Module::TypeReference::ToString(void) const
		{
			auto cls = GetReferenceClass();
			if (cls == Class::Class) return GetClassName();
			else if (cls == Class::AbstractPlaceholder) return L"?" + string(GetAbstractPlaceholderIndex());
			else if (cls == Class::Array) return GetArrayElement().ToString() + L" [" + string(GetArrayVolume()) + L"]";
			else if (cls == Class::Pointer) return GetPointerDestination().ToString() + L" PTR";
			else if (cls == Class::Reference) return GetReferenceDestination().ToString() + L" REF";
			else if (cls == Class::Function) {
				SafePointer< Array<TypeReference> > sign = GetFunctionSignature();
				DynamicString result;
				result << L"(";
				for (int i = 1; i < sign->Length(); i++) { if (i > 1) result << L", "; result << sign->ElementAt(i).ToString(); }
				result << L") -> (" << sign->ElementAt(0).ToString() << L")";
				return result.ToString();
			} else if (cls == Class::AbstractInstance) {
				DynamicString result;
				result << GetAbstractInstanceBase().ToString();
				result << L" WITH ?" << string(GetAbstractInstanceParameterIndex()) << L" = (";
				result << GetAbstractInstanceParameterType().ToString() << L")";
				return result.ToString();
			} else return L"UNKNOWN";
		}

		Module::Module(void) : ModuleDependsOn(0x10)
		{
			// TODO: IMPLEMENT
		}
		Module::Module(Streaming::Stream * source) : ModuleDependsOn(0x10)
		{
			// TODO: IMPLEMENT
		}
		Module::~Module(void)
		{
			// TODO: IMPLEMENT
		}
	}
}