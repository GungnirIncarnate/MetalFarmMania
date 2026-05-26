#include "Loader/MetalFarmTableApplier.h"

#include <string>
#include <initializer_list>

#include <Unreal/Core/Containers/Map.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	struct FGameplayTagLite
	{
		FName TagName{};
	};

	struct FGameplayTagContainerLite
	{
		TArray<FGameplayTagLite> GameplayTags{};
		TArray<FGameplayTagLite> ParentTags{};
	};

	auto ToWideString(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto IsTargetMetalFarm(const UObject* metalFarmInstance) -> bool
	{
		if (!metalFarmInstance || !metalFarmInstance->GetClassPrivate())
		{
			return false;
		}

		return metalFarmInstance->GetClassPrivate()->GetName() == STR("BP_MetalFarm_C");
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

		if (normalized.compare(dotIndex, 2, ".0") == 0)
		{
			const auto assetName = normalized.substr(nameStart, dotIndex - nameStart);
			if (!assetName.empty())
			{
				normalized = normalized.substr(0, dotIndex + 1) + assetName;
			}
		}

		return normalized;
	}

	auto IsGameplayTagContainerProperty(FStructProperty* property) -> bool
	{
		if (!property)
		{
			return false;
		}

		const auto structType = property->GetStruct();
		return structType && structType->GetName() == STR("GameplayTagContainer");
	}

	auto IsGameplayTagProperty(FStructProperty* property) -> bool
	{
		if (!property)
		{
			return false;
		}

		const auto structType = property->GetStruct();
		return structType && structType->GetName() == STR("GameplayTag");
	}

	auto FindGameplayTagContainerValue(UObject* object, std::initializer_list<const wchar_t*> candidatePropertyNames) -> FGameplayTagContainerLite*
	{
		if (!object)
		{
			return nullptr;
		}

		for (const auto* candidateName : candidatePropertyNames)
		{
			auto* structProperty = CastField<FStructProperty>(object->GetPropertyByNameInChain(candidateName));
			if (!IsGameplayTagContainerProperty(structProperty))
			{
				continue;
			}

			return structProperty->ContainerPtrToValuePtr<FGameplayTagContainerLite>(object);
		}

		return nullptr;
	}

	auto HasTag(const FGameplayTagContainerLite& container, const FGameplayTagLite& tag) -> bool
	{
		for (const auto& existingTag : container.GameplayTags)
		{
			if (existingTag.TagName == tag.TagName)
			{
				return true;
			}
		}

		return false;
	}

	auto AddTagIfMissing(FGameplayTagContainerLite& destination, const FGameplayTagLite& tag) -> bool
	{
		if (HasTag(destination, tag))
		{
			return false;
		}

		destination.GameplayTags.Add(tag);
		return true;
	}

	auto MergeTagContainers(FGameplayTagContainerLite& destination, const FGameplayTagContainerLite& source) -> int32
	{
		int32 addedCount = 0;
		for (const auto& tag : source.GameplayTags)
		{
			if (HasTag(destination, tag))
			{
				continue;
			}

			destination.GameplayTags.Add(tag);
			++addedCount;
		}

		return addedCount;
	}
}

namespace MFM::Loader
{
	auto MetalFarmTableApplier::Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void
	{
		s_entries = std::move(entries);
		s_hasApplied = false;
		s_hasPatchedSeedClassMap = false;
		s_hasPatchedMetalSeedMap = false;
		s_hasPatchedInventoryFilter = false;
	}

