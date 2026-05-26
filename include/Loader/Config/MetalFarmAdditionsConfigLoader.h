#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace MFM::Loader::Config
{
	struct MetalFarmOutputEntry
	{
		std::string resourceClassPath;
		int yield{1};
		float dropChance{1.0f};
	};

	struct MetalFarmAdditionEntry
	{
		std::string id;
		std::string inputItemPath;
		std::string growthSpeed;
		std::vector<MetalFarmOutputEntry> outputs;

		// Legacy/internal fields retained for compatibility with the existing patch flow.
		std::string itemTypePath;
		std::string resonatableDataPath;
		std::string seedMaterialPath;
		std::string metalTierTag;

		auto GetInputItemPath() const -> const std::string&
		{
			return !inputItemPath.empty() ? inputItemPath : itemTypePath;
		}
	};

	class MetalFarmAdditionsConfigLoader
	{
	public:
		static auto GetDefaultConfigPath() -> std::filesystem::path;
		static auto LoadDefault() -> std::vector<MetalFarmAdditionEntry>;
		static auto Load(const std::filesystem::path& configPath) -> std::vector<MetalFarmAdditionEntry>;
	};
}
