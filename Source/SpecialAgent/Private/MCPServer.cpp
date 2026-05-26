// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPServer.h"
#include "MCPRequestRouter.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Guid.h"
#include "Async/Async.h"
#include "IPAddress.h"

FSpecialAgentMCPServer::FSpecialAgentMCPServer()
	: bIsRunning(false)
	, ServerPort(8767)
	, LastClientActivity(FDateTime::MinValue())
{
	Settings = FSpecialAgentSettings::Load();
	RequestRouter = MakeShared<FMCPRequestRouter>();
}

FSpecialAgentMCPServer::~FSpecialAgentMCPServer()
{
	StopServer();
}

bool FSpecialAgentMCPServer::StartServer(int32 Port)
{
	FSpecialAgentSettings LocalSettings = FSpecialAgentSettings::Load();
	LocalSettings.ServerPort = Port;
	return StartServer(LocalSettings);
}

bool FSpecialAgentMCPServer::StartServer(const FSpecialAgentSettings& InSettings)
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpecialAgent: MCP Server is already running"));
		return false;
	}

	Settings = InSettings;
	ServerPort = Settings.ServerPort;
	RouteHandles.Reset();

	// Get the HTTP server module
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	
	// Start listeners on the specified port
	HttpServerModule.StartAllListeners();
	
	// Get the HTTP router for our port
	HttpRouter = HttpServerModule.GetHttpRouter(ServerPort);
	
	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("SpecialAgent: Failed to get HTTP router for port %d"), ServerPort);
		return false;
	}

	auto BindRoute = [this](const TCHAR* Path, EHttpServerRequestVerbs Verb, FHttpRequestHandler Handler) -> bool
	{
		FHttpRouteHandle Handle = HttpRouter->BindRoute(FHttpPath(Path), Verb, Handler);
		if (!Handle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SpecialAgent: Failed to bind route %s"), Path);
			return false;
		}

		RouteHandles.Add(Handle);
		return true;
	};

	bool bRoutesBound = true;

	// Register MCP endpoint (POST /mcp) - Main streamable HTTP endpoint
	bRoutesBound &= BindRoute(
		TEXT("/mcp"),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleMessage));

	// Register legacy SSE endpoint with a clear unsupported response.
	bRoutesBound &= BindRoute(
		TEXT("/sse"),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleSSEConnection));
	
	// Keep POST /sse as a compatibility alias for streamable HTTP clients that were configured before /mcp.
	bRoutesBound &= BindRoute(
		TEXT("/sse"),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleMessage));

	// Register message endpoint (POST /message)
	bRoutesBound &= BindRoute(
		TEXT("/message"),
		EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleMessage));

	// Register health endpoint (GET /health)
	bRoutesBound &= BindRoute(
		TEXT("/health"),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleHealth));

	// Register OPTIONS handlers for CORS preflight on all endpoints
	bRoutesBound &= BindRoute(
		TEXT("/mcp"),
		EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleCORS));
	
	bRoutesBound &= BindRoute(
		TEXT("/sse"),
		EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleCORS));
	
	bRoutesBound &= BindRoute(
		TEXT("/message"),
		EHttpServerRequestVerbs::VERB_OPTIONS,
		FHttpRequestHandler::CreateRaw(this, &FSpecialAgentMCPServer::HandleCORS));

	if (!bRoutesBound)
	{
		for (const FHttpRouteHandle& RouteHandle : RouteHandles)
		{
			HttpRouter->UnbindRoute(RouteHandle);
		}
		RouteHandles.Reset();
		return false;
	}

	bIsRunning = true;
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP HTTP Server started on port %d"), ServerPort);
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP endpoint: %s"), *GetMcpEndpointUrl());
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: SSE GET endpoint is disabled; use streamable HTTP"));
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Message endpoint: http://localhost:%d/message"), ServerPort);
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Health endpoint: http://localhost:%d/health"), ServerPort);

	return true;
}

