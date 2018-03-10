#include <OpenGP/GL/Application.h>
#include <OpenGP/external/LodePNG/lodepng.cpp>
#include <math.h>

using namespace OpenGP;

const int width=720, height=720;
typedef Eigen::Transform<float,3,Eigen::Affine> Transform;

const char* fb_vshader =
#include "fb_vshader.glsl"
;
const char* fb_fshader =
#include "fb_fshader.glsl"
;
const char* quad_vshader =
#include "quad_vshader.glsl"
;
const char* quad_fshader =
#include "quad_fshader.glsl"
;

const char* line_vshader =
#include "line_vshader.glsl"
;
const char* line_fshader =
#include "line_fshader.glsl"
;

#define POINTSIZE 10.0f

const float SpeedFactor = 1;
const float bezier_speed = 0.1f;
void init();
void quadInit(std::unique_ptr<GPUMesh> &quad);
void loadTexture(std::unique_ptr<RGBA8Texture> &texture, const char* filename);
void drawScene(float timeCount);

std::unique_ptr<Shader> lineShader;
std::unique_ptr<GPUMesh> line;
std::vector<Vec2> controlPoints;

std::unique_ptr<GPUMesh> quad;

std::unique_ptr<Shader> quadShader;
std::unique_ptr<Shader> fbShader;

std::unique_ptr<RGBA8Texture> left_wing;
std::unique_ptr<RGBA8Texture> right_wing;
std::unique_ptr<RGBA8Texture> stars;

std::unique_ptr<RGBA8Texture> bat_right_wing;
std::unique_ptr<RGBA8Texture> bat_left_wing;
std::unique_ptr<RGBA8Texture> bat_body;

/// TODO: declare Framebuffer and color buffer texture
std::unique_ptr <Framebuffer> fb;
std::unique_ptr <RGBA8Texture> c_buf;


//the bezier curve gets passed in the 4 control points and a t value representing how far along the curve it is
Vec2 bezier(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, float t) {
    Vec2 point = Vec2(0.0f, 0.0f);
    point(0) = pow(1 - t, 3) * p0(0) +  pow(1 - t, 2) * 3 * t * p1(0) + (1 - t) * 3 * pow(t, 2)* p2(0) +  pow(t, 3)* p3(0);
    point(1) = pow(1 - t, 3) * p0(1) + pow(1 - t, 2) * 3 * t * p1(1) + (1 - t) * 3 * pow(t, 2) * p2(1) + pow(t, 3) * p3(1);
    return point;
}


int main(int, char**){

    Application app;
    init();

    //initializes framebuffer
    fb = std::unique_ptr<Framebuffer>(new Framebuffer());
    //initializes color buffer texture, and allocate memory
    c_buf = std::unique_ptr<RGBA8Texture>(new RGBA8Texture());
    c_buf->allocate(width, height);

    //attaches color texture to framebuffer
    fb->attach_color_texture(*c_buf);

    // Mouse position and selected point
    Vec2 position = Vec2(0,0);
    Vec2 *selection = nullptr;




    Window& window = app.create_window([&](Window&){
        glViewport(0,0,width,height);

        //First draws the scene onto framebuffer
        // binds and then unbind framebuffer
        fb->bind();
        glClear(GL_COLOR_BUFFER_BIT);

        drawScene(glfwGetTime());
        fb->unbind();

        // Render sto Window
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        fbShader->bind();
        //Binds texture and set uniforms
        glActiveTexture(GL_TEXTURE0);
        c_buf->bind();
        fbShader->set_uniform("tex", 0);
        fbShader->set_uniform("tex_width", float(width));
        fbShader->set_uniform("tex_height", float(height));



        quad->set_attributes(*fbShader);
        quad->draw();
        fbShader->unbind();


        glPointSize(POINTSIZE);
        lineShader->bind();
        // Draws line red
        lineShader->set_uniform("selection", -1);
        line->set_attributes(*lineShader);
        line->set_mode(GL_LINE_STRIP);
        line->draw();

        // Draw points red and selected point blue
        if(selection!=nullptr) lineShader->set_uniform("selection", int(selection-&controlPoints[0]));
        line->set_mode(GL_POINTS);
        line->draw();

        lineShader->unbind();

    });
    window.set_title("FrameBuffer");
    window.set_size(width, height);

    // Mouse movement callback
    window.add_listener<MouseMoveEvent>([&](const MouseMoveEvent &m){
            // Mouse position in clip coordinates
            Vec2 p = 2.0f*(Vec2(m.position.x()/width,-m.position.y()/height) - Vec2(0.5f,-0.5f));
            if( selection && (p-position).norm() > 0.0f) {
                /// TODO: Make selected control points move with cursor
                selection->x() = position.x();
                selection->y() = position.y();
                line->set_vbo<Vec2>("vposition", controlPoints);
            }
            position = p;
        });

        // Mouse click callback
        window.add_listener<MouseButtonEvent>([&](const MouseButtonEvent &e){
            // Mouse selection case
            if( e.button == GLFW_MOUSE_BUTTON_LEFT && !e.released) {
                selection = nullptr;
                for(auto&& v : controlPoints) {
                    if ( (v-position).norm() < POINTSIZE/std::min(width,height) ) {
                        selection = &v;
                        break;
                    }
                }
            }
            // Mouse release case
            if( e.button == GLFW_MOUSE_BUTTON_LEFT && e.released) {
                if(selection) {
                    selection->x() = position.x();
                    selection->y() = position.y();
                    selection = nullptr;
                    line->set_vbo<Vec2>("vposition", controlPoints);
                }
            }
        });

    return app.run();
}

