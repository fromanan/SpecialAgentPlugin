// Copyright Epic Games, Inc. All Rights Reserved.

#include "MCPStatusBarWidget.h"
#include "MCPServer.h"
#include "SpecialAgentSettings.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "MCPStatusBarWidget"

void SMCPStatusBarWidget::Construct(const FArguments& InArgs, TSharedPtr<FSpecialAgentMCPServer> InMCPServer)
{
	MCPServer = InMCPServer;
	CachedStatus = EMCPServerStatus::Offline;
	ConnectedClients = 0;

	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(this, &SMCPStatusBarWidget::OnStatusClicked)
		.ToolTipText(this, &SMCPStatusBarWidget::GetStatusTooltip)
		.ContentPadding(FMargin(4.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
				.ColorAndOpacity(this, &SMCPStatusBarWidget::GetStatusColor)
				.DesiredSizeOverride(FVector2D(10, 10))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MCPLabel", "SpecialAgent"))
				.TextStyle(FAppStyle::Get(), "SmallText")
			]
		]
	];

	// Register timer to update status every 0.5 seconds
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &SMCPStatusBarWidget::UpdateStatus));
}

FSlateColor SMCPStatusBarWidget::GetStatusColor() const
{
	switch (CachedStatus)
	{
		case EMCPServerStatus::Connected:
			return FSlateColor(FLinearColor::Green);
		case EMCPServerStatus::Listening:
			return FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f)); // Orange
		case EMCPServerStatus::Offline:
		default:
			return FSlateColor(FLinearColor::Red);
	}
}

FText SMCPStatusBarWidget::GetStatusTooltip() const
{
	TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer.Pin();
	const int32 Port = Server.IsValid() ? Server->GetPort() : FSpecialAgentSettings::Load().ServerPort;

	switch (CachedStatus)
	{
		case EMCPServerStatus::Connected:
			return FText::FromString(FString::Printf(TEXT("MCP Server: Connected (%d client(s))\nPort: %d\nClick to copy config to clipboard"), ConnectedClients, Port));
		case EMCPServerStatus::Listening:
			return FText::FromString(FString::Printf(TEXT("MCP Server: Listening\nPort: %d\nWaiting for MCP client...\nClick to copy config to clipboard"), Port));
		case EMCPServerStatus::Offline:
		default:
			return LOCTEXT("MCPOfflineTooltip", "MCP Server: Offline\nServer failed to start or is disabled.\nClick to attempt restart");
	}
}

EMCPServerStatus SMCPStatusBarWidget::GetServerStatus() const
{
	TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer.Pin();
	if (!Server.IsValid() || !Server->IsRunning())
	{
		return EMCPServerStatus::Offline;
	}

	// For now, we'll show Listening when running
	// In the future, we could track actual client connections
	int32 ClientCount = Server->GetConnectedClientCount();
	if (ClientCount > 0)
	{
		return EMCPServerStatus::Connected;
	}

	return EMCPServerStatus::Listening;
}

FReply SMCPStatusBarWidget::OnStatusClicked()
{
	TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer.Pin();
	const FSpecialAgentSettings Settings = FSpecialAgentSettings::Load();
	const int32 Port = Server.IsValid() ? Server->GetPort() : Settings.ServerPort;
	const FString EndpointUrl = FString::Printf(TEXT("http://localhost:%d/mcp"), Port);
	
	// MCP configuration JSON - use /mcp endpoint for streamable HTTP transport
	const FString ConfigJson = FString::Printf(TEXT("{\n  \"mcpServers\": {\n    \"SpecialAgent\": {\n      \"url\": \"%s\"\n    }\n  }\n}"), *EndpointUrl);
	
	FText Message;
	SNotificationItem::ECompletionState State;

	switch (CachedStatus)
	{
		case EMCPServerStatus::Connected:
			// Copy config to clipboard
			FPlatformApplicationMisc::ClipboardCopy(*ConfigJson);
			Message = FText::FromString(FString::Printf(TEXT("MCP Server Connected (%d client(s))\n\nConfiguration copied to clipboard!\n\nEndpoints:\nMCP: %s\nHealth: http://localhost:%d/health"), ConnectedClients, *EndpointUrl, Port));
			State = SNotificationItem::CS_Success;
			break;

		case EMCPServerStatus::Listening:
			// Copy config to clipboard
			FPlatformApplicationMisc::ClipboardCopy(*ConfigJson);
			Message = FText::FromString(FString::Printf(TEXT("MCP Server Listening - Configuration copied to clipboard!\n\nPaste this into your MCP client config:\n%s"), *ConfigJson));
			State = SNotificationItem::CS_Pending;
			break;

		case EMCPServerStatus::Offline:
		default:
			Message = LOCTEXT("MCPOfflineMessage", "MCP Server Offline\n\nCheck the Output Log for errors.\nMake sure the plugin is enabled and ServerEnabled=true in config.");
			State = SNotificationItem::CS_Fail;
			
			// Attempt to restart the server
			if (Server.IsValid() && !Server->IsRunning())
			{
				UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Attempting to restart MCP server..."));
				if (Server->StartServer(Settings))
				{
					FPlatformApplicationMisc::ClipboardCopy(*ConfigJson);
					Message = LOCTEXT("MCPRestartedMessage", "MCP server restarted successfully!\n\nConfiguration copied to clipboard.");
					State = SNotificationItem::CS_Success;
				}
			}
			break;
	}

	// Show notification
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(State);
	}

	return FReply::Handled();
}

EActiveTimerReturnType SMCPStatusBarWidget::UpdateStatus(double InCurrentTime, float InDeltaTime)
{
	EMCPServerStatus NewStatus = GetServerStatus();
	
	TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer.Pin();
	if (Server.IsValid())
	{
		ConnectedClients = Server->GetConnectedClientCount();
	}
	else
	{
		ConnectedClients = 0;
	}

	// Log status changes
	if (NewStatus != CachedStatus)
	{
		switch (NewStatus)
		{
			case EMCPServerStatus::Connected:
				UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP client connected"));
				break;
			case EMCPServerStatus::Listening:
				UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP server listening"));
				break;
			case EMCPServerStatus::Offline:
				UE_LOG(LogTemp, Warning, TEXT("SpecialAgent: MCP server went offline"));
				break;
		}
	}

	CachedStatus = NewStatus;

	// Continue the timer
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE

