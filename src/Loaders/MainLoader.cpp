#include "Loader/MainLoader.h"
#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"
#include "Loader/MetalFarmAppliers/MetalFarmTagApplier.h"
#include "Loader/MetalFarmAppliers/MetalFarmTableApplier.h"
#include "Loader/ResonableMaterials/ResonableMaterialAssetCloner.h"
#include "Hooks/ReceiveBeginPlayHook.h"
#include "Logger/Logger.h"

namespace MFM::Loader
{
	auto MainLoader::Initialize() -> void
	{
		PCL_Log("MainLoader initialized.");

		const auto entries = Config::MetalFarmAdditionsConfigLoader::LoadDefault();
		PCL_Log("MetalFarm additions ready with {} entries.", entries.size());

		ResonableMaterials::ResonableMaterialAssetCloner::Initialize();
		MetalFarmTagApplier::Initialize(entries);
		MetalFarmTableApplier::Initialize(entries);
		MFM::Hooks::ReceiveBeginPlayHook::Initialize();
	}

}