void FSpecialAgentMCPServer::StopServer()
{
	if (!bIsRunning)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP Server stopping"));

	// Unbind routes
	if (HttpRouter.IsValid())
	{
		for (const FHttpRouteHandle& RouteHandle : RouteHandles)
		{
			if (RouteHandle.IsValid())
			{
				HttpRouter->UnbindRoute(RouteHandle);
			}
		}
	}
	RouteHandles.Reset();

	// Clear connections
	{
		FScopeLock Lock(&ConnectionsLock);
		SSEConnections.Empty();
	}

	bIsRunning = false;
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP Server stopped"));
}

FString FSpecialAgentMCPServer::GetMcpEndpointUrl() const
{
	return FString::Printf(TEXT("http://localhost:%d/mcp"), ServerPort);
}

FString FSpecialAgentMCPServer::GenerateSessionId()
{
	return FGuid::NewGuid().ToString();
}

void FSpecialAgentMCPServer::AddStandardHeaders(FHttpServerResponse& Response, const FString& CorsOrigin) const
{
	if (!CorsOrigin.IsEmpty())
	{
		Response.Headers.Add(TEXT("Access-Control-Allow-Origin"), { CorsOrigin });
		Response.Headers.Add(TEXT("Vary"), { TEXT("Origin") });
	}

	Response.Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
	Response.Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept, Authorization, X-SpecialAgent-Token") });
}

FString FSpecialAgentMCPServer::GetAllowedCorsOrigin(const FHttpServerRequest& Request) const
{
	auto FindHeader = [&Request](const FString& HeaderName) -> FString
	{
		if (const TArray<FString>* Values = Request.Headers.Find(HeaderName))
		{
			return Values->Num() > 0 ? (*Values)[0] : FString();
		}
		return FString();
	};

	FString Origin = FindHeader(TEXT("Origin"));
	if (Origin.IsEmpty())
	{
		Origin = FindHeader(TEXT("origin"));
	}

	return Settings.IsOriginAllowed(Origin) ? Origin : FString();
}

bool FSpecialAgentMCPServer::CompleteWithError(const FHttpResultCallback& OnComplete, EHttpServerResponseCodes Code, const FString& Message, const FString& CorsOrigin) const
{
	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetBoolField(TEXT("success"), false);
	ErrorObj->SetStringField(TEXT("error"), Message);

	FString ResponseJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJson);
	FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
	AddStandardHeaders(*Response, CorsOrigin);
	Response->Code = Code;
	OnComplete(MoveTemp(Response));
	return true;
}

bool FSpecialAgentMCPServer::AuthorizeRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, bool bRequireToken) const
{
	const FString CorsOrigin = GetAllowedCorsOrigin(Request);

	auto FindHeader = [&Request](const FString& HeaderName) -> FString
	{
		if (const TArray<FString>* Values = Request.Headers.Find(HeaderName))
		{
			return Values->Num() > 0 ? (*Values)[0] : FString();
		}
		return FString();
	};

	const FString Origin = FindHeader(TEXT("Origin")).IsEmpty() ? FindHeader(TEXT("origin")) : FindHeader(TEXT("Origin"));
	if (!Origin.IsEmpty() && CorsOrigin.IsEmpty())
	{
		return CompleteWithError(OnComplete, EHttpServerResponseCodes::Forbidden, TEXT("Origin is not allowed"), FString());
	}

	if (Settings.bLoopbackOnly)
	{
		const FString PeerAddress = Request.PeerAddress.IsValid() ? Request.PeerAddress->ToString(false) : FString();
		const bool bIsLoopback =
			PeerAddress == TEXT("127.0.0.1") ||
			PeerAddress.StartsWith(TEXT("127.")) ||
			PeerAddress == TEXT("::1") ||
			PeerAddress == TEXT("0:0:0:0:0:0:0:1");

		if (!bIsLoopback)
		{
			return CompleteWithError(OnComplete, EHttpServerResponseCodes::Forbidden, TEXT("Only loopback clients are allowed"), CorsOrigin);
		}
	}

	if (bRequireToken && Settings.bRequireAuthToken)
	{
		const FString Authorization = FindHeader(TEXT("Authorization")).IsEmpty() ? FindHeader(TEXT("authorization")) : FindHeader(TEXT("Authorization"));
		const FString HeaderToken = FindHeader(TEXT("X-SpecialAgent-Token")).IsEmpty() ? FindHeader(TEXT("x-specialagent-token")) : FindHeader(TEXT("X-SpecialAgent-Token"));
		const FString BearerToken = FString::Printf(TEXT("Bearer %s"), *Settings.AuthToken);
		const bool bAuthorized = !Settings.AuthToken.IsEmpty() && (Authorization == BearerToken || HeaderToken == Settings.AuthToken);

		if (!bAuthorized)
		{
			return CompleteWithError(OnComplete, EHttpServerResponseCodes::Denied, TEXT("Missing or invalid SpecialAgent token"), CorsOrigin);
		}
	}

	return false;
}

