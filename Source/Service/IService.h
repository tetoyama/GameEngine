#pragma once
class IService {
public:
    virtual ~IService() = default;
    virtual void Shutdown() {}
};
