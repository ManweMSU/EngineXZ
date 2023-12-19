#include "xe_powerapi.h"

namespace Engine
{
	namespace XE
	{
		class PowerControlExtension : public IAPIExtension
		{
			struct _power_info
			{
				double power_value;
				uint power_mode;
			};
			static bool _shutdown_machine(uint mode, bool forced) noexcept
			{
				if (mode == 0) return Power::ExitSystem(Power::Exit::Shutdown, forced);
				else if (mode == 1) return Power::ExitSystem(Power::Exit::Reboot, forced);
				else if (mode == 2) return Power::ExitSystem(Power::Exit::Logout, forced);
				else return false;
			}
			static bool _suspend_machine(uint mode, bool allow_make_up) noexcept
			{
				if (mode == 0) return Power::SuspendSystem(false, allow_make_up);
				else if (mode == 1) return Power::SuspendSystem(true, allow_make_up);
				else return false;
			}
			static void _prevent_suspention(bool machine, bool screen) noexcept
			{
				if (screen) Power::PreventIdleSleep(Power::Prevent::IdleDisplaySleep);
				else if (machine) Power::PreventIdleSleep(Power::Prevent::IdleSystemSleep);
				else Power::PreventIdleSleep(Power::Prevent::None);
			}
			static void _read_power_info(_power_info & info) noexcept
			{
				auto type = Power::GetBatteryStatus();
				info.power_value = Power::GetBatteryChargeLevel();
				if (type == Power::BatteryStatus::Charging) info.power_mode = 2;
				else if (type == Power::BatteryStatus::InUse) info.power_mode = 3;
				else if (type == Power::BatteryStatus::NoBattery) info.power_mode = 1;
				else info.power_mode = 0;
			}
		public:
			PowerControlExtension(void) {}
			virtual ~PowerControlExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"pt_spnde") < 0) {
					if (string::Compare(routine_name, L"pt_siste") < 0) {
						if (string::Compare(routine_name, L"pt_lstpt") == 0) return reinterpret_cast<const void *>(_read_power_info);
					} else {
						if (string::Compare(routine_name, L"pt_siste") == 0) return reinterpret_cast<const void *>(_shutdown_machine);
					}
				} else {
					if (string::Compare(routine_name, L"pt_vtspn") < 0) {
						if (string::Compare(routine_name, L"pt_spnde") == 0) return reinterpret_cast<const void *>(_suspend_machine);
					} else {
						if (string::Compare(routine_name, L"pt_vtspn") == 0) return reinterpret_cast<const void *>(_prevent_suspention);
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		void ActivatePowerControl(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new PowerControlExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
	}
}