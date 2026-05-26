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
			auto* itemType = MFM::Tags::TagLookupHelper::ResolveItemTypeFromPath(entry.GetInputItemPath());
			if (!itemType)
			{
				PCL_WarnLog("MetalFarm item-type seed registration skipped for '{}' because inputItemPath '{}' could not be resolved.", ToWideString(entry.id), ToWideString(entry.GetInputItemPath()));
				continue;
			}

			auto* itemTypeTagContainer = FindGameplayTagContainerValue(itemType, itemTypeTagPropertyCandidates);
			if (!itemTypeTagContainer)
			{
				PCL_WarnLog("MetalFarm item-type seed registration skipped for '{}' because no gameplay tag container was found on {}.", ToWideString(entry.id), itemType->GetFullName());
				continue;
			}

			const int32 addedCount = MergeTagNames(*itemTypeTagContainer, {mineralTagName});
			(void)addedCount;
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
		}

		PatchConfiguredItemTypeTags(s_entries);

		const auto discoveredTagNames = MFM::Tags::TagLookupHelper::CollectConfiguredItemTypeTagNames(s_entries);
		if (discoveredTagNames.empty())
		{
			return true;
		}

		if (auto* inventoryAllowedTags = FindGameplayTagContainerValue(inventoryComponent, {STR("AllowedTags")}))
		{
			MergeTagNames(*inventoryAllowedTags, discoveredTagNames);
		}

		if (auto* actorAllowedItemTags = FindGameplayTagContainerValue(metalFarmActor, {STR("AllowedItemTags")}))
		{
			MergeTagNames(*actorAllowedItemTags, discoveredTagNames);
		}

		return true;
	}

	auto MetalFarmTagApplier::TryApply(UObject* metalFarmInstance) -> bool
	{
		if (s_entries.empty())
		{
			return false;
		}

		if (!IsTargetMetalFarm(metalFarmInstance))
		{
			return false;
		}

		return PatchInventoryFilter(metalFarmInstance);
	}
}
