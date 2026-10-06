#include "engineContext.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#endif

void EngineContext::ReportMissingService() noexcept {
#ifdef _WIN32
    OutputDebugStringA("EngineContext: service not registered.\n");
#else
    std::fputs("EngineContext: service not registered.\n", stderr);
#endif
}

void EngineContext::Shutdown() {
    // Services can resolve their dependencies during shutdown. Destroy them
    // only after all shutdown callbacks complete, in reverse registration order.
    for(auto iterator = m_serviceOrder.rbegin(); iterator != m_serviceOrder.rend(); ++iterator) {
        auto found = m_services.find(*iterator);
        if(found != m_services.end() && found->second) found->second->Shutdown();
    }
    for(auto iterator = m_serviceOrder.rbegin(); iterator != m_serviceOrder.rend(); ++iterator)
        m_services.erase(*iterator);
    m_serviceOrder.clear();
}
