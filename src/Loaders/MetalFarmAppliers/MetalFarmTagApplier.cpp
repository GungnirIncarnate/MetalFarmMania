#include "Loader/MetalFarmAppliers/MetalFarmTagApplier.h"

#include <initializer_list>

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
}
