#include "Loader/ResonableMaterials/ResonableMaterialAlterationApplier.h"

#include <string>
#include <vector>

#include <Unreal/Core/Containers/ScriptArray.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

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
		}

		return normalized;
	}

	auto ResolveObjectFromPath(const std::string& objectPath) -> UObject*
	{
		if (objectPath.empty())
		{
			return nullptr;
		}

		const auto tryResolve = [](const std::string& candidate) -> UObject*
		{
			if (candidate.empty())
			{
				return nullptr;
			}

			const auto wideCandidate = ToWideString(candidate);
			return UObjectGlobals::StaticFindObject(nullptr, nullptr, wideCandidate.c_str());
		};

		if (auto* resolved = tryResolve(objectPath))
		{
			return resolved;
		}

		const auto normalized = NormalizeObjectPath(objectPath);
		if (normalized != objectPath)
		{
			if (auto* resolved = tryResolve(normalized))
			{
				return resolved;
			}
		}

		return nullptr;
	}


	auto SetSoftPathStruct(FStructProperty* structProperty, void* structValue, const std::string& objectPath) -> bool
	{
		if (!structProperty || !structValue || objectPath.empty())
		{
			return false;
		}

		const auto normalizedPath = NormalizeObjectPath(objectPath);
		const auto dotIndex = normalizedPath.find_last_of('.');
		const auto packagePath = (dotIndex == std::string::npos) ? normalizedPath : normalizedPath.substr(0, dotIndex);
		const auto assetName = (dotIndex == std::string::npos) ? std::string{} : normalizedPath.substr(dotIndex + 1);

		auto setPackageAssetOnStruct = [](UStruct* targetStruct, void* targetValue, const std::wstring& packageValue, const std::wstring& assetValue) -> bool
		{
			if (!targetStruct || !targetValue || packageValue.empty() || assetValue.empty())
			{
				return false;
			}

			bool patchedInner = false;
			if (auto* packageNameProperty = CastField<FNameProperty>(targetStruct->GetPropertyByNameInChain(STR("PackageName"))))
			{
				auto* packageNameValue = packageNameProperty->ContainerPtrToValuePtr<FName>(targetValue);
				if (packageNameValue)
				{
					*packageNameValue = FName(packageValue.c_str(), FNAME_Add);
					patchedInner = true;
				}
			}

			if (auto* assetNameProperty = CastField<FNameProperty>(targetStruct->GetPropertyByNameInChain(STR("AssetName"))))
			{
				auto* assetNameValue = assetNameProperty->ContainerPtrToValuePtr<FName>(targetValue);
				if (assetNameValue)
				{
					*assetNameValue = FName(assetValue.c_str(), FNAME_Add);
					patchedInner = true;
				}
			}

			return patchedInner;
		};

		bool patched = false;
		// Best-effort assignment for wrapped variants that expose path members.
		if (auto* assetPathNameProperty = CastField<FNameProperty>(structProperty->GetStruct()->GetPropertyByNameInChain(STR("AssetPathName"))))
		{
			auto* assetPathNameValue = assetPathNameProperty->ContainerPtrToValuePtr<FName>(structValue);
			if (assetPathNameValue)
			{
				*assetPathNameValue = FName(ToWideString(normalizedPath).c_str(), FNAME_Add);
				patched = true;
			}
		}

		if (auto* assetPathNameStructProperty = CastField<FStructProperty>(structProperty->GetStruct()->GetPropertyByNameInChain(STR("AssetPathName"))))
		{
			auto* assetPathNameStructValue = assetPathNameStructProperty->ContainerPtrToValuePtr<void>(structValue);
			if (assetPathNameStructValue)
			{
				patched = setPackageAssetOnStruct(
					assetPathNameStructProperty->GetStruct(),
					assetPathNameStructValue,
					ToWideString(packagePath),
					ToWideString(assetName)) || patched;
			}
		}

		if (auto* assetPathStructProperty = CastField<FStructProperty>(structProperty->GetStruct()->GetPropertyByNameInChain(STR("AssetPath"))))
		{
			auto* assetPathStructValue = assetPathStructProperty->ContainerPtrToValuePtr<void>(structValue);
			if (assetPathStructValue)
			{
				patched = setPackageAssetOnStruct(
					assetPathStructProperty->GetStruct(),
					assetPathStructValue,
					ToWideString(packagePath),
					ToWideString(assetName)) || patched;
			}
		}

		patched = setPackageAssetOnStruct(
			structProperty->GetStruct(),
			structValue,
			ToWideString(packagePath),
			ToWideString(assetName)) || patched;

		return patched;
	}

	auto SetResourceClass(FProperty* property, void* container, const std::string& resourceClassPath) -> bool
	{
		if (!property || !container || resourceClassPath.empty())
		{
			return false;
		}

		if (resourceClassPath.find("/Data/ItemType/") != std::string::npos)
		{
			PCL_WarnLog(
				"Resonatable output resourceClassPath '{}' looks like an ItemType asset path. This field requires a world actor class path (for example '/Game/Blueprints/Items/Resources/BP_Copper.BP_Copper_C').",
				ToWideString(resourceClassPath));
		}

		if (auto* resolved = ResolveObjectFromPath(resourceClassPath))
		{
			if (resolved->GetClassPrivate() && resolved->GetClassPrivate()->GetName() == STR("UWEItemType"))
			{
				PCL_WarnLog(
					"Resonatable output resourceClassPath '{}' resolved to ItemType object '{}'. Expected an actor class path (for example '/Game/Blueprints/Items/Resources/BP_Copper.BP_Copper_C').",
					ToWideString(resourceClassPath),
					resolved->GetFullName());
				return false;
			}
		}

		if (auto* softClassProperty = CastField<FSoftClassProperty>(property))
		{
			auto* propertyValue = softClassProperty->ContainerPtrToValuePtr<void>(container);
			if (!propertyValue)
			{
				PCL_WarnLog(
					"Resonatable ResourceClass soft-class patch could not access property value for path '{}' on property '{}'.",
					ToWideString(resourceClassPath),
					property->GetName());
				return false;
			}

			auto normalizedClassPath = NormalizeObjectPath(resourceClassPath);
			const auto slashIndex = normalizedClassPath.find_last_of('/');
			const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
			const auto dotIndex = normalizedClassPath.find('.', nameStart);
			if (dotIndex != std::string::npos)
			{
				const auto objectName = normalizedClassPath.substr(dotIndex + 1);
				if (!objectName.empty() && (objectName.size() < 2 || objectName.compare(objectName.size() - 2, 2, "_C") != 0))
				{
					normalizedClassPath += "_C";
				}
			}

			const auto normalizedWide = ToWideString(normalizedClassPath);
			const std::vector<std::wstring> importCandidates{
				normalizedWide,
				L"Class'" + normalizedWide + L"'",
				L"BlueprintGeneratedClass'" + normalizedWide + L"'"};

			for (const auto& candidateText : importCandidates)
			{
				if (softClassProperty->ImportText_Direct(candidateText.c_str(), propertyValue, nullptr, 0, nullptr) != nullptr)
				{
					return true;
				}
			}

			PCL_WarnLog(
				"Resonatable ResourceClass soft-class patch could not resolve/import class for path '{}' on property '{}'.",
				ToWideString(resourceClassPath),
				property->GetName());
			return false;
		}

		if (auto* structProperty = CastField<FStructProperty>(property))
		{
			return SetSoftPathStruct(structProperty, structProperty->ContainerPtrToValuePtr<void>(container), resourceClassPath);
		}

		return false;
	}

	auto SetOutputYield(FProperty* property, void* container, int value) -> bool
	{
		if (!property || !container)
		{
			return false;
		}

		if (auto* intProperty = CastField<FIntProperty>(property))
		{
			auto* outValue = intProperty->ContainerPtrToValuePtr<int32>(container);
			if (outValue)
			{
				*outValue = value;
				return true;
			}
		}

		if (auto* int64Property = CastField<FInt64Property>(property))
		{
			auto* outValue = int64Property->ContainerPtrToValuePtr<int64>(container);
			if (outValue)
			{
				*outValue = value;
				return true;
			}
		}

		return false;
	}

	auto SetOutputDropChance(FProperty* property, void* container, float value) -> bool
	{
		if (!property || !container)
		{
			return false;
		}

		if (auto* floatProperty = CastField<FFloatProperty>(property))
		{
			auto* outValue = floatProperty->ContainerPtrToValuePtr<float>(container);
			if (outValue)
			{
				*outValue = value;
				return true;
			}
		}

		if (auto* doubleProperty = CastField<FDoubleProperty>(property))
		{
			auto* outValue = doubleProperty->ContainerPtrToValuePtr<double>(container);
			if (outValue)
			{
				*outValue = value;
				return true;
			}
		}

		return false;
	}
}

