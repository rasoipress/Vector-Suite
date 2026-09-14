#ifndef VECTOR_SUITE_SNAP_H
#define VECTOR_SUITE_SNAP_H

#include <algorithm>
#include <cmath>
#include <vector>
#include <array>

namespace VSSnap {
struct Point { double x, y; };
struct Segment { Point a, b; };
enum Mode : unsigned { Endpoint=1, Midpoint=2, Intersection=4, Nearest=8, Perpendicular=16, Center=32, Quadrant=64, Tangent=128 };
constexpr unsigned All = Endpoint|Midpoint|Intersection|Nearest|Perpendicular|Center|Quadrant|Tangent;
struct Settings { bool enabled=false; unsigned modes=Endpoint|Midpoint|Intersection; double pixels=8; };
struct Cubic { std::array<Point,4> p; Point midpoint{}; };
struct Geometry { std::vector<Point> anchors{}; std::vector<Segment> lines{};
    std::vector<Cubic> curves{}; std::vector<Point> centers{}; };
struct Hit { bool found=false; Point point{}; unsigned mode=0; double distance2=0; };
inline Point sub(Point a, Point b) { return {a.x-b.x,a.y-b.y}; }
inline double dot(Point a, Point b) { return a.x*b.x+a.y*b.y; }
inline double cross(Point a, Point b) { return a.x*b.y-a.y*b.x; }
inline bool finite(Point p) { return std::isfinite(p.x)&&std::isfinite(p.y); }
inline double distance2(Point a, Point b) { return dot(sub(a,b),sub(a,b)); }
inline Point at(Segment s,double t) { return {s.a.x+(s.b.x-s.a.x)*t,s.a.y+(s.b.y-s.a.y)*t}; }
inline Point at(const Cubic& c,double t) {
    const double u=1-t;
    return {u*u*u*c.p[0].x+3*u*u*t*c.p[1].x+3*u*t*t*c.p[2].x+t*t*t*c.p[3].x,
            u*u*u*c.p[0].y+3*u*u*t*c.p[1].y+3*u*t*t*c.p[2].y+t*t*t*c.p[3].y};
}
inline std::array<Point,4> coefficients(const Cubic& c,Point origin={0,0}) {
    return {sub(c.p[0],origin),
        Point{3*(c.p[1].x-c.p[0].x),3*(c.p[1].y-c.p[0].y)},
        Point{3*(c.p[2].x-2*c.p[1].x+c.p[0].x),3*(c.p[2].y-2*c.p[1].y+c.p[0].y)},
        Point{c.p[3].x-3*c.p[2].x+3*c.p[1].x-c.p[0].x,c.p[3].y-3*c.p[2].y+3*c.p[1].y-c.p[0].y}};
}
inline Point derivative(const Cubic& c,double t) {
    const auto a=coefficients(c);
    return {a[1].x+2*a[2].x*t+3*a[3].x*t*t,a[1].y+2*a[2].y*t+3*a[3].y*t*t};
}
inline double evaluate(const std::vector<double>& a,double t) {
    double r=0;for(auto i=a.rbegin();i!=a.rend();++i)r=r*t+*i;return r;
}
// Isolate roots on monotone intervals, including repeated roots at derivative zeros.
inline std::vector<double> roots(std::vector<double> a) {
    double scale=0;for(double x:a) {if(!std::isfinite(x))return {};scale=std::max(scale,std::abs(x));}
    if(scale==0)return {};
    for(double& x:a)x/=scale;
    while(a.size()>1&&std::abs(a.back())<1e-14)a.pop_back();
    if(a.size()<2)return {};
    if(a.size()==2) {const double t=-a[0]/a[1];return t>=0&&t<=1?std::vector<double>{t}:std::vector<double>{};}
    std::vector<double> d;for(size_t i=1;i<a.size();++i)d.push_back(i*a[i]);
    std::vector<double> bounds=roots(d),out;bounds.insert(bounds.begin(),0);bounds.push_back(1);
    for(double t:bounds)if(std::abs(evaluate(a,t))<1e-12)out.push_back(t);
    for(size_t i=1;i<bounds.size();++i) {
        double lo=bounds[i-1],hi=bounds[i],f=evaluate(a,lo),g=evaluate(a,hi);
        if(f*g>=0)continue;
        for(int n=0;n<50;++n) {double mid=(lo+hi)/2,v=evaluate(a,mid);if(f*v<=0)hi=mid;else {lo=mid;f=v;}}
        out.push_back((lo+hi)/2);
    }
    std::sort(out.begin(),out.end());
    out.erase(std::unique(out.begin(),out.end(),[](double a,double b){return std::abs(a-b)<1e-9;}),out.end());
    return out;
}
inline std::vector<double> curveConstraint(const Cubic& c,Point origin,bool tangent) {
    const auto a=coefficients(c,origin);
    std::vector<double> p(6,0);
    for(size_t i=0;i<4;++i)for(size_t j=1;j<4;++j)
        p[i+j-1]+=j*(tangent?cross(a[i],a[j]):dot(a[i],a[j]));
    return roots(p);
}
inline double arcLength(const Cubic& c,double lo,double hi,int depth=0) {
    auto speed=[&](double t){const auto d=derivative(c,t);return std::hypot(d.x,d.y);};
    const double mid=(lo+hi)/2,fa=speed(lo),fb=speed(hi),fm=speed(mid);
    const double coarse=(hi-lo)*(fa+4*fm+fb)/6;
    const double fine=(hi-lo)*(fa+4*speed((lo+mid)/2)+2*fm+4*speed((mid+hi)/2)+fb)/12;
    if(depth>=16||std::abs(fine-coarse)<1e-8*(1+fine))return fine+(fine-coarse)/15;
    return arcLength(c,lo,mid,depth+1)+arcLength(c,mid,hi,depth+1);
}
inline Point arcMidpoint(const Cubic& c) {
    const double half=arcLength(c,0,1)/2;double lo=0,hi=1;
    for(int n=0;n<35;++n) {double t=(lo+hi)/2;if(arcLength(c,0,t)<half)lo=t;else hi=t;}
    return at(c,(lo+hi)/2);
}
inline bool centroid(const std::vector<Cubic>& path,Point& center) {
    if(path.empty())return false;
    const Point origin=path.front().p[0];
    constexpr double nodes[]={0,0.5384693101056831,-0.5384693101056831,0.9061798459386640,-0.9061798459386640};
    constexpr double weights[]={0.5688888888888889,0.4786286704993665,0.4786286704993665,0.2369268850561891,0.2369268850561891};
    double twiceArea=0,xMoment=0,yMoment=0,absoluteArea=0;
    for(const auto& c:path)for(int i=0;i<5;++i) {
        const double t=(nodes[i]+1)/2,w=weights[i]/2;
        const Point p=sub(at(c,t),origin),d=derivative(c,t);
        const double area=w*cross(p,d);twiceArea+=area;absoluteArea+=std::abs(area);
        xMoment+=w*p.x*p.x*d.y;yMoment-=w*p.y*p.y*d.x;
    }
    if(std::abs(twiceArea)<=1e-12*std::max(1.0,absoluteArea))return false;
    center={origin.x+xMoment/twiceArea,origin.y+yMoment/twiceArea};return finite(center);
}
inline bool projection(Segment s,Point p,Point& result,bool clamp) {
    const Point d=sub(s.b,s.a);
    const double length2=dot(d,d);
    if (!finite(s.a)||!finite(s.b)||!finite(p)||length2<=1e-20) return false;
    double t=dot(sub(p,s.a),d)/length2;
    if (!clamp&&(t<0||t>1)) return false;
    result=at(s,std::clamp(t,0.0,1.0)); return finite(result);
}
inline bool intersection(Segment a,Segment b,Point& result) {
    const Point u=sub(a.b,a.a),v=sub(b.b,b.a),w=sub(b.a,a.a);
    const double determinant=cross(u,v),scale=std::sqrt(dot(u,u)*dot(v,v));
    if (!std::isfinite(scale)||scale<=1e-20||std::abs(determinant)<=1e-12*scale) return false;
    const double t=cross(w,v)/determinant,s=cross(w,u)/determinant;
    if(t<0||t>1||s<0||s>1) return false;
    result=at(a,t); return finite(result);
}
inline const char* label(unsigned mode) {
    switch(mode) {
        case Endpoint:return "Estremo"; case Midpoint:return "Punto medio";
        case Intersection:return "Intersezione"; case Perpendicular:return "Perpendicolare";
        case Center:return "Centro geometrico";case Quadrant:return "Estremo X/Y";case Tangent:return "Tangente";
        default:return "Punto vicino";
    }
}
inline Hit find(const Geometry& geometry,Point cursor,const Point* origin,
                Settings settings,double zoom,bool shift=false) {
    Hit best;
    if (!settings.enabled||!finite(cursor)||!std::isfinite(zoom)||zoom<=0||
        !std::isfinite(settings.pixels)||settings.pixels<=0) return best;
    const double tolerance=std::clamp(settings.pixels,1.0,32.0)/zoom;
    best.distance2=tolerance*tolerance;
    auto consider=[&](Point p,unsigned mode) {
        if(!finite(p))return;
        if(shift&&origin) {
            const Point d=sub(p,*origin);
            const double e=1e-9*std::max(1.0,std::hypot(d.x,d.y));
            if(std::abs(d.x)>e&&std::abs(d.y)>e&&std::abs(std::abs(d.x)-std::abs(d.y))>e)return;
        }
        const double d=distance2(p,cursor);
        if(d<=best.distance2&&(!best.found||d<best.distance2)) best={true,p,mode,d};
    };
    if(settings.modes&Endpoint)for(Point p:geometry.anchors)consider(p,Endpoint);
    if(settings.modes&Center)for(Point p:geometry.centers)consider(p,Center);
    std::vector<const Cubic*> nearbyCurves;
    for(const Cubic& c:geometry.curves) {
        double xmin=c.p[0].x,xmax=xmin,ymin=c.p[0].y,ymax=ymin;
        for(Point p:c.p) {xmin=std::min(xmin,p.x);xmax=std::max(xmax,p.x);ymin=std::min(ymin,p.y);ymax=std::max(ymax,p.y);}
        if(cursor.x<xmin-tolerance||cursor.x>xmax+tolerance||cursor.y<ymin-tolerance||cursor.y>ymax+tolerance)continue;
        nearbyCurves.push_back(&c);
        if(settings.modes&Midpoint)consider(c.midpoint,Midpoint);
        if(settings.modes&Quadrant) {
            const auto a=coefficients(c);
            for(int axis=0;axis<2;++axis) {
                auto ts=roots({axis?a[1].y:a[1].x,2*(axis?a[2].y:a[2].x),3*(axis?a[3].y:a[3].x)});
                for(double t:ts)if(dot(derivative(c,t),derivative(c,t))>1e-20)consider(at(c,t),Quadrant);
            }
        }
        if(origin)for(unsigned mode:{(unsigned)Perpendicular,(unsigned)Tangent})if(settings.modes&mode)
            for(double t:curveConstraint(c,*origin,mode==Tangent))
                if(dot(derivative(c,t),derivative(c,t))>1e-20&&distance2(at(c,t),*origin)>1e-20)consider(at(c,t),mode);
    }
    std::vector<const Segment*> nearby;
    for(const Segment& s:geometry.lines) {
        Point closest;
        if(!projection(s,cursor,closest,true)||distance2(closest,cursor)>tolerance*tolerance)continue;
        if(settings.modes&Midpoint)consider(at(s,0.5),Midpoint);
        if((settings.modes&Perpendicular)&&origin) {
            Point foot;
            if(projection(s,*origin,foot,false)&&distance2(foot,*origin)>1e-20)consider(foot,Perpendicular);
        }
        if(settings.modes&Intersection)nearby.push_back(&s);
    }
    // ponytail: pairwise within the aperture; add a spatial index if dense scenes make this slow.
    for(size_t i=0;i<nearby.size();++i)for(size_t j=i+1;j<nearby.size();++j) {
        Point p;if(intersection(*nearby[i],*nearby[j],p))consider(p,Intersection);
    }
    // Discrete targets take precedence over continuously available nearest points.
    if(!best.found&&(settings.modes&Nearest))for(const Segment& s:geometry.lines) {
        Point p;if(projection(s,cursor,p,true))consider(p,Nearest);
    }
    if((!best.found||best.mode==Nearest)&&(settings.modes&Nearest))for(const Cubic* curve:nearbyCurves) {
        const Cubic& c=*curve;
        consider(c.p[0],Nearest);consider(c.p[3],Nearest);
        for(double t:curveConstraint(c,cursor,false))consider(at(c,t),Nearest);
    }
    return best;
}
}
#endif
