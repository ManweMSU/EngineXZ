#pragma once

#include "Interfaces/Base.h"

#include <new>
#include <type_traits>

#define ENGINE_MAKE_ITERATORS(array_type, element_type, length) \
	ArrayEnumerator< array_type<element_type>, element_type > Elements(void) { return ArrayEnumerator< array_type<element_type>, element_type >(*this, 0, length, 1); } \
	ArrayEnumerator< const array_type<element_type>, const element_type > Elements(void) const { return ArrayEnumerator< const array_type<element_type>, const element_type >(*this, 0, length, 1); } \
	ArrayEnumerator< array_type<element_type>, element_type > InversedElements(void) { return ArrayEnumerator< array_type<element_type>, element_type >(*this, length - 1, -1, -1); } \
	ArrayEnumerator< const array_type<element_type>, const element_type > InversedElements(void) const { return ArrayEnumerator< const array_type<element_type>, const element_type >(*this, length - 1, -1, -1); } \
	ArrayIterator< array_type<element_type>, element_type > begin(void) { return ArrayIterator< array_type<element_type>, element_type >(*this, 0, 1); } \
	ArrayIterator< const array_type<element_type>, const element_type > begin(void) const { return ArrayIterator< const array_type<element_type>, const element_type >(*this, 0, 1); } \
	ArrayIterator< array_type<element_type>, element_type > end(void) { return ArrayIterator< array_type<element_type>, element_type >(*this, length, 1); } \
	ArrayIterator< const array_type<element_type>, const element_type > end(void) const { return ArrayIterator< const array_type<element_type>, const element_type >(*this, length, 1); }

namespace Engine
{
	class Object;
	class ImmutableString;
	template <class V> class Array;
	template <class V> class SafeArray;
	template <class V> class ObjectArray;

	class Object
	{
	private:
		uint _refcount;
	public:
		Object(void);
		Object(const Object & src) = delete;
		Object & operator = (const Object & src) = delete;
		virtual uint Retain(void);
		virtual uint Release(void);
		virtual ~Object(void);
		virtual ImmutableString ToString(void) const;
		uint GetReferenceCount(void) const;
	};

	class Exception : public Object
	{
	public:
		Exception(void);
		Exception(const Exception & e);
		Exception(Exception && e);
		Exception & operator = (const Exception & e);
		ImmutableString ToString(void) const override;
	};
	class OutOfMemoryException : public Exception { public: ImmutableString ToString(void) const override; };
	class InvalidArgumentException : public Exception { public: ImmutableString ToString(void) const override; };
	class InvalidFormatException : public Exception { public: ImmutableString ToString(void) const override; };
	class InvalidStateException : public Exception { public: ImmutableString ToString(void) const override; };

	class ImmutableString final
	{
	private:
		widechar * text;
		int GeneralizedFindFirst(int from, const widechar * seq, int length) const;
		ImmutableString GeneralizedReplace(const widechar * seq, int length, const widechar * with, int withlen) const;
		uint64 GeneralizedToUInt64(int dfrom, int dto) const;
		uint64 GeneralizedToUInt64(int dfrom, int dto, const ImmutableString & digits, bool case_sensitive) const;
	public:
		ImmutableString(void);
		ImmutableString(const ImmutableString & src);
		ImmutableString(ImmutableString && src);
		ImmutableString(const widechar * src);
		ImmutableString(const widechar * sequence, int length);
		ImmutableString(const char * src);
		ImmutableString(int8 src);
		ImmutableString(uint8 src);
		ImmutableString(int16 src);
		ImmutableString(uint16 src);
		ImmutableString(int32 src);
		ImmutableString(uint32 src);
		ImmutableString(int64 src);
		ImmutableString(uint64 src);
		ImmutableString(uint32 value, const ImmutableString & digits, int minimal_length = 0);
		ImmutableString(uint64 value, const ImmutableString & digits, int minimal_length = 0);
		ImmutableString(const void * Sequence, int Length, Encoding SequenceEncoding);
		ImmutableString(float src, widechar separator = L'.', int digits = 7);
		ImmutableString(double src, widechar separator = L'.', int digits = 16);
		ImmutableString(bool src);
		ImmutableString(widechar src);
		ImmutableString(widechar src, int repeats);
		ImmutableString(const Object * object);
		explicit ImmutableString(const void * src);
		~ImmutableString(void);