	auto MetalFarmTableApplier::ResolveObject(const std::string& objectPath) -> UObject*
	{
		const auto normalizedPath = NormalizeObjectPath(objectPath);
		const auto widePath = ToWideString(normalizedPath);
		return UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str());
	}

	auto MetalFarmTableApplier::PatchSeedMap(UObject* container, const wchar_t* mapPropertyName) -> bool
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

		for (const auto& entry : s_entries)
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
			patchedAnyEntry = true;
		}

		return patchedAnyEntry;
	}

	auto MetalFarmTableApplier::PatchInventoryFilter(UObject* metalFarmActor) -> bool
	{
		if (!metalFarmActor)
		{
			return false;
		}

		auto* inventoryComponentProperty = CastField<FObjectProperty>(metalFarmActor->GetPropertyByNameInChain(STR("InventoryComponent")));
		if (!inventoryComponentProperty)
		{
			PCL_WarnLog("MetalFarm inventory filter patch deferred because 'InventoryComponent' was not found on {}", metalFarmActor->GetFullName());
			return false;
		}

		auto* inventoryComponent = *inventoryComponentProperty->ContainerPtrToValuePtr<UObject*>(metalFarmActor);
		if (!inventoryComponent)
		{
			PCL_WarnLog("MetalFarm inventory filter patch deferred because BP_MetalFarm InventoryComponent is null.");
			return false;
		}

		auto* allowAnyItemsProperty = CastField<FBoolProperty>(inventoryComponent->GetPropertyByNameInChain(STR("AllowAddingAnyItems")));
		if (!allowAnyItemsProperty)
		{
			PCL_WarnLog("MetalFarm inventory filter patch skipped because 'AllowAddingAnyItems' was not found on {}", inventoryComponent->GetFullName());
			return false;
		}

		if (!allowAnyItemsProperty->GetPropertyValueInContainer(inventoryComponent))
		{
			allowAnyItemsProperty->SetPropertyValueInContainer(inventoryComponent, true);
			PCL_Log("MetalFarm inventory filter bypass enabled (AllowAddingAnyItems=true) on {}.", inventoryComponent->GetFullName());
		}

		const std::initializer_list<const wchar_t*> tagPropertyCandidates{
			STR("Tags"),
			STR("ItemTags"),
			STR("GameplayTags"),
			STR("AllowedTags"),
			STR("RequiredTags")};

		FGameplayTagContainerLite configuredItemTags{};
		bool hasConfiguredItemTags = false;

		for (const auto& entry : s_entries)
		{
			auto* itemType = ResolveObject(entry.itemTypePath);
			if (!itemType)
			{
				continue;
			}

			if (auto* itemTags = FindGameplayTagContainerValue(itemType, tagPropertyCandidates))
			{
				hasConfiguredItemTags = true;
				(void)MergeTagContainers(configuredItemTags, *itemTags);
			}

			auto* typeTagProperty = CastField<FStructProperty>(itemType->GetPropertyByNameInChain(STR("TypeTag")));
			if (IsGameplayTagProperty(typeTagProperty))
			{
				auto* typeTag = typeTagProperty->ContainerPtrToValuePtr<FGameplayTagLite>(itemType);
				if (typeTag && typeTag->TagName != FName())
				{
					hasConfiguredItemTags = true;
					(void)AddTagIfMissing(configuredItemTags, *typeTag);
				}
			}

			auto* tunableDataProperty = CastField<FObjectProperty>(itemType->GetPropertyByNameInChain(STR("TunableData")));
			if (!tunableDataProperty)
			{
				continue;
			}

			auto* tunableData = *tunableDataProperty->ContainerPtrToValuePtr<UObject*>(itemType);
			if (!tunableData)
			{
				continue;
			}

			if (auto* tunableDataTags = FindGameplayTagContainerValue(tunableData, tagPropertyCandidates))
			{
				hasConfiguredItemTags = true;
				(void)MergeTagContainers(configuredItemTags, *tunableDataTags);
			}
		}

		if (!hasConfiguredItemTags)
		{
			PCL_WarnLog("MetalFarm tag patch could not locate gameplay tags on configured item types; inventory may still reject non-mineral items.");
		}

		if (!hasConfiguredItemTags)
		{
			return true;
		}

		if (auto* inventoryAllowedTags = FindGameplayTagContainerValue(inventoryComponent, {STR("AllowedTags")}))
		{
			const auto beforeCount = inventoryAllowedTags->GameplayTags.Num();
			const int32 addedCount = MergeTagContainers(*inventoryAllowedTags, configuredItemTags);
			const auto afterCount = inventoryAllowedTags->GameplayTags.Num();
			if (addedCount > 0)
			{
				PCL_Log("MetalFarm inventory AllowedTags merged {} configured item tags on {} ({} -> {}).", addedCount, inventoryComponent->GetFullName(), beforeCount, afterCount);
			}
		}

		if (auto* actorAllowedItemTags = FindGameplayTagContainerValue(metalFarmActor, {STR("AllowedItemTags")}))
		{
			const int32 addedCount = MergeTagContainers(*actorAllowedItemTags, configuredItemTags);
			if (addedCount > 0)
			{
				PCL_Log("MetalFarm actor AllowedItemTags merged {} configured item tags on {}.", addedCount, metalFarmActor->GetFullName());
			}
		}

		return true;
	}

	auto MetalFarmTableApplier::ApplyFromMetalFarm(UObject* metalFarmInstance) -> bool
	{
		if (s_hasApplied || (s_hasPatchedSeedClassMap && s_hasPatchedMetalSeedMap && s_hasPatchedInventoryFilter))
		{
			s_hasApplied = true;
			return true;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return false;
		}

		const auto metalFarmClass = metalFarmInstance->GetClassPrivate();
		const auto metalFarmCDO = metalFarmClass ? metalFarmClass->GetClassDefaultObject() : nullptr;
		if (!metalFarmCDO)
		{
			PCL_WarnLog("MetalFarm table patch deferred because the BP_MetalFarm CDO is not available yet.");
			return false;
		}

		auto* dataMapProperty = CastField<FObjectProperty>(metalFarmCDO->GetPropertyByNameInChain(STR("Data Map")));
		if (!dataMapProperty)
		{
			PCL_WarnLog("MetalFarm table patch deferred because the BP_MetalFarm 'Data Map' property is not available yet.");
			return false;
		}

		const auto dataMap = *dataMapProperty->ContainerPtrToValuePtr<UObject*>(metalFarmCDO);
		if (!dataMap)
		{
			PCL_WarnLog("MetalFarm table patch deferred because the BP_MetalFarm 'Data Map' value is still null.");
			return false;
		}

		if (!s_hasPatchedSeedClassMap)
		{
			s_hasPatchedSeedClassMap = PatchSeedMap(metalFarmCDO, STR("ItemTypeToSeedClass"));
		}

		if (!s_hasPatchedMetalSeedMap)
		{
			s_hasPatchedMetalSeedMap = PatchSeedMap(dataMap, STR("ItemTypeToMetalSeed"));
		}

		if (s_hasPatchedSeedClassMap && s_hasPatchedMetalSeedMap)
		{
			s_hasApplied = true;
			PCL_Log("MetalFarm tables patched from ReceiveBeginPlay using {} configured entries.", s_entries.size());
			return true;
		}

		if (s_hasPatchedSeedClassMap || s_hasPatchedMetalSeedMap)
		{
			PCL_Log("MetalFarm partially patched and is still waiting on remaining maps after a trigger.");
		}
		else
		{
			PCL_WarnLog("MetalFarm patch is still waiting for table map properties to be available after a trigger.");
		}

		return false;
	}

	auto MetalFarmTableApplier::TryApply(UObject* metalFarmInstance) -> void
	{
		if (s_entries.empty())
		{
			return;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return;
		}

		// Patch the live actor's own InventoryComponent on every BeginPlay so
		// every Metal Farm instance in the world receives the filter bypass.
		PatchInventoryFilter(metalFarmInstance);

		if (!s_hasApplied)
		{
			(void)ApplyFromMetalFarm(metalFarmInstance);
		}
	}
}