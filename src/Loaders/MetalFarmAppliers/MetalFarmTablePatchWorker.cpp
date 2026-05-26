#include "Loader/MetalFarmAppliers/MetalFarmTablePatchWorker.h"

#include <string>
#include <vector>

#include <Unreal/Core/Containers/Map.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	struct MetalFarmSeedData
	{
		UObject* ResonatableData{};
		UObject* SeedMaterial{};
		FName MetalTier{};
	};

	auto ToWideString(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto NormalizeObjectPath(const std::string& objectPath) -> std::string
	{
		if (objectPath.empty())
		{
			return objectPath;
		}

		auto normalized = objectPath;
		const auto slashIndex = normalized.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = normalized.find('.', nameStart);

		if (dotIndex == std::string::npos)
		{
			const auto assetName = normalized.substr(nameStart);
			if (!assetName.empty())
			{
				normalized += "." + assetName;
			}
			return normalized;
		}

		return normalized;
	}

	auto BuildObjectPathCandidates(const std::string& objectPath) -> std::vector<std::string>
	{
		std::vector<std::string> candidates{};
		if (objectPath.empty())
		{
			return candidates;
		}

		auto addCandidate = [&candidates](const std::string& candidate) -> void
		{
			if (candidate.empty())
			{
				return;
			}

			for (const auto& existing : candidates)
			{
				if (existing == candidate)
				{
					return;
				}
			}

			candidates.emplace_back(candidate);
		};

		addCandidate(objectPath);

		const auto normalized = NormalizeObjectPath(objectPath);
		addCandidate(normalized);

		const auto slashIndex = normalized.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = normalized.find('.', nameStart);
		if (dotIndex == std::string::npos)
		{
			return candidates;
		}

		const auto assetName = normalized.substr(nameStart, dotIndex - nameStart);
		const auto objectName = normalized.substr(dotIndex + 1);
		if (assetName.empty() || objectName.empty())
		{
			return candidates;
		}

		if (objectName.size() > 2 && objectName.compare(objectName.size() - 2, 2, "_C") == 0)
		{
			addCandidate(normalized.substr(0, dotIndex + 1) + objectName.substr(0, objectName.size() - 2));
		}

		if (objectName == assetName + "_C")
		{
			addCandidate(normalized.substr(0, dotIndex + 1) + assetName);
		}

		return candidates;
	}

	auto ResolveObject(const std::string& objectPath) -> UObject*
	{
		for (const auto& candidatePath : BuildObjectPathCandidates(objectPath))
		{
			const auto widePath = ToWideString(candidatePath);
			if (auto* resolved = UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str()))
			{
				return resolved;
			}
		}

		return nullptr;
	}

	auto PatchSeedMap(
		UObject* container,
		const wchar_t* mapPropertyName,
		const std::vector<MFM::Loader::Config::MetalFarmAdditionEntry>& entries) -> bool
	{
		if (!container)
		{
			return false;
		}

		auto* mapProperty = CastField<FMapProperty>(container->GetPropertyByNameInChain(mapPropertyName));
		if (!mapProperty)
		{
			PCL_WarnLog("MetalFarm table patch skipped because '{}' was not found on {}", mapPropertyName, container->GetFullName());
			return false;
		}

		auto* mapValue = mapProperty->ContainerPtrToValuePtr<void>(container);
		if (!mapValue)
		{
			PCL_WarnLog("MetalFarm table patch skipped because '{}' has no value pointer on {}", mapPropertyName, container->GetFullName());
			return false;
		}

		auto* seedMap = reinterpret_cast<TMap<UObject*, MetalFarmSeedData>*>(mapValue);
		bool patchedAnyEntry = false;

		for (const auto& entry : entries)
		{
			const auto itemType = ResolveObject(entry.itemTypePath);
			const auto resonatableData = ResolveObject(entry.resonatableDataPath);
			const auto seedMaterial = ResolveObject(entry.seedMaterialPath);
			const auto metalTierTagName = FName(ToWideString(entry.metalTierTag).c_str(), FNAME_Find);

			if (!itemType || !resonatableData || !seedMaterial)
			{
				if (!itemType)
				{
					PCL_WarnLog("MetalFarm entry '{}' unresolved itemTypePath '{}'", ToWideString(entry.id), ToWideString(entry.itemTypePath));
				}
				if (!resonatableData)
				{
					PCL_WarnLog("MetalFarm entry '{}' unresolved resonatableDataPath '{}'", ToWideString(entry.id), ToWideString(entry.resonatableDataPath));
				}
				if (!seedMaterial)
				{
					PCL_WarnLog("MetalFarm entry '{}' unresolved seedMaterialPath '{}'", ToWideString(entry.id), ToWideString(entry.seedMaterialPath));
				}
				PCL_WarnLog("Skipping MetalFarm entry '{}' because one or more referenced objects could not be resolved.", ToWideString(entry.id));
				continue;
			}

			if (metalTierTagName == FName())
			{
				PCL_WarnLog("MetalFarm entry '{}' unresolved metalTierTag '{}'; skipping entry.", ToWideString(entry.id), ToWideString(entry.metalTierTag));
				continue;
			}

			// Root all three objects so UE's garbage collector doesn't free them.
			// MetalFarmSeedData is not a UPROPERTY struct so the GC cannot trace
			// the raw UObject* fields inside the map value; without rooting them
			// the GC will dangling-pointer crash ~60 seconds into gameplay.
			itemType->SetRootSet();
			resonatableData->SetRootSet();
			seedMaterial->SetRootSet();

			auto& seedData = seedMap->FindOrAdd(itemType);
			seedData.ResonatableData = resonatableData;
			seedData.SeedMaterial = seedMaterial;
			seedData.MetalTier = metalTierTagName;
			PCL_VerboseLog(
				"MetalFarm '{}' patched {} key='{}' resonatable='{}' seedMaterial='{}' metalTier='{}'.",
				ToWideString(entry.id),
				mapPropertyName,
				itemType->GetFullName(),
				resonatableData->GetFullName(),
				seedMaterial->GetFullName(),
				ToWideString(entry.metalTierTag));
			patchedAnyEntry = true;
		}

		return patchedAnyEntry;
	}

	auto InspectSeedMap(
		UObject* container,
		const wchar_t* mapPropertyName,
		UObject* currentItemType,
		int& outCount,
		bool& outHasCurrent) -> void
	{
		outCount = -1;
		outHasCurrent = false;

		if (!container)
		{
			return;
		}

		if (auto* mapProperty = CastField<FMapProperty>(container->GetPropertyByNameInChain(mapPropertyName)))
		{
			if (auto* mapValue = mapProperty->ContainerPtrToValuePtr<void>(container))
			{
				auto* seedMap = reinterpret_cast<TMap<UObject*, MetalFarmSeedData>*>(mapValue);
				outCount = seedMap->Num();
				outHasCurrent = currentItemType && seedMap->Find(currentItemType) != nullptr;
			}
		}
	}
}

