// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct SPECIALAGENT_API FSpecialAgentSettings
{
	bool bServerEnabled = true;
	int32 ServerPort = 8767;
	bool bLoopbackOnly = true;
	bool bRequireAuthToken = false;
	FString AuthToken;
	TArray<FString> AllowedOrigins;

	bool bPythonExecutionEnabled = true;
	bool bPythonSandboxEnabled = true;
	float PythonExecutionTimeout = 30.0f;
	TSet<FString> AllowedPythonModules;

	int32 DefaultScreenshotWidth = 1280;
	int32 DefaultScreenshotHeight = 720;
	int32 MaxScreenshotWidth = 4096;
	int32 MaxScreenshotHeight = 4096;

	static FSpecialAgentSettings Load();

	bool IsOriginAllowed(const FString& Origin) const;
	bool IsPythonModuleAllowed(const FString& ModuleName) const;
};
