#pragma once
#include "pc_p2_retail_scene_input.h"

namespace p2retail {
struct GeometryFacts {unsigned vertices=0,triangles=0,routePoints=0,routeLinks=0;};
// Admission for the two source-validated development profiles. Checks native
// MOD bounds, finite collision/vertex data, indices, grid and embedded-route
// agreement plus the raw INI graph. Does not allocate a Shape or activate it.
bool parseRetailGeometry(const SelectedSceneInputs&,GeometryFacts&,std::string&);
}