		ImmutableString & operator = (const ImmutableString & src);
		ImmutableString & operator = (const widechar * src);
		operator const widechar * (void) const;
		int Length(void) const;

		bool friend operator == (const ImmutableString & a, const ImmutableString & b);
		bool friend operator == (const widechar * a, const ImmutableString & b);
		bool friend operator == (const ImmutableString & a, const widechar * b);
		bool friend operator != (const ImmutableString & a, const ImmutableString & b);
		bool friend operator != (const widechar * a, const ImmutableString & b);
		bool friend operator != (const ImmutableString & a, const widechar * b);

		bool friend operator <= (const ImmutableString & a, const ImmutableString & b);
		bool friend operator >= (const ImmutableString & a, const ImmutableString & b);
		bool friend operator < (const ImmutableString & a, const ImmutableString & b);
		bool friend operator > (const ImmutableString & a, const ImmutableString & b);

		static int Compare(const ImmutableString & a, const ImmutableString & b);

		widechar operator [] (int index) const;
		widechar CharAt(int index) const;

		ImmutableString ToString(void) const;

		void Concatenate(const ImmutableString & str);
		
		ImmutableString friend operator + (const ImmutableString & a, const ImmutableString & b);
		ImmutableString friend operator + (const widechar * a, const ImmutableString & b);
		ImmutableString friend operator + (const ImmutableString & a, const widechar * b);
		ImmutableString & operator += (const ImmutableString & str);

		int FindFirst(widechar letter) const;
		int FindFirst(const ImmutableString & str) const;
		int FindLast(widechar letter) const;
		int FindLast(const ImmutableString & str) const;

		ImmutableString Fragment(int PosStart, int CharLength) const;
		ImmutableString Replace(const ImmutableString & Substring, const ImmutableString & with) const;
		ImmutableString Replace(widechar Substring, const ImmutableString & with) const;
		ImmutableString Replace(const ImmutableString & Substring, widechar with) const;
		ImmutableString Replace(widechar Substring, widechar with) const;

		int GetEncodedLength(Encoding encoding) const;
		void Encode(void * buffer, Encoding encoding, bool include_terminator) const;
		Array<uint8> * EncodeSequence(Encoding encoding, bool include_terminator) const;
		Array<ImmutableString> Split(widechar divider) const;

		uint64 ToUInt64(void) const;
		uint64 ToUInt64(const ImmutableString & digits, bool case_sensitive = false) const;
		int64 ToInt64(void) const;
		int64 ToInt64(const ImmutableString & digits, bool case_sensitive = false) const;
		uint32 ToUInt32(void) const;
		uint32 ToUInt32(const ImmutableString & digits, bool case_sensitive = false) const;
		int32 ToInt32(void) const;
		int32 ToInt32(const ImmutableString & digits, bool case_sensitive = false) const;
		float ToFloat(void) const;
		float ToFloat(const ImmutableString & separators) const;
		double ToDouble(void) const;
		double ToDouble(const ImmutableString & separators) const;
		bool ToBoolean(void) const;
	};

	typedef ImmutableString string;
	typedef bool Boolean;

	constexpr const widechar * DecimalBase = L"0123456789";
	constexpr const widechar * HexadecimalBase = L"0123456789ABCDEF";
	constexpr const widechar * HexadecimalBaseLowerCase = L"0123456789abcdef";
	constexpr const widechar * OctalBase = L"01234567";
	constexpr const widechar * BinaryBase = L"01";

	template <class V> void swap(V & a, V & b) { if (&a == &b) return; uint8 buffer[sizeof(V)]; MemoryCopy(buffer, &a, sizeof(V)); MemoryCopy(&a, &b, sizeof(V)); MemoryCopy(&b, buffer, sizeof(V)); }
	template <class V> void safe_swap(V & a, V & b) { V e = a; a = b; b = e; }

	template <class V> V min(V a, V b) { return (a < b) ? a : b; }
	template <class V> V max(V a, V b) { return (a < b) ? b : a; }

	double sgn(double x);
	float sgn(float x);
	int sgn(int x);

