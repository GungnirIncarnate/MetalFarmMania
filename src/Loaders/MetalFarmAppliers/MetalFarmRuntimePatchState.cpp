#include "Loader/MetalFarmAppliers/MetalFarmRuntimePatchState.h"

#include <unordered_set>

#include <Unreal/UObject.hpp>

namespace MFM::Loader
{
	namespace
	{
		std::unordered_set<const RC::Unreal::UObject*> s_patchedSeedClassInstances{};
		std::unordered_set<const RC::Unreal::UObject*> s_patchedMetalSeedDataMaps{};
	}

	auto MetalFarmRuntimePatchState::Reset() -> void
	{
		s_patchedSeedClassInstances.clear();
		s_patchedMetalSeedDataMaps.clear();
	}

	auto MetalFarmRuntimePatchState::HasSeedClassPatch(const RC::Unreal::UObject* metalFarmInstance) -> bool
	{
		return metalFarmInstance && s_patchedSeedClassInstances.find(metalFarmInstance) != s_patchedSeedClassInstances.end();
	}

	auto MetalFarmRuntimePatchState::HasMetalSeedPatch(const RC::Unreal::UObject* dataMap) -> bool
	{
		return dataMap && s_patchedMetalSeedDataMaps.find(dataMap) != s_patchedMetalSeedDataMaps.end();
	}

	auto MetalFarmRuntimePatchState::MarkSeedClassPatched(RC::Unreal::UObject* metalFarmInstance) -> void
	{
		if (!metalFarmInstance)
		{
			return;
		}

		s_patchedSeedClassInstances.insert(metalFarmInstance);
	}

	auto MetalFarmRuntimePatchState::MarkMetalSeedPatched(RC::Unreal::UObject* dataMap) -> void
	{
		if (!dataMap)
		{
			return;
		}

		s_patchedMetalSeedDataMaps.insert(dataMap);
	}
}