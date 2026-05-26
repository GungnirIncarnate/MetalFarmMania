#include "Hooks/ReceiveBeginPlayHook.h"

#include <UE4SSRuntime.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/Hooks/Hooks.hpp>

#include "Loader/MetalFarmTagApplicator.h"
#include "Loader/MetalFarmTableApplier.h"
#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	auto IsReceiveBeginPlayForMetalFarm(UObject* context, UFunction* function) -> bool
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

		const auto objectClass = context->GetClassPrivate();
		return objectClass && objectClass->GetName() == STR("BP_MetalFarm_C");
	}

	auto OnProcessEvent([[maybe_unused]] Hook::TCallbackIterationData<void>& callbackData, UObject* context, UFunction* function, void* params) -> void
	{
		if (!IsReceiveBeginPlayForMetalFarm(context, function))
		{
			return;
		}

		PCL_Log("MetalFarm trigger matched {} on {}.", function->GetName(), context->GetFullName());
		MFM::Loader::MetalFarmTagApplicator::TryApply(context);
		MFM::Loader::MetalFarmTableApplier::TryApply(context);
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