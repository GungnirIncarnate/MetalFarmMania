#pragma once

#include <Unreal/Core/CoreFwd.hpp>

namespace RC::Unreal
{
	class UObject;
}

namespace MFM::Loader
{
	class MetalFarmRuntimePatchState
	{
	public:
		static auto Reset() -> void;
		static auto HasSeedClassPatch(const RC::Unreal::UObject* metalFarmInstance) -> bool;
		static auto HasMetalSeedPatch(const RC::Unreal::UObject* dataMap) -> bool;
		static auto MarkSeedClassPatched(RC::Unreal::UObject* metalFarmInstance) -> void;
		static auto MarkMetalSeedPatched(RC::Unreal::UObject* dataMap) -> void;
	};
}