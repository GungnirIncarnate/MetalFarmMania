#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

#include <fstream>
#include <sstream>

#include <UE4SSProgram.hpp>
#include <nlohmann/json.hpp>

#include "Logger/Logger.h"

namespace
{
	using nlohmann::json;

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
		if (versionIt == root.end() || !versionIt->is_number_integer() || versionIt->get<int>() != 1)
		{
			PCL_ErrorLog("Unsupported MetalFarm additions schemaVersion in {} (expected 1)", configPath.wstring());
			return entries;
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
			entry.enabled = entryJson.value("enabled", true);
			if (!entry.enabled)
			{
				continue;
			}

			const bool valid = ReadRequiredString(entryJson, "id", entry.id) &&
			                  ReadRequiredString(entryJson, "itemTypePath", entry.itemTypePath) &&
			                  ReadRequiredString(entryJson, "resonatableDataPath", entry.resonatableDataPath) &&
			                  ReadRequiredString(entryJson, "seedMaterialPath", entry.seedMaterialPath) &&
			                  ReadRequiredString(entryJson, "metalTierTag", entry.metalTierTag);

			if (!valid)
			{
				PCL_WarnLog("Skipping MetalFarm entry #{} due to missing required fields", index);
				continue;
			}

			entries.emplace_back(std::move(entry));
		}

		PCL_Log("Loaded {} enabled MetalFarm addition entries from {}", entries.size(), configPath.wstring());
		return entries;
	}
}
