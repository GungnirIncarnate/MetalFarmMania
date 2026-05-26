#pragma once

#include <DynamicOutput/Output.hpp>

#define PCL_PREFIX STR("[MetalFarmMania] ")

#define PCL_Log(fmt, ...) \
	RC::Output::send<RC::LogLevel::Normal>(PCL_PREFIX STR(fmt) STR("\n") __VA_OPT__(,) __VA_ARGS__)

#define PCL_WarnLog(fmt, ...) \
	RC::Output::send<RC::LogLevel::Warning>(PCL_PREFIX STR(fmt) STR("\n") __VA_OPT__(,) __VA_ARGS__)

#define PCL_ErrorLog(fmt, ...) \
	RC::Output::send<RC::LogLevel::Error>(PCL_PREFIX STR(fmt) STR("\n") __VA_OPT__(,) __VA_ARGS__)

#define PCL_VerboseLog(fmt, ...) \
	RC::Output::send<RC::LogLevel::Verbose>(PCL_PREFIX STR(fmt) STR("\n") __VA_OPT__(,) __VA_ARGS__)
