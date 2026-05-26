#include "Loader/MetalFarmAppliers/MetalFarmTablePatchWorker.h"

#include <string>
#include <vector>

#include <Unreal/Core/Containers/Map.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Loader/ResonableMaterials/ResonableMaterialAlterationApplier.h"
#include "Loader/ResonableMaterials/ResonableMaterialAssetCloner.h"
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

	auto ResolveObjectCandidates(const std::string& objectPath) -> std::vector<UObject*>
	{
		std::vector<UObject*> resolvedCandidates{};

		for (const auto& candidatePath : BuildObjectPathCandidates(objectPath))
		{
			const auto widePath = ToWideString(candidatePath);
			auto* resolved = UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str());
			if (!resolved)
			{
				continue;
			}

			bool alreadyPresent = false;
			for (const auto* existing : resolvedCandidates)
			{
				if (existing == resolved)
				{
					alreadyPresent = true;
					break;
				}
			}

			if (!alreadyPresent)
			{
				resolvedCandidates.emplace_back(resolved);
			}
		}

		return resolvedCandidates;
	}

	auto AddUniqueObject(std::vector<UObject*>& objects, UObject* candidate) -> void
	{
		if (!candidate)
		{
			return;
		}

		for (const auto* existing : objects)
		{
			if (existing == candidate)
			{
				return;
			}
		}

		objects.emplace_back(candidate);
	}

	auto ExpandItemTypeKeyCandidates(const std::vector<UObject*>& baseCandidates) -> std::vector<UObject*>
	{
		std::vector<UObject*> expanded{};

		for (auto* itemTypeObject : baseCandidates)
		{
			AddUniqueObject(expanded, itemTypeObject);

			if (itemTypeObject && itemTypeObject->GetClassPrivate())
			{
				auto* itemTypeClass = static_cast<UObject*>(itemTypeObject->GetClassPrivate());
				AddUniqueObject(expanded, itemTypeClass);

				if (itemTypeObject->GetClassPrivate()->GetClassDefaultObject())
				{
					AddUniqueObject(expanded, itemTypeObject->GetClassPrivate()->GetClassDefaultObject());
				}
			}
		}

		return expanded;
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
		bool failedAnyEntry = false;

		for (const auto& entry : entries)
		{
			const auto itemTypeCandidates = ExpandItemTypeKeyCandidates(ResolveObjectCandidates(entry.GetInputItemPath()));
			auto* resonatableData = static_cast<UObject*>(nullptr);
			if (!entry.outputs.empty())
			{
				resonatableData = MFM::Loader::ResonableMaterials::ResonableMaterialAssetCloner::CloneForEntry(entry);
				if (resonatableData)
				{
					const bool appliedOutputs = MFM::Loader::ResonableMaterials::ResonableMaterialAlterationApplier::ApplyOutputs(resonatableData, entry.outputs);
					if (!appliedOutputs)
					{
						PCL_WarnLog("MetalFarm entry '{}' could not apply outputs to cloned resonatable data yet; using cloned template object unchanged.", ToWideString(entry.id));
					}
				}
			}

			if (!resonatableData)
			{
				const auto resonatableCandidates = ResolveObjectCandidates(entry.resonatableDataPath);
				if (!resonatableCandidates.empty())
				{
					resonatableData = resonatableCandidates.front();
				}
			}

			UObject* seedMaterial = nullptr;
			const auto seedMaterialCandidates = ResolveObjectCandidates(entry.seedMaterialPath);
			if (!seedMaterialCandidates.empty())
			{
				seedMaterial = seedMaterialCandidates.front();
			}
			const auto metalTierTagName = FName(ToWideString(entry.metalTierTag).c_str(), FNAME_Add);

			if (itemTypeCandidates.empty() || !resonatableData || !seedMaterial)
			{
				failedAnyEntry = true;
				if (itemTypeCandidates.empty())
				{
					PCL_WarnLog("MetalFarm entry '{}' unresolved inputItemPath '{}'", ToWideString(entry.id), ToWideString(entry.GetInputItemPath()));
				}
				if (!resonatableData)
				{
					PCL_WarnLog("MetalFarm entry '{}' unresolved resonatable data (clone+outputs or fallback path '{}').", ToWideString(entry.id), ToWideString(entry.resonatableDataPath));
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
				failedAnyEntry = true;
				PCL_WarnLog("MetalFarm entry '{}' unresolved metalTierTag '{}'; skipping entry.", ToWideString(entry.id), ToWideString(entry.metalTierTag));
				continue;
			}

			// Root all three objects so UE's garbage collector doesn't free them.
			// MetalFarmSeedData is not a UPROPERTY struct so the GC cannot trace
			// the raw UObject* fields inside the map value; without rooting them
			// the GC will dangling-pointer crash ~60 seconds into gameplay.
			resonatableData->SetRootSet();
			seedMaterial->SetRootSet();

			for (auto* itemType : itemTypeCandidates)
			{
				if (!itemType)
				{
					continue;
				}

				itemType->SetRootSet();

				auto& seedData = seedMap->FindOrAdd(itemType);
				seedData.ResonatableData = resonatableData;
				seedData.SeedMaterial = seedMaterial;
				seedData.MetalTier = metalTierTagName;
			}
			patchedAnyEntry = true;
		}

		if (!patchedAnyEntry)
		{
			return false;
		}

		return !failedAnyEntry;
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
}