	template<class T, class = void> struct HasToStringTraits { static constexpr const bool value = false; };
	template<class T> struct HasToStringTraits<T, std::void_t<decltype(&T::ToString)> > { static constexpr const bool value = true; };
	
	template<class T> std::enable_if_t<std::is_convertible<T, ImmutableString>::value, ImmutableString> GetStringRepresentation(const T & t) { return t; }
	template<class T> std::enable_if_t<!std::is_convertible<T, ImmutableString>::value && HasToStringTraits<T>::value, ImmutableString> GetStringRepresentation(const T & t) { return t.ToString(); }
	template<class T> std::enable_if_t<!std::is_convertible<T, ImmutableString>::value && !HasToStringTraits<T>::value, ImmutableString> GetStringRepresentation(const T & t) { return L"?"; }

	template<class A, class V> class ArrayIterator
	{
		A & _array;
		int _current;
		int _delta;
	public:
		ArrayIterator(A & volume, int current, int delta) : _array(volume), _current(current), _delta(delta) {}
		V & operator * (void) const { return _array[_current]; }
		V & operator * (void) { return _array[_current]; }
		V & operator -> (void) const { return _array[_current]; }
		V & operator -> (void) { return _array[_current]; }

		bool operator == (const ArrayIterator & b) const { return _current == b._current; }
		bool operator != (const ArrayIterator & b) const { return _current != b._current; }
		bool operator == (int b) const { return _current == b; }
		bool operator != (int b) const { return _current != b; }

		ArrayIterator & operator ++ (void) { _current += _delta; return *this; }
		ArrayIterator operator ++ (int) { ArrayIterator result(_array, _current, _delta); _current += _delta; return result; }
	};
	template<class A, class V> class ArrayEnumerator
	{
		A & _array;
		int _begin;
		int _end;
		int _delta;
	public:
		ArrayEnumerator(A & volume, int begin, int end, int delta) : _array(volume), _begin(begin), _end(end), _delta(delta) {}
		ArrayIterator<A, V> begin(void) { return ArrayIterator<A, V>(_array, _begin, _delta); }
		ArrayIterator<A, V> end(void) { return ArrayIterator<A, V>(_array, _end, _delta); }
	};

