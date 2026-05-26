#include "Loader/MainLoader.h"
#include "Loader/Config/MetalFarmAdditionsConfigLoader.h"
#include "Loader/MetalFarmAppliers/MetalFarmTagApplier.h"
#include "Loader/MetalFarmAppliers/MetalFarmTableApplier.h"
#include "Loader/ResonableMaterials/ResonableMaterialAssetCloner.h"
#include "Hooks/ReceiveBeginPlayHook.h"

namespace MFM::Loader
{
	auto MainLoader::Initialize() -> void
	{
		const auto entries = Config::MetalFarmAdditionsConfigLoader::LoadDefault();

		ResonableMaterials::ResonableMaterialAssetCloner::Initialize();
		MetalFarmTagApplier::Initialize(entries);
		MetalFarmTableApplier::Initialize(entries);
		MFM::Hooks::ReceiveBeginPlayHook::Initialize();
	}

}
