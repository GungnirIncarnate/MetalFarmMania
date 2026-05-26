#pragma once

#include <vector>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader
{
	class MetalFarmTagApplicator
	{
	public:
		static auto Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void;
		static auto TryApply(RC::Unreal::UObject* metalFarmInstance) -> void;

	private:
		static auto PatchInventoryFilter(RC::Unreal::UObject* metalFarmActor) -> bool;

		static inline std::vector<Config::MetalFarmAdditionEntry> s_entries{};
	};
}
