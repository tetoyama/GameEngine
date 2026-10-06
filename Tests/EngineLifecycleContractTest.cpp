#include "Engine/engineContext.h"
#include "Service/Runtime/TimeService/timeService.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

static void Require(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
template<int ID> struct TrackedService final : IService {
    EngineContext& context;
    std::vector<int>& events;
    TrackedService(EngineContext& c,std::vector<int>& e):context(c),events(e){}
    ~TrackedService() override { events.push_back(-ID); }
    void Shutdown() override {
        Require(bool(context.Get<TrackedService<1>>()),"Dependency destroyed before shutdown completed");
        events.push_back(ID);
    }
};
int main() {
    try {
        std::vector<int> events;
        EngineContext context;
        auto* first=context.Emplace<TrackedService<1>>(context,events);
        auto* second=context.Emplace<TrackedService<2>>(context,events);
        Require(first && second && context.Get<TrackedService<1>>().get()==first,"Service registration failed");
        Require(!context.Emplace<TrackedService<1>>(context,events),"Duplicate service replaced its owner");
        Require(context.Get<TrackedService<1>>().get()==first,"Duplicate changed dependency identity");
        events.clear();
        context.Shutdown();
        Require(events==std::vector<int>{2,1,-2,-1},"Shutdown/destruction order changed");
        context.Shutdown();
        Require(events.size()==4,"Shutdown was not idempotent");
        Require(bool(context.Emplace<TimeService>()),"Context cannot be reused after shutdown");
        auto& time=*context.Get<TimeService>().get();
        time.Initialize();
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        time.Tick();
        Require(std::isfinite(time.GetDeltaTime()) && time.GetDeltaTime()>0 &&
            time.GetTotalTime()>=time.GetDeltaTime(),"Clock did not advance monotonically");
        Require(time.ShouldRunFixedUpdate(),"Fixed update did not consume elapsed time");
        time.BeginDeltaUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        time.EndDeltaUpdate();
        time.BeginDraw(17); time.BeginDrawSection(DrawTimingSection::RenderSchedule);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        time.EndDrawSection(DrawTimingSection::Present); // mismatched section must not consume it
        time.EndDraw(); // an open section is included exactly once
        const auto timing=time.GetDrawTimingBreakdown();
        Require(timing.frameSerial==17 && timing.update>0 && timing.renderSchedule>0 &&
            timing.present==0 && timing.total>=timing.renderSchedule,"Draw timing lost a section");
        time.Initialize();
        Require(time.GetDeltaTime()==0 && time.GetTotalTime()==0 &&
            !time.ShouldRunFixedUpdate() && time.GetDrawTimingBreakdown().frameSerial==0,
            "Reinitialization retained elapsed/fixed/draw state");
        context.Shutdown();
        std::cout<<"Engine service ownership, reverse shutdown and frame timing passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
