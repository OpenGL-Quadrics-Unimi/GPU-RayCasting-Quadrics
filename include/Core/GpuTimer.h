#pragma once
#include <glad/glad.h>

// GPU timer using GL_TIME_ELAPSED with two buffered queries.
// Wrap one pass with begin() and end(). Then readMs() returns the
// time of the previous frame, so the CPU never waits on the GPU.
struct GpuTimer {
    GLuint q[2] = {0, 0};
    int    cur  = 0;
    bool   primed = false;

    void create()  { glGenQueries(2, q); }
    void destroy() { if (q[0]) { glDeleteQueries(2, q); q[0] = q[1] = 0; } }

    void begin() { glBeginQuery(GL_TIME_ELAPSED, q[cur]); }
    void end()   { glEndQuery(GL_TIME_ELAPSED); }

    // Reads the other query (last frame). First call returns 0.
    double readMs() {
        int other = 1 - cur;
        if (!primed) { primed = true; cur = other; return 0.0; }
        GLuint64 ns = 0;
        glGetQueryObjectui64v(q[other], GL_QUERY_RESULT, &ns);
        cur = other;
        return ns / 1.0e6;
    }
};
