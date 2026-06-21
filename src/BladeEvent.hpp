//
// Created by LithiumFish on 2026/5/21.
//

#ifndef BLADEEVENT
#define BLADEEVENT
#include <vector>

template <typename Res, typename... ArgTypes>
class IBladeDelegate {
public:
    virtual ~IBladeDelegate() = default;
    virtual Res invoke(ArgTypes...) = 0;
    virtual const void* getTypeIdentity() const = 0;
    virtual bool is_equal(const IBladeDelegate*) const = 0;
};

template <typename ClassType, typename Res, typename... ArgTypes>
class BladeMethod : public IBladeDelegate<Res, ArgTypes...> {
    ClassType* instance;
    Res (ClassType::*method)(ArgTypes...);
public:
    BladeMethod(ClassType* obj, Res (ClassType::*m)(ArgTypes...))
        : instance(obj), method(m) {}
    Res invoke(ArgTypes... args) override {
        return (instance->*method)(std::forward<ArgTypes>(args)...);
    }
    const void* getTypeIdentity() const override {
        // ReSharper disable once CppVariableCanBeMadeConstexpr
        static const char marker = 0;
        return &marker;
    }
    bool is_equal(const IBladeDelegate<Res, ArgTypes...>* other) const override {
        if (this->getTypeIdentity() != other->getTypeIdentity()) return false;
        auto* typed_other = static_cast<const BladeMethod*>(other);
        return this->instance == typed_other->instance && this->method == typed_other->method;
    }
};

template <typename ClassType, typename Res, typename... ArgTypes>
std::shared_ptr<IBladeDelegate<Res, ArgTypes...>> bMethod(ClassType* obj, Res (ClassType::*method)(ArgTypes...)) {
    return std::make_shared<BladeMethod<ClassType, Res, ArgTypes...>>(obj, method);
}

template <typename Callable, typename Res, typename... ArgTypes>
class BladeCallable : public IBladeDelegate<Res, ArgTypes...> {
    Callable callable;
    int id;
    static int generate_id() {
        static int counter = 0;
        return ++counter;
    }
public:
    explicit BladeCallable(Callable c) : callable(std::move(c)), id(generate_id()) {}
    Res invoke(ArgTypes... args) override {
        return callable(std::forward<ArgTypes>(args)...);
    }
    const void* getTypeIdentity() const override {
        // ReSharper disable once CppVariableCanBeMadeConstexpr
        static const char marker = 0;
        return &marker;
    }
    bool is_equal(const IBladeDelegate<Res, ArgTypes...>* other) const override {
        if (this->getTypeIdentity() != other->getTypeIdentity()) return false;
        auto* typed_other = static_cast<const BladeCallable*>(other);
        return this->id == typed_other->id;
    }
};

template <typename Res, typename... ArgTypes, typename Callable>
std::shared_ptr<IBladeDelegate<Res, ArgTypes...>> bLambda(Callable&& c) {
    using DecayedCallable = typename std::decay<Callable>::type;
    return std::make_shared<BladeCallable<DecayedCallable, Res, ArgTypes...>>(std::forward<Callable>(c));
}

template <typename Res, typename... ArgTypes>
class BladeEvent {
public:
    using DelegatePtr = std::shared_ptr<IBladeDelegate<Res, ArgTypes...>>;
private:
    std::vector<DelegatePtr> subscribers;
public:
    template <typename... CallArgs>
    void invoke(CallArgs&&... args) {
        auto snapshot = subscribers;
        for (auto& sub : snapshot) {
            sub->invoke(std::forward<CallArgs>(args)...);
        }
    }

    BladeEvent& operator+=(DelegatePtr delegate) {
        if (delegate) {
            subscribers.push_back(std::move(delegate));
        }
        return *this;
    }

    BladeEvent& operator-=(const DelegatePtr& delegate) {
        if (!delegate) return *this;
        for (auto it = subscribers.begin(); it != subscribers.end(); ++it) {
            if ((*it)->is_equal(delegate.get())) {
                subscribers.erase(it);
                break;
            }
        }
        return *this;
    }

    void clear() {
        subscribers.clear();
    }
};

#endif
