#include "connection.h"

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME
#include <windows.h>

struct WinRpcConnection : public RpcConnection {
    HANDLE pipe{INVALID_HANDLE_VALUE};
    RpcMessageFrame frame;
};

static const wchar_t* PipeName = L"\\\\.\\pipe\\DiscordRpcServer";

/*static*/ RpcConnection* RpcConnection::Create()
{
    return new WinRpcConnection;
}

/*static*/ void RpcConnection::Destroy(RpcConnection*& c)
{
    auto self = reinterpret_cast<WinRpcConnection*>(c);
    delete self;
    c = nullptr;
}

void RpcConnection::Open()
{
    auto self = reinterpret_cast<WinRpcConnection*>(this);
    for (;;) {
        self->pipe = ::CreateFileW(PipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (self->pipe != INVALID_HANDLE_VALUE) {
            if (self->onConnect) {
                self->onConnect();
            }
            break;
        }

        if (GetLastError() != ERROR_PIPE_BUSY) {
            printf("Could not open pipe. Error: %d\n", GetLastError());
            return;
        }

        if (!WaitNamedPipeW(PipeName, 10000)) {
            printf("Could not open pipe: 10 second wait timed out.\n");
            return;
        }
    }
}

void RpcConnection::Close()
{
    auto self = reinterpret_cast<WinRpcConnection*>(this);
    ::CloseHandle(self->pipe);
    self->pipe = INVALID_HANDLE_VALUE;
    if (self->onDisconnect) {
        self->onDisconnect();
    }
}

void RpcConnection::Write(const void* data, size_t length)
{
    auto self = reinterpret_cast<WinRpcConnection*>(this);

    if (self->pipe == INVALID_HANDLE_VALUE) {
        self->Open();
        if (self->pipe == INVALID_HANDLE_VALUE) {
            return;
        }
    }
    BOOL success = ::WriteFile(self->pipe, data, length, nullptr, nullptr);
    if (!success) {
        self->Close();
    }
}

RpcMessageFrame* RpcConnection::GetNextFrame()
{
    auto self = reinterpret_cast<WinRpcConnection*>(this);
    return &(self->frame);
}

void RpcConnection::WriteFrame(RpcMessageFrame* frame)
{
    auto self = reinterpret_cast<WinRpcConnection*>(this);
    self->Write(frame, frame->length);
}