// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpecialAgentSettings.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
	const TCHAR* SpecialAgentConfigSection = TEXT("/Script/SpecialAgent.SpecialAgentSettings");

	void ParseCommaList(const FString& Source, TArray<FString>& OutValues)
	{
		OutValues.Reset();

		TArray<FString> Parts;
		Source.ParseIntoArray(Parts, TEXT(","), true);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty())
			{
				OutValues.Add(Part);
			}
		}
	}

	void ParseCommaSet(const FString& Source, TSet<FString>& OutValues)
	{
		OutValues.Reset();

		TArray<FString> Parts;
		Source.ParseIntoArray(Parts, TEXT(","), true);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty())
			{
				OutValues.Add(Part.ToLower());
			}
		}
	}

	void ApplyConfigFile(const FConfigFile& ConfigFile, FSpecialAgentSettings& Settings)
	{
		ConfigFile.GetBool(SpecialAgentConfigSection, TEXT("ServerEnabled"), Settings.bServerEnabled);
		ConfigFile.GetInt(SpecialAgentConfigSection, TEXT("ServerPort"), Settings.ServerPort);
		ConfigFile.GetBool(SpecialAgentConfigSection, TEXT("LoopbackOnly"), Settings.bLoopbackOnly);
		ConfigFile.GetBool(SpecialAgentConfigSection, TEXT("RequireAuthToken"), Settings.bRequireAuthToken);
		ConfigFile.GetString(SpecialAgentConfigSection, TEXT("AuthToken"), Settings.AuthToken);

		FString AllowedOrigins;
		if (ConfigFile.GetString(SpecialAgentConfigSection, TEXT("AllowedOrigins"), AllowedOrigins))
		{
			ParseCommaList(AllowedOrigins, Settings.AllowedOrigins);
		}

		ConfigFile.GetBool(SpecialAgentConfigSection, TEXT("PythonExecutionEnabled"), Settings.bPythonExecutionEnabled);
		ConfigFile.GetBool(SpecialAgentConfigSection, TEXT("PythonSandboxEnabled"), Settings.bPythonSandboxEnabled);
		ConfigFile.GetFloat(SpecialAgentConfigSection, TEXT("PythonExecutionTimeout"), Settings.PythonExecutionTimeout);

		FString AllowedPythonModules;
		if (ConfigFile.GetString(SpecialAgentConfigSection, TEXT("AllowedPythonModules"), AllowedPythonModules))
		{
			ParseCommaSet(AllowedPythonModules, Settings.AllowedPythonModules);
		}

		ConfigFile.GetInt(SpecialAgentConfigSection, TEXT("DefaultScreenshotWidth"), Settings.DefaultScreenshotWidth);
		ConfigFile.GetInt(SpecialAgentConfigSection, TEXT("DefaultScreenshotHeight"), Settings.DefaultScreenshotHeight);
		ConfigFile.GetInt(SpecialAgentConfigSection, TEXT("MaxScreenshotWidth"), Settings.MaxScreenshotWidth);
		ConfigFile.GetInt(SpecialAgentConfigSection, TEXT("MaxScreenshotHeight"), Settings.MaxScreenshotHeight);
	}

	void ApplyGConfig(FSpecialAgentSettings& Settings)
	{
		if (!GConfig)
		{
			return;
		}

		GConfig->GetBool(SpecialAgentConfigSection, TEXT("ServerEnabled"), Settings.bServerEnabled, GGameIni);
		GConfig->GetInt(SpecialAgentConfigSection, TEXT("ServerPort"), Settings.ServerPort, GGameIni);
		GConfig->GetBool(SpecialAgentConfigSection, TEXT("LoopbackOnly"), Settings.bLoopbackOnly, GGameIni);
		GConfig->GetBool(SpecialAgentConfigSection, TEXT("RequireAuthToken"), Settings.bRequireAuthToken, GGameIni);
		GConfig->GetString(SpecialAgentConfigSection, TEXT("AuthToken"), Settings.AuthToken, GGameIni);

		FString AllowedOrigins;
		if (GConfig->GetString(SpecialAgentConfigSection, TEXT("AllowedOrigins"), AllowedOrigins, GGameIni))
		{
			ParseCommaList(AllowedOrigins, Settings.AllowedOrigins);
		}

		GConfig->GetBool(SpecialAgentConfigSection, TEXT("PythonExecutionEnabled"), Settings.bPythonExecutionEnabled, GGameIni);
		GConfig->GetBool(SpecialAgentConfigSection, TEXT("PythonSandboxEnabled"), Settings.bPythonSandboxEnabled, GGameIni);
		GConfig->GetFloat(SpecialAgentConfigSection, TEXT("PythonExecutionTimeout"), Settings.PythonExecutionTimeout, GGameIni);

		FString AllowedPythonModules;
		if (GConfig->GetString(SpecialAgentConfigSection, TEXT("AllowedPythonModules"), AllowedPythonModules, GGameIni))
		{
			ParseCommaSet(AllowedPythonModules, Settings.AllowedPythonModules);
		}

		GConfig->GetInt(SpecialAgentConfigSection, TEXT("DefaultScreenshotWidth"), Settings.DefaultScreenshotWidth, GGameIni);
		GConfig->GetInt(SpecialAgentConfigSection, TEXT("DefaultScreenshotHeight"), Settings.DefaultScreenshotHeight, GGameIni);
		GConfig->GetInt(SpecialAgentConfigSection, TEXT("MaxScreenshotWidth"), Settings.MaxScreenshotWidth, GGameIni);
		GConfig->GetInt(SpecialAgentConfigSection, TEXT("MaxScreenshotHeight"), Settings.MaxScreenshotHeight, GGameIni);
	}
}

