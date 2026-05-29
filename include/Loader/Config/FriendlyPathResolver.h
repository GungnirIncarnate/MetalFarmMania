#pragma once

#include <cstddef>
#include <string>

namespace MFM::Loader::Config
{
	class FriendlyPathResolver
	{
	public:
		static auto ResolveInputItemPath(const std::string& configuredValue, const std::string& entryId) -> std::string;
		static auto ResolveOutputResourceClassPath(const std::string& configuredValue, const std::string& entryId, std::size_t outputIndex) -> std::string;
	};
}
