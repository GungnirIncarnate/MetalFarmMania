#include "Loader/MainLoader.h"
#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"
#include "Loader/MetalFarmTableApplier.h"
#include "Hooks/ReceiveBeginPlayHook.h"
#include "Logger/Logger.h"

namespace MFM::Loader
{
	auto MainLoader::Initialize() -> void
	{
		PCL_Log("MainLoader initialized.");

		const auto entries = Config::MetalFarmAdditionsConfigLoader::LoadDefault();
		PCL_Log("MetalFarm additions ready with {} entries.", entries.size());

		MetalFarmTableApplier::Initialize(entries);
		MFM::Hooks::ReceiveBeginPlayHook::Initialize();
	}

}
