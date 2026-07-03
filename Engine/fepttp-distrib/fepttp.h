#include "config.h"
#include "utilities.h"
#include "definitions.h"
#include "timeout.h"
#include "httpparsing.h"
#include "httpresponses.h" 
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>

using namespace Config;
using namespace Definitions;

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "kernel32.lib")

enum OpType { Accept, Recv, Send };

struct SOCKET_INFO {
    std::string acc = "";
    bool io_no_retry = false;
    std::chrono::time_point<std::chrono::steady_clock> last_use;
};

struct CmapShard{
    std::mutex mtx;
    std::unordered_map<SOCKET, SOCKET_INFO> shard;
};

class ShardedCmap{
private:
    int GetShardIndex(SOCKET sock){
        return Utils::hashint(sock, CMAP_NUM_SHARDS);
    }

public:
    // Only for direct access in sweepers
    std::vector<CmapShard> shards{CMAP_NUM_SHARDS};

    CmapShard * GetShard(SOCKET sock){
        const int sh = GetShardIndex(sock);
        return &shards[sh];
    }
};

struct SOCKET_INFO_s {
    std::string drain;
    std::string stream;
    bool io_no_retry = false;
    bool draining = false;
};

struct CmapShard_s {
    std::mutex mtx;
    std::unordered_map<SOCKET, SOCKET_INFO_s> shard;
};

class ShardedCmap_s{
private:
    int GetShardIndex(SOCKET sock){
        return Utils::hashint(sock, CMAP_NUM_SHARDS);
    }

    std::vector<CmapShard_s> shards{CMAP_NUM_SHARDS};

public:
    CmapShard_s * GetShard(SOCKET sock){
        const int sh = GetShardIndex(sock);
        return &shards[sh];
    }
};

struct TimeoutList{
    std::mutex mtx;
    std::vector<SetTimeout::Timeout> timeouts;
};

struct IOContextPtr{
    int optype;
    OVERLAPPED ov{};
    void * ptr;
};

struct AcceptContext{
    SOCKET sock = INVALID_SOCKET;
    char addr_buf[(sizeof(sockaddr_in) + 16) * 2];
};

struct RecvContext{
    SOCKET sock;
    WSABUF bufs[1];
    char recvbuf[RECV_BUFFER_SIZE];

    RecvContext(){
        bufs[0].buf = recvbuf;
        bufs[0].len = sizeof(recvbuf);
    }
};

struct SendContext{
    SOCKET sock;
    WSABUF bufs[1];
    char * sendbuf;

    SendContext(){
        bufs[0].buf = sendbuf;
    }
};

class Fepttp{
public:
    Fepttp(int PORT){
        StartServer(PORT);
    }

    ~Fepttp(){

    }

    void run(){
        SYSTEM_INFO sysinfo{}; GetSystemInfo(&sysinfo); auto worker_count = 2 * sysinfo.dwNumberOfProcessors;
        for(int i = 0; i < worker_count; i++){
            workers.emplace_back([this]() { this->WorkerFunction((LPVOID)IOCP); });
        }

        sweepers.emplace_back([this]() { this->TimeoutSweeper(); });

        for(int i = 0; i < CONCURRENT_ACCEPTEXS; i++){ PostAcceptEx(); posted_acceptexs++; }

        // Housekeeping loop
        while(true){
            Sleep(HOUSEKEEPER_POLL_RATE_MS);
            // Maintain the AcceptExs
            for(int i = posted_acceptexs; i < CONCURRENT_ACCEPTEXS; i++){
                if(PostAcceptEx() == IO_POST_SUCCESS){ posted_acceptexs++; }
            }

            // Handle (set)timeouts
            auto now = std::chrono::steady_clock::now();
            for(auto& t : tl.timeouts){
                if(now >= t.expiry){
                t.ExecuteCallback();
                switch(t.callbacktype){
                case SetTimeout::CallbackType::Generic: // ---------- Generic callback ----------
                { break; }

                case SetTimeout::CallbackType::PostWSARecv: // ---------- WSARecv post callback ----------
                {
                CmapShard * sh = scmap.GetShard(t.sock);
                if(!sh->shard[t.sock].io_no_retry){ RepeatTimeout(t, IO_RETRY_DELAY_MS); }
                break;
                }

                case SetTimeout::CallbackType::PostWSASend: // ---------- WSASend post callback ----------
                {
                CmapShard_s * sh_s = scmap_s.GetShard(t.sock);
                if(!sh_s->shard[t.sock].io_no_retry){ RepeatTimeout(t, IO_RETRY_DELAY_MS); }
                break;
                }

                default: { break; } // --------------------
                }
                }
            }

            // Erase the completed timeouts
            {   // <<<--------------->>>
            std::scoped_lock lock(tl.mtx);
            tl.timeouts.erase(
                std::remove_if(tl.timeouts.begin(), tl.timeouts.end(), [now](const SetTimeout::Timeout& t) { return (now >= t.expiry); }),
                tl.timeouts.end()
            );
            }   // <<<--------------->>>
        }
    }

