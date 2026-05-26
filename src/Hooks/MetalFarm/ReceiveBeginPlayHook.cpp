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

	auto OnProcessEvent([[maybe_unused]] Hook::TCallbackIterationData<void>& callbackData, UObject* context, UFunction* function, void* params) -> void
	{
		if (!IsTrackedMetalFarmEvent(context, function))
		{
			return;
		}

		try
		{
			PCL_Log("MetalFarm trigger matched {} on {}.", function->GetName(), context->GetFullName());
			MFM::Loader::MetalFarmTagApplier::TryApply(context);
			MFM::Loader::MetalFarmTableApplier::TryApply(context);
			MFM::Loader::MetalFarmTagApplier::LogDiagnostics(context);
			MFM::Loader::MetalFarmTableApplier::LogDiagnostics(context);
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

		PCL_Log("ReceiveBeginPlay hook installed.");
	}
}