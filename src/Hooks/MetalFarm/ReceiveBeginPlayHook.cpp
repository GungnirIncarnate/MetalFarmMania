#include "Hooks/ReceiveBeginPlayHook.h"

#include <exception>
#include <string>

#include <UE4SSRuntime.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/Hooks/Hooks.hpp>

#include "Loader/MetalFarmAppliers/MetalFarmTagApplier.h"
#include "Loader/MetalFarmAppliers/MetalFarmTableApplier.h"
#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	auto ToWideString(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto IsTargetMetalFarm(const UObject* context) -> bool
	{
		if (!context || !context->GetClassPrivate())
		{
			return false;
		}

		return context->GetClassPrivate()->GetName() == STR("BP_MetalFarm_C");
	}

	auto IsTrackedMetalFarmEvent(UObject* context, UFunction* function) -> bool
	{
		if (!context || !function)
		{
			return false;
		}

		const auto functionName = function->GetName();
		if (functionName != STR("ReceiveBeginPlay"))
		{
			return false;
		}

		return IsTargetMetalFarm(context);
	}

	auto OnProcessEvent([[maybe_unused]] Hook::TCallbackIterationData<void>& callbackData, UObject* context, UFunction* function, [[maybe_unused]] void* params) -> void
	{
		if (!IsTrackedMetalFarmEvent(context, function))
		{
			return;
		}

		try
		{
			const bool tagPatched = MFM::Loader::MetalFarmTagApplier::TryApply(context);
			const bool tablePatched = MFM::Loader::MetalFarmTableApplier::TryApply(context);
			if (!(tagPatched && tablePatched))
			{
				PCL_WarnLog("MetalFarm setup failed on {} (tagsPatched={}, tablesPatched={}).", context->GetFullName(), tagPatched, tablePatched);
			}
		}
		catch (const std::exception& exception)
		{
			PCL_ErrorLog("ReceiveBeginPlay hook caught exception on {}: {}", context ? context->GetFullName() : STR("<null>"), ToWideString(exception.what()));
		}
		catch (...)
		{
			PCL_ErrorLog("ReceiveBeginPlay hook caught unknown exception on {}.", context ? context->GetFullName() : STR("<null>"));
		}
	}
}

namespace MFM::Hooks
{
	auto ReceiveBeginPlayHook::Initialize() -> void
	{
		if (!RC::UE4SSRuntime::IsProcessEventAvailable())
		{
			PCL_WarnLog("ReceiveBeginPlay hook not installed because ProcessEvent is unavailable yet.");
			return;
		}

		static const auto callbackOptions = Hook::FCallbackOptions{false, true, STR("MetalFarmMania"), STR("ReceiveBeginPlayHook")};
		static const auto hookId = Hook::RegisterProcessEventPreCallback(OnProcessEvent, callbackOptions);

		if (hookId == Hook::ERROR_ID)
		{
			PCL_WarnLog("ReceiveBeginPlay hook registration failed.");
			return;
		}
	}
}