    std::vector<std::string> CORS;

    // Chain up functions to be run on each HTTP request. Signature HTTPReqRes (HTTPRequest, HTTPResponse)
    std::vector<std::function<HTTPReqRes(HTTPRequest, HTTPResponse)>> chain;

private:
    WSADATA wsa_data;
    SOCKET lsocket;
    HANDLE IOCP;
    int worker_count;
    DWORD flags = 0;
    std::atomic<int> posted_acceptexs = 0;

    ShardedCmap scmap;
    ShardedCmap_s scmap_s;
    
    TimeoutList tl;

    std::vector<std::thread> workers;
    std::vector<std::thread> sweepers;

    void UpdateSocketLastUse(SOCKET sock, CmapShard * sh){
        auto now = std::chrono::steady_clock::now();
        {   // <<<--------------->>>
        std::scoped_lock lock(sh->mtx);
        sh->shard[sock].last_use = now;
        }   // <<<--------------->>>
    }

    void RepeatTimeout(SetTimeout::Timeout t, int ms){
        SetTimeout::Timeout tt = SetTimeout::RecreateTimeout(t, ms);
        { std::scoped_lock lock(tl.mtx); tl.timeouts.push_back(tt); }
    }

    int ParseErrorWSARecv(int err){
        switch(err){
        case ERROR_IO_PENDING:
        {
            return NO_ERR;
            break;
        }
        case WSAENOTSOCK:
        {
            return ERASE_CONTINUE;
            break;
        }
        default:
        {
            return SCHEDULE_RETRY;
            break;
        }
        }
    }

    int ParseErrorWSASend(int err){
        switch(err){
        case ERROR_IO_PENDING:
        {
            return NO_ERR;
            break;
        }
        case WSAENOTSOCK:
        {
            return ERASE_CONTINUE;
            break;
        }
        default:
        {
            return SCHEDULE_RETRY;
            break;
        }
        }
    }

    int PostAcceptEx(){
        AcceptContext * ctx = new AcceptContext();
        IOContextPtr * ctxptr = new IOContextPtr();
        ctxptr->ptr = (void *)ctx;
        ctxptr->optype = OpType::Accept;
        ctx->sock = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
        BOOL success = AcceptEx(
            lsocket,                      // The listening socket
            ctx->sock,                    // The socket to accept the connection on
            ctx->addr_buf,                // Where to store the remote and local addresses
            0,                            // Number of bytes of TCP stream data to initially receive
            sizeof(sockaddr_in) + 16,     // Number of bytes for the local address
            sizeof(sockaddr_in) + 16,     // Number of bytes for the remote address
            nullptr,                      // AcceptEx will set this to the number of bytes received (can find in OVERLAPPED)
            &ctxptr->ov                   // AcceptEx will send the completion packet here
        );

        if(success == FALSE){
            auto err = WSAGetLastError();
            if(err != ERROR_IO_PENDING){
                std::cout << "Error with AcceptEx. Error: " << err << std::endl; 
                closesocket(ctx->sock); delete ctx; delete ctxptr;
                return IO_POST_FAILURE;
            }
        }

        return IO_POST_SUCCESS;
    }

