/* 
*   Transform component for LaMancha Engine
*   Copyright (C) 2025 Alonso Moreno <ph0nsy> 
*
*   
*/ 

#pragma once
#include "mathLM.h"

namespace LaMancha {

struct Transform2D {
    Vec2 position;
    f32 zOrder;
    f32 rotation;  // radians 
    Vec2 scale;
    
    Transform2D() : position(0, 0), zOrder(0), rotation(0), scale(1, 1) {}
    
    // Convert to 3x3 matrix for rendering
    void toMatrix(f32* out_matrix) const {

        // 3x3 2D transformation matrix (column-major for OpenGL)
        // [ cos(r)*sx  -sin(r)*sy   tx ]
        // [ sin(r)*sx   cos(r)*sy   ty ]
        // [     0           0        1 ]

        f32 cos = LaMancha::fcos(rotation);
        f32 sin = LaMancha::fsin(rotation);
        
        out_matrix[0] = cos * scale.x;
        out_matrix[1] = sin * scale.x;
        out_matrix[2] = 0.f;
        
        out_matrix[3] = -sin * scale.y;
        out_matrix[4] = cos * scale.y;
        out_matrix[5] = 0.f;
        
        out_matrix[6] = position.x;
        out_matrix[7] = position.y;
        out_matrix[8] = 1.f;
    }
};

}