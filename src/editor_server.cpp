// @STL

// Isolate ENet from Windows GUI conflicts
#ifdef _WIN32
#undef NOUSER
#undef NOGDI
#endif

#include <enet/enet.h>

// Re-apply restrictions after ENet
#ifdef _WIN32
#define NOGDI  
#define NOUSER
#endif

#include "server.h"
#include "../../src/editor/editor_messages.h"

// @types
static ENetHost* g_server = nullptr;
static ENetPeer* g_client = nullptr;

static Stream* CreateEditorMessage(EditorMessage event)
{
    Stream* output_stream = CreateStream(ALLOCATOR_SCRATCH, 1024);
    WriteEditorMessage(output_stream, event);
    return output_stream;
}

static void SendEditorMessage(Stream* stream)
{
    if (ENetPacket* packet = enet_packet_create(GetData(stream), GetSize(stream), ENET_PACKET_FLAG_RELIABLE))
    {
        enet_peer_send(g_client, 0, packet);
        enet_host_flush(g_server);
    }

    Free(stream);
}

bool HasConnectedClient()
{
    return g_client != nullptr;
}

static void HandleStatsAck(Stream* stream)
{
    i32 fps = ReadI32(stream);
    EditorEventStats stats{
        .fps = fps
    };
    Send(EDITOR_EVENT_STATS, &stats);
}

static void HandleClientMessage(void* data, u32 data_size)
{
    auto stream = LoadStream(ALLOCATOR_DEFAULT, (u8*)data, data_size);
    switch (ReadEditorMessage(stream))
    {
    case EDITOR_MESSAGE_STATS_ACK:
        HandleStatsAck(stream);
        break;

    default:
        break;
    }
    Free(stream);
}


// @server
void UpdateEditorServer()
{
    if (!g_server)
        return;

    ENetEvent event;
    while (enet_host_service(g_server, &event, 0) > 0)
    {
        switch (event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                if (g_client != nullptr)
                {
                    LogError("editor client already connected");
                    enet_peer_disconnect_now(event.peer, 0);
                    return;
                }

                LogInfo("Editor client connected from %d.%d.%d.%d:%d",
                       (event.peer->address.host & 0xFF),
                       ((event.peer->address.host >> 8) & 0xFF),
                       ((event.peer->address.host >> 16) & 0xFF),
                       ((event.peer->address.host >> 24) & 0xFF),
                       event.peer->address.port);

                g_client = event.peer;
                break;
            }
            
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                if (g_client != event.peer)
                    return;

                LogInfo("Editor client disconnected");
                g_client = nullptr;
                break;
            }
            
            case ENET_EVENT_TYPE_RECEIVE:
                HandleClientMessage(event.packet->data, (u32)event.packet->dataLength);
                enet_packet_destroy(event.packet);
                break;
        }
    }
}

// @broadcast
void BroadcastAssetChange(const Name* name)
{
    if (!HasConnectedClient())
        return;

    auto msg = CreateEditorMessage(EDITOR_MESSAGE_HOTLOAD);

    WriteString(msg, name->value);
    SendEditorMessage(msg);
}

void RequestStats()
{
    SendEditorMessage(CreateEditorMessage(EDITOR_MESSAGE_STATS));
}

// @init
void InitEditorServer(Props* config)
{
    u16 port = (u16)config->GetInt("server", "port", 8080);

    if (enet_initialize() != 0)
    {
        LogWarning("Failed to create server on port %d", port);
        return;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    // Create a server with up to 32 clients and 2 channels
    g_server = enet_host_create(&address, 32, 2, 0, 0);
    if (!g_server)
    {
        LogWarning("Failed to create server on port %d", address.port);
        enet_deinitialize();
        return;
    }

    LogInfo("Server started on port %d", address.port);
}

void ShutdownEditorServer()
{
    if (g_server)
    {
        enet_host_destroy(g_server);
        g_server = nullptr;
    }

    g_client = nullptr;
    enet_deinitialize();
}
