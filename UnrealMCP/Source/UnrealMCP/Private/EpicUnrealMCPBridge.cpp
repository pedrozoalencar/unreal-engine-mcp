#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/GeometryScript/UnrealMCPGeometryScriptCommands.h"
#include "Commands/PythonExec/UnrealMCPPythonExecCommands.h"
#include "Commands/Material/UnrealMCPMaterialCommands.h"
#include "Commands/Level/UnrealMCPLevelCommands.h"
#include "Commands/Asset/UnrealMCPAssetCommands.h"
#include "Commands/Introspection/UnrealMCPIntrospectionCommands.h"
#include "Commands/Transaction/UnrealMCPTransactionCommands.h"
#include "Commands/Playtest/UnrealMCPPlaytestCommands.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
    BlueprintGraphCommands = MakeShared<FEpicUnrealMCPBlueprintGraphCommands>();
    GeometryScriptCommands = MakeShared<FUnrealMCPGeometryScriptCommands>();
    PythonExecCommands = MakeShared<FUnrealMCPPythonExecCommands>();
    MaterialCommands = MakeShared<FUnrealMCPMaterialCommands>();
    LevelCommands = MakeShared<FUnrealMCPLevelCommands>();
    AssetCommands = MakeShared<FUnrealMCPAssetCommands>();
    IntrospectionCommands = MakeShared<FUnrealMCPIntrospectionCommands>();
    TransactionCommands = MakeShared<FUnrealMCPTransactionCommands>();
    PlaytestCommands = MakeShared<FUnrealMCPPlaytestCommands>();
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintGraphCommands.Reset();
    GeometryScriptCommands.Reset();
    PythonExecCommands.Reset();
    MaterialCommands.Reset();
    LevelCommands.Reset();
    AssetCommands.Reset();
    IntrospectionCommands.Reset();
    TransactionCommands.Reset();
    PlaytestCommands.Reset();
}

// Initialize subsystem
void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