bool FSpecialAgentMCPServer::HandleSSEConnection(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (AuthorizeRequest(Request, OnComplete, true))
	{
		return true;
	}

	return CompleteWithError(
		OnComplete,
		EHttpServerResponseCodes::NotSupported,
		FString::Printf(TEXT("SSE transport is not supported by this build. Use %s"), *GetMcpEndpointUrl()),
		GetAllowedCorsOrigin(Request));
}

bool FSpecialAgentMCPServer::HandleMessage(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (AuthorizeRequest(Request, OnComplete, true))
	{
		return true;
	}

	// Get session ID from query parameters (optional)
	FString SessionId;
	const FString* SessionIdValue = Request.QueryParams.Find(TEXT("sessionId"));
	if (SessionIdValue)
	{
		SessionId = *SessionIdValue;
	}

	// Get request body - handle potentially empty or malformed data
	FString BodyString;
	if (Request.Body.Num() > 0)
	{
		// Ensure null termination for string conversion
		TArray<uint8> BodyWithNull = Request.Body;
		BodyWithNull.Add(0);
		BodyString = UTF8_TO_TCHAR(reinterpret_cast<const char*>(BodyWithNull.GetData()));
	}
	
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Received message (session: %s, size: %d): %s"), 
		*SessionId, Request.Body.Num(), *BodyString.Left(1000));

	// Record client activity for status tracking
	RecordClientActivity();

	// Handle empty body - some clients send empty POST to check connection
	if (BodyString.IsEmpty() || BodyString.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("SpecialAgent: Received empty request body"));
		
		// Return a simple acknowledgment for empty requests
		TSharedPtr<FJsonObject> AckResult = MakeShared<FJsonObject>();
		AckResult->SetStringField(TEXT("status"), TEXT("ready"));
		AckResult->SetStringField(TEXT("server"), TEXT("SpecialAgent"));
		
		FString ResponseJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJson);
		FJsonSerializer::Serialize(AckResult.ToSharedRef(), Writer);
		
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
		AddStandardHeaders(*Response, GetAllowedCorsOrigin(Request));
		Response->Code = EHttpServerResponseCodes::Ok;
		OnComplete(MoveTemp(Response));
		return true;
	}

	// Parse the JSON-RPC request
	FMCPRequest MCPRequest;
	if (!ParseRequest(BodyString, MCPRequest))
	{
		UE_LOG(LogTemp, Error, TEXT("SpecialAgent: Failed to parse JSON: %s"), *BodyString.Left(500));
		
		FMCPResponse ErrorResponse = FMCPResponse::Error(
			TEXT(""),
			-32700,
			TEXT("Parse error: Invalid JSON")
		);
		ErrorResponse.bHasId = false;
		ErrorResponse.IdValue.Reset();

		FString ResponseJson = FormatResponse(ErrorResponse);
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
		AddStandardHeaders(*Response, GetAllowedCorsOrigin(Request));
		Response->Code = EHttpServerResponseCodes::BadRequest;
		OnComplete(MoveTemp(Response));
		return true;
	}

	// Process on game thread and send response
	const TSharedPtr<FMCPRequestRouter> Router = RequestRouter;
	const FString CorsOrigin = GetAllowedCorsOrigin(Request);
	AsyncTask(ENamedThreads::GameThread, [Router, MCPRequest, OnComplete, SessionId, CorsOrigin]()
	{
		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Processing request on game thread: %s"), *MCPRequest.Method);
		
		FMCPResponse MCPResponse = Router->RouteRequest(MCPRequest);
		
		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: RouteRequest completed for: %s"), *MCPRequest.Method);

		if (MCPRequest.bIsNotification)
		{
			TUniquePtr<FHttpServerResponse> NotificationResponse = FHttpServerResponse::Create(TEXT(""), TEXT("application/json"));
			if (!CorsOrigin.IsEmpty())
			{
				NotificationResponse->Headers.Add(TEXT("Access-Control-Allow-Origin"), { CorsOrigin });
				NotificationResponse->Headers.Add(TEXT("Vary"), { TEXT("Origin") });
			}
			NotificationResponse->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
			NotificationResponse->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept, Authorization, X-SpecialAgent-Token") });
			NotificationResponse->Code = EHttpServerResponseCodes::NoContent;
			OnComplete(MoveTemp(NotificationResponse));
			return;
		}

		MCPResponse.Id = MCPRequest.Id;
		MCPResponse.IdValue = MCPRequest.IdValue;
		MCPResponse.bHasId = MCPRequest.bHasId;
		
		FString ResponseJson = FSpecialAgentMCPServer::FormatResponse(MCPResponse);

		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Response ready for %s (size=%d): %s"), 
			*MCPRequest.Method, ResponseJson.Len(), *ResponseJson.Left(300));

		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
		if (!CorsOrigin.IsEmpty())
		{
			Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { CorsOrigin });
			Response->Headers.Add(TEXT("Vary"), { TEXT("Origin") });
		}
		Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST, OPTIONS") });
		Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type, Accept, Authorization, X-SpecialAgent-Token") });
		Response->Code = EHttpServerResponseCodes::Ok;

		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Calling OnComplete for: %s"), *MCPRequest.Method);
		OnComplete(MoveTemp(Response));
		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: OnComplete returned for: %s"), *MCPRequest.Method);
	});

	return true;
}

