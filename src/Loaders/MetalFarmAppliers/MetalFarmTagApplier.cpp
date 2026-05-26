#include "Loader/MetalFarmAppliers/MetalFarmTagApplier.h"

#include <initializer_list>
#include <string>

#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/UObject.hpp>

#include "Logger/Logger.h"
#include "Tags/TagLookupHelper.h"

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

	auto IsTargetMetalFarm(const UObject* metalFarmInstance) -> bool
	{
		if (!metalFarmInstance || !metalFarmInstance->GetClassPrivate())
		{
			return false;
		}

		return metalFarmInstance->GetClassPrivate()->GetName() == STR("BP_MetalFarm_C");
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

	auto HasTagName(const FGameplayTagContainerLite& container, const FName& tagName) -> bool
	{
		for (const auto& existingTag : container.GameplayTags)
		{
			if (existingTag.TagName == tagName)
			{
				return true;
			}
		}

		return false;
	}

	auto MergeTagNames(FGameplayTagContainerLite& destination, const std::vector<FName>& tagNames) -> int32
	{
		int32 addedCount = 0;
		for (const auto& tagName : tagNames)
		{
			if (tagName == FName() || HasTagName(destination, tagName))
			{
				continue;
			}

			destination.GameplayTags.Add(FGameplayTagLite{tagName});
			++addedCount;
		}

		return addedCount;
	}

	auto ToWideString(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto PatchConfiguredItemTypeTags(const std::vector<MFM::Loader::Config::MetalFarmAdditionEntry>& entries) -> void
	{
		const auto mineralTagName = FName(STR("ItemType.Mineral"), FNAME_Add);
		const std::initializer_list<const wchar_t*> itemTypeTagPropertyCandidates{
			STR("Tags"),
			STR("ItemTags"),
			STR("GameplayTags")};

		for (const auto& entry : entries)
		{
			auto* itemType = MFM::Tags::TagLookupHelper::ResolveItemTypeFromPath(entry.itemTypePath);
			if (!itemType)
			{
				PCL_WarnLog("MetalFarm item-type seed registration skipped for '{}' because itemTypePath '{}' could not be resolved.", ToWideString(entry.id), ToWideString(entry.itemTypePath));
				continue;
			}

			auto* itemTypeTagContainer = FindGameplayTagContainerValue(itemType, itemTypeTagPropertyCandidates);
			if (!itemTypeTagContainer)
			{
				PCL_WarnLog("MetalFarm item-type seed registration skipped for '{}' because no gameplay tag container was found on {}.", ToWideString(entry.id), itemType->GetFullName());
				continue;
			}

			const int32 addedCount = MergeTagNames(*itemTypeTagContainer, {mineralTagName});
			if (addedCount > 0)
			{
				PCL_Log("MetalFarm registered '{}' as a native mineral seed item by tagging {} with ItemType.Mineral.", ToWideString(entry.id), itemType->GetFullName());
			}
		}
	}
}

namespace MFM::Loader
{
	auto MetalFarmTagApplier::Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void
	{
		s_entries = std::move(entries);
	}

	auto MetalFarmTagApplier::PatchInventoryFilter(UObject* metalFarmActor) -> bool
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

		PatchConfiguredItemTypeTags(s_entries);

		const auto discoveredTagNames = MFM::Tags::TagLookupHelper::CollectConfiguredItemTypeTagNames(s_entries);
		if (discoveredTagNames.empty())
		{
			return true;
		}

		if (auto* inventoryAllowedTags = FindGameplayTagContainerValue(inventoryComponent, {STR("AllowedTags")}))
		{
			const auto beforeCount = inventoryAllowedTags->GameplayTags.Num();
			const int32 addedCount = MergeTagNames(*inventoryAllowedTags, discoveredTagNames);
			const auto afterCount = inventoryAllowedTags->GameplayTags.Num();
			if (addedCount > 0)
			{
				PCL_Log("MetalFarm inventory AllowedTags merged {} configured item tags on {} ({} -> {}).", addedCount, inventoryComponent->GetFullName(), beforeCount, afterCount);
			}
		}

		if (auto* actorAllowedItemTags = FindGameplayTagContainerValue(metalFarmActor, {STR("AllowedItemTags")}))
		{
			const int32 addedCount = MergeTagNames(*actorAllowedItemTags, discoveredTagNames);
			if (addedCount > 0)
			{
				PCL_Log("MetalFarm actor AllowedItemTags merged {} configured item tags on {}.", addedCount, metalFarmActor->GetFullName());
			}
		}

		return true;
	}

	auto MetalFarmTagApplier::TryApply(UObject* metalFarmInstance) -> void
	{
		if (s_entries.empty())
		{
			return;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return;
		}

		(void)PatchInventoryFilter(metalFarmInstance);
	}

	auto MetalFarmTagApplier::LogDiagnostics(UObject* metalFarmInstance) -> void
	{
		if (!metalFarmInstance || !IsTargetMetalFarm(metalFarmInstance))
		{
			return;
		}

		auto* inventoryComponentProperty = CastField<FObjectProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("InventoryComponent")));
		auto* inventoryComponent = inventoryComponentProperty ? *inventoryComponentProperty->ContainerPtrToValuePtr<UObject*>(metalFarmInstance) : nullptr;
		if (!inventoryComponent)
		{
			PCL_WarnLog("MetalFarm tag diagnostics: actor='{}' has null InventoryComponent.", metalFarmInstance->GetFullName());
			return;
		}

		bool allowAnyItems = false;
		if (auto* allowAnyItemsProperty = CastField<FBoolProperty>(inventoryComponent->GetPropertyByNameInChain(STR("AllowAddingAnyItems"))))
		{
			allowAnyItems = allowAnyItemsProperty->GetPropertyValueInContainer(inventoryComponent);
		}

		int32 inventoryAllowedTagsCount = -1;
		if (auto* inventoryAllowedTags = FindGameplayTagContainerValue(inventoryComponent, {STR("AllowedTags")}))
		{
			inventoryAllowedTagsCount = inventoryAllowedTags->GameplayTags.Num();
		}

		int32 actorAllowedItemTagsCount = -1;
		if (auto* actorAllowedItemTags = FindGameplayTagContainerValue(metalFarmInstance, {STR("AllowedItemTags")}))
		{
			actorAllowedItemTagsCount = actorAllowedItemTags->GameplayTags.Num();
		}

		auto* currentItemTypeProperty = CastField<FObjectProperty>(metalFarmInstance->GetPropertyByNameInChain(STR("CurrentItemType")));
		auto* currentItemType = currentItemTypeProperty ? *currentItemTypeProperty->ContainerPtrToValuePtr<UObject*>(metalFarmInstance) : nullptr;
		const auto currentItemTagNames = MFM::Tags::TagLookupHelper::CollectItemTypeTagNames(currentItemType);

		PCL_Log(
			"MetalFarm tag diagnostics: actor='{}' inventory='{}' allowAnyItems={} AllowedTags={} AllowedItemTags={} currentItemType='{}' currentItemTags={} configuredEntries={}.",
			metalFarmInstance->GetFullName(),
			inventoryComponent->GetFullName(),
			allowAnyItems,
			inventoryAllowedTagsCount,
			actorAllowedItemTagsCount,
			currentItemType ? currentItemType->GetFullName() : STR("<null>"),
			currentItemTagNames.size(),
			s_entries.size());
	}
}