FSpecialAgentSettings FSpecialAgentSettings::Load()
{
	FSpecialAgentSettings Settings;

	Settings.AllowedOrigins.Add(TEXT("http://localhost"));
	Settings.AllowedOrigins.Add(TEXT("http://127.0.0.1"));
	Settings.AllowedOrigins.Add(TEXT("http://[::1]"));

	Settings.AllowedPythonModules.Add(TEXT("unreal"));
	Settings.AllowedPythonModules.Add(TEXT("math"));
	Settings.AllowedPythonModules.Add(TEXT("json"));
	Settings.AllowedPythonModules.Add(TEXT("random"));
	Settings.AllowedPythonModules.Add(TEXT("sys"));
	Settings.AllowedPythonModules.Add(TEXT("io"));
	Settings.AllowedPythonModules.Add(TEXT("traceback"));

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SpecialAgent"));
	if (Plugin.IsValid())
	{
		const FString ConfigPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("DefaultSpecialAgent.ini"));
		FConfigFile PluginConfig;
		PluginConfig.Read(ConfigPath);
		ApplyConfigFile(PluginConfig, Settings);
	}

	ApplyGConfig(Settings);

	Settings.ServerPort = FMath::Clamp(Settings.ServerPort, 1, 65535);
	Settings.PythonExecutionTimeout = FMath::Max(Settings.PythonExecutionTimeout, 0.1f);
	Settings.MaxScreenshotWidth = FMath::Max(Settings.MaxScreenshotWidth, 64);
	Settings.MaxScreenshotHeight = FMath::Max(Settings.MaxScreenshotHeight, 64);
	Settings.DefaultScreenshotWidth = FMath::Clamp(Settings.DefaultScreenshotWidth, 64, Settings.MaxScreenshotWidth);
	Settings.DefaultScreenshotHeight = FMath::Clamp(Settings.DefaultScreenshotHeight, 64, Settings.MaxScreenshotHeight);

	return Settings;
}

bool FSpecialAgentSettings::IsOriginAllowed(const FString& Origin) const
{
	if (Origin.IsEmpty())
	{
		return true;
	}

	for (const FString& AllowedOrigin : AllowedOrigins)
	{
		if (!AllowedOrigin.IsEmpty() && Origin.StartsWith(AllowedOrigin, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

bool FSpecialAgentSettings::IsPythonModuleAllowed(const FString& ModuleName) const
{
	if (ModuleName.IsEmpty())
	{
		return true;
	}

	FString RootModule;
	FString Remainder;
	if (!ModuleName.Split(TEXT("."), &RootModule, &Remainder))
	{
		RootModule = ModuleName;
	}

	return AllowedPythonModules.Contains(RootModule.ToLower());
}
