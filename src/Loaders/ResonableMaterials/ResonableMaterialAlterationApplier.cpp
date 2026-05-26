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
		if (dotIndex != std::string::npos)
		{
			const auto objectName = normalized.substr(dotIndex + 1);
			if (objectName.size() > 2 && objectName.compare(objectName.size() - 2, 2, "_C") == 0)
			{
				auto withoutClassSuffix = normalized;
				withoutClassSuffix.erase(withoutClassSuffix.size() - 2);
				addCandidate(withoutClassSuffix);
			}

			if (!objectName.empty() && (objectName.size() < 2 || objectName.compare(objectName.size() - 2, 2, "_C") != 0))
			{
				addCandidate(normalized + "_C");
			}
		}

		return candidates;
	}

	auto ResolveObject(const std::string& objectPath) -> UObject*
	{
		for (const auto& candidatePath : BuildObjectPathCandidates(objectPath))
		{
			if (auto* resolved = UObjectGlobals::StaticFindObject(nullptr, nullptr, ToWideString(candidatePath).c_str()))
			{
				return resolved;
			}
		}

		return nullptr;
	}

	auto ExtractGeneratedClass(UObject* object) -> UClass*
	{
		if (!object)
		{
			return nullptr;
		}

		if (auto* objectClass = Cast<UClass>(object))
		{
			return objectClass;
		}

		auto* generatedClassProperty = CastField<FObjectPropertyBase>(object->GetPropertyByNameInChain(STR("GeneratedClass")));
		if (!generatedClassProperty)
		{
			return nullptr;
		}

		auto* generatedClassValue = generatedClassProperty->ContainerPtrToValuePtr<void>(object);
		if (!generatedClassValue)
		{
			return nullptr;
		}

		auto* generatedClassObject = generatedClassProperty->GetObjectPropertyValue(generatedClassValue);
		return generatedClassObject ? Cast<UClass>(generatedClassObject) : nullptr;
	}

	auto GetObjectShortNameFromPath(const std::string& objectPath) -> std::string
	{
		if (objectPath.empty())
		{
			return {};
		}

		const auto dotIndex = objectPath.find_last_of('.');
		if (dotIndex != std::string::npos && dotIndex + 1 < objectPath.size())
		{
			return objectPath.substr(dotIndex + 1);
		}

		const auto slashIndex = objectPath.find_last_of('/');
		if (slashIndex != std::string::npos && slashIndex + 1 < objectPath.size())
		{
			return objectPath.substr(slashIndex + 1);
		}

		return objectPath;
	}

	auto ResolveClassObject(const std::string& objectPath) -> UClass*
	{
		for (const auto& candidatePath : BuildObjectPathCandidates(objectPath))
		{
			if (auto* resolved = UObjectGlobals::StaticFindObject(nullptr, nullptr, ToWideString(candidatePath).c_str()))
			{
				if (auto* resolvedClass = ExtractGeneratedClass(resolved))
				{
					return resolvedClass;
				}
			}
		}

		std::vector<std::string> shortNameCandidates{};
		auto addShortNameCandidate = [&shortNameCandidates](const std::string& candidate) -> void
		{
			if (candidate.empty())
			{
				return;
			}

			for (const auto& existing : shortNameCandidates)
			{
				if (existing == candidate)
				{
					return;
				}
			}

			shortNameCandidates.emplace_back(candidate);
		};

		addShortNameCandidate(GetObjectShortNameFromPath(objectPath));
		for (const auto& candidatePath : BuildObjectPathCandidates(objectPath))
		{
			addShortNameCandidate(GetObjectShortNameFromPath(candidatePath));
		}

		for (const auto& shortName : shortNameCandidates)
		{
			auto blueprintName = shortName;
			if (blueprintName.size() > 2 && blueprintName.compare(blueprintName.size() - 2, 2, "_C") == 0)
			{
				blueprintName.erase(blueprintName.size() - 2);
			}

			for (const auto* className : {STR("Class"), STR("BlueprintGeneratedClass"), STR("Blueprint")})
			{
				if (auto* resolved = UObjectGlobals::FindObject(className, ToWideString(shortName).c_str()))
				{
					if (auto* resolvedClass = ExtractGeneratedClass(resolved))
					{
						return resolvedClass;
					}
				}

				if (!blueprintName.empty())
				{
					if (auto* resolved = UObjectGlobals::FindObject(className, ToWideString(blueprintName).c_str()))
					{
						if (auto* resolvedClass = ExtractGeneratedClass(resolved))
						{
							return resolvedClass;
						}
					}
				}
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

		if (auto* classProperty = CastField<FClassProperty>(property))
		{
			auto* resolvedClass = ResolveClassObject(resourceClassPath);
			auto* classValue = classProperty->ContainerPtrToValuePtr<UClass*>(container);
			if (resolvedClass && classValue)
			{
				*classValue = resolvedClass;
				return true;
			}
			return false;
		}

		if (auto* objectProperty = CastField<FObjectProperty>(property))
		{
			auto* resolved = ResolveObject(resourceClassPath);
			auto* objectValue = objectProperty->ContainerPtrToValuePtr<UObject*>(container);
			if (resolved && objectValue)
			{
				*objectValue = resolved;
				return true;
			}
			return false;
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
				PCL_Log(
					"Resonatable output patch expanded Content on {} from {} to {} entries.",
					resonatableData->GetFullName(),
					existingCount,
					contentArray->Num());
			}
		}

		const int32 targetCount = std::min(desiredCount, contentArray->Num());

		int32 patchedEntries = 0;
		bool loggedResourceClassDiagnostics = false;
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

			if (!classPatched && !loggedResourceClassDiagnostics)
			{
				loggedResourceClassDiagnostics = true;
				auto* resourceClassStructProperty = CastField<FStructProperty>(resourceClassProperty);
				auto resourceClassStruct = resourceClassStructProperty ? resourceClassStructProperty->GetStruct() : nullptr;
				const bool hasAssetPathName = resourceClassStruct && resourceClassStruct->GetPropertyByNameInChain(STR("AssetPathName"));
				const bool hasAssetPath = resourceClassStruct && resourceClassStruct->GetPropertyByNameInChain(STR("AssetPath"));
				const bool hasPackageName = resourceClassStruct && resourceClassStruct->GetPropertyByNameInChain(STR("PackageName"));
				const bool hasAssetName = resourceClassStruct && resourceClassStruct->GetPropertyByNameInChain(STR("AssetName"));

				PCL_WarnLog(
					"Resonatable ResourceClass diagnostics on {}: property='{}' full='{}' softClass={} struct={} objectBase={} class={} object={} structFull='{}' hasAssetPathName={} hasAssetPath={} hasPackageName={} hasAssetName={} valuePath='{}'.",
					resonatableData->GetFullName(),
					resourceClassProperty->GetName(),
					resourceClassProperty->GetFullName(),
					CastField<FSoftClassProperty>(resourceClassProperty) != nullptr,
					resourceClassStructProperty != nullptr,
					CastField<FObjectPropertyBase>(resourceClassProperty) != nullptr,
					CastField<FClassProperty>(resourceClassProperty) != nullptr,
					CastField<FObjectProperty>(resourceClassProperty) != nullptr,
					resourceClassStruct ? resourceClassStruct->GetFullName() : STR("<none>"),
					hasAssetPathName,
					hasAssetPath,
					hasPackageName,
					hasAssetName,
					ToWideString(output.resourceClassPath));
			}

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

		PCL_Log("Resonatable output patch applied {} of {} targeted entries on {}.", patchedEntries, targetCount, resonatableData->GetFullName());
		return patchedEntries == targetCount;
	}
}