namespace MFM::Loader::ResonableMaterials
{
	auto ResonableMaterialAlterationApplier::ApplyOutputs(
		RC::Unreal::UObject* resonatableData,
		const std::vector<Config::MetalFarmOutputEntry>& outputs) -> bool
	{
		if (!resonatableData || outputs.empty())
		{
			return false;
		}

		auto* contentProperty = CastField<FArrayProperty>(resonatableData->GetPropertyByNameInChain(STR("Content")));
		if (!contentProperty)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: missing 'Content' array property.", resonatableData->GetFullName());
			return false;
		}

		auto* contentStructProperty = CastField<FStructProperty>(contentProperty->GetInner());
		if (!contentStructProperty)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: 'Content' inner type is not a struct.", resonatableData->GetFullName());
			return false;
		}

		auto* contentArrayValue = contentProperty->ContainerPtrToValuePtr<void>(resonatableData);
		if (!contentArrayValue)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: could not get 'Content' value pointer.", resonatableData->GetFullName());
			return false;
		}

		auto* contentArray = static_cast<FScriptArray*>(contentArrayValue);
		auto* contentInner = contentProperty->GetInner();
		if (!contentArray || !contentInner)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: Content array internals unavailable.", resonatableData->GetFullName());
			return false;
		}

		const int32 existingCount = contentArray->Num();
		if (existingCount <= 0)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: Content array has no entries to patch.", resonatableData->GetFullName());
			return false;
		}

		auto* resourceClassProperty = contentStructProperty->GetStruct()->GetPropertyByNameInChain(STR("ResourceClass"));
		auto* yieldProperty = contentStructProperty->GetStruct()->GetPropertyByNameInChain(STR("NumResourcesToDrop"));
		auto* dropChanceProperty = contentStructProperty->GetStruct()->GetPropertyByNameInChain(STR("DropChance"));

		if (!resourceClassProperty || !yieldProperty || !dropChanceProperty)
		{
			PCL_WarnLog(
				"Resonatable output patch failed on {}: expected fields ResourceClass/NumResourcesToDrop/DropChance were not found in Content element struct '{}'.",
				resonatableData->GetFullName(),
				contentStructProperty->GetStruct()->GetFullName());
			return false;
		}

		const int32 desiredCount = static_cast<int32>(outputs.size());
		if (desiredCount > existingCount)
		{
			const int32 toAdd = desiredCount - existingCount;
			const int32 baseIndex = contentArray->Add(toAdd, contentInner->GetElementSize(), contentInner->GetMinAlignment());
			for (int32 index = 0; index < toAdd; ++index)
			{
				auto* newValue = static_cast<uint8*>(contentArray->GetData()) + (static_cast<size_t>(baseIndex + index) * contentInner->GetElementSize());
				contentInner->InitializeValue(newValue);
			}

			if (contentArray->Num() < desiredCount)
			{
				PCL_WarnLog(
					"Resonatable output patch on {} requested Content growth from {} to {}, but only reached {}; patching available entries.",
					resonatableData->GetFullName(),
					existingCount,
					desiredCount,
					contentArray->Num());
			}
			else
			{
				// Growth succeeded; no routine info log to keep setup output concise.
			}
		}

		const int32 targetCount = std::min(desiredCount, contentArray->Num());

		int32 patchedEntries = 0;
		for (int32 index = 0; index < targetCount; ++index)
		{
			const auto& output = outputs[static_cast<size_t>(index)];
			void* contentElement = static_cast<uint8*>(contentArray->GetData()) + (static_cast<size_t>(index) * contentInner->GetElementSize());
			if (!contentElement)
			{
				continue;
			}

			const bool classPatched = SetResourceClass(resourceClassProperty, contentElement, output.resourceClassPath);
			const bool yieldPatched = SetOutputYield(yieldProperty, contentElement, output.yield);
			const bool chancePatched = SetOutputDropChance(dropChanceProperty, contentElement, output.dropChance);

			if (!classPatched || !yieldPatched || !chancePatched)
			{
				PCL_WarnLog(
					"Resonatable output patch partial failure on {} entry {} (classPatched={}, yieldPatched={}, chancePatched={}) class='{}'.",
					resonatableData->GetFullName(),
					index,
					classPatched,
					yieldPatched,
					chancePatched,
					ToWideString(output.resourceClassPath));
				continue;
			}

			++patchedEntries;
		}

		if (patchedEntries == 0)
		{
			PCL_WarnLog("Resonatable output patch failed on {}: no output entries were successfully patched.", resonatableData->GetFullName());
			return false;
		}

		return patchedEntries == targetCount;
	}
}
