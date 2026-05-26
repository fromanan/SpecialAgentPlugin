// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/StreamingService.h"
#include "Services/PythonService.h"
#include "GameThreadDispatcher.h"

FStreamingService::FStreamingService()
{
}

FString FStreamingService::GetServiceDescription() const
{
	return TEXT("Level streaming management - load, unload, and control level visibility");
}

// Helper function to execute Python code from request params
FMCPResponse FStreamingService::ExecutePythonFromParams(const FMCPRequest& Request)
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

FMCPResponse FStreamingService::HandleRequest(const FMCPRequest& Request, const FString& MethodName)
{
	if (MethodName == TEXT("list_levels")) return HandleListLevels(Request);
	if (MethodName == TEXT("load_level")) return HandleLoadLevel(Request);
	if (MethodName == TEXT("unload_level")) return HandleUnloadLevel(Request);
	if (MethodName == TEXT("set_level_visibility")) return HandleSetLevelVisibility(Request);

	return MethodNotFound(Request.Id, TEXT("streaming"), MethodName);
}

FMCPResponse FStreamingService::HandleListLevels(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}

FMCPResponse FStreamingService::HandleLoadLevel(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}

FMCPResponse FStreamingService::HandleUnloadLevel(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}

FMCPResponse FStreamingService::HandleSetLevelVisibility(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}


TArray<FMCPToolInfo> FStreamingService::GetAvailableTools() const
{
	TArray<FMCPToolInfo> Tools;
	// TODO: Add tool definitions
	return Tools;
}
