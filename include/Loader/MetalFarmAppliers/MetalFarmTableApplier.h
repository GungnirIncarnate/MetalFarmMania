#pragma once

#include <vector>

#include <Unreal/Core/CoreFwd.hpp>

#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader
{
	class MetalFarmTableApplier
	{
	public:
		static auto Initialize(std::vector<Config::MetalFarmAdditionEntry> entries) -> void;
		static auto TryApply(RC::Unreal::UObject* metalFarmInstance) -> void;
		static auto LogDiagnostics(RC::Unreal::UObject* metalFarmInstance) -> void;

	private:
		static auto ApplyFromMetalFarm(RC::Unreal::UObject* metalFarmInstance) -> bool;

		static inline std::vector<Config::MetalFarmAdditionEntry> s_entries{};
	};
}