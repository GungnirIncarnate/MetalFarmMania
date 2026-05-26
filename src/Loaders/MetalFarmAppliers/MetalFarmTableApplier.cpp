#include "Loader/MetalFarmAppliers/MetalFarmTableApplier.h"

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UObject.hpp>

#include "Loader/MetalFarmAppliers/MetalFarmRuntimePatchState.h"
#include "Loader/MetalFarmAppliers/MetalFarmTablePatchWorker.h"
#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	auto IsTargetMetalFarm(const UObject* metalFarmInstance) -> bool
	{
		if (!metalFarmInstance || !metalFarmInstance->GetClassPrivate())
		{
			return false;
		}

		return metalFarmInstance->GetClassPrivate()->GetName() == STR("BP_MetalFarm_C");
	}

}

namespace MFM::Loader
{
	auto MetalFarmTableApplier::Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void
	{
		s_entries = std::move(entries);
		MetalFarmRuntimePatchState::Reset();
	}

	auto MetalFarmTableApplier::ApplyFromMetalFarm(UObject* metalFarmInstance) -> bool
	{
		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return false;
		}

		auto* dataMapProperty = CastField<FObjectProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("Data Map")));
		if (!dataMapProperty)
		{
			PCL_WarnLog("MetalFarm table patch deferred because the BP_MetalFarm instance 'Data Map' property is not available yet.");
			return false;
		}

		const auto dataMap = *dataMapProperty->ContainerPtrToValuePtr<UObject*>(metalFarmInstance);
		if (!dataMap)
		{
			PCL_WarnLog("MetalFarm table patch deferred because the BP_MetalFarm instance 'Data Map' value is still null.");
			return false;
		}

		const bool hasSeedClassPatch = MetalFarmRuntimePatchState::HasSeedClassPatch(metalFarmInstance);
		const bool hasMetalSeedPatch = MetalFarmRuntimePatchState::HasMetalSeedPatch(dataMap);

		bool patchedSeedClassNow = false;
		if (!hasSeedClassPatch)
		{
			patchedSeedClassNow = MetalFarmTablePatchWorker::PatchItemTypeToSeedClass(metalFarmInstance, s_entries);
			if (patchedSeedClassNow)
			{
				MetalFarmRuntimePatchState::MarkSeedClassPatched(metalFarmInstance);
			}
		}

		bool patchedMetalSeedNow = false;
		if (!hasMetalSeedPatch)
		{
			patchedMetalSeedNow = MetalFarmTablePatchWorker::PatchItemTypeToMetalSeed(dataMap, s_entries);
			if (patchedMetalSeedNow)
			{
				MetalFarmRuntimePatchState::MarkMetalSeedPatched(dataMap);
			}
		}

		const bool seedClassPatched = hasSeedClassPatch || patchedSeedClassNow;
		const bool metalSeedPatched = hasMetalSeedPatch || patchedMetalSeedNow;

		if (seedClassPatched && metalSeedPatched)
		{
			if (patchedSeedClassNow || patchedMetalSeedNow)
			{
				PCL_Log("MetalFarm tables patched for actor '{}' using {} configured entries.", metalFarmInstance->GetFullName(), s_entries.size());
			}
			return true;
		}

		if (seedClassPatched || metalSeedPatched)
		{
			PCL_Log("MetalFarm partially patched for actor '{}' and is still waiting on remaining maps after a trigger.", metalFarmInstance->GetFullName());
		}
		else
		{
			PCL_WarnLog("MetalFarm patch is still waiting for table map properties to be available after a trigger on actor '{}'.", metalFarmInstance->GetFullName());
		}

		return false;
	}

	auto MetalFarmTableApplier::TryApply(UObject* metalFarmInstance) -> void
	{
		if (s_entries.empty())
		{
			return;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return;
		}

		(void)ApplyFromMetalFarm(metalFarmInstance);
	}

	auto MetalFarmTableApplier::LogDiagnostics(UObject* metalFarmInstance) -> void
	{
		if (!metalFarmInstance || !IsTargetMetalFarm(metalFarmInstance))
		{
			return;
		}

		auto* currentItemTypeProperty = CastField<FObjectProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("CurrentItemType")));
		const auto currentItemType = currentItemTypeProperty ? *currentItemTypeProperty->ContainerPtrToValuePtr<UObject*>(metalFarmInstance) : nullptr;

		int32 seedGrowerCount = -1;
		if (auto* seedGrowerComponentsProperty = CastField<FArrayProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("SeedGrowerComponents"))))
		{
			if (auto* seedGrowers = seedGrowerComponentsProperty->ContainerPtrToValuePtr<TArray<UObject*>>(metalFarmInstance))
			{
				seedGrowerCount = seedGrowers->Num();
			}
		}

		int seedClassMapCount = -1;
		bool seedClassHasCurrent = false;
		MetalFarmTablePatchWorker::InspectItemTypeToSeedClass(metalFarmInstance, currentItemType, seedClassMapCount, seedClassHasCurrent);

		int metalSeedMapCount = -1;
		bool metalSeedHasCurrent = false;
		auto dataMap = static_cast<UObject*>(nullptr);
		if (auto* dataMapProperty = CastField<FObjectProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("Data Map"))))
		{
			dataMap = *dataMapProperty->ContainerPtrToValuePtr<UObject*>(metalFarmInstance);
		}

		if (dataMap)
		{
			MetalFarmTablePatchWorker::InspectItemTypeToMetalSeed(dataMap, currentItemType, metalSeedMapCount, metalSeedHasCurrent);
		}

		const bool seedClassPatched = MetalFarmRuntimePatchState::HasSeedClassPatch(metalFarmInstance);
		const bool metalSeedPatched = MetalFarmRuntimePatchState::HasMetalSeedPatch(dataMap);

		PCL_Log(
			"MetalFarm diagnostics: actor='{}' currentItemType='{}' seedGrowers={} ItemTypeToSeedClass(count={}, hasCurrent={}) ItemTypeToMetalSeed(count={}, hasCurrent={}) entries={} seedClassPatched={} metalSeedPatched={}.",
			metalFarmInstance->GetFullName(),
			currentItemType ? currentItemType->GetFullName() : STR("<null>"),
			seedGrowerCount,
			seedClassMapCount,
			seedClassHasCurrent,
			metalSeedMapCount,
			metalSeedHasCurrent,
			s_entries.size(),
			seedClassPatched,
			metalSeedPatched);
	}
}