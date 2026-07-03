#pragma once
#include "config.h"
#include <winsock2.h>
#include <chrono>
#include <functional>
#include <iostream>

namespace SetTimeout {
    enum CallbackType {
        Generic,
        PostWSARecv,
        PostWSASend
    };

    class Timeout {
    public:
        void ExecuteCallback(){
            callbackfn();
        }

        int callbacktype;
        std::chrono::steady_clock::time_point expiry;
        SOCKET sock;

        Timeout(std::function<void()> cb, int cbtype, int ms, SOCKET s){
            expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
            callbackfn = cb;
            callbacktype = cbtype;
            sock = s;
        }
    
    private:
        std::function<void()> callbackfn;
    };

    Timeout RecreateTimeout(Timeout t, int ms){
        Timeout tt = t;
        tt.expiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        return tt;
    }
}