    int PostWSARecv(SOCKET sock){
        RecvContext * ctx = new RecvContext();
        IOContextPtr * ctxptr = new IOContextPtr();
        ctxptr->ptr = (void *)ctx;
        ctxptr->optype = OpType::Recv;
        ctx->sock = sock;
        flags = 0;
        int success = WSARecv(
            sock,                        // The socket to receive from
            ctx->bufs,                   // The array of receive buffers
            1,                           // The number of buffers
            NULL,                        // Number of bytes received from the socket
            &flags,                      // A pointer to flags that can modify the behaviour of WSARecv
            &ctxptr->ov,                 // WSARecv will send the completion packet here
            NULL                         // Completion routine
        );

        if(success == SOCKET_ERROR){
            auto err = WSAGetLastError();
            return err;
        }
        return success;
    }

    void ScheduleRetryPostWSARecv(SOCKET sock){
        // Create a timeout to retry the WSARecv post which sets the flag to true if successful
        SetTimeout::Timeout t([sock, this](){
            if(PostWSARecv(sock) == IO_POST_SUCCESS){
                CmapShard * sh = scmap.GetShard(sock);
                {   // <<<--------------->>>
                std::scoped_lock lock(sh->mtx);
                sh->shard[sock].io_no_retry = true;
                }   // <<<--------------->>>
            };

        }, SetTimeout::CallbackType::PostWSARecv, IO_RETRY_DELAY_MS, sock);

        // Set the timeout
        { std::scoped_lock lock(tl.mtx); tl.timeouts.push_back(t); }
    }

    int PostWSASend(SOCKET sock, std::string res_chunk){
        SendContext * ctx = new SendContext();
        IOContextPtr * ctxptr = new IOContextPtr();
        ctxptr->ptr = (void *)ctx;
        ctxptr->optype = OpType::Send;
        ctx->sock = sock;
        flags = 0;
        ctx->bufs[0].buf = res_chunk.data();
        ctx->bufs[0].len = res_chunk.length();
        int success = WSASend(
            sock,
            ctx->bufs,
            1,
            NULL,
            flags,
            &ctxptr->ov,
            NULL
        );

        if(success == SOCKET_ERROR){
            auto err = WSAGetLastError();
            return err;
        }
        return success;
    }

    void ScheduleRetryPostWSASend(SOCKET sock, std::string res_chunk){
        SetTimeout::Timeout t([sock, res_chunk, this](){
            if(PostWSASend(sock, res_chunk) == IO_POST_SUCCESS){
                CmapShard_s * sh_s = scmap_s.GetShard(sock);
                {   // <<<--------------->>>
                std::scoped_lock lock(sh_s->mtx);
                sh_s->shard[sock].io_no_retry = true;
                }   // <<<--------------->>>
            };
        }, SetTimeout::CallbackType::PostWSASend, IO_RETRY_DELAY_MS, sock);

        { std::scoped_lock lock(tl.mtx); tl.timeouts.push_back(t); }
    }

