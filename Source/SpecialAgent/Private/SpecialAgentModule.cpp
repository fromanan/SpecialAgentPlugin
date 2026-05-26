// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpecialAgentModule.h"
#include "MCPServer.h"
#include "MCPStatusBarWidget.h"
#include "SpecialAgentSettings.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxExtender.h"

#define LOCTEXT_NAMESPACE "FSpecialAgentModule"

void FSpecialAgentModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Module starting up"));

	// Create the MCP server instance
	MCPServer = MakeShared<FSpecialAgentMCPServer>();

	const FSpecialAgentSettings Settings = FSpecialAgentSettings::Load();
	
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: ServerEnabled=%d, ServerPort=%d"), Settings.bServerEnabled, Settings.ServerPort);

	if (Settings.bServerEnabled)
	{
		if (MCPServer->StartServer(Settings))
		{
			UE_LOG(LogTemp, Log, TEXT("SpecialAgent: MCP Server started on port %d"), Settings.ServerPort);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SpecialAgent: Failed to start MCP Server"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpecialAgent: MCP Server auto-start is disabled"));
	}

	// Register status bar widget
	RegisterStatusBarWidget();
}

void FSpecialAgentModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Module shutting down"));

	UnregisterStatusBarWidget();

	if (MCPServer.IsValid())
	{
		MCPServer->StopServer();
		MCPServer.Reset();
	}
}

bool FSpecialAgentModule::IsMCPServerRunning() const
{
	return MCPServer.IsValid() && MCPServer->IsRunning();
}

void FSpecialAgentModule::RegisterStatusBarWidget()
{
	// Use UToolMenus to add our status widget to the level editor status bar
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return;
	}

	// Register with the status bar
	const FName StatusBarName = TEXT("LevelEditor.StatusBar.ToolBar");
	UToolMenu* StatusBarMenu = ToolMenus->ExtendMenu(StatusBarName);
	
	if (StatusBarMenu)
	{
		TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer;
		
		FToolMenuSection& Section = StatusBarMenu->FindOrAddSection(TEXT("SpecialAgent"));
		Section.AddEntry(FToolMenuEntry::InitWidget(
			TEXT("MCPStatus"),
			SNew(SMCPStatusBarWidget, Server),
			FText::GetEmpty(),
			true,  // bNoIndent
			false  // bSearchable
		));
		
		UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Status bar widget registered via ToolMenus"));
	}
	else
	{
		// Fallback: Register with level editor module directly
		if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
			
			TSharedPtr<FSpecialAgentMCPServer> Server = MCPServer;
			ToolBarExtender = MakeShareable(new FExtender);
			
			ToolBarExtender->AddToolBarExtension(
				TEXT("SourceControl"),
				EExtensionHook::After,
				nullptr,
				FToolBarExtensionDelegate::CreateLambda([Server](FToolBarBuilder& Builder)
				{
					Builder.AddWidget(SNew(SMCPStatusBarWidget, Server));
				})
			);
			
			LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolBarExtender);
			UE_LOG(LogTemp, Log, TEXT("SpecialAgent: Status bar widget registered via toolbar extender"));
		}
	}
}

void FSpecialAgentModule::UnregisterStatusBarWidget()
{
	// Remove from ToolMenus
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (ToolMenus)
	{
		const FName StatusBarName = TEXT("LevelEditor.StatusBar.ToolBar");
		UToolMenu* StatusBarMenu = ToolMenus->FindMenu(StatusBarName);
		if (StatusBarMenu)
		{
			StatusBarMenu->RemoveSection(TEXT("SpecialAgent"));
		}
	}

	// Remove toolbar extender if used
	if (ToolBarExtender.IsValid() && FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditorModule.GetToolBarExtensibilityManager()->RemoveExtender(ToolBarExtender);
		ToolBarExtender.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSpecialAgentModule, SpecialAgent)
