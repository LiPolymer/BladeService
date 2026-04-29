//
// Created by LithiumFish on 2026/4/23.
//

#ifndef BLADESERVICE
#define BLADESERVICE
#include <vector>

#include "BladeTask.hpp"

class IBladeService {
public:
    virtual ~IBladeService() = default;
    virtual void setup() {}
    virtual void update() {}
    virtual void shutdown() {}
};

class IBladeServiceWrapper : public IBladeService {
public:
    explicit IBladeServiceWrapper(IBladeService& service) : service(service) {}
    IBladeService& service;
};

class AsyncBootstrap final : public IBladeServiceWrapper, public IBladePayload{
public:
    explicit AsyncBootstrap(IBladeService& service) :
        IBladeServiceWrapper(service), task(this){}
    BladeTask task;
    void setup() override {
        service.setup();
    }
    void update() override {
        if (!task.isRunning) task.start();
    }
    void payload(BladeTask* _) override {
        while (!task.isShutdownRequested) {
            service.update();
        } service.shutdown();
    }
    void shutdown() override {
        task.stop(0);
    }
};

class BladeHost final : public IBladeService {
public:
    std::vector<IBladeService*> services; //todo: 考虑使用独特指针
    void setup() override {
        for (IBladeService* service : services) {
            service->setup();
        }
    }
    void update() override {
        for (IBladeService* service : services) {
            service->update();
        }
    }
    void shutdown() override {
        for (IBladeService* service : services) {
            service->shutdown();
        }
    }
    void addService(IBladeService* service) {
        services.push_back(service);
    }
};

#ifdef ARDUINO
#define BLADE_SETUP(host, set) \
    void setup() { set;host.setup(); } \
    void loop() { host.update(); }
#endif //ARDUINO

#endif //BLADESERVICE