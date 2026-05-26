#pragma once

#include <vector>

#include <Unreal/NameTypes.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class FName;
}

namespace MFM::Loader
{
	class MetalFarmItemTagLookup
	{
	public:
		static auto CollectConfiguredItemTagNames(const std::vector<Config::MetalFarmAdditionEntry>& entries) -> std::vector<RC::Unreal::FName>;
	};
}
