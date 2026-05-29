#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

#include <cctype>
#include <fstream>
#include <sstream>

#include <UE4SSProgram.hpp>
#include <nlohmann/json.hpp>

#include "Logger/Logger.h"

namespace
{
	using nlohmann::json;
	constexpr const char* kDefaultCopperResonatablePath = "/Game/Data/Resonatable/DA_MetalFarm_CopperDeposit_ResonatableData.DA_MetalFarm_CopperDeposit_ResonatableData_C";
	constexpr const char* kDefaultSeedMaterialPath = "/Game/Art/Resources/MI_Resource_CopperNode_02a.MI_Resource_CopperNode_02a_C";

	auto NormalizeGrowthSpeedToTierTag(const std::string& growthSpeed) -> std::string
	{
		if (growthSpeed == "fast")
		{
			return "ItemType.TunableData.SeedGrowerTime.Fast";
		}

		if (growthSpeed == "slow")
		{
			return "ItemType.TunableData.SeedGrowerTime.Slow";
		}

		return "ItemType.TunableData.SeedGrowerTime.Medium";
	}

	auto BuildCustomSeedGrowerTimeTag(const std::string& entryId) -> std::string
	{
		std::string suffix{};
		suffix.reserve(entryId.size());

		for (const unsigned char ch : entryId)
		{
			if (std::isalnum(ch))
			{
				suffix.push_back(static_cast<char>(ch));
			}
			else
			{
				suffix.push_back('_');
			}
		}

		if (suffix.empty())
		{
			suffix = "Entry";
		}

		return "ItemType.TunableData.SeedGrowerTime.MFM." + suffix;
	}

	auto ReadRequiredString(const json& object, const char* fieldName, std::string& outValue) -> bool
	{
		const auto fieldIt = object.find(fieldName);
		if (fieldIt == object.end() || !fieldIt->is_string())
		{
			return false;
		}

		outValue = fieldIt->get<std::string>();
		return !outValue.empty();
	}

	auto ReadFileToString(const std::filesystem::path& path, std::string& outContent) -> bool
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		std::ostringstream buffer;
		buffer << file.rdbuf();
		outContent = buffer.str();
		return true;
	}

	auto TryReadOutputs(const json& entryJson, std::vector<MFM::Loader::Config::MetalFarmOutputEntry>& outOutputs) -> bool
	{
		const auto outputsIt = entryJson.find("outputs");
		if (outputsIt == entryJson.end() || !outputsIt->is_array() || outputsIt->empty())
		{
			return false;
		}

		for (const auto& outputJson : *outputsIt)
		{
			if (!outputJson.is_object())
			{
				return false;
			}

			auto findFieldByNormalizedName = [&outputJson](const char* normalizedName) -> const json*
			{
				const auto directIt = outputJson.find(normalizedName);
				if (directIt != outputJson.end())
				{
					return &(*directIt);
				}

				for (const auto& item : outputJson.items())
				{
					std::string key = item.key();
					key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char ch) {
						return std::isspace(ch);
					}), key.end());

					if (key == normalizedName)
					{
						return &item.value();
					}
				}

				return nullptr;
			};

			MFM::Loader::Config::MetalFarmOutputEntry output{};
			if (!ReadRequiredString(outputJson, "resourceClassPath", output.resourceClassPath))
			{
				return false;
			}

			const auto* yieldValue = findFieldByNormalizedName("yield");
			if (!yieldValue || !yieldValue->is_number_integer())
			{
				return false;
			}

			output.yield = yieldValue->get<int>();
			if (output.yield <= 0)
			{
				return false;
			}

			const auto* dropChanceValue = findFieldByNormalizedName("dropChance");
			output.dropChance = dropChanceValue && dropChanceValue->is_number() ? dropChanceValue->get<float>() : 1.0f;
			if (output.dropChance < 0.0f || output.dropChance > 1.0f)
			{
				return false;
			}

			outOutputs.emplace_back(std::move(output));
		}

		return !outOutputs.empty();
	}
}

namespace MFM::Loader::Config
{
	auto MetalFarmAdditionsConfigLoader::GetDefaultConfigPath() -> std::filesystem::path
	{
		return std::filesystem::path(RC::UE4SSProgram::get_program().get_working_directory()) /
		       "Mods" /
		       "MetalFarmMania" /
		       "MetalFarmAdditions.jsonc";
	}

