#pragma once

#include <vector>

#include <Unreal/Core/CoreFwd.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader
{
	class MetalFarmTablePatchWorker
	{
	public:
		static auto PatchItemTypeToSeedClass(
			RC::Unreal::UObject* metalFarmInstance,
			const std::vector<Config::MetalFarmAdditionEntry>& entries) -> bool;

		static auto PatchItemTypeToMetalSeed(
			RC::Unreal::UObject* dataMap,
			const std::vector<Config::MetalFarmAdditionEntry>& entries) -> bool;

		static auto InspectItemTypeToSeedClass(
			RC::Unreal::UObject* metalFarmInstance,
			RC::Unreal::UObject* currentItemType,
			int& outCount,
			bool& outHasCurrent) -> void;

		static auto InspectItemTypeToMetalSeed(
			RC::Unreal::UObject* dataMap,
			RC::Unreal::UObject* currentItemType,
			int& outCount,
			bool& outHasCurrent) -> void;
	};
}