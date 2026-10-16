#include "gradient_noise.h"
#include <cmath>

namespace terrain {
namespace {
double fade(double t) { return t*t*t*(t*(t*6.0-15.0)+10.0); }
double lerp(double a, double b, double t) { return a + t * (b - a); }
uint64_t mix(uint64_t v) {
    v = (v ^ (v >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    v = (v ^ (v >> 27)) * UINT64_C(0x94d049bb133111eb);
    return v ^ (v >> 31);
}
}
uint64_t GradientNoise::hash(int64_t x, int64_t y, int64_t z) const {
    return mix(seed ^ mix(uint64_t(x) + UINT64_C(0x9e3779b97f4a7c15)) ^
            mix(uint64_t(y) + UINT64_C(0xd1b54a32d192ed03)) ^
            mix(uint64_t(z) + UINT64_C(0x8cb92baa3f3d8dd7)));
}
double GradientNoise::noise2(double x, double z) const {
    const int64_t ix = int64_t(std::floor(x)), iz = int64_t(std::floor(z));
    const double fx = x - double(ix), fz = z - double(iz);
    constexpr double q = 0.7071067811865475;
    constexpr double gradient[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{q,q},{-q,q},{q,-q},{-q,-q}};
    double row[2];
    for (int dz = 0; dz < 2; ++dz) {
        double values[2];
        for (int dx = 0; dx < 2; ++dx) {
            const auto &g = gradient[hash(ix+dx, 0, iz+dz) % 8];
            values[dx] = g[0]*(fx-dx) + g[1]*(fz-dz);
        }
        row[dz] = lerp(values[0], values[1], fade(fx));
    }
    return lerp(row[0], row[1], fade(fz)) * 1.5;
}
double GradientNoise::noise3(double x, double y, double z) const {
    const int64_t ix = int64_t(std::floor(x)), iy = int64_t(std::floor(y)), iz = int64_t(std::floor(z));
    const double fx=x-double(ix), fy=y-double(iy), fz=z-double(iz);
    constexpr int gradients[12][3] = {{1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
        {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},{0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}};
    double planes[2];
    for (int dz=0; dz<2; ++dz) {
        double rows[2];
        for (int dy=0; dy<2; ++dy) {
            double values[2];
            for (int dx=0; dx<2; ++dx) {
                const auto &g=gradients[hash(ix+dx,iy+dy,iz+dz)%12];
                values[dx]=(g[0]*(fx-dx)+g[1]*(fy-dy)+g[2]*(fz-dz))*0.7071067811865475;
            }
            rows[dy]=lerp(values[0],values[1],fade(fx));
        }
        planes[dz]=lerp(rows[0],rows[1],fade(fy));
    }
    return lerp(planes[0],planes[1],fade(fz))*1.5;
}
double GradientNoise::fbm2(double x,double z,int octaves) const {
    double result=0, weight=1, total=0;
    for (int i=0;i<octaves;++i) {
        result+=noise2(x,z)*weight; total+=weight;
        x*=2; z*=2; weight*=0.5;
    }
    return total > 0 ? result/total : 0;
}
double GradientNoise::fbm3(double x,double y,double z,int octaves) const {
    double result=0,weight=1,total=0;
    for (int i=0;i<octaves;++i) {
        result+=noise3(x,y,z)*weight; total+=weight;
        x*=2; y*=2; z*=2; weight*=0.5;
    }
    return total > 0 ? result/total : 0;
}
} // namespace terrain