namespace MFM::Loader
{
	auto MetalFarmTablePatchWorker::PatchItemTypeToSeedClass(
		RC::Unreal::UObject* metalFarmInstance,
		const std::vector<Config::MetalFarmAdditionEntry>& entries) -> bool
	{
		return PatchSeedMap(metalFarmInstance, STR("ItemTypeToSeedClass"), entries);
	}

	auto MetalFarmTablePatchWorker::PatchItemTypeToMetalSeed(
		RC::Unreal::UObject* dataMap,
		const std::vector<Config::MetalFarmAdditionEntry>& entries) -> bool
	{
		return PatchSeedMap(dataMap, STR("ItemTypeToMetalSeed"), entries);
	}

	auto MetalFarmTablePatchWorker::InspectItemTypeToSeedClass(
		RC::Unreal::UObject* metalFarmInstance,
		RC::Unreal::UObject* currentItemType,
		int& outCount,
		bool& outHasCurrent) -> void
	{
		InspectSeedMap(metalFarmInstance, STR("ItemTypeToSeedClass"), currentItemType, outCount, outHasCurrent);
	}

	auto MetalFarmTablePatchWorker::InspectItemTypeToMetalSeed(
		RC::Unreal::UObject* dataMap,
		RC::Unreal::UObject* currentItemType,
		int& outCount,
		bool& outHasCurrent) -> void
	{
		InspectSeedMap(dataMap, STR("ItemTypeToMetalSeed"), currentItemType, outCount, outHasCurrent);
	}
}