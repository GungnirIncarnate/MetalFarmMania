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

		return seedClassPatched && metalSeedPatched;
	}

	auto MetalFarmTableApplier::TryApply(UObject* metalFarmInstance) -> bool
	{
		if (s_entries.empty())
		{
			return false;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return false;
		}

		return ApplyFromMetalFarm(metalFarmInstance);
	}
}