	template <class V> class Array : public Object
	{
		V * data;
		int count;
		int allocated;
		int block;
		int block_align(int element_count) { return ((int64(element_count) + block - 1) / block) * block; }
		void require(int elements, bool nothrow = false)
		{
			int new_size = block_align(elements);
			if (new_size != allocated) {
				if (new_size > allocated) {
					V * new_data = reinterpret_cast<V*>(realloc(data, sizeof(V) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					} else if (!nothrow) throw OutOfMemoryException();
				} else {
					V * new_data = reinterpret_cast<V*>(realloc(data, sizeof(V) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					}
				}
			}
		}
		void append(const V & v) { new (data + count) V(v); count++; }
	public:
		Array(void) : count(0), allocated(0), data(0), block(0x400) {}
		Array(const Array & src) : count(src.count), allocated(0), data(0), block(src.block)
		{
			require(count); int i = 0;
			try { for (i = 0; i < count; i++) new (data + i) V(src.data[i]); }
			catch (...) {
				for (int j = i - 1; j >= 0; j--) data[j].V::~V();
				free(data); throw;
			}
		}
		Array(Array && src) : count(src.count), allocated(src.allocated), data(src.data), block(src.block) { src.data = 0; src.allocated = 0; src.count = 0; }
		explicit Array(int BlockSize) : count(0), allocated(0), data(0), block(max(BlockSize, 1)) {}
		~Array(void) override { for (int i = 0; i < count; i++) data[i].V::~V(); free(data); }

		Array & operator = (const Array & src)
		{
			if (this == &src) return *this;
			Array Copy(src.block);
			Copy.require(src.count);
			for (int i = 0; i < src.count; i++) Copy.append(src.data[i]);
			for (int i = 0; i < count; i++) data[i].V::~V();
			free(data);
			data = Copy.data; count = Copy.count; allocated = Copy.allocated; block = Copy.block;
			Copy.data = 0; Copy.count = 0; Copy.allocated = 0;
			return *this;
		}

		void Append(const V & v) { require(count + 1); append(v); }
		void Append(const Array & src) { if (&src == this) throw InvalidArgumentException(); require(count + src.count); for (int i = 0; i < src.count; i++) append(src.data[i]); }
		void Append(const V * v, int Count) { if (!Count) return; if (data == v) throw InvalidArgumentException(); require(count + Count); for (int i = 0; i < Count; i++) append(v[i]); }
		void SwapAt(int i, int j) { swap(data[i], data[j]); }
		void Insert(const V & v, int IndexAt)
		{
			require(count + 1);
			for (int i = count - 1; i >= IndexAt; i--) swap(data[i], data[i + 1]);
			try { new (data + IndexAt) V(v); count++; }
			catch (...) { for (int i = IndexAt; i < count; i++) swap(data[i], data[i + 1]); throw; }
		}
		V & ElementAt(int index) { return data[index]; }
		const V & ElementAt(int index) const { return data[index]; }
		V & FirstElement(void) { return data[0]; }
		const V & FirstElement(void) const { return data[0]; }
		V & LastElement(void) { return data[count - 1]; }
		const V & LastElement(void) const { return data[count - 1]; }
		void Remove(int index) { data[index].V::~V(); for (int i = index; i < count - 1; i++) swap(data[i], data[i + 1]); count--; require(count, true); }
		void RemoveFirst(void) { Remove(0); }
		void RemoveLast(void) { Remove(count - 1); }
		void Clear(void) { for (int i = 0; i < count; i++) data[i].V::~V(); free(data); data = 0; count = 0; allocated = 0; }
		void SetLength(int length)
		{
			if (length > count) {
				require(length); int i;
				try { for (i = count; i < length; i++) new (data + i) V(); }
				catch (...) { for (int j = i - 1; j >= count; j--) data[i].V::~V(); throw; }
				count = length;
			} else if (length < count) {
				if (length < 0) throw InvalidArgumentException();
				for (int i = length; i < count; i++) data[i].V::~V(); count = length;
				require(count, true);
			}
		}
		int Length(void) const { return count; }
		V * GetBuffer(void) { return data; }
		const V * GetBuffer(void) const { return data; }
		
		string ToString(void) const override
		{
			string result = L"Array : [";
			for (int i = 0; i < count; i++) {
				if (i) result += L", ";
				result += GetStringRepresentation(data[i]);
			}
			return result + L"]";
		}

		operator V * (void) { return data; }
		operator const V * (void) { return data; }
		Array & operator << (const V & v) { Append(v); return *this; }
		Array & operator << (const Array & src) { Append(src); return *this; }
		V & operator [] (int index) { return data[index]; }
		const V & operator [] (int index) const { return data[index]; }
		bool friend operator == (const Array & a, const Array & b)
		{
			if (a.count != b.count) return false;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return false;
			return true;
		}
		bool friend operator != (const Array & a, const Array & b)
		{
			if (a.count != b.count) return true;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return true;
			return false;
		}

		ENGINE_MAKE_ITERATORS(Array, V, count)
	};

	template <class V> class SafeArray : public Object
	{
		V ** data;
		int count;
		int allocated;
		int block;
		int block_align(int element_count) { return ((int64(element_count) + block - 1) / block) * block; }
		void require(int elements, bool nothrow = false)
		{
			int new_size = block_align(elements);
			if (new_size != allocated) {
				if (new_size > allocated) {
					V ** new_data = reinterpret_cast<V**>(realloc(data, sizeof(V*) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					} else if (!nothrow) throw OutOfMemoryException();
				} else {
					V ** new_data = reinterpret_cast<V**>(realloc(data, sizeof(V*) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					}
				}
			}
		}
		void append(const V & v) { data[count] = new V(v); count++; }
	public:
		SafeArray(void) : count(0), allocated(0), data(0), block(0x400) {}
		SafeArray(const SafeArray & src) : count(src.count), allocated(0), data(0), block(src.block)
		{
			require(count); int i = 0;
			try { for (i = 0; i < count; i++) data[i] = new V(*src.data[i]); } catch (...) {
				for (int j = i - 1; j >= 0; j--) delete data[j];
				free(data); throw;
			}
		}
		SafeArray(SafeArray && src) : count(src.count), allocated(src.allocated), data(src.data), block(src.block) { src.data = 0; src.allocated = 0; src.count = 0; }
		explicit SafeArray(int BlockSize) : count(0), allocated(0), data(0), block(max(BlockSize, 1)) {}
		~SafeArray(void) override { for (int i = 0; i < count; i++) delete data[i]; free(data); }

		SafeArray & operator = (const SafeArray & src)
		{
			if (this == &src) return *this;
			SafeArray Copy(src.block);
			Copy.require(src.count);
			for (int i = 0; i < src.count; i++) Copy.append(*src.data[i]);
			for (int i = 0; i < count; i++) delete data[i];
			free(data);
			data = Copy.data; count = Copy.count; allocated = Copy.allocated; block = Copy.block;
			Copy.data = 0; Copy.count = 0; Copy.allocated = 0;
			return *this;
		}

		void Append(const V & v) { require(count + 1); append(v); }
		void Append(const SafeArray & src) { if (&src == this) throw InvalidArgumentException(); require(count + src.count); for (int i = 0; i < src.count; i++) append(*src.data[i]); }
		void Append(const V * v, int Count) { require(count + Count); for (int i = 0; i < Count; i++) append(v[i]); }
		void SwapAt(int i, int j) { safe_swap(data[i], data[j]); }
		void Insert(const V & v, int IndexAt)
		{
			require(count + 1);
			for (int i = count - 1; i >= IndexAt; i--) safe_swap(data[i], data[i + 1]);
			try { data[IndexAt] = new V(v); count++; } catch (...) { for (int i = IndexAt; i < count; i++) safe_swap(data[i], data[i + 1]); throw; }
		}
		V & ElementAt(int index) { return *data[index]; }
		const V & ElementAt(int index) const { return *data[index]; }
		V & FirstElement(void) { return *data[0]; }
		const V & FirstElement(void) const { return *data[0]; }
		V & LastElement(void) { return *data[count - 1]; }
		const V & LastElement(void) const { return *data[count - 1]; }
		void Remove(int index) { delete data[index]; for (int i = index; i < count - 1; i++) safe_swap(data[i], data[i + 1]); count--; require(count, true); }
		void RemoveFirst(void) { Remove(0); }
		void RemoveLast(void) { Remove(count - 1); }
		void Clear(void) { for (int i = 0; i < count; i++) delete data[i]; free(data); data = 0; count = 0; allocated = 0; }
		void SetLength(int length)
		{
			if (length > count) {
				require(length); int i;
				try { for (i = count; i < length; i++) data[i] = new V(); } catch (...) { for (int j = i - 1; j >= count; j--) delete data[i]; throw; }
				count = length;
			} else if (length < count) {
				if (length < 0) throw InvalidArgumentException();
				for (int i = length; i < count; i++) delete data[i]; count = length;
				require(count, true);
			}
		}
		int Length(void) const { return count; }

		string ToString(void) const override
		{
			string result = L"Safe Array : [";
			for (int i = 0; i < count; i++) {
				if (i) result += L", ";
				result += GetStringRepresentation(*data[i]);
			}
			return result + L"]";
		}

		SafeArray & operator << (const V & v) { Append(v); return *this; }
		SafeArray & operator << (const SafeArray & src) { Append(src); return *this; }
		V & operator [] (int index) { return *data[index]; }
		const V & operator [] (int index) const { return *data[index]; }
		bool friend operator == (const SafeArray & a, const SafeArray & b)
		{
			if (a.count != b.count) return false;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return false;
			return true;
		}
		bool friend operator != (const SafeArray & a, const SafeArray & b)
		{
			if (a.count != b.count) return true;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return true;
			return false;
		}

		ENGINE_MAKE_ITERATORS(SafeArray, V, count)
	};

	template <class V> class ObjectArray : public Object
	{
		V ** data;
		int count;
		int allocated;
		int block;
		int block_align(int element_count) { return ((int64(element_count) + block - 1) / block) * block; }
		void require(int elements, bool nothrow = false)
		{
			int new_size = block_align(elements);
			if (new_size != allocated) {
				if (new_size > allocated) {
					V ** new_data = reinterpret_cast<V**>(realloc(data, sizeof(V*) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					} else if (!nothrow) throw OutOfMemoryException();
				} else {
					V ** new_data = reinterpret_cast<V**>(realloc(data, sizeof(V*) * new_size));
					if (new_data || new_size == 0) {
						data = new_data;
						allocated = new_size;
					}
				}
			}
		}
		void append(V * v) { data[count] = v; if (v) v->Retain(); count++; }
	public:
		ObjectArray(void) : count(0), allocated(0), data(0), block(0x400) {}
		ObjectArray(const ObjectArray & src) : count(src.count), allocated(0), data(0), block(src.block)
		{
			require(count); int i = 0;
			try { for (i = 0; i < count; i++) { data[i] = src.data[i]; if (src.data[i]) src.data[i]->Retain(); } } catch (...) {
				for (int j = i - 1; j >= 0; j--) if (data[j]) data[j]->Release();
				free(data); throw;
			}
		}
		ObjectArray(ObjectArray && src) : count(src.count), allocated(src.allocated), data(src.data), block(src.block) { src.data = 0; src.allocated = 0; src.count = 0; }
		explicit ObjectArray(int BlockSize) : count(0), allocated(0), data(0), block(max(BlockSize, 1)) {}
		~ObjectArray(void) override { for (int i = 0; i < count; i++) if (data[i]) data[i]->Release(); free(data); }

		ObjectArray & operator = (const ObjectArray & src)
		{
			if (this == &src) return *this;
			ObjectArray Copy(src.block);
			Copy.require(src.count);
			for (int i = 0; i < src.count; i++) Copy.append(src.data[i]);
			for (int i = 0; i < count; i++) if (data[i]) data[i]->Release();
			free(data);
			data = Copy.data; count = Copy.count; allocated = Copy.allocated; block = Copy.block;
			Copy.data = 0; Copy.count = 0; Copy.allocated = 0;
			return *this;
		}

		void Append(V * v) { require(count + 1); append(v); }
		void Append(const ObjectArray & src) { if (&src == this) throw InvalidArgumentException(); require(count + src.count); for (int i = 0; i < src.count; i++) append(src.data[i]); }
		void SwapAt(int i, int j) { safe_swap(data[i], data[j]); }
		void Insert(V * v, int IndexAt)
		{
			require(count + 1);
			for (int i = count - 1; i >= IndexAt; i--) safe_swap(data[i], data[i + 1]);
			try { data[IndexAt] = v; if (v) v->Retain(); count++; } catch (...) { for (int i = IndexAt; i < count; i++) safe_swap(data[i], data[i + 1]); throw; }
		}
		V * ElementAt(int index) const { return data[index]; }
		V * FirstElement(void) const { return data[0]; }
		V * LastElement(void) const { return data[count - 1]; }
		void Remove(int index) { if (data[index]) data[index]->Release(); for (int i = index; i < count - 1; i++) safe_swap(data[i], data[i + 1]); count--; require(count, true); }
		void RemoveFirst(void) { Remove(0); }
		void RemoveLast(void) { Remove(count - 1); }
		void Clear(void) { for (int i = 0; i < count; i++) if (data[i]) data[i]->Release(); free(data); data = 0; count = 0; allocated = 0; }
		void SetElement(V * v, int index) { if (data[index]) data[index]->Release(); data[index] = v; if (v) v->Retain(); }
		int Length(void) const { return count; }

		string ToString(void) const override
		{
			string result = L"Object Array : [";
			if (count) for (int i = 0; i < count; i++) { result += data[i]->ToString() + ((i == count - 1) ? L"]" : L", "); }
			else result += L"]";
			return result;
		}

		ObjectArray & operator << (V * v) { Append(v); return *this; }
		ObjectArray & operator << (const ObjectArray & src) { Append(src); return *this; }
		V & operator [] (int index) const { return *data[index]; }
		bool friend operator == (const ObjectArray & a, const ObjectArray & b)
		{
			if (a.count != b.count) return false;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return false;
			return true;
		}
		bool friend operator != (const ObjectArray & a, const ObjectArray & b)
		{
			if (a.count != b.count) return true;
			for (int i = 0; i < a.count; i++) if (a[i] != b[i]) return true;
			return false;
		}

		ENGINE_MAKE_ITERATORS(ObjectArray, V, count)
	};

	typedef Array<uint8> DataBlock;

	ImmutableString StringFromDataBlock(const DataBlock * data, int max_length, bool byte_spaces);

	template <class A, class F> void SortArrayRange(A & volume, F comparator, int from, int count)
	{
		if (count < 2) return;
		int ref = from + count / 2;
		int l = from - 1;
		int r = from + count;
		while (true) {
			do l++; while (comparator(volume[l], volume[ref]) < 0);
			do r--; while (comparator(volume[r], volume[ref]) > 0);
			if (l >= r) {
				if (l > r) ref = r;
				else if (r == from + count - 1) ref = r - 1;
				else ref = r;
				break;
			}
			volume.SwapAt(l, r);
			if (ref == l) ref = r;
			else if (ref == r) ref = l;
		}
		SortArrayRange(volume, comparator, from, ref - from + 1);
		SortArrayRange(volume, comparator, ref + 1, count - ref + from - 1);
	}
	template <class A> void SortArrayRange(A & volume, bool ascending, int from, int count)
	{
		if (count < 2) return;
		int ref = from + count / 2;
		int l = from - 1;
		int r = from + count;
		while (true) {
			if (ascending) {
				do l++; while (volume[l] < volume[ref]);
				do r--; while (volume[r] > volume[ref]);
			} else {
				do l++; while (volume[l] > volume[ref]);
				do r--; while (volume[r] < volume[ref]);
			}
			if (l >= r) {
				if (l > r) ref = r;
				else if (r == from + count - 1) ref = r - 1;
				else ref = r;
				break;
			}
			volume.SwapAt(l, r);
			if (ref == l) ref = r;
			else if (ref == r) ref = l;
		}
		SortArrayRange(volume, ascending, from, ref - from + 1);
		SortArrayRange(volume, ascending, ref + 1, count - ref + from - 1);
	}
	template <class A, class F> void SortArray(A & volume, F comparator) { SortArrayRange(volume, comparator, 0, volume.Length()); }
	template <class A> void SortArray(A & volume, bool ascending = true) { SortArrayRange(volume, ascending, 0, volume.Length()); }

	template <class V> int BinarySearchLE(const Array<V> & volume, const V & value)
	{
		if (!volume.Length()) throw InvalidArgumentException();
		int bl = 0, bh = volume.Length();
		while (bh - bl > 1) {
			int mid = (bl + bh) / 2;
			if (volume[mid] > value) { bh = mid; } else { bl = mid; }
		}
		if (bl == 0) {
			if (value < volume[0]) bl = -1;
		}
		return bl;
	}

	template <class O> class SafePointer final
	{
	private:
		O * reference = 0;
	public:
		SafePointer(void) : reference(0) {}
		SafePointer(O * ref) : reference(ref) {}
		SafePointer(const SafePointer & src)
		{
			reference = src.reference;
			if (reference) reference->Retain();
		}
		SafePointer(SafePointer && src) : reference(src.reference) { src.reference = 0; }
		~SafePointer(void) { if (reference) reference->Release(); reference = 0; }
		SafePointer & operator = (const SafePointer & src)
		{
			if (this == &src) return *this;
			if (reference) reference->Release();
			reference = src.reference;
			if (reference) reference->Retain();
			return *this;
		}

		O & operator * (void) const { return *reference; }
		O * operator -> (void) const { return reference; }
		operator O * (void) const { return reference; }
		operator bool (void) const { return reference != 0; }
		O * Inner(void) const { return reference; }
		O ** InnerRef(void) { return &reference; }

		void SetReference(O * ref) { if (reference) reference->Release(); reference = ref; }
		void SetRetain(O * ref) { if (reference) reference->Release(); reference = ref; if (reference) reference->Retain(); }

		string ToString(void) const { return string(reference); }

		bool friend operator == (const SafePointer & a, const SafePointer & b) { return a.reference == b.reference; }
		bool friend operator != (const SafePointer & a, const SafePointer & b) { return a.reference != b.reference; }
		bool friend operator == (const SafePointer & a, O * b) { return a.reference == b; }
		bool friend operator != (const SafePointer & a, O * b) { return a.reference != b; }
		bool friend operator == (O * a, const SafePointer & b) { return a == b.reference; }
		bool friend operator != (O * a, const SafePointer & b) { return a != b.reference; }
	};
}