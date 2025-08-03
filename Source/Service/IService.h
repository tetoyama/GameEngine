#pragma once
// -----------------------------------------------------------------------
// IService.h
// -----------------------------------------------------------------------
class IService {
public:
    virtual ~IService() = default;
    virtual void Shutdown() = 0;
};
