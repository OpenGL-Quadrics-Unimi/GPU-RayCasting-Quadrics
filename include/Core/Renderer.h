#pragma once

class Renderer {
public:
    Renderer();
    
    ~Renderer(); // Destructor

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void initQuad();
    void drawQuad() const;

private:
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;
};