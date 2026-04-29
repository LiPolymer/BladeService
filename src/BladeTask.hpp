//
// Created by LithiumFish on 2026/4/23.
//

#ifndef BLADETASK
#define BLADETASK


#ifdef IDF_VER
#include <freertos/task.h>
#endif

#ifdef MBED_BUILD_TIMESTAMP
#include <Thread.h>
#include <chrono>
#include <mbed_events.h>
#endif

#include <atomic>
#include <memory>
#include <utility>

class BladeTask;

using BladeAction = std::function<void(BladeTask*)>;

class IBladePayload {
public:
    virtual ~IBladePayload() = default;
    virtual void payload(BladeTask* task) =0;
};

class BladeActionPayload : public IBladePayload {
public:
    explicit BladeActionPayload(BladeAction payload) : action(std::move(payload)) {}
    BladeAction action;
    void payload(BladeTask* task) override {
        if(action)action(task);
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

    explicit BladeTask(BladeAction action) :
        ownedPayload(std::unique_ptr<IBladePayload>(new BladeActionPayload(std::move(action)))),
        payload(ownedPayload.get()) {}

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
            if (!wait(timeoutMs, checkAlivePeriodMs)) {
                this->kill();
                _taskHandle = nullptr;
                return false;
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
        if (!isRunning) return;
        vTaskDelete(_taskHandle);
        isShutdownRequested = false;
        isRunning = false;
    }
#endif
#ifdef  MBED_BUILD_TIMESTAMP
    std::unique_ptr<rtos::Thread> _thread = nullptr;

    static void suicideAgent(BladeTask* task) {
        delete task;
    }

    void processBootstrap() {
        isRunning = true;

        if (payload != nullptr) payload->payload(this);

        isShutdownRequested = false;
        isRunning = false;
        if (isOneShoot && !isDestroying) {
            mbed::mbed_event_queue() ->
                call(mbed::callback(suicideAgent), this); //甩全局队列里防止左手砍右手
        } //todo: 测试这个 不知道会不会炸
    }

    void start(const bool oneShoot = false) {
        if (isRunning) return;
        isOneShoot = oneShoot;
        _thread = std::make_unique<rtos::Thread>();
        _thread->start(mbed::callback(this, &BladeTask::processBootstrap));
    }

    bool stop(const unsigned int timeoutMs = 0, const unsigned short int checkAlivePeriodMs = 5,
        const bool cleanBlockedStatus = false) {
        if (isRunning) {
            isShutdownRequested = true;
            if (!wait(timeoutMs, checkAlivePeriodMs)) {
                this->kill();
                return false;
            }
        }
        return true;
    }

    bool wait(const unsigned int timeoutMs = 0, const unsigned short int checkAlivePeriodMs = 5) {
        if (isRunning) {
            if (timeoutMs == 0) _thread->join();
            else { //todo: 考虑使用eventFlag
                unsigned int elapsed = 0;
                while (isRunning) {
                    if (elapsed > timeoutMs) {
                        return false;
                    }
                    rtos::ThisThread::sleep_for(std::chrono::milliseconds(checkAlivePeriodMs));
                    elapsed += checkAlivePeriodMs;
                }
            }
        }
        return true;
    }

    void kill() {
        if (!isRunning) return;
        _thread->terminate();
        isShutdownRequested = false;
        isRunning = false;
    }
#endif
    ~BladeTask() {
        isDestroying = true;
        this->stop(0);
    }
};
#endif