#pragma once
#include <cstdint>

// Geometric curve types used to construct custom brush shapes.
enum class CurveType : uint8_t {
    Circle = 0,
    Square = 1,
    Triangle = 2,
};

inline int curveTypeCount() { return 3; }

inline const char* curveName(CurveType t) {
    switch (t) {
        case CurveType::Circle:   return "Circle";
        case CurveType::Square:   return "Square";
        case CurveType::Triangle: return "Triangle";
    }
    return "Circle";
}

// Deterministic, commutative combination of two curve types.
//   Triangle + Triangle -> Square
//   Square   + Square   -> Circle
//   Circle   + Circle   -> Square
//   Triangle + Square   -> Triangle
//   Triangle + Circle   -> Triangle
//   Circle   + Square   -> Circle
inline CurveType combineCurves(CurveType a, CurveType b) {
    if (a == b) {
        switch (a) {
            case CurveType::Triangle: return CurveType::Square;
            case CurveType::Square:   return CurveType::Circle;
            case CurveType::Circle:   return CurveType::Square;
        }
    }
    bool hasTriangle = (a == CurveType::Triangle || b == CurveType::Triangle);
    if (hasTriangle) return CurveType::Triangle;
    return CurveType::Circle; // Circle + Square
}