	auto MetalFarmAdditionsConfigLoader::LoadDefault() -> std::vector<MetalFarmAdditionEntry>
	{
		return Load(GetDefaultConfigPath());
	}

	auto MetalFarmAdditionsConfigLoader::Load(const std::filesystem::path& configPath) -> std::vector<MetalFarmAdditionEntry>
	{
		std::vector<MetalFarmAdditionEntry> entries;

		if (!std::filesystem::exists(configPath))
		{
			PCL_WarnLog("MetalFarm additions config not found at {}", configPath.wstring());
			return entries;
		}

		std::string fileContent;
		if (!ReadFileToString(configPath, fileContent))
		{
			PCL_ErrorLog("Failed to read MetalFarm additions config from {}", configPath.wstring());
			return entries;
		}

		json root;
		try
		{
			// JSONC support: allow comments in end-user config files.
			root = json::parse(fileContent, nullptr, true, true);
		}
		catch (const std::exception& e)
		{
			const std::string errorText = e.what();
			const std::wstring wideErrorText(errorText.begin(), errorText.end());
			PCL_ErrorLog("Failed to parse MetalFarm additions config {}: {}", configPath.wstring(), wideErrorText);
			return entries;
		}

		if (!root.is_object())
		{
			PCL_ErrorLog("MetalFarm additions config root must be an object: {}", configPath.wstring());
			return entries;
		}

		const auto versionIt = root.find("schemaVersion");
		if (versionIt != root.end())
		{
			if (!versionIt->is_number_integer() || versionIt->get<int>() != 2)
			{
				PCL_ErrorLog("Unsupported MetalFarm additions schemaVersion in {} (expected 2)", configPath.wstring());
				return entries;
			}
		}

		const auto entriesIt = root.find("entries");
		if (entriesIt == root.end() || !entriesIt->is_array())
		{
			PCL_ErrorLog("Missing or invalid 'entries' array in {}", configPath.wstring());
			return entries;
		}

		std::size_t index = 0;
		for (const auto& entryJson : *entriesIt)
		{
			++index;
			if (!entryJson.is_object())
			{
				PCL_WarnLog("Skipping MetalFarm entry #{} because it is not an object", index);
				continue;
			}

			MetalFarmAdditionEntry entry{};
			const bool hasCoreFields = ReadRequiredString(entryJson, "id", entry.id) &&
			                           ReadRequiredString(entryJson, "inputItemPath", entry.inputItemPath);
			if (!hasCoreFields)
			{
				PCL_WarnLog("Skipping MetalFarm entry #{} due to missing required fields", index);
				continue;
			}

			entry.growthSpeed = entryJson.value("growthSpeed", std::string{"medium"});
			entry.metalTierTag = NormalizeGrowthSpeedToTierTag(entry.growthSpeed);

			const auto growthTimeIt = entryJson.find("growthTimeSeconds");
			if (growthTimeIt != entryJson.end())
			{
				if (!growthTimeIt->is_number())
				{
					PCL_WarnLog("Skipping MetalFarm entry #{} due to non-numeric growthTimeSeconds", index);
					continue;
				}

				const auto parsedGrowthTimeSeconds = growthTimeIt->get<float>();
				if (parsedGrowthTimeSeconds <= 0.0f)
				{
					PCL_WarnLog("Skipping MetalFarm entry #{} due to non-positive growthTimeSeconds", index);
					continue;
				}

				entry.growthTimeSeconds = parsedGrowthTimeSeconds;
				entry.metalTierTag = BuildCustomSeedGrowerTimeTag(entry.id);
			}

			entry.seedMaterialPath = kDefaultSeedMaterialPath;
			entry.resonatableDataPath = kDefaultCopperResonatablePath;
			entry.itemTypePath = entry.inputItemPath;

			if (!TryReadOutputs(entryJson, entry.outputs))
			{
				PCL_WarnLog("Skipping MetalFarm entry #{} due to invalid or missing outputs array", index);
				continue;
			}

			entries.emplace_back(std::move(entry));
		}

		return entries;
	}
}
