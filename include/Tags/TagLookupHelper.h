#pragma once

#include <string>
#include <vector>

#include <Unreal/NameTypes.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class FName;
	class UObject;
}

namespace MFM::Tags
{
	class TagLookupHelper
	{
	public:
		// Resolve item type UObject by object path (supports configured path formats).
		static auto ResolveItemTypeFromPath(const std::string& itemTypePath) -> RC::Unreal::UObject*;

		// Preferred generic API: pass any resolved UUWEItemType UObject.
		static auto CollectItemTypeTagNames(RC::Unreal::UObject* itemType) -> std::vector<RC::Unreal::FName>;

		// Convenience API: resolve item type by object path then collect tags.
		static auto CollectItemTypeTagNamesFromPath(const std::string& itemTypePath) -> std::vector<RC::Unreal::FName>;

		// Config helper API: aggregate discovered tags across configured entries.
		static auto CollectConfiguredItemTypeTagNames(const std::vector<MFM::Loader::Config::MetalFarmAdditionEntry>& entries) -> std::vector<RC::Unreal::FName>;
	};
}