    void SendResponse(SOCKET sock, HTTPResponse res){
        CmapShard * sh = scmap.GetShard(sock);
        CmapShard_s * sh_s = scmap_s.GetShard(sock);
        std::string res_str = res.Assemble();

        {   // <<<--------------->>>
        std::scoped_lock lock(sh_s->mtx);
        // Unconditionally append the whole response to stream
        sh_s->shard[sock].stream.append(res_str);

        // If draining has finished (so, drain is empty)
        if(!sh_s->shard[sock].draining){
            // Swap drain and stream
            std::swap(sh_s->shard[sock].drain, sh_s->shard[sock].stream);
            // Toggle 'draining' on, since we are about to start it
            sh_s->shard[sock].draining = true;
            // Copy the first buffer-sized chunk of drain and erase it
            std::string res_chunk = res_str.substr(0, SEND_BUFFER_SIZE);
            sh_s->shard[sock].drain.erase(0, res_chunk.length());
            // Send it off. Completion will be caught in OpType::Send
            int result = PostWSASend(sock, res_chunk);
            if(result != 0){
                switch(ParseErrorWSASend(result)){

                case NO_ERR: {
                    break;
                }

                case ERASE_CONTINUE: {
                    {   // <<<--------------->>>
                    std::scoped_lock lock(sh->mtx);
                    sh->shard.erase(sock);
                    }   // <<<--------------->>>
                    sh_s->shard.erase(sock); break;
                }

                case SCHEDULE_RETRY: {
                    ScheduleRetryPostWSASend(sock, res_chunk); break;
                }

                default: {
                    break;
                }
                }
            }
        }

        // Otherwise, take no action and let OpType::Send do the swap later
        }   // <<<--------------->>>
    }

