#include "pc_p2_original_number_grid.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace p2originalnumber::motion;
int main(){
 unsigned checks=0;auto check=[&](bool ok){++checks;assert(ok);};std::string error;
 std::vector<Vec3> vertices{{0,0,0},{128,0,0},{0,0,128},{128,0,128}};
 std::vector<std::array<unsigned,3>> triangles{{0,1,2},{1,3,2}};CaveGrid grid;
 check(buildCaveGrid(vertices,triangles,grid,error));
 check(grid.minimum.x==-10&&grid.maximum.x==138&&grid.countX==2&&grid.countZ==2&&grid.scaleX==74&&grid.scaleZ==74);
 // Source uses projected AABB, not a precise triangle-cell intersection.
 for(const auto& cell:grid.cells)check(cell==std::vector<unsigned>({0,1}));
 std::vector<unsigned> out;
 check(caveCandidates(grid,{64,1,64},1,out,error)&&out==std::vector<unsigned>({0,1,0,1,0,1,0,1}));
 // A real original per-cell order/repeat is retained through query.
 grid.cells={{7,2},{9},{2,7},{11}};
 check(caveCandidates(grid,{64,1,64},1,out,error)&&out==std::vector<unsigned>({7,2,9,2,7,11}));
 check(caveCandidates(grid,{-1000,1,-1000},1,out,error)&&out==std::vector<unsigned>({7,2}));
 check(caveCandidates(grid,{1000,1,1000},1,out,error)&&out==std::vector<unsigned>({11}));
 auto bad=triangles;bad[0][1]=999;const auto unchanged=grid.cells;
 check(!buildCaveGrid(vertices,bad,grid,error)&&grid.cells==unchanged);
 auto malformed=vertices;malformed[0].y=std::numeric_limits<float>::infinity();
 check(!buildCaveGrid(malformed,triangles,grid,error)&&grid.cells==unchanged);
 check(!buildCaveGrid({{0,0,0},{1,0,0},{0,0,1}},{{0,1,2}},grid,error)&&grid.cells==unchanged);
 auto broad=vertices;broad[1].x=10000;broad[3].x=10000;
 check(buildCaveGrid(broad,triangles,grid,error)&&grid.countX==48&&grid.countZ==2);
 std::vector<std::array<unsigned,3>> crowded(1026,{0,1,2});
 check(buildCaveGrid(vertices,crowded,grid,error));
 check(grid.cells.front().size()==1024&&grid.cells.front().front()==0&&grid.cells.front().back()==1023);
 const auto oldOut=out;
 check(!caveCandidates(grid,{0,0,0},0,out,error)&&out==oldOut);
 grid.cells.front().push_back(1024);
 check(!caveCandidates(grid,{-100,0,-100},1,out,error)&&out==oldOut);
 std::printf("Original number source cave grid: %u controls passed\n",checks);
}
