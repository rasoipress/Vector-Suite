#include "../native/VectorSuiteNative/Source/VectorSuiteSnap.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace VSSnap;
int main() {
    Geometry g{{{0,0},{100,0},{50,-20},{50,20}},{{{0,0},{100,0}},{{50,-20},{50,20}}}};
    Settings s{true,Endpoint,8};
    auto h=find(g,{1,2},nullptr,s,1);assert(h.found&&h.mode==Endpoint&&h.point.x==0);
    s.modes=Midpoint;h=find(g,{51,2},nullptr,s,1);assert(h.found&&h.point.x==50&&h.point.y==0);
    s.modes=Intersection;h=find(g,{53,2},nullptr,s,1);assert(h.found&&h.mode==Intersection);
    s.modes=Nearest;h=find(g,{20,3},nullptr,s,1);assert(h.found&&h.point.x==20&&h.point.y==0);
    Point origin{20,30};s.modes=Perpendicular;h=find(g,{22,2},&origin,s,1);assert(h.found&&h.point.x==20&&h.point.y==0);
    assert(!find(g,{22,2},nullptr,s,1).found);
    s.modes=Midpoint|Nearest;h=find(g,{54,1},nullptr,s,1);assert(h.mode==Midpoint);
    s.modes=Endpoint;assert(!find(g,{5,0},nullptr,s,2).found);assert(find(g,{3,0},nullptr,s,2).found);
    s.enabled=false;assert(!find(g,{0,0},nullptr,s,1).found);s.enabled=true;
    assert(!find(g,{0,0},nullptr,s,0).found);
    assert(!find(g,{std::numeric_limits<double>::quiet_NaN(),0},nullptr,s,1).found);
    Point p;assert(!intersection({{0,0},{100,0}},{{0,1},{100,1}},p));
    assert(!intersection({{0,0},{1,0}},{{2,-1},{2,1}},p));
    assert(!projection({{1,1},{1,1}},{0,0},p,true));
    assert(!projection({{0,0},{1,0}},{2,2},p,false));
    Geometry shift{{{10,3},{10,10}}, {}};origin={0,0};
    assert(!find(shift,{10,3},&origin,s,2,true).found);
    assert(find(shift,{10,10},&origin,s,2,true).found);
    Geometry translated{{},{{{1000000,1000000},{1000100,1000000}},{{1000050,999980},{1000050,1000020}}}};
    s.modes=Intersection;h=find(translated,{1000051,1000001},nullptr,s,1);
    assert(h.found&&h.point.x==1000050&&h.point.y==1000000);
    // Exact cubic representation of y=x*x, x in [0,1].
    Cubic parabola;parabola.p={Point{0,0},Point{1.0/3,0},Point{2.0/3,1.0/3},Point{1,1}};
    parabola.midpoint=arcMidpoint(parabola);
    Geometry curved;curved.curves.push_back(parabola);
    origin={0,-0.25};s={true,Tangent,1};h=find(curved,{0.5,0.25},&origin,s,100);
    assert(h.found&&std::abs(h.point.x-0.5)<1e-8&&std::abs(h.point.y-0.25)<1e-8);
    origin={0,0.75};s.modes=Perpendicular;h=find(curved,{0.5,0.25},&origin,s,100);
    assert(h.found&&std::abs(h.point.x-0.5)<1e-8);
    s.modes=Nearest;h=find(curved,{0.499,0.251},nullptr,s,100);assert(h.found);
    assert(std::abs(h.point.y-h.point.x*h.point.x)<1e-10);
    s.modes=Midpoint;h=find(curved,parabola.midpoint,nullptr,s,100);assert(h.found);
    // Integral from 0 to x of sqrt(1+4x*x).
    auto length=[](double x){return 0.5*x*std::sqrt(1+4*x*x)+0.25*std::asinh(2*x);};
    assert(std::abs(length(h.point.x)-length(1)/2)<1e-7);
    assert(roots({0.25,-1,1}).size()==1&&std::abs(roots({0.25,-1,1})[0]-0.5)<1e-9);
    std::vector<Cubic> square;
    std::vector<Point> corners={{0,0},{20,0},{20,10},{0,10}};
    for(size_t i=0;i<4;++i) {Cubic edge;edge.p={corners[i],corners[i],corners[(i+1)%4],corners[(i+1)%4]};square.push_back(edge);}
    assert(centroid(square,p)&&std::abs(p.x-10)<1e-10&&std::abs(p.y-5)<1e-10);
    Geometry centered;centered.centers.push_back(p);s.modes=Center;assert(find(centered,{10,5},nullptr,s,100).found);
    Cubic arch;arch.p={Point{0,0},Point{0,1},Point{1,1},Point{1,0}};
    Geometry extrema;extrema.curves.push_back(arch);s.modes=Quadrant;
    h=find(extrema,{0.5,0.75},nullptr,s,100);assert(h.found&&h.point.x==0.5&&h.point.y==0.75);
    std::cout<<"Snap: eight modes, exact curve fixtures, arc midpoint, centroid, zoom, Shift and degeneracies verified.\n";
}
