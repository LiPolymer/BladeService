//
// Created by LithiumFish on 2026/4/23.
//

#ifndef BLADETASK
#define BLADETASK

#ifdef IDF_VER
#include <freertos/task.h>
#endif

#include <atomic>
#include <memory>

class BladeTask;

using BladeAction = void (*)(BladeTask*);

class IBladePayload {
public:
    virtual ~IBladePayload() = default;
    virtual void payload(BladeTask* task) =0;
};

class BladeActionPayload : public IBladePayload {
public:
    explicit BladeActionPayload(const BladeAction payload) : action(payload) {}
    BladeAction action;
    void payload(BladeTask* task) override {
        action(task);
    }
};

class BladeTask {
    bool isOneShoot = false;
    std::atomic<bool> isDestroying {false};
    std::unique_ptr<IBladePayload> ownedPayload;
public:
    BladeTask(const BladeTask&) = delete;
    BladeTask& operator=(const BladeTask&) = delete;

    explicit BladeTask(IBladePayload* payload) :
        ownedPayload(nullptr), payload(payload) {}

    explicit BladeTask(const BladeAction action) :
        ownedPayload(new BladeActionPayload(action)), payload(ownedPayload.get()) {}

    std::atomic<bool> isShutdownRequested {false};
    std::atomic<bool> isRunning {false};
    IBladePayload* payload;

#ifdef FREERTOS_CONFIG_H
    TaskHandle_t _taskHandle = nullptr;

    static void processBootstrap(void *pvParameters) {
        auto* task = static_cast<BladeTask*>(pvParameters);
        task->isRunning = true;

        if (task->payload != nullptr) task->payload->payload(task);

        task->isShutdownRequested = false;
        task->isRunning = false;
        task->_taskHandle = nullptr;
        if (task->isOneShoot && !task->isDestroying) delete task;
        vTaskDelete(nullptr);
    }

    void start(const bool oneShoot = false) {
        if (isRunning) return;
        isOneShoot = oneShoot;
        xTaskCreate(processBootstrap,"processor",2048,this,1,&_taskHandle);
    }

    bool stop(const unsigned int timeoutMs = 0, const unsigned short int checkAlivePeriodMs = 5,
        const bool cleanBlockedStatus = false) {
        if (isRunning) {
            isShutdownRequested = true;
            if (cleanBlockedStatus) xTaskAbortDelay(_taskHandle);
            unsigned int elapsed = 0;
            while (isRunning) {
                if (elapsed > timeoutMs && timeoutMs != 0) {
                    this->kill();
                    _taskHandle = nullptr;
                    return false;
                }
                vTaskDelay(pdMS_TO_TICKS(checkAlivePeriodMs));
                if (timeoutMs != 0) elapsed += checkAlivePeriodMs;
            }
        }
        return true;
    }

    bool wait(const unsigned int timeoutMs = 0, const unsigned short int checkAlivePeriodMs = 5) const {
        if (isRunning) {
            unsigned int elapsed = 0;
            while (isRunning) {
                if (elapsed > timeoutMs && timeoutMs != 0) {
                    return false;
                }
                vTaskDelay(pdMS_TO_TICKS(checkAlivePeriodMs));
                if (timeoutMs != 0) elapsed += checkAlivePeriodMs;
            }
        }
        return true;
    }

    void kill() {
        vTaskDelete(_taskHandle);
        isShutdownRequested = false;
        isRunning = false;
    }
#endif

    ~BladeTask() {
        isDestroying = true;
        this->stop(0);
    }
};

#endif //BLADETASK