#include "xvsl_fjson.h"

using namespace Engine;
using namespace Engine::Reflection;

namespace Serializer
{
	void WriteStringToken(DataBlock & dest, const string & input)
	{
		int len = input.Length();
		for (int i = 0; i < len; i++) {
			if (input[i] < 0x20 || input[i] == L'\\' || input[i] == L'\"') {
				if (input[i] == L'\\') {
					dest << '\\' << '\\';
				} else if (input[i] == L'\"') {
					dest << '\\' << '\"';
				} else if (input[i] == L'\n') {
					dest << '\\' << 'n';
				} else if (input[i] == L'\r') {
					dest << '\\' << 'r';
				} else if (input[i] == L'\t') {
					dest << '\\' << 't';
				} else {
					auto hex = string(uint(input[i]), HexadecimalBase, 4);
					dest << '\\' << 'u' << hex[0] << hex[1] << hex[2] << hex[3];
				}
			} else {
				if (SystemEncoding == Encoding::UTF16) {
					auto ucs_len = MeasureSequenceLength(static_cast<const widechar *>(input) + i, 2, Encoding::UTF16, Encoding::UTF32);
					auto utf16_len = (ucs_len > 1) ? 1 : 2;
					auto utf8_len = MeasureSequenceLength(static_cast<const widechar *>(input) + i, utf16_len, Encoding::UTF16, Encoding::UTF8);
					uint8 utf8[4];
					ConvertEncoding(&utf8, static_cast<const widechar *>(input) + i, utf16_len, Encoding::UTF16, Encoding::UTF8);
					for (int j = 0; j < utf8_len; j++) dest << utf8[j];
					if (utf16_len > 1) i++;
				} else {
					auto utf8_len = MeasureSequenceLength(static_cast<const widechar *>(input) + i, 1, Encoding::UTF32, Encoding::UTF8);
					uint8 utf8[4];
					ConvertEncoding(&utf8, static_cast<const widechar *>(input) + i, 1, Encoding::UTF32, Encoding::UTF8);
					for (int j = 0; j < utf8_len; j++) dest << utf8[j];
				}
			}
		}
	}
	class PropertyWriter : public IPropertyEnumerator
	{
		DataBlock _internal;
		DataBlock & _dest;
		bool _first;
	private:
		void _make_utf8(const string & input, const uint8 ** conv, int * len);
		void _write_string(const string & input);
		void _write_simple(PropertyInfo & prop);
	public:
		PropertyWriter(DataBlock & dest) : _internal(0x1000), _dest(dest), _first(true) {}
		virtual void EnumerateProperty(const string & name, void * address, PropertyType type, PropertyType inner, int volume, int element_size) override;
	};
	void WriteStructure(DataBlock & dest, Engine::Reflection::Reflected & object)
	{
		PropertyWriter wri(dest);
		dest << '{';
		object.EnumerateProperties(wri);
		dest << '}';
	}
	void PropertyWriter::_make_utf8(const string & input, const uint8 ** conv, int * len)
	{
		int length = input.GetEncodedLength(Encoding::UTF8);
		if (length > _internal.Length()) _internal.SetLength(length);
		input.Encode(_internal.GetBuffer(), Encoding::UTF8, false);
		*conv = _internal.GetBuffer();
		*len = length;
	}
	void PropertyWriter::_write_string(const string & input)
	{
		const uint8 * data;
		int length;
		_make_utf8(input, &data, &length);
		_dest.Append(data, length);
	}
	void PropertyWriter::_write_simple(PropertyInfo & prop)
	{
		if (prop.Type == PropertyType::UInt8) {
			_write_string(string(prop.Get<uint8>()));
		} else if (prop.Type == PropertyType::Int8) {
			_write_string(string(prop.Get<int8>()));
		} else if (prop.Type == PropertyType::UInt16) {
			_write_string(string(prop.Get<uint16>()));
		} else if (prop.Type == PropertyType::Int16) {
			_write_string(string(prop.Get<int16>()));
		} else if (prop.Type == PropertyType::UInt32) {
			_write_string(string(prop.Get<uint32>()));
		} else if (prop.Type == PropertyType::Int32) {
			_write_string(string(prop.Get<int32>()));
		} else if (prop.Type == PropertyType::UInt64) {
			_write_string(string(prop.Get<uint64>()));
		} else if (prop.Type == PropertyType::Int64) {
			_write_string(string(prop.Get<int64>()));
		} else if (prop.Type == PropertyType::Boolean) {
			auto v = prop.Get<bool>();
			if (v) _dest << 't' << 'r' << 'u' << 'e';
			else _dest << 'f' << 'a' << 'l' << 's' << 'e';
		} else if (prop.Type == PropertyType::String) {
			_dest << '\"';
			WriteStringToken(_dest, prop.Get<string>());
			_dest << '\"';
		} else {
			_dest << 'n' << 'u' << 'l' << 'l';
		}
	}
	void PropertyWriter::EnumerateProperty(const string & name, void * address, PropertyType type, PropertyType inner, int volume, int element_size)
	{
		if (_first) _first = false;
		else _dest << ',';
		_dest << '\"';
		_write_string(name);
		_dest << '\"' << ':';
		PropertyInfo pi = { address, type, inner, volume, element_size, name };
		if (volume == 1) {
			if (pi.Type == PropertyType::Array) {
				if (pi.InnerType == PropertyType::UInt32) {
					auto & v = pi.Get< Array<uint32> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::Int32) {
					auto & v = pi.Get< Array<int32> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::UInt64) {
					auto & v = pi.Get< Array<uint64> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::Int64) {
					auto & v = pi.Get< Array<int64> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::Boolean) {
					auto & v = pi.Get< Array<bool> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::String) {
					auto & v = pi.Get< Array<string> >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { PropertyInfo spi = { v.GetBuffer() + i, inner, PropertyType::Unknown, 1, sizeof(*v.GetBuffer()), L"" }; _write_simple(spi); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else if (pi.InnerType == PropertyType::Structure) {
					auto & v = pi.Get< ReflectedArray >();
					_dest << '[';
					for (int i = 0; i < v.Length(); i++) { auto & obj = v.ElementAt(i); WriteStructure(_dest, obj); if (i < v.Length() - 1) _dest << ','; }
					_dest << ']';
				} else {
					_dest << 'n' << 'u' << 'l' << 'l';
				}
			} else if (pi.Type == PropertyType::Structure) {
				auto & obj = pi.Get<Reflected>();
				WriteStructure(_dest, obj);
			} else _write_simple(pi);
		} else {
			_dest << '[';
			for (int i = 0; i < volume; i++) {
				auto pl = pi.VolumeElement(i);
				_write_simple(pl);
				if (i < volume - 1) _dest << ',';
			}
			_dest << ']';
		}
	}
}

Engine::DataBlock * SerializeToJSON(Engine::Reflection::Reflected & object)
{
	SafePointer<DataBlock> result = new DataBlock(0x10000);
	Serializer::WriteStructure(*result, object);
	result->Retain();
	return result;
}