    DWORD WINAPI WorkerFunction(LPVOID iocpptr){
    HANDLE iocp = (HANDLE)iocpptr;
    while(true){
        OVERLAPPED_ENTRY entries[GQCSEX_MAX_INTAKE];
        ULONG num_removed;

        if(!GetQueuedCompletionStatusEx(iocp, entries, GQCSEX_MAX_INTAKE, &num_removed, INFINITE, FALSE)){
            const auto err = WSAGetLastError();
            std::cout << "GQCSEx error: " << err << std::endl;
            if(err == ERROR_ABANDONED_WAIT_0){ break; /* IOCP gone. All threads will shut down */ }
            continue;
        }

        for(int i = 0; i < num_removed; i++){
            OVERLAPPED * ovptr = entries[i].lpOverlapped;
            IOContextPtr * ctxptr = CONTAINING_RECORD(ovptr, IOContextPtr, ov);

            switch(ctxptr->optype){
            case OpType::Accept: // ------------------------------ Accept operation -------------------------------------------
            {
            posted_acceptexs--;

            AcceptContext * ctx = (AcceptContext *)ctxptr->ptr; delete ctxptr;
            CmapShard * sh = scmap.GetShard(ctx->sock);

            if(PostAcceptEx() == IO_POST_SUCCESS){ posted_acceptexs++; }

            if(!CreateIoCompletionPort((HANDLE)ctx->sock, iocp, 0, 0)){
                std::cout << "Error associating socket " << ctx->sock << " to IOCP. Error: " << WSAGetLastError() << std::endl;
                delete ctx; continue;
            }

            std::cout << "Socket " << ctx->sock << " connected and associated to IOCP" << std::endl;

            UpdateSocketLastUse(ctx->sock, sh);

            int result = PostWSARecv(ctx->sock);
            if(result != 0){ // WSARecv error handling
                switch(ParseErrorWSARecv(result)){

                case NO_ERR: {
                    break;
                }

                case ERASE_CONTINUE: {
                    // The key (socket) has not been put into scmap yet
                    break;
                }

                case SCHEDULE_RETRY: {
                    ScheduleRetryPostWSARecv(ctx->sock); break;
                }

                default: {
                    break;
                }
                }
            }

            delete ctx;
            break;
            }

            case OpType::Recv: // --------------------------------- Recv operation --------------------------------------------
            {
            RecvContext * ctx = (RecvContext *)ctxptr->ptr; delete ctxptr;
            CmapShard * sh = scmap.GetShard(ctx->sock);
            CmapShard_s * sh_s = scmap_s.GetShard(ctx->sock);
            auto bytes = entries[i].dwNumberOfBytesTransferred;

            UpdateSocketLastUse(ctx->sock, sh);

            if(bytes == 0){
                const auto result = closesocket(ctx->sock);
                if(result == SOCKET_ERROR){
                    auto err = WSAGetLastError();
                    if(err != WSAENOTSOCK){
                        std::cout << "Error closing socket " << ctx->sock << std::endl;
                        delete ctx; continue;
                    }
                }
                {   // <<<--------------->>>
                std::scoped_lock lock(sh->mtx);
                sh->shard.erase(ctx->sock);
                }   // <<<--------------->>>
                {   // <<<--------------->>>
                std::scoped_lock lock(sh_s->mtx);
                sh_s->shard.erase(ctx->sock);
                }   // <<<--------------->>>
                std::cout << "Socket gracefully closed: " << ctx->sock << std::endl;
                delete ctx; continue;
            }

            {   // <<<--------------->>>
            std::scoped_lock lock(sh->mtx);
            sh->shard[ctx->sock].acc.append(ctx->bufs[0].buf, bytes);
            }   // <<<--------------->>>

            int req_length = parser.RequestLength(sh->shard[ctx->sock].acc);
            if(req_length == CONTINUE){ // WSARecv is posted here
                {   // <<<--------------->>>
                std::scoped_lock lock(sh->mtx);
                sh->shard[ctx->sock].io_no_retry = false;
                }   // <<<--------------->>>
                int result = PostWSARecv(ctx->sock);
                if(result != 0){ // WSARecv error handling
                    switch(ParseErrorWSARecv(result)){

                    // Not an actual error - proceed
                    case NO_ERR: {
                        break;
                    }

                    // Socket has disappeared - erase its info in both scmaps and continue
                    case ERASE_CONTINUE: {
                        {   // <<<--------------->>>
                        std::scoped_lock lock(sh->mtx);
                        sh->shard.erase(ctx->sock);
                        }   // <<<--------------->>>
                        {   // <<<--------------->>>
                        std::scoped_lock lock(sh_s->mtx);
                        sh_s->shard.erase(ctx->sock);
                        }   // <<<--------------->>>
                        break;
                    }

                    // This error can be fixed by trying again - schedule a retry
                    case SCHEDULE_RETRY: {
                        ScheduleRetryPostWSARecv(ctx->sock); break;
                    }

                    // The WSARecv error parser defaults to SCHEDULE_RETRY, so this should be unreachable
                    default: {
                        break;
                    }
                    }
                }
                delete ctx; continue;
            }
            if(req_length == CLOSE_ERASE_CONTINUE){
                closesocket(ctx->sock);
                {   // <<<--------------->>>
                std::scoped_lock lock(sh->mtx);
                sh->shard.erase(ctx->sock);
                }   // <<<--------------->>>
                delete ctx; continue;
            }

            std::cout << sh->shard[ctx->sock].acc.substr(0, req_length) << std::endl;
            HTTPRequest req = parser.ParseRequest(sh->shard[ctx->sock].acc);
            if(req.error == REQUEST_MALFORMED){ // Close, erase, continue
                closesocket(ctx->sock);
                {   // <<<--------------->>>
                std::scoped_lock lock(sh->mtx);
                sh->shard.erase(ctx->sock);
                }   // <<<--------------->>>
                delete ctx; continue;
            }

            {   // <<<--------------->>>
            std::scoped_lock lock(sh->mtx);
            sh->shard[ctx->sock].acc.erase(0, req_length);
            }   // <<<--------------->>>

            {   // <<<--------------->>>
            std::scoped_lock lock(sh->mtx);
            sh->shard[ctx->sock].io_no_retry = false;
            }   // <<<--------------->>>
            int result = PostWSARecv(ctx->sock);
            if(result != 0){ // WSARecv error handling
                switch(ParseErrorWSARecv(result)){

                case NO_ERR: {
                    break;
                }

                case ERASE_CONTINUE: {
                    {   // <<<--------------->>>
                    std::scoped_lock lock(sh->mtx);
                    sh->shard.erase(ctx->sock);
                    }   // <<<--------------->>>
                    {   // <<<--------------->>>
                    std::scoped_lock lock(sh_s->mtx);
                    sh_s->shard.erase(ctx->sock);
                    }   // <<<--------------->>>
                    delete ctx; continue; break;
                }

                case SCHEDULE_RETRY: {
                    ScheduleRetryPostWSARecv(ctx->sock); break;
                }

                default: {
                    break;
                }
                }
            }

            // Middleware -------------------------------------------
            HTTPResponse res; HTTPReqRes pair = HTTPReqRes(req, res);

            // CORS
            std::string_view origin = pair.req.GetHeader("Origin");
            auto it = std::find(CORS.begin(), CORS.end(), origin);
            if(it != CORS.end()){ pair.res.AddHeader("Access-Control-Allow-Origin", CORS[std::distance(CORS.begin(), it)]); }

            for(const auto& fn : chain){
                pair = fn(pair.req, pair.res);
            }

            SendResponse(ctx->sock, pair.res);
            // ------------------------------------------------------

            delete ctx;
            break;
            }

            case OpType::Send: // --------------------------------- Send operation --------------------------------------------
            {
            SendContext * ctx = (SendContext *)ctxptr->ptr; delete ctxptr;
            CmapShard * sh = scmap.GetShard(ctx->sock);
            CmapShard_s * sh_s = scmap_s.GetShard(ctx->sock);
            auto bytes = entries[i].dwNumberOfBytesTransferred;

            UpdateSocketLastUse(ctx->sock, sh);

            std::cout << "Send operation completed on socket " << ctx->sock << std::endl;

            {   // <<<--------------->>>
            std::scoped_lock lock(sh_s->mtx);
            // If drain is empty ...
            if(sh_s->shard[ctx->sock].drain.empty()){
                // If stream is also emtpy, toggle 'draining' off and continue without reposting WSASend
                if(sh_s->shard[ctx->sock].stream.empty()){ sh_s->shard[ctx->sock].draining = false; delete ctx; continue; }

                // If stream is non-empty, swap drain and stream, and restart draining
                std::swap(sh_s->shard[ctx->sock].drain, sh_s->shard[ctx->sock].stream);
                // Toggle on 'draining'
                sh_s->shard[ctx->sock].draining = true;
                // Copy a buffer sized chunk of drain and erase it
                std::string res_chunk = sh_s->shard[ctx->sock].drain.substr(0, SEND_BUFFER_SIZE);
                sh_s->shard[ctx->sock].drain.erase(0, res_chunk.length());
                // Send it off with error handling and continue
                sh_s->shard[ctx->sock].io_no_retry = false;
                int result = PostWSASend(ctx->sock, res_chunk);
                if(result != 0){ // WSASend error handling
                    switch(ParseErrorWSASend(result)){

                    case NO_ERR: {
                        break;
                    }

                    case ERASE_CONTINUE: {
                        {   // <<<--------------->>>
                        std::scoped_lock lock(sh->mtx);
                        sh->shard.erase(ctx->sock);
                        }   // <<<--------------->>>
                        sh_s->shard.erase(ctx->sock); // Already have mutex
                        delete ctx; continue; break;
                    }

                    case SCHEDULE_RETRY: {
                        ScheduleRetryPostWSASend(ctx->sock, res_chunk); break;
                    }

                    default: {
                        break;
                    }
                    }
                }
                delete ctx; continue;
            }

            // If drain is not empty, we proceed with draining
            // Same procedure as above, but 'draining' will already be toggled on
            std::string res_chunk = sh_s->shard[ctx->sock].drain.substr(0, SEND_BUFFER_SIZE);
            sh_s->shard[ctx->sock].drain.erase(0, res_chunk.length());
            int result = PostWSASend(ctx->sock, res_chunk);
            if(result != 0){ // WSASend error handling
                switch(ParseErrorWSASend(result)){

                case NO_ERR: {
                    break;
                }

                case ERASE_CONTINUE: {
                    {   // <<<--------------->>>
                    std::scoped_lock lock(sh->mtx);
                    sh->shard.erase(ctx->sock);
                    }   // <<<--------------->>>
                    sh_s->shard.erase(ctx->sock); // Already have mutex
                    delete ctx; continue; break;
                }

                case SCHEDULE_RETRY: {
                    ScheduleRetryPostWSASend(ctx->sock, res_chunk); break;
                }

                default: {
                    break;
                }
                }
            }
            delete ctx; continue;
            }   // <<<--------------->>>

            delete ctx;
            break;
            }
            }
        }
    }

    return 0;
    }

