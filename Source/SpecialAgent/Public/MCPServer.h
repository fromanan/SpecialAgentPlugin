// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Containers/Queue.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "SpecialAgentSettings.h"

class FMCPRequestRouter;
struct FMCPRequest;
struct FMCPResponse;

/**
 * SSE Client Connection
 * Represents an active Server-Sent Events connection
 */
struct FSSEConnection
{
	FString SessionId;
	TSharedPtr<FHttpServerResponse> Response;
	FDateTime ConnectedTime;
	bool bIsValid;

	FSSEConnection()
		: bIsValid(false)
	{}
};

/**
 * MCP Server Implementation
 * 
 * Implements the Model Context Protocol with native streamable HTTP transport.
 * Handles incoming requests from MCP clients (like Cursor) and routes them to appropriate services.
 */
class SPECIALAGENT_API FSpecialAgentMCPServer
{
public:
	FSpecialAgentMCPServer();
	~FSpecialAgentMCPServer();

	/**
	 * Start the MCP HTTP server on the specified port
	 * @param Port The port to listen on (default 8767)
	 * @return true if server started successfully
	 */
	bool StartServer(int32 Port = 8767);

	/**
	 * Start the MCP HTTP server using resolved SpecialAgent settings
	 */
	bool StartServer(const FSpecialAgentSettings& InSettings);

	/**
	 * Stop the MCP server
	 */
	void StopServer();

	/**
	 * Check if the server is running
	 */
	bool IsRunning() const { return bIsRunning; }

	/**
	 * Get the request router
	 */
	TSharedPtr<FMCPRequestRouter> GetRouter() const { return RequestRouter; }

	/**
	 * Get the active server port
	 */
	int32 GetPort() const { return ServerPort; }

	/**
	 * Get the streamable HTTP endpoint URL
	 */
	FString GetMcpEndpointUrl() const;

	/**
	 * Get number of recently connected clients (based on recent request activity)
	 */
	int32 GetConnectedClientCount() const;

	/**
	 * Record a client request (called internally to track activity)
	 */
	void RecordClientActivity();

private:
	/** Return a clear unsupported response for legacy SSE connection requests (GET /sse) */
	bool HandleSSEConnection(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle MCP message request (POST /message) */
	bool HandleMessage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle health check (GET /health) */
	bool HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Handle CORS preflight (OPTIONS) */
	bool HandleCORS(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	/** Parse JSON-RPC request from body */
	bool ParseRequest(const FString& JsonString, FMCPRequest& OutRequest);

	/** Format JSON-RPC response */
	static FString FormatResponse(const FMCPResponse& Response);

	/** Add standard CORS/cache headers to a response */
	void AddStandardHeaders(FHttpServerResponse& Response, const FString& CorsOrigin) const;

	/** Return an allowed CORS origin for this request, or empty if no Origin was provided */
	FString GetAllowedCorsOrigin(const FHttpServerRequest& Request) const;

	/** Check origin, loopback, and optional token policy before handling a request */
	bool AuthorizeRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, bool bRequireToken) const;

	/** Create and complete an HTTP error response */
	bool CompleteWithError(const FHttpResultCallback& OnComplete, EHttpServerResponseCodes Code, const FString& Message, const FString& CorsOrigin) const;

	/** Send SSE event to a specific session */
	void SendSSEEvent(const FString& SessionId, const FString& EventType, const FString& Data);

	/** Send SSE event to all connected clients */
	void BroadcastSSEEvent(const FString& EventType, const FString& Data);

	/** Generate unique session ID */
	FString GenerateSessionId();

	/** Clean up stale connections */
	void CleanupConnections();

private:
	/** HTTP router */
	TSharedPtr<IHttpRouter> HttpRouter;

	/** Route handles for cleanup */
	TArray<FHttpRouteHandle> RouteHandles;

	/** Request router */
	TSharedPtr<FMCPRequestRouter> RequestRouter;

	/** Active SSE connections */
	TMap<FString, TSharedPtr<FSSEConnection>> SSEConnections;

	/** Pending responses to send via SSE */
	TQueue<TPair<FString, FString>> PendingSSEResponses;

	/** Server running flag */
	bool bIsRunning;

	/** Server port */
	int32 ServerPort;

	/** Resolved runtime settings */
	FSpecialAgentSettings Settings;

	/** Critical section for thread safety */
	FCriticalSection ConnectionsLock;

	/** Last time we received a request from a client */
	FDateTime LastClientActivity;

	/** Consider client "connected" if activity within this many seconds */
	static constexpr double ClientActivityTimeoutSeconds = 30.0;
};


/**
 * MCP Request Structure
 * 
 * Represents a JSON-RPC 2.0 request
 */
struct FMCPRequest
{
	FString JsonRpc;  // Should be "2.0"
	FString Method;
	TSharedPtr<FJsonObject> Params;
	FString Id;  // Can be string or number
	TSharedPtr<FJsonValue> IdValue;
	bool bHasId;
	bool bIsNotification;

	FMCPRequest()
		: JsonRpc(TEXT("2.0"))
		, bHasId(false)
		, bIsNotification(true)
	{}
};


/**
 * MCP Response Structure
 * 
 * Represents a JSON-RPC 2.0 response
 */
struct FMCPResponse
{
	FString JsonRpc;  // Should be "2.0"
	TSharedPtr<FJsonObject> Result;
	TSharedPtr<FJsonObject> ErrorObject;
	FString Id;
	TSharedPtr<FJsonValue> IdValue;
	bool bHasId;

	bool bSuccess;

	FMCPResponse()
		: JsonRpc(TEXT("2.0"))
		, bHasId(false)
		, bSuccess(true)
	{}

	/** Create success response */
	static FMCPResponse Success(const FString& InId, TSharedPtr<FJsonObject> InResult)
	{
		FMCPResponse Response;
		Response.Id = InId;
		Response.bHasId = true;
		Response.IdValue = MakeShared<FJsonValueString>(InId);
		Response.Result = InResult;
		Response.bSuccess = true;
		return Response;
	}

	/** Create error response */
	static FMCPResponse Error(const FString& InId, int32 ErrorCode, const FString& ErrorMessage, TSharedPtr<FJsonObject> ErrorData = nullptr)
	{
		FMCPResponse Response;
		Response.Id = InId;
		Response.bHasId = true;
		Response.IdValue = MakeShared<FJsonValueString>(InId);
		Response.bSuccess = false;

		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetNumberField(TEXT("code"), ErrorCode);
		ErrorObj->SetStringField(TEXT("message"), ErrorMessage);
		if (ErrorData.IsValid())
		{
			ErrorObj->SetObjectField(TEXT("data"), ErrorData);
		}

		Response.ErrorObject = ErrorObj;
		return Response;
	}
};
