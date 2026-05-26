#include <Mod/CppUserModBase.hpp>

#include "Loader/MainLoader.h"

class MetalFarmMania : public RC::CppUserModBase
{
public:
	MetalFarmMania()
	{
		ModName = STR("MetalFarmMania");
		ModVersion = STR("0.1.0");
		ModDescription = STR("UE4SS C++ mod base for Metal Farm Mania");
		ModAuthors = STR("GungnirIncarnate");
	}

	~MetalFarmMania() override = default;

	auto on_unreal_init() -> void override
	{
		MFM::Loader::MainLoader::Initialize();
	}
};

#define METALFARMMANIA_API __declspec(dllexport)
extern "C"
{
	METALFARMMANIA_API RC::CppUserModBase* start_mod()
	{
		return new MetalFarmMania();
	}

	METALFARMMANIA_API void uninstall_mod(RC::CppUserModBase* mod)
	{
		delete mod;
	}
}
