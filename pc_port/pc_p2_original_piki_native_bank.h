#pragma once
#include <string>
#include <cstdint>
#include <memory>
class Shape;
namespace p2retail { class SceneContext; }
namespace p2original { namespace piki {
// Singular selected development bank, not a body/world readiness grant.
// Its buffers are read from actual selectedStartingPiki, never caller kit data.
class NativeBodyBank {
public:
 // This owner is retained for the process lifetime. A scope-local wrapper
 // must never destroy the cleanup census while native/graphics storage is live.
 static NativeBodyBank& instance();
 NativeBodyBank(const NativeBodyBank&)=delete;
 NativeBodyBank& operator=(const NativeBodyBank&)=delete;
 bool prepare(const p2retail::SceneContext&,std::string&);
 bool current()const noexcept;
 Shape* shape(const std::string& exactSelectedRole)const noexcept;
 const std::string* bytes(const std::string& exactSelectedRole)const noexcept;
 bool owned()const noexcept;
 bool canRelease(std::string&)const;
 // Composer MUST retire all source bodies/animators/material users first.
 // Partial bank ownership is independently retained until native disposal.
 bool release(std::string&);
 bool retired(std::string&)const;
private:
 NativeBodyBank();~NativeBodyBank();
 struct Impl;std::unique_ptr<Impl> m;
};
} }