bool FSpecialAgentMCPServer::HandleCORS(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (AuthorizeRequest(Request, OnComplete, false))
	{
		return true;
	}

	const FString CorsOrigin = GetAllowedCorsOrigin(Request);
	const TArray<FString>* OriginValues = Request.Headers.Find(TEXT("Origin"));
	if (!OriginValues)
	{
		OriginValues = Request.Headers.Find(TEXT("origin"));
	}
	const FString Origin = OriginValues && OriginValues->Num() > 0 ? (*OriginValues)[0] : FString();
	if (!Origin.IsEmpty() && CorsOrigin.IsEmpty())
	{
		return CompleteWithError(OnComplete, EHttpServerResponseCodes::Forbidden, TEXT("Origin is not allowed"), FString());
	}

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(TEXT(""), TEXT("text/plain"));
	AddStandardHeaders(*Response, CorsOrigin);
	Response->Headers.Add(TEXT("Access-Control-Max-Age"), { TEXT("86400") });
	Response->Code = EHttpServerResponseCodes::NoContent;
	OnComplete(MoveTemp(Response));
	return true;
}

bool FSpecialAgentMCPServer::HandleHealth(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (AuthorizeRequest(Request, OnComplete, false))
	{
		return true;
	}

	TSharedPtr<FJsonObject> HealthObj = MakeShared<FJsonObject>();
	HealthObj->SetStringField(TEXT("status"), TEXT("healthy"));
	HealthObj->SetStringField(TEXT("server"), TEXT("SpecialAgent MCP Server"));
	HealthObj->SetStringField(TEXT("version"), TEXT("1.0.0"));
	HealthObj->SetNumberField(TEXT("port"), ServerPort);
	HealthObj->SetBoolField(TEXT("running"), bIsRunning);

	FString ResponseJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseJson);
	FJsonSerializer::Serialize(HealthObj.ToSharedRef(), Writer);

	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseJson, TEXT("application/json"));
	AddStandardHeaders(*Response, GetAllowedCorsOrigin(Request));
	Response->Code = EHttpServerResponseCodes::Ok;

	OnComplete(MoveTemp(Response));
	return true;
}