void init(){
    glClearColor(1,1,1, /*solid*/1.0 );

    fbShader = std::unique_ptr<Shader>(new Shader());
    fbShader->verbose = true;
    fbShader->add_vshader_from_source(fb_vshader);
    fbShader->add_fshader_from_source(fb_fshader);
    fbShader->link();

    quadShader = std::unique_ptr<Shader>(new Shader());
    quadShader->verbose = true;
    quadShader->add_vshader_from_source(quad_vshader);
    quadShader->add_fshader_from_source(quad_fshader);
    quadShader->link();

    quadInit(quad);

    loadTexture(left_wing, "dragon_wing.png");
    loadTexture(right_wing, "dragon_wing.png");
    loadTexture(bat_body, "bat_body.png");
    loadTexture(stars, "background.png");


    //line stuff

    lineShader = std::unique_ptr<Shader>(new Shader());
    lineShader->verbose = true;
    lineShader->add_vshader_from_source(line_vshader);
    lineShader->add_fshader_from_source(line_fshader);
    lineShader->link();

    //4 bezier control points
    controlPoints.push_back(Vec2(-0.7f,-0.2f));
    controlPoints.push_back(Vec2(-0.3f, 0.2f));
    controlPoints.push_back(Vec2( 0.3f, 0.5f));
    controlPoints.push_back(Vec2( 0.7f, 0.0f));

    line = std::unique_ptr<GPUMesh>(new GPUMesh());
    line->set_vbo<Vec2>("vposition", controlPoints);
    std::vector<unsigned int> indices = {0,1,2,3};
    line->set_triangles(indices);
}


//inits a mesh with vectors, a face and vertex coordinates
void quadInit(std::unique_ptr<GPUMesh> &quad) {
    quad = std::unique_ptr<GPUMesh>(new GPUMesh());
    std::vector<Vec3> quad_vposition = {
        Vec3(-1, -1, 0),
        Vec3(-1,  1, 0),
        Vec3( 1, -1, 0),
        Vec3( 1,  1, 0)
    };
    quad->set_vbo<Vec3>("vposition", quad_vposition);
    std::vector<unsigned int> quad_triangle_indices = {
        0, 2, 1, 1, 2, 3
    };
    quad->set_triangles(quad_triangle_indices);
    std::vector<Vec2> quad_vtexcoord = {
        Vec2(0, 0),
        Vec2(0,  1),
        Vec2( 1, 0),
        Vec2( 1,  1)
    };
    quad->set_vtexcoord(quad_vtexcoord);
}

//loads the texture
void loadTexture(std::unique_ptr<RGBA8Texture> &texture, const char *filename) {
    // Used snippet from https://raw.githubusercontent.com/lvandeve/lodepng/master/examples/example_decode.cpp
    std::vector<unsigned char> image; //the raw pixels
    unsigned width, height;
    //decode
    unsigned error = lodepng::decode(image, width, height, filename);
    //if there's an error, display it
    if(error) std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
    //the pixels are now in the vector "image", 4 bytes per pixel, ordered RGBARGBA..., use it as texture, draw it, ...

    // unfortunately they are upside down...lets fix that
    unsigned char* row = new unsigned char[4*width];
    for(int i = 0; i < int(height)/2; ++i) {
        memcpy(row, &image[4*i*width], 4*width*sizeof(unsigned char));
        memcpy(&image[4*i*width], &image[image.size() - 4*(i+1)*width], 4*width*sizeof(unsigned char));
        memcpy(&image[image.size() - 4*(i+1)*width], row, 4*width*sizeof(unsigned char));
    }
    delete row;

    texture = std::unique_ptr<RGBA8Texture>(new RGBA8Texture());
    texture->upload_raw(width, height, &image[0]);
}