    void TimeoutSweeper(){
    while(true){
    Sleep(TIMEOUT_SWEEPER_POLL_RATE_MS);

    auto now = std::chrono::steady_clock::now();

    for(auto& sh : scmap.shards){
    std::vector<SOCKET> time_out;
        for(auto& s : sh.shard){
            if(now - s.second.last_use > std::chrono::seconds(IDLE_TIMEOUT_S)){
                time_out.push_back(s.first);
            }
        }
        
        // Close the expired sockets and erase their info
        // Note: if IO begins on a socket after it has been deemed expired, the closure should result in 10038 on IO repost.
        // At this point, any socket info put into the scmaps for that socket SHOULD be erased again by 10038 error handling.
        for(SOCKET& s : time_out){
            std::cout << "Socket " << s << " timed out" << std::endl;
            closesocket(s);
            {   // <<<--------------->>>
            std::scoped_lock lock(sh.mtx);
            sh.shard.erase(s);
            }   // <<<--------------->>>
            CmapShard_s * sh_s = scmap_s.GetShard(s);
            {   // <<<--------------->>>
            std::scoped_lock lock(sh_s->mtx);
            sh_s->shard.erase(s);
            }   // <<<--------------->>>
        }
    }
    }
    }

    int StartServer(int PORT){
        std::cout << "Starting server on port " << PORT << std::endl;

        // Initialise WSA
        if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0){
            std::cout << "WSA Startup failed. Error: " << WSAGetLastError() << std::endl; WSACleanup(); return 1;
        }

