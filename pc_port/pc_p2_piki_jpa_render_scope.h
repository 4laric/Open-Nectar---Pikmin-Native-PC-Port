#pragma once
#include <memory>
#include <string>
class Graphics;
namespace p2original { namespace pikiJPA {
// Actual Body-owned Scene forwards its checked begin/end callbacks here.
// This owns rendering state only; it grants no scene/body/activity authority.
// Graphics, textures and matrices must remain alive until successful end.
class HaloRenderScope final {
public:
 HaloRenderScope();
 ~HaloRenderScope();
 HaloRenderScope(const HaloRenderScope&)=delete;
 HaloRenderScope& operator=(const HaloRenderScope&)=delete;
 bool begin(Graphics&,std::string&);
 bool end(Graphics&,std::string&);
 bool retained()const noexcept;
private:
 struct Impl;
 std::unique_ptr<Impl> m;
};
} }
