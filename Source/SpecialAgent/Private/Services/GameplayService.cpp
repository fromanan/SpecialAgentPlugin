// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/GameplayService.h"
#include "Services/PythonService.h"
#include "GameThreadDispatcher.h"

FGameplayService::FGameplayService()
{
}

FString FGameplayService::GetServiceDescription() const
{
	return TEXT("Gameplay actor management - spawn trigger volumes, player starts, and game logic actors");
}

// Helper function to execute Python code from request params
FMCPResponse FGameplayService::ExecutePythonFromParams(const FMCPRequest& Request)
{
	if (!Request.Params.IsValid())
	{
		return InvalidParams(Request.Id, TEXT("Missing required parameter: 'code' (Python script)"));
	}

	FString Code;
	if (!Request.Params->TryGetStringField(TEXT("code"), Code))
	{
		return InvalidParams(Request.Id, TEXT("Missing required parameter: 'code' (Python script)"));
	}

	float Timeout = 30.0f;
	Request.Params->TryGetNumberField(TEXT("timeout"), Timeout);

	FPythonService PythonService;
	TSharedPtr<FJsonObject> PythonParams = MakeShared<FJsonObject>();
	PythonParams->SetStringField(TEXT("code"), Code);
	PythonParams->SetNumberField(TEXT("timeout"), Timeout);
	
	FMCPRequest PythonRequest;
	PythonRequest.JsonRpc = Request.JsonRpc;
	PythonRequest.Id = Request.Id;
	PythonRequest.Method = TEXT("python/execute");
	PythonRequest.Params = PythonParams;
	
	return PythonService.HandleExecute(PythonRequest);
}

FMCPResponse FGameplayService::HandleRequest(const FMCPRequest& Request, const FString& MethodName)
{
	if (MethodName == TEXT("spawn_trigger_volume")) return HandleSpawnTriggerVolume(Request);
	if (MethodName == TEXT("spawn_player_start")) return HandleSpawnPlayerStart(Request);

	return MethodNotFound(Request.Id, TEXT("gameplay"), MethodName);
}

FMCPResponse FGameplayService::HandleSpawnTriggerVolume(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}

FMCPResponse FGameplayService::HandleSpawnPlayerStart(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}


TArray<FMCPToolInfo> FGameplayService::GetAvailableTools() const
{
	TArray<FMCPToolInfo> Tools;
	// TODO: Add tool definitions
	return Tools;
}
