#include "Loader/Config/FriendlyPathResolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include "Logger/Logger.h"

namespace
{
	using namespace RC::Unreal;

	enum class PathKind
	{
		InputItem,
		OutputResource,
	};

	struct CandidatePath
	{
		std::string objectPath;
		std::string loweredPath;
		std::string loweredAssetName;
		std::string loweredObjectName;
		std::string loweredUnderscoreAlias;
		std::string loweredBaseName;
	};

	struct ResolveResult
	{
		std::string objectPath;
		std::size_t matchCount{0};
		int bestScore{std::numeric_limits<int>::min()};
	};

	struct HeuristicResolveResult
	{
		std::string objectPath;
		bool foundLoadedObject{false};
	};

	std::vector<CandidatePath> s_inputCandidates;
	std::vector<CandidatePath> s_outputCandidates;
	bool s_cacheBuilt = false;

	constexpr const char* kInputRoot = "/Game/Data/ItemType/";
	constexpr const char* kOutputRoot = "/Game/Blueprints/Items/";

	auto ToWide(const std::string& value) -> std::wstring
	{
		return {value.begin(), value.end()};
	}

	auto ToNarrowAscii(const std::wstring& value) -> std::string
	{
		std::string out;
		out.reserve(value.size());
		for (const wchar_t ch : value)
		{
			if (ch >= 0 && ch <= 0x7F)
			{
				out.push_back(static_cast<char>(ch));
			}
			else
			{
				out.push_back('?');
			}
		}
		return out;
	}

	auto Trim(const std::string& value) -> std::string
	{
		const auto begin = value.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
		{
			return {};
		}

		const auto end = value.find_last_not_of(" \t\r\n");
		return value.substr(begin, end - begin + 1);
	}

	auto Lower(const std::string& value) -> std::string
	{
		std::string lowered;
		lowered.reserve(value.size());
		for (const unsigned char ch : value)
		{
			lowered.push_back(static_cast<char>(std::tolower(ch)));
		}
		return lowered;
	}

	auto NormalizeToken(const std::string& value) -> std::string
	{
		std::string lowered = Lower(value);
		lowered.erase(std::remove_if(lowered.begin(), lowered.end(), [](unsigned char ch) {
			return !std::isalnum(ch);
		}), lowered.end());
		return lowered;
	}

	auto StartsWithInsensitive(const std::string& value, const std::string& prefix) -> bool
	{
		if (prefix.size() > value.size())
		{
			return false;
		}

		for (std::size_t index = 0; index < prefix.size(); ++index)
		{
			if (std::tolower(static_cast<unsigned char>(value[index])) != std::tolower(static_cast<unsigned char>(prefix[index])))
			{
				return false;
			}
		}

		return true;
	}

	auto NormalizeObjectPath(const std::string& objectPath) -> std::string
	{
		if (objectPath.empty())
		{
			return {};
		}

		std::string normalized = Trim(objectPath);
		const auto slashIndex = normalized.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = normalized.find('.', nameStart);
		if (dotIndex == std::string::npos)
		{
			const auto assetName = normalized.substr(nameStart);
			if (!assetName.empty())
			{
				normalized += "." + assetName;
			}
		}

		return normalized;
	}

	auto IsExplicitObjectPath(const std::string& configuredValue) -> bool
	{
		const std::string trimmed = Trim(configuredValue);
		return StartsWithInsensitive(trimmed, "/Game/") || StartsWithInsensitive(trimmed, "/Script/");
	}

	auto ExtractObjectPath(const std::wstring& fullName) -> std::string
	{
		const std::string narrow = ToNarrowAscii(fullName);
		const auto gameIndex = narrow.find("/Game/");
		if (gameIndex == std::string::npos)
		{
			return {};
		}

		return NormalizeObjectPath(narrow.substr(gameIndex));
	}

	auto ExtractObjectName(const std::string& objectPath) -> std::string
	{
		const auto slashIndex = objectPath.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = objectPath.find('.', nameStart);
		if (dotIndex == std::string::npos)
		{
			return objectPath.substr(nameStart);
		}
		return objectPath.substr(dotIndex + 1);
	}

	auto ExtractAssetName(const std::string& objectPath) -> std::string
	{
		const auto slashIndex = objectPath.find_last_of('/');
		const auto nameStart = (slashIndex == std::string::npos) ? 0 : slashIndex + 1;
		const auto dotIndex = objectPath.find('.', nameStart);
		if (dotIndex == std::string::npos)
		{
			return objectPath.substr(nameStart);
		}
		return objectPath.substr(nameStart, dotIndex - nameStart);
	}

	auto BuildUnderscoreAlias(const std::string& assetName) -> std::string
	{
		if (assetName.empty())
		{
			return {};
		}

		const std::string lowered = Lower(assetName);
		const auto firstUnderscore = lowered.find('_');
		const auto lastUnderscore = lowered.find_last_of('_');
		if (firstUnderscore == std::string::npos || lastUnderscore == std::string::npos || lastUnderscore <= firstUnderscore + 1)
		{
			return NormalizeToken(lowered);
		}

		return NormalizeToken(lowered.substr(firstUnderscore + 1, lastUnderscore - firstUnderscore - 1));
	}