//draws the various objects
void drawScene(float timeCount)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float t = timeCount * SpeedFactor;

    //background
    Transform background_transform = Transform::Identity();
    //background.draw(TRS.matrix());
    quadShader->bind();
    quadShader->set_uniform("M", background_transform.matrix());
    // Make texture unit 0 active
    glActiveTexture(GL_TEXTURE0);
    // Bind the texture to the active unit for drawing
    stars->bind();
    // Set the shader's texture uniform to the index of the texture unit we have
    // bound the texture to
    quadShader->set_uniform("tex", 0);
    quad->set_attributes(*quadShader);
    quad->draw();
    stars->unbind();


    //bezier value
    float bezier_value = (glfwGetTime()*bezier_speed)-(int)(glfwGetTime()*bezier_speed);
    Vec2 new_point = bezier(controlPoints[0], controlPoints[1], controlPoints[2], controlPoints[3],bezier_value);



    //transform of the bat location. must be calculated first so it can be applied to its wings
    Transform bat_transform = Transform::Identity();
    bat_transform *= Eigen::Translation3f(new_point(0), new_point(1), 0);
    bat_transform *= Eigen::AlignedScaling3f(0.2f, 0.2f, 1);
    float xcord = 3*std::cos(t);
    float ycord = 3*std::sin(t);
    bat_transform *= Eigen::AngleAxisf(t, Eigen::Vector3f::UnitZ());
    //bat_transform *= Eigen::AlignedScaling3f(0.2f, 0.2f, 1);




    //drawing bat wings
    Transform left_wing_transform = bat_transform;

    xcord = std::cos(std::cos(t*3));
    ycord = std::sin(std::cos(t*3));
    left_wing_transform *= Eigen::Translation3f(xcord, ycord, 0);
    left_wing_transform *= Eigen::AngleAxisf(std::cos(t*3), Eigen::Vector3f::UnitZ());
    left_wing_transform *= Eigen::AlignedScaling3f(-1, 1, 1);

    quadShader->bind();
    quadShader->set_uniform("M", left_wing_transform.matrix());
    // Make texture unit 0 active
    glActiveTexture(GL_TEXTURE0);
    // Bind the texture to the active unit for drawing
    left_wing->bind();
    // Set the shader's texture uniform to the index of the texture unit we have
    // bound the texture to
    quadShader->set_uniform("tex", 0);
    quad->set_attributes(*quadShader);
    quad->draw();
    left_wing->unbind();
    quadShader->unbind();

    Transform right_wing_transform = bat_transform;

    xcord = -1*std::cos(std::cos(t*3+M_PI));
    ycord = -1*std::sin(std::cos(t*3+M_PI));
    right_wing_transform *= Eigen::Translation3f(xcord, ycord, 0);
    right_wing_transform *= Eigen::AngleAxisf(std::cos(t*3+M_PI), Eigen::Vector3f::UnitZ());
    right_wing_transform *= Eigen::AlignedScaling3f(1, 1, 1);

    quadShader->bind();
    quadShader->set_uniform("M", right_wing_transform.matrix());
    // Make texture unit 0 active
    glActiveTexture(GL_TEXTURE0);
    // Bind the texture to the active unit for drawing
    left_wing->bind();
    // Set the shader's texture uniform to the index of the texture unit we have
    // bound the texture to
    quadShader->set_uniform("tex", 0);
    quad->set_attributes(*quadShader);
    quad->draw();
    left_wing->unbind();
    quadShader->unbind();


    //draw bat body
    quadShader->bind();
    quadShader->set_uniform("M", bat_transform.matrix());
    // Make texture unit 0 active
    glActiveTexture(GL_TEXTURE0);
    // Bind the texture to the active unit for drawing
    bat_body->bind();
    // Set the shader's texture uniform to the index of the texture unit we have
    // bound the texture to
    quadShader->set_uniform("tex", 0);
    quad->set_attributes(*quadShader);
    quad->draw();
    bat_body->unbind();


    glDisable(GL_BLEND);
}