bool FSpecialAgentMCPServer::ParseRequest(const FString& JsonString, FMCPRequest& OutRequest)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Parse JSON-RPC fields
	if (!JsonObject->TryGetStringField(TEXT("jsonrpc"), OutRequest.JsonRpc) || OutRequest.JsonRpc != TEXT("2.0"))
	{
		return false;
	}

	if (!JsonObject->TryGetStringField(TEXT("method"), OutRequest.Method) || OutRequest.Method.IsEmpty())
	{
		return false;
	}
	
	// Params can be object or omitted
	const TSharedPtr<FJsonObject>* ParamsObj;
	if (JsonObject->TryGetObjectField(TEXT("params"), ParamsObj))
	{
		OutRequest.Params = *ParamsObj;
	}
	else
	{
		OutRequest.Params = MakeShared<FJsonObject>();
	}

	// ID can be string or number
	const TSharedPtr<FJsonValue> IdValue = JsonObject->TryGetField(TEXT("id"));
	if (IdValue.IsValid())
	{
		OutRequest.IdValue = IdValue;
		OutRequest.bHasId = true;
		OutRequest.bIsNotification = false;

		if (IdValue->Type == EJson::String)
		{
			OutRequest.Id = IdValue->AsString();
		}
		else if (IdValue->Type == EJson::Number)
		{
			OutRequest.Id = FString::SanitizeFloat(IdValue->AsNumber());
		}
		else if (IdValue->Type == EJson::Null)
		{
			OutRequest.Id = TEXT("");
		}
		else
		{
			return false;
		}
	}
	else
	{
		OutRequest.bHasId = false;
		OutRequest.bIsNotification = true;
	}

	return true;
}

FString FSpecialAgentMCPServer::FormatResponse(const FMCPResponse& Response)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("jsonrpc"), Response.JsonRpc);
	
	if (Response.IdValue.IsValid())
	{
		JsonObject->SetField(TEXT("id"), Response.IdValue);
	}
	else if (Response.bHasId)
	{
		JsonObject->SetStringField(TEXT("id"), Response.Id);
	}
	else
	{
		JsonObject->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
	}

	if (Response.bSuccess && Response.Result.IsValid())
	{
		JsonObject->SetObjectField(TEXT("result"), Response.Result);
	}
	else if (!Response.bSuccess && Response.ErrorObject.IsValid())
	{
		JsonObject->SetObjectField(TEXT("error"), Response.ErrorObject);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

void FSpecialAgentMCPServer::SendSSEEvent(const FString& SessionId, const FString& EventType, const FString& Data)
{
	UE_LOG(LogTemp, Verbose, TEXT("SpecialAgent: Ignoring SSE event '%s' for %s because SSE streaming is disabled"), *EventType, *SessionId);
}

void FSpecialAgentMCPServer::BroadcastSSEEvent(const FString& EventType, const FString& Data)
{
	UE_LOG(LogTemp, Verbose, TEXT("SpecialAgent: Ignoring SSE broadcast '%s' because SSE streaming is disabled"), *EventType);
}

void FSpecialAgentMCPServer::CleanupConnections()
{
	FScopeLock Lock(&ConnectionsLock);

	TArray<FString> ToRemove;
	for (auto& Pair : SSEConnections)
	{
		if (!Pair.Value->bIsValid)
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FString& Key : ToRemove)
	{
		SSEConnections.Remove(Key);
		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Cleaned up stale SSE connection: %s"), *Key);
	}
}

int32 FSpecialAgentMCPServer::GetConnectedClientCount() const
{
	if (!bIsRunning)
	{
		return 0;
	}

	// Consider a client "connected" if we've received activity recently
	FTimespan TimeSinceActivity = FDateTime::Now() - LastClientActivity;
	if (TimeSinceActivity.GetTotalSeconds() < ClientActivityTimeoutSeconds)
	{
		return 1; // At least one active client
	}

	return 0;
}

void FSpecialAgentMCPServer::RecordClientActivity()
{
	LastClientActivity = FDateTime::Now();
}