// Route a single command to the appropriate handler and return the response JSON object.
// This is called from ExecuteCommand (for single commands) and from batch_execute (for each sub-command).
// MUST be called on the Game Thread.
TSharedPtr<FJsonObject> UEpicUnrealMCPBridge::RouteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

    try
    {
        TSharedPtr<FJsonObject> ResultJson;

        if (CommandType == TEXT("ping"))
        {
            ResultJson = MakeShareable(new FJsonObject);
            ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
        }
        // Batch Execute - dispatch an array of sub-commands
        else if (CommandType == TEXT("batch_execute"))
        {
            const TArray<TSharedPtr<FJsonValue>>& Commands = Params->GetArrayField(TEXT("commands"));
            TArray<TSharedPtr<FJsonValue>> Results;

            for (const auto& CmdVal : Commands)
            {
                const TSharedPtr<FJsonObject>& CmdObj = CmdVal->AsObject();
                if (!CmdObj.IsValid())
                {
                    TSharedPtr<FJsonObject> ErrResult = MakeShareable(new FJsonObject);
                    ErrResult->SetStringField(TEXT("status"), TEXT("error"));
                    ErrResult->SetStringField(TEXT("error"), TEXT("Invalid command object in batch"));
                    Results.Add(MakeShareable(new FJsonValueObject(ErrResult)));
                    continue;
                }

                FString SubType = CmdObj->GetStringField(TEXT("type"));
                TSharedPtr<FJsonObject> SubParams = CmdObj->HasField(TEXT("params"))
                    ? CmdObj->GetObjectField(TEXT("params"))
                    : MakeShareable(new FJsonObject);

                UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Batch sub-command: %s"), *SubType);
                TSharedPtr<FJsonObject> SubResponse = RouteCommand(SubType, SubParams);
                Results.Add(MakeShareable(new FJsonValueObject(SubResponse)));
            }

            ResultJson = MakeShareable(new FJsonObject);
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetNumberField(TEXT("count"), Results.Num());
            ResultJson->SetArrayField(TEXT("results"), Results);

            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            return ResponseJson;
        }
        // Scoped Transaction - wraps sub-commands in an undo transaction
        else if (CommandType == TEXT("scoped_transaction"))
        {
            FString Label = Params->HasField(TEXT("label")) ? Params->GetStringField(TEXT("label")) : TEXT("MCP Scoped Transaction");
            const TArray<TSharedPtr<FJsonValue>>& Commands = Params->GetArrayField(TEXT("commands"));

            // Begin transaction
            int32 TxIndex = GEditor->BeginTransaction(FText::FromString(Label));

            TArray<TSharedPtr<FJsonValue>> Results;
            bool bAllSuccess = true;

            for (const auto& CmdVal : Commands)
            {
                const TSharedPtr<FJsonObject>& CmdObj = CmdVal->AsObject();
                FString SubType = CmdObj->GetStringField(TEXT("type"));
                TSharedPtr<FJsonObject> SubParams = CmdObj->HasField(TEXT("params"))
                    ? CmdObj->GetObjectField(TEXT("params"))
                    : MakeShareable(new FJsonObject);

                TSharedPtr<FJsonObject> SubResult = RouteCommand(SubType, SubParams);
                Results.Add(MakeShareable(new FJsonValueObject(SubResult)));

                // Check if sub-command failed
                if (SubResult->HasField(TEXT("status")) && SubResult->GetStringField(TEXT("status")) == TEXT("error"))
                {
                    bAllSuccess = false;
                    // If fail_fast, cancel transaction
                    bool bFailFast = Params->HasField(TEXT("fail_fast")) ? Params->GetBoolField(TEXT("fail_fast")) : false;
                    if (bFailFast)
                    {
                        GEditor->CancelTransaction(TxIndex);
                        ResultJson = MakeShareable(new FJsonObject);
                        ResultJson->SetBoolField(TEXT("success"), false);
                        ResultJson->SetStringField(TEXT("error"), TEXT("Transaction cancelled due to sub-command failure"));
                        ResultJson->SetArrayField(TEXT("results"), Results);
                        ResultJson->SetBoolField(TEXT("rolled_back"), true);
                        // Return early - don't EndTransaction
                        break;
                    }
                }
            }

            if (bAllSuccess || !(Params->HasField(TEXT("fail_fast")) && Params->GetBoolField(TEXT("fail_fast"))))
            {
                GEditor->EndTransaction();
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetBoolField(TEXT("success"), bAllSuccess);
                ResultJson->SetArrayField(TEXT("results"), Results);
                ResultJson->SetNumberField(TEXT("count"), Results.Num());
                ResultJson->SetBoolField(TEXT("committed"), true);
                ResultJson->SetStringField(TEXT("label"), Label);
            }

            ResponseJson->SetStringField(TEXT("status"), bAllSuccess ? TEXT("success") : TEXT("error"));
            ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            return ResponseJson;
        }
        // Editor Commands (including actor manipulation)
        else if (CommandType == TEXT("get_actors_in_level") ||
                 CommandType == TEXT("find_actors_by_name") ||
                 CommandType == TEXT("spawn_actor") ||
                 CommandType == TEXT("delete_actor") ||
                 CommandType == TEXT("set_actor_transform") ||
                 CommandType == TEXT("spawn_blueprint_actor"))
        {
            ResultJson = EditorCommands->HandleCommand(CommandType, Params);
        }
        // Blueprint Commands
        else if (CommandType == TEXT("create_blueprint") ||
                 CommandType == TEXT("add_component_to_blueprint") ||
                 CommandType == TEXT("set_physics_properties") ||
                 CommandType == TEXT("compile_blueprint") ||
                 CommandType == TEXT("set_static_mesh_properties") ||
                 CommandType == TEXT("set_mesh_material_color") ||
                 CommandType == TEXT("get_available_materials") ||
                 CommandType == TEXT("apply_material_to_actor") ||
                 CommandType == TEXT("apply_material_to_blueprint") ||
                 CommandType == TEXT("get_actor_material_info") ||
                 CommandType == TEXT("get_blueprint_material_info") ||
                 CommandType == TEXT("read_blueprint_content") ||
                 CommandType == TEXT("analyze_blueprint_graph") ||
                 CommandType == TEXT("get_blueprint_variable_details") ||
                 CommandType == TEXT("get_blueprint_function_details"))
        {
            ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
        }
        // Blueprint Graph Commands
        else if (CommandType == TEXT("add_blueprint_node") ||
                 CommandType == TEXT("connect_nodes") ||
                 CommandType == TEXT("create_variable") ||
                 CommandType == TEXT("set_blueprint_variable_properties") ||
                 CommandType == TEXT("add_event_node") ||
                 CommandType == TEXT("delete_node") ||
                 CommandType == TEXT("set_node_property") ||
                 CommandType == TEXT("create_function") ||
                 CommandType == TEXT("add_function_input") ||
                 CommandType == TEXT("add_function_output") ||
                 CommandType == TEXT("delete_function") ||
                 CommandType == TEXT("rename_function") ||
                 CommandType == TEXT("set_pin_value") ||
                 CommandType == TEXT("add_event_override"))
        {
            ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
        }
        // Geometry Script Commands (all gs_* prefixed)
        else if (CommandType.StartsWith(TEXT("gs_")))
        {
            ResultJson = GeometryScriptCommands->HandleCommand(CommandType, Params);
        }
        // Python Execution Commands
        else if (CommandType == TEXT("execute_python") ||
                 CommandType == TEXT("execute_python_file"))
        {
            ResultJson = PythonExecCommands->HandleCommand(CommandType, Params);
        }
        // Material Commands (all mat_* prefixed) - strip prefix for internal routing
        else if (CommandType.StartsWith(TEXT("mat_")))
        {
            FString SubCommand = CommandType.RightChop(4); // Remove "mat_"
            ResultJson = MaterialCommands->HandleCommand(SubCommand, Params);
        }
        // Level Commands (all level_* prefixed) - strip prefix for internal routing
        else if (CommandType.StartsWith(TEXT("level_")))
        {
            FString SubCommand = CommandType.RightChop(6); // Remove "level_"
            ResultJson = LevelCommands->HandleCommand(SubCommand, Params);
        }
        // Asset Commands (all asset_* prefixed) - strip prefix for internal routing
        else if (CommandType.StartsWith(TEXT("asset_")))
        {
            FString SubCommand = CommandType.RightChop(6); // Remove "asset_"
            ResultJson = AssetCommands->HandleCommand(SubCommand, Params);
        }
        // Introspection Commands (all inspect_* prefixed)
        else if (CommandType.StartsWith(TEXT("inspect_")))
        {
            ResultJson = IntrospectionCommands->HandleCommand(CommandType, Params);
        }
        // Transaction Commands (all tx_* prefixed)
        else if (CommandType.StartsWith(TEXT("tx_")))
        {
            FString SubCommand = CommandType.RightChop(3); // Remove "tx_"
            ResultJson = TransactionCommands->HandleCommand(SubCommand, Params);
        }
        // Playtest Commands (all play_* prefixed)
        else if (CommandType.StartsWith(TEXT("play_")))
        {
            FString SubCommand = CommandType.RightChop(5); // Remove "play_"
            ResultJson = PlaytestCommands->HandleCommand(SubCommand, Params);
        }
        // Console Log - read the project log file tail
        else if (CommandType == TEXT("read_console_log"))
        {
            int32 Limit = Params->HasField(TEXT("limit")) ? (int32)Params->GetNumberField(TEXT("limit")) : 50;
            FString Level = Params->HasField(TEXT("level")) ? Params->GetStringField(TEXT("level")) : TEXT("all");

            FString MCPLogFilePath = FPaths::ProjectLogDir() / FApp::GetProjectName() + TEXT(".log");
            FString LogContent;
            FFileHelper::LoadFileToString(LogContent, *MCPLogFilePath);

            TArray<FString> Lines;
            LogContent.ParseIntoArrayLines(Lines);

            int32 Start = FMath::Max(0, Lines.Num() - Limit);
            TArray<TSharedPtr<FJsonValue>> LogEntries;
            for (int32 i = Start; i < Lines.Num(); i++)
            {
                if (Level != TEXT("all"))
                {
                    if (Level == TEXT("error") && !Lines[i].Contains(TEXT("Error"))) continue;
                    if (Level == TEXT("warning") && !Lines[i].Contains(TEXT("Warning"))) continue;
                }
                LogEntries.Add(MakeShareable(new FJsonValueString(Lines[i])));
            }

            ResultJson = MakeShareable(new FJsonObject);
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetArrayField(TEXT("entries"), LogEntries);
            ResultJson->SetNumberField(TEXT("count"), LogEntries.Num());
            ResultJson->SetStringField(TEXT("log_path"), MCPLogFilePath);
        }
        else
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
            return ResponseJson;
        }

        // Check if the result contains an error
        bool bSuccess = true;
        FString ErrorMessage;

        if (ResultJson->HasField(TEXT("success")))
        {
            bSuccess = ResultJson->GetBoolField(TEXT("success"));
            if (!bSuccess && ResultJson->HasField(TEXT("error")))
            {
                ErrorMessage = ResultJson->GetStringField(TEXT("error"));
            }
        }

        if (bSuccess)
        {
            // Set success status and include the result
            ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
            ResponseJson->SetObjectField(TEXT("result"), ResultJson);
        }
        else
        {
            // Set error status and include the error message
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
        }
    }
    catch (const std::exception& e)
    {
        ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
        ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
    }

    return ResponseJson;
}

// Execute a command received from a client
FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);

    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();

    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = RouteCommand(CommandType, Params);

        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });

    return Future.Get();
}