	auto BuildBaseName(const std::string& objectName) -> std::string
	{
		std::string lowered = Lower(objectName);
		if (lowered.size() > 2 && lowered.compare(lowered.size() - 2, 2, "_c") == 0)
		{
			lowered.resize(lowered.size() - 2);
		}
		if (StartsWithInsensitive(lowered, "bp_"))
		{
			lowered.erase(0, 3);
		}
		if (StartsWithInsensitive(lowered, "da_"))
		{
			lowered.erase(0, 3);
		}
		const std::string itemTypeSuffix = "_itemtype";
		if (lowered.size() > itemTypeSuffix.size() &&
			lowered.compare(lowered.size() - itemTypeSuffix.size(), itemTypeSuffix.size(), itemTypeSuffix) == 0)
		{
			lowered.resize(lowered.size() - itemTypeSuffix.size());
		}
		return NormalizeToken(lowered);
	}

	auto AddCandidate(std::vector<CandidatePath>& storage, const std::string& objectPath) -> void
	{
		if (objectPath.empty())
		{
			return;
		}

		for (const auto& existing : storage)
		{
			if (existing.objectPath == objectPath)
			{
				return;
			}
		}

		CandidatePath candidate{};
		candidate.objectPath = objectPath;
		candidate.loweredPath = Lower(objectPath);
		candidate.loweredAssetName = Lower(ExtractAssetName(objectPath));
		candidate.loweredObjectName = Lower(ExtractObjectName(objectPath));
		candidate.loweredUnderscoreAlias = BuildUnderscoreAlias(candidate.loweredAssetName);
		candidate.loweredBaseName = BuildBaseName(candidate.loweredObjectName);
		storage.emplace_back(std::move(candidate));
	}

	auto EnsureCandidateCache() -> void
	{
		if (s_cacheBuilt)
		{
			return;
		}

		s_inputCandidates.clear();
		s_outputCandidates.clear();

		UObjectGlobals::ForEachUObject([](UObject* object, int32, int32) {
			if (!object)
			{
				return RC::LoopAction::Continue;
			}

			const auto objectPath = ExtractObjectPath(object->GetFullName());
			if (objectPath.empty())
			{
				return RC::LoopAction::Continue;
			}

			if (StartsWithInsensitive(objectPath, kInputRoot))
			{
				AddCandidate(s_inputCandidates, objectPath);
			}
			if (StartsWithInsensitive(objectPath, kOutputRoot))
			{
				AddCandidate(s_outputCandidates, objectPath);
			}

			return RC::LoopAction::Continue;
		});

		s_cacheBuilt = true;
	}

	auto ScoreCandidate(const CandidatePath& candidate, const std::string& tokenNormalized) -> int
	{
		if (tokenNormalized.empty())
		{
			return std::numeric_limits<int>::min();
		}

		if (candidate.loweredUnderscoreAlias == tokenNormalized)
		{
			return 1300;
		}

		if (candidate.loweredBaseName == tokenNormalized)
		{
			return 1000;
		}
		if (NormalizeToken(candidate.loweredAssetName) == tokenNormalized)
		{
			return 975;
		}
		if (candidate.loweredObjectName == tokenNormalized)
		{
			return 950;
		}

		int score = std::numeric_limits<int>::min();
		if (candidate.loweredBaseName.find(tokenNormalized) != std::string::npos)
		{
			score = std::max(score, 600);
		}
		if (candidate.loweredObjectName.find(tokenNormalized) != std::string::npos)
		{
			score = std::max(score, 500);
		}
		if (candidate.loweredPath.find(tokenNormalized) != std::string::npos)
		{
			score = std::max(score, 300);
		}

		return score;
	}

	auto ResolveFriendlyToken(const std::string& configuredValue, PathKind kind) -> ResolveResult
	{
		EnsureCandidateCache();

		const auto trimmed = Trim(configuredValue);
		const auto tokenNormalized = NormalizeToken(trimmed);
		if (tokenNormalized.empty())
		{
			return {};
		}

		const auto& candidates = (kind == PathKind::InputItem) ? s_inputCandidates : s_outputCandidates;
		ResolveResult result{};
		for (const auto& candidate : candidates)
		{
			const int score = ScoreCandidate(candidate, tokenNormalized);
			if (score <= std::numeric_limits<int>::min() / 2)
			{
				continue;
			}

			if (score > result.bestScore)
			{
				result.bestScore = score;
				result.objectPath = candidate.objectPath;
				result.matchCount = 1;
			}
			else if (score == result.bestScore)
			{
				result.matchCount++;
				if (candidate.objectPath.size() < result.objectPath.size() ||
					(candidate.objectPath.size() == result.objectPath.size() && candidate.objectPath < result.objectPath))
				{
					result.objectPath = candidate.objectPath;
				}
			}
		}

		return result;
	}

