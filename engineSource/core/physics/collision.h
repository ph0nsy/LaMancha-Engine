/* collisionLM.h - Pure Logic */
#pragma once
#include "mathLM.h"

namespace LaMancha {
namespace Collision {

    // --- Primitives ---

    LAMANCHA_INLINE bool getCollisionAABB(const AABB& _a, const AABB& _b) {
        return (_a.right >= _b.left && _a.left <= _b.right && 
                _a.bottom >= _b.top && _a.top <= _b.bottom);
    }

    LAMANCHA_INLINE bool pointVsCircle(const Vec2& _p, const Circle& _c, const Transform& _t) {
        Vec2 worldPos = _t.getWorldLoc();
        return (_p - worldPos).lengthSq() < (_c.radius * _c.radius);
    }

    LAMANCHA_INLINE bool pointVsRect(const Vec2& _p, const Rect& _r, const Transform& _t) { 
        Vec2 worldPos = _t.getWorldLoc();
        return  _p.x >= worldPos.x && _p.x <= worldPos.x + _r.width && 
                _p.y >= worldPos.y && _p.y <= worldPos.y + _r.height; 
    }

    LAMANCHA_INLINE bool pointVsTriangle(const Vec2& _p, const Triangle& _tg, const Transform& _tm) { 
        Vec2 vtx_t[3];
        Vec2 worldPos = _tm.getWorldLoc(); // center

        vtx_t[0] = _tg.vtx[0] + worldPos;
        vtx_t[1] = _tg.vtx[1] + worldPos;
        vtx_t[2] = _tg.vtx[2] + worldPos;

        // using barycentric coordinates - analytical solution 
        f32 ar = _tg.area();
        f32 s = 1/(2*ar)*(vtx_t[0].y*vtx_t[2].x - vtx_t[0].x*vtx_t[2].y + (vtx_t[2].y - vtx_t[0].y)*_px + (vtx_t[0].x - vtx_t[2].x)*_py);
        f32 t = 1/(2*ar)*(vtx_t[0].x*vtx_t[2].y - vtx_t[0].y*vtx_t[2].x + (vtx_t[0].y - vtx_t[2].y)*_px + (vtx_t[2].x - vtx_t[0].x)*_py);
        
        return s > 0 && t > 0 && 1-(s+t) > 0;
    }

    static AABB getAABB(const Circle& _c, const Transform& _t) {
        Vec2 worldPos = _t.getWorldLoc(); // center
        return { worldPos.x - c.radius, worldPos.x + c.radius, 
                 worldPos.y - c.radius, worldPos.y + c.radius };
    }
    
    static AABB getAABB(const Rect& _r, const Transform& _t) {
        Vec2 worldPos = _t.getWorldLoc() - Vec2(_r.width / 2.f, _r.height / 2.f); // center
        return { worldPos.x, worldPos.x + _r.width, 
                 worldPos.y, worldPos.y + _r.height };
    }

    static AABB getAABB(const Triangle& _tg, const Transform& _tm) {
        Vec2 vtx_t[3];
        Vec2 worldPos = _t.getWorldLoc(); // center

        vtx_t[0] = _tg.vtx[0] + worldPos;
        vtx_t[1] = _tg.vtx[1] + worldPos;
        vtx_t[2] = _tg.vtx[2] + worldPos;

        f32 minX = vtx_t[0].x, maxX = vtx_t[0].x;
        f32 minY = vtx_t[0].y, maxY = vtx_t[0].y;

        for (i8 i = 1; i < 3; i++) {
            minX = minX < vtx_t[i].x ? minX : vtx_t[i].x;
            maxX = maxX > vtx_t[i].x ? maxX : vtx_t[i].x;
            minY = minY < vtx_t[i].y ? minY : vtx_t[i].y;
            maxY = maxY > vtx_t[i].y ? maxY : vtx_t[i].y;
        }

        minX += worldPos.x;  
        maxX += worldPos.x;
        minY += worldPos.y;  
        maxY += worldPos.y;

        return { minX, maxX, minY, maxY };
    }

    // --- Shape vs Shape ---

    bool CircleVsCircle(const Circle& a, const Circle& b) {
        f32 r = a.radius + b.radius;
        return (a.center - b.center).lengthSq() < (r * r);
    }

    bool RectVsRect(const Rect& a, const Rect& b) {
        return (a.position.x < b.position.x + b.width &&
                a.position.x + a.width > b.position.x &&
                a.position.y < b.position.y + b.height &&
                a.position.y + a.height > b.position.y);
    }

    bool CircleVsRect(const Circle& c, const Rect& r) {
        // Find the closest point on the rectangle to the circle
        f32 closestX = fclamp(c.center.x, r.position.x, r.position.x + r.width);
        f32 closestY = fclamp(c.center.y, r.position.y, r.position.y + r.height);

        // Calculate the distance between the circle's center and this closest point
        f32 dX = c.center.x - closestX;
        f32 dY = c.center.y - closestY;

        // If the distance is less than the circle's radius, an intersection occurs
        return (dX * dX + dY * dY) < (c.radius * c.radius);
    }
}
}