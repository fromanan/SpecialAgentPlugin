// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/NavigationService.h"
#include "Services/PythonService.h"
#include "GameThreadDispatcher.h"

FNavigationService::FNavigationService()
{
}

FString FNavigationService::GetServiceDescription() const
{
	return TEXT("Navigation mesh management - rebuild navmesh and test pathfinding");
}

// Helper function to execute Python code from request params
FMCPResponse FNavigationService::ExecutePythonFromParams(const FMCPRequest& Request)
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

FMCPResponse FNavigationService::HandleRequest(const FMCPRequest& Request, const FString& MethodName)
{
	if (MethodName == TEXT("rebuild_navmesh")) return HandleRebuildNavMesh(Request);
	if (MethodName == TEXT("test_path")) return HandleTestPath(Request);

	return MethodNotFound(Request.Id, TEXT("navigation"), MethodName);
}

FMCPResponse FNavigationService::HandleRebuildNavMesh(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}

FMCPResponse FNavigationService::HandleTestPath(const FMCPRequest& Request)
{
	return ExecutePythonFromParams(Request);
}


TArray<FMCPToolInfo> FNavigationService::GetAvailableTools() const
{
	TArray<FMCPToolInfo> Tools;
	// TODO: Add tool definitions
	return Tools;
}
