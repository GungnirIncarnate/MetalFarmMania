#include "Loader/MetalFarmItemTagLookup.h"

#include <string>
#include <initializer_list>

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

	auto ResolveObject(const std::string& objectPath) -> UObject*
	{
		const auto normalizedPath = NormalizeObjectPath(objectPath);
		const auto widePath = ToWideString(normalizedPath);
		return UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str());
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

	auto ContainsTagName(const std::vector<FName>& tagNames, const FName& tagName) -> bool
	{
		for (const auto& existingTagName : tagNames)
		{
			if (existingTagName == tagName)
			{
				return true;
			}
		}

		return false;
	}

	auto AddTagNameIfMissing(std::vector<FName>& tagNames, const FName& tagName) -> bool
	{
		if (tagName == FName())
		{
			return false;
		}

		if (ContainsTagName(tagNames, tagName))
		{
			return false;
		}

		tagNames.emplace_back(tagName);
		return true;
	}
}

namespace MFM::Loader
{
	auto MetalFarmItemTagLookup::CollectConfiguredItemTagNames(const std::vector<Config::MetalFarmAdditionEntry>& entries) -> std::vector<FName>
	{
		const std::initializer_list<const wchar_t*> tagPropertyCandidates{
			STR("Tags"),
			STR("ItemTags"),
			STR("GameplayTags"),
			STR("AllowedTags"),
			STR("RequiredTags")};

		std::vector<FName> discoveredTagNames{};

		for (const auto& entry : entries)
		{
			auto* itemType = ResolveObject(entry.itemTypePath);
			if (!itemType)
			{
				PCL_WarnLog("MetalFarm tag lookup could not resolve itemTypePath '{}' for entry '{}'.", ToWideString(entry.itemTypePath), ToWideString(entry.id));
				continue;
			}

			if (auto* itemTags = FindGameplayTagContainerValue(itemType, tagPropertyCandidates))
			{
				for (const auto& itemTag : itemTags->GameplayTags)
				{
					(void)AddTagNameIfMissing(discoveredTagNames, itemTag.TagName);
				}
			}

			auto* typeTagProperty = CastField<FStructProperty>(itemType->GetPropertyByNameInChain(STR("TypeTag")));
			if (IsGameplayTagProperty(typeTagProperty))
			{
				auto* typeTag = typeTagProperty->ContainerPtrToValuePtr<FGameplayTagLite>(itemType);
				if (typeTag)
				{
					(void)AddTagNameIfMissing(discoveredTagNames, typeTag->TagName);
				}
			}
		}

		if (discoveredTagNames.empty())
		{
			PCL_WarnLog("MetalFarm tag lookup could not discover gameplay tags from configured item types.");
		}

		return discoveredTagNames;
	}
}