	auto KeepWordAndUnderscoreChars(const std::string& value) -> std::string
	{
		std::string out;
		out.reserve(value.size());
		for (const unsigned char ch : value)
		{
			if (std::isalnum(ch) || ch == '_')
			{
				out.push_back(static_cast<char>(ch));
			}
		}
		return out;
	}

	auto TryResolveExistingObjectPath(const std::string& objectPath) -> bool
	{
		if (objectPath.empty())
		{
			return false;
		}

		const auto widePath = ToWide(objectPath);
		return UObjectGlobals::StaticFindObject(nullptr, nullptr, widePath.c_str()) != nullptr;
	}

	auto BuildHeuristicPathCandidates(const std::string& configuredValue, PathKind kind) -> std::vector<std::string>
	{
		std::vector<std::string> candidates;

		const std::string token = KeepWordAndUnderscoreChars(Trim(configuredValue));
		if (token.empty())
		{
			return candidates;
		}

		auto addCandidate = [&candidates](const std::string& path) -> void
		{
			if (path.empty())
			{
				return;
			}
			for (const auto& existing : candidates)
			{
				if (existing == path)
				{
					return;
				}
			}
			candidates.emplace_back(path);
		};

		if (kind == PathKind::InputItem)
		{
			const std::array<std::string, 6> roots{
				"/Game/Data/ItemType/Equipment/",
				"/Game/Data/ItemType/Tools/",
				"/Game/Data/ItemType/Resource/",
				"/Game/Data/ItemType/Consumable/",
				"/Game/Data/ItemType/Ingredients/",
				"/Game/Data/ItemType/"};

			const std::array<std::string, 3> stems{
				"DA_" + token + "_ItemType",
				"DA_" + token + "_EquippableItemType",
				"DA_" + token};

			for (const auto& root : roots)
			{
				for (const auto& stem : stems)
				{
					addCandidate(root + stem + "." + stem + "_C");
					addCandidate(root + stem + "." + stem);
				}
			}
		}
		else
		{
			const std::array<std::string, 5> roots{
				"/Game/Blueprints/Items/Resources/",
				"/Game/Blueprints/Items/Equipment/",
				"/Game/Blueprints/Items/Tools/",
				"/Game/Blueprints/Items/Crafting/",
				"/Game/Blueprints/Items/"};

			const std::array<std::string, 3> stems{
				"BP_" + token,
				"BP_Resource_" + token,
				token};

			for (const auto& root : roots)
			{
				for (const auto& stem : stems)
				{
					addCandidate(root + stem + "." + stem + "_C");
					addCandidate(root + stem + "." + stem);
				}
			}
		}

		return candidates;
	}

	auto ResolveHeuristicPath(const std::string& configuredValue, PathKind kind) -> HeuristicResolveResult
	{
		HeuristicResolveResult result{};
		const auto candidates = BuildHeuristicPathCandidates(configuredValue, kind);
		for (const auto& candidate : candidates)
		{
			if (TryResolveExistingObjectPath(candidate))
			{
				result.objectPath = candidate;
				result.foundLoadedObject = true;
				return result;
			}
		}

		if (!candidates.empty())
		{
			result.objectPath = candidates.front();
		}

		return result;
	}

	auto ResolveConfiguredValue(const std::string& configuredValue, PathKind kind, const std::string& contextLabel) -> std::string
	{
		if (configuredValue.empty())
		{
			return configuredValue;
		}

		if (IsExplicitObjectPath(configuredValue))
		{
			return NormalizeObjectPath(configuredValue);
		}

		const auto resolved = ResolveFriendlyToken(configuredValue, kind);
		if (resolved.objectPath.empty())
		{
			const auto heuristicResult = ResolveHeuristicPath(configuredValue, kind);
			if (!heuristicResult.objectPath.empty())
			{
				if (!heuristicResult.foundLoadedObject)
				{
					PCL_WarnLog("Friendly path resolver could not resolve '{}' for {}. Heuristic guess '{}' was not found as a loaded object.",
					            ToWide(configuredValue),
					            ToWide(contextLabel),
					            ToWide(heuristicResult.objectPath));
				}

				return heuristicResult.objectPath;
			}

			PCL_WarnLog("Friendly path resolver could not resolve '{}' for {}. Keep using full object path for this entry.",
			            ToWide(configuredValue),
			            ToWide(contextLabel));
			return configuredValue;
		}

		return resolved.objectPath;
	}
}

namespace MFM::Loader::Config
{
	auto FriendlyPathResolver::ResolveInputItemPath(const std::string& configuredValue, const std::string& entryId) -> std::string
	{
		return ResolveConfiguredValue(configuredValue, PathKind::InputItem, "inputItemPath entry=" + entryId);
	}

	auto FriendlyPathResolver::ResolveOutputResourceClassPath(const std::string& configuredValue, const std::string& entryId, std::size_t outputIndex) -> std::string
	{
		return ResolveConfiguredValue(
			configuredValue,
			PathKind::OutputResource,
			"outputs[" + std::to_string(outputIndex) + "].resourceClassPath entry=" + entryId);
	}
}
