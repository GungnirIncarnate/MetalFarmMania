#pragma once

#include <vector>

#include <Unreal/Core/CoreFwd.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader::ResonableMaterials
{
	class ResonableMaterialAlterationApplier
	{
	public:
		static auto ApplyOutputs(
			RC::Unreal::UObject* resonatableData,
			const std::vector<Config::MetalFarmOutputEntry>& outputs) -> bool;
	};
}
