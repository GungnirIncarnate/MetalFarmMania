#include "Loader/ResonableMaterials/ResonableMaterialAssetCloner.h"

#include <cctype>
#include <exception>
#include <string>
#include <unordered_map>
#include <vector>

#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Logger/Logger.h"

namespace
{
	constexpr const char* kBaseResonatableTemplatePath = "/Game/Data/Resonatable/DA_MetalFarm_CopperDeposit_ResonatableData.DA_MetalFarm_CopperDeposit_ResonatableData";
	constexpr const wchar_t* kTransientOuterPathQualified = STR("/Engine/Transient.Transient");

	std::unordered_map<std::string, RC::Unreal::UObject*> g_clonesByEntryId{};

	auto ToWideString(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto BuildCloneObjectName(const std::string& entryId) -> std::wstring
	{
		std::string normalizedId = entryId;
		for (auto& character : normalizedId)
		{
			if (!std::isalnum(static_cast<unsigned char>(character)))
			{
				character = '_';
			}
		}

		return ToWideString("ResonatableClone_" + normalizedId);
	}

	auto ResolveObjectFromPath(const std::string& objectPath) -> RC::Unreal::UObject*
	{
		if (objectPath.empty())
		{
			return nullptr;
		}

		auto tryPath = [](const std::string& candidate) -> RC::Unreal::UObject*
		{
			if (candidate.empty())
			{
				return nullptr;
			}

			// UE4SS FindObject can throw on long names that are missing package delimiters.
			if (candidate.front() != '/' || candidate.find('.') == std::string::npos)
			{
				return nullptr;
			}

			auto widePath = ToWideString(candidate);
			try
			{
				return RC::Unreal::UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str());
			}
			catch (const std::exception& exception)
			{
				PCL_WarnLog("Resonable material path lookup threw for '{}': {}", ToWideString(candidate), ToWideString(exception.what()));
				return nullptr;
			}
			catch (...)
			{
				PCL_WarnLog("Resonable material path lookup threw for '{}': unknown exception.", ToWideString(candidate));
				return nullptr;
			}
		};

		auto addUnique = [](std::vector<std::string>& paths, const std::string& candidate) -> void
		{
			if (candidate.empty())
			{
				return;
			}

			for (const auto& existing : paths)
			{
				if (existing == candidate)
				{
					return;
				}
			}

			paths.emplace_back(candidate);
		};

		std::vector<std::string> candidates{};
		addUnique(candidates, objectPath);

		const auto slashIndex = objectPath.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = objectPath.find('.', nameStart);
		if (dotIndex == std::string::npos)
		{
			const auto assetName = objectPath.substr(nameStart);
			if (!assetName.empty())
			{
				addUnique(candidates, objectPath + "." + assetName);
			}
		}

		for (size_t i = 0; i < candidates.size(); ++i)
		{
			const auto& candidate = candidates[i];
			const auto localSlash = candidate.find_last_of('/');
			const auto localNameStart = (localSlash == std::string::npos) ? 0 : localSlash + 1;
			const auto localDot = candidate.find('.', localNameStart);
			if (localDot == std::string::npos)
			{
				continue;
			}

			const auto objectName = candidate.substr(localDot + 1);
			if (objectName.size() >= 2 && objectName.compare(objectName.size() - 2, 2, "_C") == 0)
			{
				addUnique(candidates, candidate.substr(0, candidate.size() - 2));
			}
			else
			{
				addUnique(candidates, candidate + "_C");
			}
		}

		for (const auto& candidate : candidates)
		{
			if (auto* resolved = tryPath(candidate))
			{
				return resolved;
			}
		}

		return nullptr;
	}

	auto FindCloneOuter(RC::Unreal::UObject* baseTemplate) -> RC::Unreal::UObject*
	{
		try
		{
			if (auto* transientOuter = RC::Unreal::UObjectGlobals::StaticFindObject(nullptr, nullptr, kTransientOuterPathQualified))
			{
				return transientOuter;
			}
		}
		catch (const std::exception& exception)
		{
			PCL_WarnLog("Resonable material transient outer lookup threw: {}", ToWideString(exception.what()));
		}
		catch (...)
		{
			PCL_WarnLog("Resonable material transient outer lookup threw: unknown exception.");
		}

		// Fallback: use the template asset's outer package so clone construction can proceed.
		if (baseTemplate && baseTemplate->GetOuterPrivate())
		{
			PCL_WarnLog(
				"Resonable material transient outer was unavailable; using template outer '{}' for clone construction.",
				baseTemplate->GetOuterPrivate()->GetFullName());
			return baseTemplate->GetOuterPrivate();
		}

		return nullptr;
	}
}

namespace MFM::Loader::ResonableMaterials
{
	auto ResonableMaterialAssetCloner::Initialize() -> void
	{
		g_clonesByEntryId.clear();
	}

	auto ResonableMaterialAssetCloner::CloneForEntry(const Config::MetalFarmAdditionEntry& entry) -> RC::Unreal::UObject*
	{
		auto cloneCacheIt = g_clonesByEntryId.find(entry.id);
		if (cloneCacheIt != g_clonesByEntryId.end() && cloneCacheIt->second)
		{
			return cloneCacheIt->second;
		}

		const auto& templatePath = entry.resonatableDataPath.empty() ? std::string{kBaseResonatableTemplatePath} : entry.resonatableDataPath;
		auto* baseTemplate = ResolveObjectFromPath(templatePath);
		if (!baseTemplate)
		{
			PCL_WarnLog("Resonable material base template '{}' could not be resolved for entry '{}'.", ToWideString(templatePath), ToWideString(entry.id));
			return nullptr;
		}

		auto* cloneOuter = FindCloneOuter(baseTemplate);
		if (!cloneOuter)
		{
			PCL_WarnLog("Resonable material clone skipped for entry '{}' because clone outer could not be resolved.", ToWideString(entry.id));
			return nullptr;
		}

		RC::Unreal::FStaticConstructObjectParameters constructParams{baseTemplate->GetClassPrivate(), cloneOuter};
		constructParams.Name = RC::Unreal::FName(BuildCloneObjectName(entry.id), RC::Unreal::FNAME_Add);
		constructParams.SetFlags = RC::Unreal::EObjectFlags::RF_Transient;
		constructParams.Template = baseTemplate;
		constructParams.bCopyTransientsFromClassDefaults = true;

		auto* clonedObject = RC::Unreal::UObjectGlobals::StaticConstructObject<RC::Unreal::UObject*>(constructParams);
		if (!clonedObject)
		{
			PCL_WarnLog("Resonable material clone construction failed for entry '{}' from template '{}'.", ToWideString(entry.id), ToWideString(templatePath));
			return nullptr;
		}

		clonedObject->SetRootSet();
		g_clonesByEntryId[entry.id] = clonedObject;

		return clonedObject;
	}
}
