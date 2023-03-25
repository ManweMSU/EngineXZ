#include "xa_trans.h"

#include "xa_t_i386.h"
#include "xa_t_x64.h"

#ifdef ENGINE_WINDOWS
#include <Windows.h>

namespace Engine
{
	namespace XA
	{
		class SystemAllocator : public IMemoryAllocator
		{
			HANDLE _heap;
		public:
			SystemAllocator(void) { _heap = HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0); if (!_heap) throw OutOfMemoryException(); }
			virtual ~SystemAllocator(void) override { HeapDestroy(_heap); }
			virtual void * ExecutableAllocate(uintptr length) noexcept override { return HeapAlloc(_heap, 0, length); }
			virtual void ExecutableFree(void * block) noexcept override { if (!block) return; HeapFree(_heap, 0, block); }
			virtual void FlushExecutionCache(const void * block, uintptr length) noexcept override { FlushInstructionCache(GetCurrentProcess(), block, length); }
		};
	}
}
#endif
#ifdef ENGINE_MACOSX
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#include <pthread.h>

namespace Engine
{
	namespace XA
	{
		class SystemAllocator : public IMemoryAllocator
		{
			Volumes::Dictionary<void *, uintptr> _allocs;
		public:
			SystemAllocator(void) {}
			virtual ~SystemAllocator(void) override {}
			virtual void * ExecutableAllocate(uintptr length) noexcept override
			{
				auto reg = mmap(0, length, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT, -1, 0);
				if (reg) try { _allocs.Append(reg, length); } catch (...) { munmap(reg, length); return 0; }
				pthread_jit_write_protect_np(0);
				return reg;
			}
			virtual void ExecutableFree(void * block) noexcept override
			{
				auto rec = _allocs.FindElementEquivalent(block);
				if (rec) {
					munmap(rec->GetValue().key, rec->GetValue().value);
					_allocs.Remove(block);
				}
			}
			virtual void FlushExecutionCache(const void * block, uintptr length) noexcept override
			{
				pthread_jit_write_protect_np(1);
				sys_icache_invalidate(const_cast<void *>(block), length);
			}
		};
	}
}
#endif

namespace Engine
{
	namespace XA
	{
		class Executable : public IExecutable
		{
			friend class ExecutableLinker;
		private:
			SafePointer<IMemoryAllocator> _alloc;
			void * _data;
			void * _code;
			Volumes::Dictionary<string, void *> _func_table;
		public:
			Executable(void) : _data(0), _code(0) {}
			virtual ~Executable(void) override { if (_data) free(_data); if (_code && _alloc) _alloc->ExecutableFree(_code); }
			virtual void * GetEntryPoint(const string & name) noexcept override { auto prop = _func_table[name]; if (prop) return *prop; return 0; }
			virtual const Volumes::Dictionary<string, void *> & GetFunctionTable(void) noexcept override { return _func_table; }
		};
		class ExecutableLinker : public IExecutableLinker
		{
			SafePointer<IMemoryAllocator> _alloc;
		public:
			ExecutableLinker(void) { _alloc = new SystemAllocator; }
			virtual ~ExecutableLinker(void) override {}
			virtual IExecutable * LinkFunctions(const Volumes::Dictionary<string, TranslatedFunction *> & functions, IReferenceResolver * resolver) noexcept override
			{
				try {
					SafePointer<Executable> exec = new Executable;
					exec->_alloc = _alloc;
					uintptr common_code = 0;
					uintptr common_data = 0, data_base = 0;
					for (auto & f : functions) if (f.value) {
						exec->_func_table.Append(f.key, reinterpret_cast<void *>(common_code));
						common_code += f.value->code.Length();
						common_data += f.value->data.Length();
					}
					exec->_data = malloc(common_data);
					exec->_code = _alloc->ExecutableAllocate(common_code);
					if ((!exec->_data && common_data) || (!exec->_code && common_code)) return 0;
					for (auto & fe : exec->_func_table) {
						auto ptr = reinterpret_cast<uintptr>(fe.value) + reinterpret_cast<uintptr>(exec->_code);
						fe.value = reinterpret_cast<void *>(ptr);
					}
					for (auto & fe : exec->_func_table) {
						auto fsp = functions[fe.key];
						if (!fsp) return 0;
						auto fs = *fsp;
						if (!fs) return 0;
						auto data_base_ptr = reinterpret_cast<uint8 *>(exec->_data) + data_base;
						auto code_base_ptr = reinterpret_cast<uint8 *>(fe.value);
						MemoryCopy(code_base_ptr, fs->code.GetBuffer(), fs->code.Length());
						MemoryCopy(data_base_ptr, fs->data.GetBuffer(), fs->data.Length());
						data_base += fs->data.Length();
						for (auto & cr : fs->code_reloc) {
							*reinterpret_cast<uintptr *>(code_base_ptr + cr) += reinterpret_cast<uintptr>(code_base_ptr);
						}
						for (auto & dr : fs->data_reloc) {
							*reinterpret_cast<uintptr *>(code_base_ptr + dr) += reinterpret_cast<uintptr>(data_base_ptr);
						}
						for (auto & ref : fs->extrefs) {
							uintptr ref_val = 0;
							auto func = exec->_func_table[ref.key];
							if (func) ref_val = reinterpret_cast<uintptr>(*func);
							else if (resolver) ref_val = resolver->ResolveReference(ref.key);
							if (!ref_val) return 0;
							for (auto & rr : ref.value) {
								*reinterpret_cast<uintptr *>(code_base_ptr + rr) = ref_val;
							}
						}
					}
					_alloc->FlushExecutionCache(exec->_code, common_code);
					exec->Retain();
					return exec;
				} catch (...) { return 0; }
			}
		};

		CallingConvention GetApplicationCallingConvention(void)
		{
			#ifdef ENGINE_WINDOWS
				return CallingConvention::Windows;
			#endif
			#ifdef ENGINE_UNIX
				return CallingConvention::Unix;
			#endif
			return CallingConvention::Unknown;
		}
		IAssemblyTranslator * CreatePlatformTranslator(void) { return CreatePlatformTranslator(ApplicationPlatform, GetApplicationCallingConvention()); }
		IAssemblyTranslator * CreatePlatformTranslator(Platform platform, CallingConvention conv)
		{
			if (platform == Platform::X86) return CreateTranslatorX86i386(conv);
			else if (platform == Platform::X64) return CreateTranslatorX64(conv);
			else if (platform == Platform::ARM) return 0; // TODO: IMPLEMENT
			else if (platform == Platform::ARM64) return 0; // TODO: IMPLEMENT
			else return 0;
		}
		IExecutableLinker * CreateLinker(void) { return new ExecutableLinker; }
	}
}