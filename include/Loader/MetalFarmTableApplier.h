#pragma once

#include <vector>

#include <Unreal/Core/CoreFwd.hpp>
#include <Unreal/NameTypes.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader
{
	class MetalFarmTableApplier
	{
	public:
		static auto Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void;
		static auto TryApply(RC::Unreal::UObject* metalFarmInstance) -> void;

	private:
		static auto ApplyFromMetalFarm(RC::Unreal::UObject* metalFarmInstance) -> bool;
		static auto PatchSeedMap(RC::Unreal::UObject* container, const wchar_t* mapPropertyName) -> bool;
		static auto PatchInventoryFilter(RC::Unreal::UObject* metalFarmCDO) -> bool;
		static auto ResolveObject(const std::string& objectPath) -> RC::Unreal::UObject*;

		struct MetalFarmSeedData
		{
			RC::Unreal::UObject* ResonatableData{};
			RC::Unreal::UObject* SeedMaterial{};
			RC::Unreal::FName MetalTier{};
		};

		static inline std::vector<Config::MetalFarmAdditionEntry> s_entries{};
		static inline bool s_hasApplied{false};
		static inline bool s_hasPatchedSeedClassMap{false};
		static inline bool s_hasPatchedMetalSeedMap{false};
		static inline bool s_hasPatchedInventoryFilter{false};
	};
}