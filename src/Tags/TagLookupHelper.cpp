#include "Tags/TagLookupHelper.h"

#include <string>
#include <initializer_list>
#include <vector>

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

namespace MFM::Tags
{
	auto TagLookupHelper::ResolveItemTypeFromPath(const std::string& itemTypePath) -> UObject*
	{
		return ResolveObject(itemTypePath);
	}

	auto TagLookupHelper::CollectItemTypeTagNames(UObject* itemType) -> std::vector<FName>
	{
		const std::initializer_list<const wchar_t*> tagPropertyCandidates{
			STR("Tags"),
			STR("ItemTags"),
			STR("GameplayTags"),
			STR("AllowedTags"),
			STR("RequiredTags")};

		std::vector<FName> discoveredTagNames{};
		if (!itemType)
		{
			return discoveredTagNames;
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

		return discoveredTagNames;
	}

	auto TagLookupHelper::CollectItemTypeTagNamesFromPath(const std::string& itemTypePath) -> std::vector<FName>
	{
		auto* itemType = ResolveObject(itemTypePath);
		if (!itemType)
		{
			PCL_WarnLog("Tag lookup could not resolve itemTypePath '{}'.", ToWideString(itemTypePath));
			return {};
		}

		return CollectItemTypeTagNames(itemType);
	}

	auto TagLookupHelper::CollectConfiguredItemTypeTagNames(const std::vector<MFM::Loader::Config::MetalFarmAdditionEntry>& entries) -> std::vector<FName>
	{
		std::vector<FName> discoveredTagNames{};

		for (const auto& entry : entries)
		{
			const auto itemTypeTagNames = CollectItemTypeTagNamesFromPath(entry.GetInputItemPath());
			if (itemTypeTagNames.empty())
			{
				PCL_WarnLog("Tag lookup found no gameplay tags for entry '{}' inputItemPath '{}'.", ToWideString(entry.id), ToWideString(entry.GetInputItemPath()));
				continue;
			}

			for (const auto& tagName : itemTypeTagNames)
			{
				(void)AddTagNameIfMissing(discoveredTagNames, tagName);
			}
		}

		if (discoveredTagNames.empty())
		{
			PCL_WarnLog("Tag lookup could not discover gameplay tags from configured item types.");
		}

		return discoveredTagNames;
	}
}