        // Create the listening socket
        lsocket = WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if(lsocket == SOCKET_ERROR){
            std::cout << "Failed to create listening socket. Error" << WSAGetLastError() << std::endl; WSACleanup(); return 1;
        }

        // Bind lsocket to a port and listen on that port
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_port = htons(PORT);
        address.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

        int bind_result = bind(lsocket, (sockaddr*) &address, sizeof(address));
        if(bind_result == SOCKET_ERROR){
            std::cout << "Failed to bind listening socket. Error: " << WSAGetLastError() << std::endl;
            WSACleanup(); return 1;
        }

        if(listen(lsocket, SOMAXCONN) == SOCKET_ERROR){
            std::cout << "Listen failed. Error: " << WSAGetLastError() << std::endl; WSACleanup(); return 1;
        }

        // Create the IOCP. Second parameter is an existing IOCP. If this is not nullptr, then this function associates the handle
        // in the first parameter to the specified IOCP. We do this below with the listening socket. Parameter 3 is a completion
        // key. Parameter 4 is the number of worker threads. 0 sets it to default
        IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if(!IOCP){
            std::cout << "Error creating IOCP. Error: " << WSAGetLastError() <<  std::endl; WSACleanup(); return 1;
        }

        if(!CreateIoCompletionPort((HANDLE)lsocket, IOCP, 0, 0)){
            std::cout << "Error associating lsocket to IOCP. Error: " << WSAGetLastError() << std::endl; WSACleanup(); return 1;
        }

        return 0;
    }
};