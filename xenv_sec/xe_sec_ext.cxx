#include "xe_sec_ext.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			class SecurityExtension : public ISecurityExtension
			{
				SafePointer<ITrustProvider> _trust;
				bool _needs_trusted_chain;
			public:
				SecurityExtension(ITrustProvider * trust, bool needs_trusted_chain) : _needs_trusted_chain(needs_trusted_chain) { _trust.SetRetain(trust); }
				virtual ~SecurityExtension(void) override {}
				virtual ModuleLoadError EvaluateTrust(const void * module_data, int module_length, Streaming::Stream * residual) noexcept override
				{
					SafePointer<IContainer> signature = LoadContainer(residual);
					if (!signature) {
						if (_needs_trusted_chain) return ModuleLoadError::ModuleTrustCompromised;
						else return ModuleLoadError::Success;
					}
					SafePointer<DataBlock> hash;
					try { hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, module_data, module_length); } catch (...) { return ModuleLoadError::AllocationFailure; }
					if (!hash) return ModuleLoadError::AllocationFailure;
					IntegrityValidationDesc desc;
					auto trust = EvaluateIntegrity(hash, signature, Time::GetCurrentTime(), _trust, &desc);
					if (trust == TrustStatus::Untrusted || (trust == TrustStatus::Undefined && _needs_trusted_chain)) {
						if (desc.object == IntegrityStatus::DataCorruption) return ModuleLoadError::ModuleConsistencyCompromised;
						else return ModuleLoadError::ModuleTrustCompromised;
					} else return ModuleLoadError::Success;
				}
			};
			ISecurityExtension * CreateStandardSecurityExtension(ITrustProvider * trust, bool needs_trusted_chain) noexcept { try { return new SecurityExtension(trust, needs_trusted_chain); } catch (...) { return 0; } }
		}
	}
}