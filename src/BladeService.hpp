//
// Created by LithiumFish on 2026/4/23.
//

#ifndef BLADESERVICE
#define BLADESERVICE
#include <vector>
#include <unordered_map>
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
    explicit IBladeServiceWrapper(IBladeService* service) : service(service) {}
    IBladeService* service;
};

class AsyncBootstrap final : public IBladeServiceWrapper, public IBladePayload{
public:
    explicit AsyncBootstrap(IBladeService* service) :
        IBladeServiceWrapper(service), task(this){}
    BladeTask task;
    void setup() override {
        service->setup();
    }
    void update() override {
        if (!task.isRunning) task.start();
    }
    void payload(BladeTask* _) override {
        while (!task.isShutdownRequested) {
            service->update();
        } service->shutdown();
    }
    void shutdown() override {
        task.stop(0);
    }
};

class SimpleBladeHost final : public IBladeService {
public:
    explicit SimpleBladeHost(const std::initializer_list<IBladeService*> svs) {
        for (auto sv : svs) {
            services.push_back(sv);
        }
    }
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
    void addServices(const std::initializer_list<IBladeService*> svs) {
        for (auto sv : svs) {
            services.push_back(sv);
        }
    }
};

class BladeHost final : public IBladeService {
    // ReSharper disable once CppTemplateParameterNeverUsed
    template <class T> static std::string getTypeIdentity() {
        return __PRETTY_FUNCTION__;
    }
public:
    std::unordered_map<std::string, std::unique_ptr<IBladeService>> services;
    void setup() override {
        for (const auto& service : services) {
            service.second->setup();
        }
    }

    void update() override {
        for (const auto& service : services) {
            service.second->update();
        }
    }

    void shutdown() override {
        for (const auto& service : services) {
            service.second->shutdown();
        }
    }

    void addService(const std::string& id, IBladeService& service) {
        services.emplace(id, &service);
    }

    template <class T> T* getService(const std::string& id) {
        static_assert(std::is_base_of<IBladeService, T>::value, "T should be a service");
        if (services.find(id) != services.end()) {
            return static_cast<T*>(services[id].get());
        }
        return nullptr;
    }

    template <class T, typename... Args> void addSingleton(Args&&... args) {
        static_assert(std::is_base_of<IBladeService, T>::value, "T should be a service");
        services.emplace(getTypeIdentity<T>(), new T(std::forward<Args>(args)...));
    }

    template <class T> void takeSingleton(T& instance) {
        static_assert(std::is_base_of<IBladeService, T>::value, "T should be a service");
        services.emplace(getTypeIdentity<T>(), &instance);
    }

    template <class T> void takeSingleton(T* instPtr) {
        static_assert(std::is_base_of<IBladeService, T>::value, "T should be a service");
        services.emplace(getTypeIdentity<T>(), instPtr);
    }

    template <class T> T* getSingleton() {
        static_assert(std::is_base_of<IBladeService, T>::value, "T should be a service");
        const std::string id = getTypeIdentity<T>();
        return getService<T>(id);
    }
};

#ifdef ARDUINO
#define BLADE_SETUP(host, set) \
    void setup() { set;host.setup(); } \
    void loop() { host.update(); }
#endif //ARDUINO
#endif //BLADESERVICE