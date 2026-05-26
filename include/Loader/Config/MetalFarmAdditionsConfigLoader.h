#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace MFM::Loader::Config
{
	struct MetalFarmAdditionEntry
	{
		std::string id;
		std::string itemTypePath;
		std::string resonatableDataPath;
		std::string seedMaterialPath;
		std::string metalTierTag;
		bool enabled{true};
	};

	class MetalFarmAdditionsConfigLoader
	{
	public:
		static auto GetDefaultConfigPath() -> std::filesystem::path;
		static auto LoadDefault() -> std::vector<MetalFarmAdditionEntry>;
		static auto Load(const std::filesystem::path& configPath) -> std::vector<MetalFarmAdditionEntry>;
	};
}
