#pragma once

#include <string>

#include <Unreal/Core/CoreFwd.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader::ResonableMaterials
{
	class ResonableMaterialAssetCloner
	{
	public:
		static auto Initialize() -> void;
		static auto CloneForEntry(const Config::MetalFarmAdditionEntry& entry) -> RC::Unreal::UObject*;
	};
}
