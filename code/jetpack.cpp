// TODO
// convert object dimensions into variables and replace wherever necessary ---- done
// levels --- done
// text rendering --- done
// game over,game start screen --- done
// change shape of lazer --- done
// improve the randomized coin path
// if possible : rotating lazers
// change colors

// increasing difficulty increase speed
// to increase speed range make lazers smaller

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <cmath>

#include "stb_image.h"
#include "shader.h"
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H


#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void RenderText(Shader &shader, std::string text, float x, float y, float scale, glm::vec3 color);

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 500;


/// Holds all state information relevant to a character as loaded using FreeType
struct Character {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2   Size;      // Size of glyph
    glm::ivec2   Bearing;   // Offset from baseline to left/top of glyph
    unsigned int Advance;   // Horizontal offset to advance to next glyph
};

std::map<GLchar, Character> Characters;







// variables to keep track of -------------------------------------

// for jetpack 
float upAcc=6;
float downAcc=2.0;
float last_frame_time_ref=0;
float cur_frame_time_ref=0;
float current_speed=0;
int fly=0;
float curr_y=-0.55;


// for lazers
float lzLength=0.4; // lazer length
const int lazerCount=6; // formula for this is (int)2/speed*spacingTime + 1 // max number of lazers that can fit
float speed=0.2;
glm::mat4 configs[lazerCount];
float spacingTime=(lzLength/speed);  // spacingTime*speed >= length of lazer
float renderTime; // time when the last lazer render happened
int render_till=0; // keeps track of which lazer to render till initially, when all lazers are not configured
int destroyLazer=0; // keeps track of which lazer to reuse and generate a new lazer config in its place


// for coins // same speed as lazers
const int coinCount=40; // formula is 2/coin-width //max coins which can fit in the viewport
glm::mat4 coinConfigs[coinCount];
float spacingCoinTime=(0.05/speed)+0.5*(0.05/speed); // formula is coin-width/speed
float renderCoinTime;
int render_till_coin=0;
int destroyCoin=0;
float coinSeqHeight=0.7;
float verticalSeqMov=0.1;
int coinsCollected=0;

// HUD
int level=1;
float distance=0;
int frames_for_dist=0;
float level_start_time;
int level_duration=30;
int max_level=5; 
const float level_disp_time=1.5;
const float gm_over_time=6;

// textRendering
unsigned int textVAO,textVBO;



// shaders ----------------------------------
const char *jetpackVertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 flyTrans;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = flyTrans*vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
const char *lazerVertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 lazerMov;\n"
    "uniform mat4 lazerConfig;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = lazerMov*lazerConfig*vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";
const char *coinVertexShaderSource = "#version 330 core\n"
                                      "layout (location = 0) in vec3 aPos;\n"
                                      "uniform mat4 coinMov;\n"
                                      "uniform mat4 coinConfig;\n"
                                      "void main()\n"
                                      "{\n"
                                      "   gl_Position = coinMov*coinConfig*vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                      "}\0";
// redundant coin shader(same as vertex shader)
//common frag shader for jetpack,lazer,coins etc
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 ourColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = ourColor;\n"
    "}\n\0";






// compiles and links together the shaders to create a shader program
unsigned int createShadProgram(const char* vertexShaderS,const char* fragShaderS)
{
    // build and compile our shader program for the jetpack
    // ------------------------------------
    // vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderS, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragShaderS, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // link shaders
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}



// creates a VAO of vertex 3D locations, considering fragShader takes care of color
// (doesnt support colors for each vertex
unsigned int createVAO(float* vertices,unsigned int* indices,int vertexAtrribLoc,unsigned long verticesSize,unsigned long indicesSize)
{
    unsigned int VAO,VBO,EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    // init the VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, verticesSize, vertices, GL_STATIC_DRAW);
    
    // init the EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesSize, indices, GL_STATIC_DRAW);

    // declare attributes
    glVertexAttribPointer(vertexAtrribLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(vertexAtrribLoc);  

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 

    return VAO;
}



// calculates the jetpacks current y-position,takes care of height bounding
void calculateJetpackY(float maxHeight)
{
    float upA;
    float timeDelta = (cur_frame_time_ref - last_frame_time_ref);
    if (fly)
    {
        upA = upAcc;
    }
    else
    {
        upA = 0;
    }
    current_speed += (upA - downAcc) * timeDelta;
    curr_y += (current_speed * timeDelta);

    if (curr_y > maxHeight)
    {
        curr_y = maxHeight;
        current_speed = 0;
    }
    else if (curr_y < -1*maxHeight)
    {
        curr_y = -maxHeight;
        current_speed = 0;
    }
}




//generates a random configuration for a lazer and return the config matrix
// if not erratic generates sequentially wrt coinSeqHeight
glm::mat4 generateRandomConfig(int rotate,float heightRange,int erratic)
{
    glm::mat4 randomConfig;
    randomConfig = glm::mat4(1.0f);
    float vertical_loc;

    if(erratic) // for truly random generation
    {
        vertical_loc=(((float)rand()*heightRange) / (RAND_MAX / 2) - heightRange);
    }
    else
    {
        int verticalDir; // up or down
        if(rand() < (RAND_MAX*0.4)) // 0.4 probability
            verticalDir=-1;
        else if(rand() < (RAND_MAX*0.6)) // 0.2 probability
            verticalDir=0;
        else
            verticalDir=1; // 0.4 probality
        
        vertical_loc=coinSeqHeight+(verticalDir*verticalSeqMov);
        // checking for out of bounds
        vertical_loc=(vertical_loc > heightRange)?heightRange:vertical_loc;
        vertical_loc=(vertical_loc < -heightRange)?(-heightRange):vertical_loc;

        coinSeqHeight=vertical_loc;
    }
    randomConfig = glm::translate(randomConfig, glm::vec3(1.0f, 1.0f * vertical_loc, 0.0f));



    if(rotate)
    {
        float rot_angle = glm::radians((float)rand());
        randomConfig = glm::rotate(randomConfig, rot_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    }
    return randomConfig;
}



// keeps the config array updated so the lazer/coin render loop renders the correct lazers/coins
void updateConfigArray(int coin)
{
    if(coin)
    {
        // taking care of getting the coinCount coins on the screen intitially
        if ((render_till_coin < coinCount) && ((glfwGetTime() - renderCoinTime) >= spacingCoinTime))
        {
            // making the config matrix
            coinConfigs[render_till_coin] = generateRandomConfig(0,0.8,0);
            renderCoinTime = glfwGetTime();
            render_till_coin++;
        }
        // the rest of the game
        else if ((render_till_coin >= coinCount) && ((glfwGetTime() - renderCoinTime) >= spacingCoinTime))
        {
            destroyCoin = (destroyCoin % (coinCount));
            coinConfigs[destroyCoin] = generateRandomConfig(0,0.8,0);
            renderCoinTime = glfwGetTime();
            destroyCoin++;
        }
    }
    else
    {
        // taking care of getting the lazerCount lazers on the screen intitially
        if ((render_till < lazerCount) && ((glfwGetTime() - renderTime) >= spacingTime))
        {
            // making the config matrix
            configs[render_till] = generateRandomConfig(1,0.6,1);
            renderTime = glfwGetTime();
            render_till++;
        }
        // the rest of the game
        else if ((render_till >= lazerCount) && ((glfwGetTime() - renderTime) >= spacingTime))
        {
            destroyLazer = (destroyLazer % (lazerCount));
            configs[destroyLazer] = generateRandomConfig(1,0.6,1);
            renderTime = glfwGetTime();
            destroyLazer++;
        }
    }
}



int checkSideColl(glm::vec3 edgePointa,glm::vec3 edgePointb,glm::vec3 colliderPoint,float accuracy)
{
    float total=glm::length(edgePointa-edgePointb);
    float part1=glm::length(edgePointa-colliderPoint);
    float part2=glm::length(edgePointb-colliderPoint);

    if(((part1 + part2)-(total)) <= accuracy)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}











int main()
{

    // initialization -------------------------------------------------------------------
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Jetpack Arcade", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }





    // setting up text rendering---------------------------------------------------

    // OpenGL state
    // ------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // compile and setup the text shader
    // ----------------------------
    Shader textShader("../src/text.vs", "../src/text.fs");
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH), 0.0f, static_cast<float>(SCR_HEIGHT));
    textShader.use();
    glUniformMatrix4fv(glGetUniformLocation(textShader.ID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));


    // FreeType
    // --------
    FT_Library ft;
    // All functions return a value different than 0 whenever an error occurred
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

	// find path to font
    std::string font_name = "../fonts/slkscr.ttf";
    if (font_name.empty())
    {
        std::cout << "ERROR::FREETYPE: Failed to load font_name" << std::endl;
        return -1;
    }
	
	// load font as face
    FT_Face face;
    if (FT_New_Face(ft, font_name.c_str(), 0, &face)) {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }
    else {
        // set size to load glyphs as
        FT_Set_Pixel_Sizes(face, 0, 48);

        // disable byte-alignment restriction
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // load first 128 characters of ASCII set
        for (unsigned char c = 0; c < 128; c++)
        {
            // Load character glyph 
            if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }
            // generate texture
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            // set texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // now store character for later use
            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    // destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // configure VAO/VBO for texture quads
    // -----------------------------------
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);





    // shaderProgram config
    unsigned int jpShaderProgram=createShadProgram(jetpackVertexShaderSource,fragmentShaderSource);
    unsigned int lazerShaderProgram=createShadProgram(lazerVertexShaderSource,fragmentShaderSource);
    unsigned int coinShaderProgram=createShadProgram(coinVertexShaderSource,fragmentShaderSource);




    // set up vertex data (and buffer(s)) and configure vertex attributes-------------------------------------
    // Jetpack
    float jetpackVertices[] = {
     -0.7f,  -0.5f, 0.0f,  // top right
     -0.7f, -0.6f, 0.0f,  // bottom right
    -0.75f, -0.6f, 0.0f,  // bottom left
    -0.75f,  -0.5f, 0.0f   // top left 
    };
    unsigned int jetpackIndices[] = {  // note that we start from 0!
        0, 1, 3,   // first triangle
        1, 2, 3    // second triangle
    };  

    // lazers
    // float lazerVertices[] = {
    //  0.05f,  0.0f, 0.0f,  // right
    //  -0.05f, 0.0f, 0.0f,  // left
    // 0.0f, lzLength/2, 0.0f,  // top
    // 0.0f,  -lzLength/2, 0.0f   // bottom 
    // };
    // unsigned int lazerIndices[] = {  // note that we start from 0!
    //     0, 1, 3,   // first triangle
    //     0, 1, 2    // second triangle
    // };
    // budweiser lazers
    float lazerVertices[] = {
     0.05f,  lzLength/2, 0.0f,  // right top
     -0.05f, lzLength/2, 0.0f,  // left top
    0.05f, -lzLength/2, 0.0f,  // right bottom
    -0.05f,  -lzLength/2, 0.0f,   // left bottom
    0.0f,   0.0f,       0.0f   // middle
    };
    unsigned int lazerIndices[] = {  // note that we start from 0!
        0, 1, 4,   // first triangle
        2, 3, 4    // second triangle
    };

    // coins(triangles)
    float coinVertices[] = {
     0.025f,  -0.025f, 0.0f,  // right
    -0.025f,  -0.025f, 0.0f,   // left
    0.0f,    0.025f,  0.0f   // top 
    };
    unsigned int coinIndices[] = {  // note that we start from 0!
        0, 1, 2   // first triangle
    };  




    // creating a VAO
    //jetpack
    unsigned int jpVAO=createVAO(jetpackVertices,jetpackIndices,0,sizeof(jetpackVertices),sizeof(jetpackIndices));
    //lazer
    unsigned int lazerVAO=createVAO(lazerVertices,lazerIndices,0,sizeof(lazerVertices),sizeof(lazerIndices));
    //coins
    unsigned int coinVAO=createVAO(coinVertices,coinIndices,0,sizeof(coinVertices),sizeof(coinIndices));



    // setting some meta data
    last_frame_time_ref=glfwGetTime();
    srand(time(0));
    renderTime=glfwGetTime();
    renderCoinTime=renderTime;
    level_start_time=renderTime;


    int count=0;
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        cur_frame_time_ref=glfwGetTime();

    



        // check for collisions---------------------------------------------------------

        // looping through all rendered lazers
        int collided=0;
        float accuracy=0.001;
        glm::vec3 jpTopR=glm::vec3(-0.7f,curr_y+0.05,0.0f);
        glm::vec3 jpBottomR=glm::vec3(-0.7f,curr_y-0.05,0.0f);
        glm::vec3 jpTopL=glm::vec3(-0.75f,curr_y+0.05,0.0f); 
        glm::vec3 jpBottomL=glm::vec3(-0.75f,curr_y-0.05,0.0f); 
        glm::vec3 curjpVertices[4]={jpTopR,jpBottomR,jpTopL,jpBottomL}; //makes its easier to check collisons
        for(int i=0;i<render_till;i++)
        {   
            // glm::vec3 currLTop=glm::vec3((configs[i]*glm::vec4(0.0f, lzLength/2, 0.0f,1.0f)));
            // glm::vec3 currLBottom=glm::vec3((configs[i]*glm::vec4(0.0f, -lzLength/2, 0.0f,1.0f)));
            // glm::vec3 currLRight=glm::vec3((configs[i]*glm::vec4(0.05f,  0.0f, 0.0f,1.0f)));
            // glm::vec3 currLLeft=glm::vec3((configs[i]*glm::vec4(-0.05f,  0.0f, 0.0f,1.0f)));
            glm::vec3 currLTopR=glm::vec3((configs[i]*glm::vec4(0.05f, lzLength/2, 0.0f,1.0f)));
            glm::vec3 currLTopL=glm::vec3((configs[i]*glm::vec4(-0.05f, lzLength/2, 0.0f,1.0f)));
            glm::vec3 currLBottomR=glm::vec3((configs[i]*glm::vec4(0.05f, -lzLength/2, 0.0f,1.0f)));
            glm::vec3 currLBottomL=glm::vec3((configs[i]*glm::vec4(-0.05f, -lzLength/2, 0.0f,1.0f)));
            glm::vec3 currLMiddle=glm::vec3((configs[i]*glm::vec4(0.0f, 0.0f, 0.0f,1.0f)));
            glm::vec3 curlzrVertices[5]={currLTopR,currLTopL,currLMiddle,currLBottomL,currLBottomR};              
            
            for(int j=0;j<4;j++)
            {
                for(int k=0;k<5;k++)
                {
                    collided=(collided || checkSideColl(curlzrVertices[k],curlzrVertices[(k+1)%5],curjpVertices[j],accuracy));
                }
            }
            if(collided==1)
            {
                break;
            }                                                                                                                                                                                                                                                                                                                                       
        }
        if(collided==1)
        {
            break;
        }


        // looping through all rendered coins
        float jpRightx=-0.7;
        float jpLeftx=-0.75;
        float jpTopy=curr_y+0.05;
        float jpBottomy=curr_y-0.05;
        for(int i=0;i<render_till_coin;i++)
        {
            float coinTopy=(coinConfigs[i]*glm::vec4(0.0f,0.025f,0.0f,1.0f)).y;
            float coinBottomy=(coinConfigs[i]*glm::vec4(0.0f,-0.025f,0.0f,1.0f)).y;
            float coinRightx=(coinConfigs[i]*glm::vec4(0.025f,0.0f,0.0f,1.0f)).x;
            float coinLeftx=(coinConfigs[i]*glm::vec4(-0.025f,0.0f,0.0f,1.0f)).x;

            bool collisionX=((jpRightx > coinLeftx) && (coinRightx > jpLeftx));
            bool collisionY=((jpTopy > coinBottomy) && (coinTopy > jpBottomy));

            // collision took place
            if(collisionX && collisionY)
            {
                coinsCollected++;
                //std::cout << coinsCollected << std::endl;
                coinConfigs[i]=glm::mat4(1.0f); // indicates it got collected
                break;
            }
        }






        // increasing difficulty
        if((glfwGetTime()-level_start_time) >= level_duration)
        {
            level_start_time=glfwGetTime();
            level++;
            speed+=0.2;
            spacingTime=(lzLength/speed);
            spacingCoinTime=(0.05/speed)+0.5*(0.05/speed);

            // changing jetpack speed, since speed changed
            int diff=upAcc-downAcc;
            downAcc++;
            upAcc=(diff+1)+downAcc;

            //exiting if max level has been reached
            if(level==(max_level+1))
            {
                break;
            }
        }
        // displaying the level
        if((glfwGetTime()-level_start_time)<=level_disp_time)
        {
            glEnable(GL_CULL_FACE);
            RenderText(textShader, "LEVEL "+std::to_string(level), 350.0f,230.0f,2.0f, glm::vec3(1.0f));
            glDisable(GL_CULL_FACE);
        }










        // rendering the jetpack-----------------------------------------------------
        glUseProgram(jpShaderProgram);
        

        float flyFactor;
        calculateJetpackY(0.85);
        flyFactor=(curr_y-(-0.55));

        // setting the flyTrans matrix
        glm::mat4 flyTrans=glm::mat4(1.0f);
        flyTrans=glm::translate(flyTrans,glm::vec3(0.0f,1.0f*flyFactor,0.0f));
        unsigned int flyTransLoc=glGetUniformLocation(jpShaderProgram,"flyTrans");
        glUniformMatrix4fv(flyTransLoc,1,GL_FALSE,glm::value_ptr(flyTrans));

        // setting the color
        unsigned int jpcolorLoc=glGetUniformLocation(jpShaderProgram,"ourColor");
        glUniform4f(jpcolorLoc,0.0f,1.0f,0.0f,1.0f);

        glBindVertexArray(jpVAO); 
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);



        // rendering the lazers----------------------------------------------------------
        glUseProgram(lazerShaderProgram);

        updateConfigArray(0);

        
        // setting the color
        unsigned int lazercolorLoc=glGetUniformLocation(lazerShaderProgram,"ourColor");
        glUniform4f(lazercolorLoc,1.0f,0.0f,0.0f,1.0f);


        // rendering all lazers on screen
        for(int i=0;i<render_till;i++)
        {
            // setting the lazer config
            unsigned int configLoc=glGetUniformLocation(lazerShaderProgram,"lazerConfig");
            glUniformMatrix4fv(configLoc,1,GL_FALSE,glm::value_ptr(configs[i]));

            // setting the lazer mov matrix
            float timeDelta=(cur_frame_time_ref-last_frame_time_ref);
            glm::mat4 lazerMov=glm::mat4(1.0f);
            lazerMov=glm::translate(lazerMov,glm::vec3(-1.0f*timeDelta*speed,0.0f,0.0f));
            unsigned int MovLoc=glGetUniformLocation(lazerShaderProgram,"lazerMov");
            glUniformMatrix4fv(MovLoc,1,GL_FALSE,glm::value_ptr(lazerMov));
            


            glBindVertexArray(lazerVAO); 
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            
            configs[i]=lazerMov*configs[i]; // updating configs (wrt to horizontal location)
        }



        // rendering the coins-----------------------------------------------------------
        glUseProgram(coinShaderProgram);

        // setting the color
        unsigned int coincolorLoc=glGetUniformLocation(coinShaderProgram,"ourColor");
        glUniform4f(coincolorLoc,0.08f,0.95f,0.93f,1.0f);

        updateConfigArray(1);
        for(int i=0;i<render_till_coin;i++)
        {
            if(coinConfigs[i]==glm::mat4(1.0f))
            {
                continue;
            }

            // setting the lazer config
            unsigned int configLoc=glGetUniformLocation(coinShaderProgram,"coinConfig");
            glUniformMatrix4fv(configLoc,1,GL_FALSE,glm::value_ptr(coinConfigs[i]));

            // setting the lazer mov matrix
            float timeDelta=(cur_frame_time_ref-last_frame_time_ref);
            glm::mat4 coinMov=glm::mat4(1.0f);
            coinMov=glm::translate(coinMov,glm::vec3(-1.0f*timeDelta*speed,0.0f,0.0f));
            unsigned int MovLoc=glGetUniformLocation(coinShaderProgram,"coinMov");
            glUniformMatrix4fv(MovLoc,1,GL_FALSE,glm::value_ptr(coinMov));
            

            glBindVertexArray(coinVAO); 
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);
            
            coinConfigs[i]=coinMov*coinConfigs[i]; // updating configs (wrt to horizontal location)
        }

        // updating distance
        frames_for_dist++;
        if(frames_for_dist==60)
        {
            distance+=(speed*10);
            frames_for_dist=0;
        }
        //distance+=(cur_frame_time_ref-last_frame_time_ref)*speed;




        // rendering text to the screen------------------------------------------------
        glEnable(GL_CULL_FACE);
        RenderText(textShader, "Score: "+std::to_string(coinsCollected)+"  Level: "+std::to_string(level), 10.0f,460.0f,0.4f, glm::vec3(1.0f));
        RenderText(textShader, "Distance: "+std::to_string((int)distance), 10.0f,430.0f,0.4f, glm::vec3(1.0f));
        glDisable(GL_CULL_FACE);


        last_frame_time_ref=cur_frame_time_ref;
        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }




    float gm_over_start=glfwGetTime();

    // game over screen
    while (!glfwWindowShouldClose(window))
    {

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);


        glEnable(GL_CULL_FACE);
        RenderText(textShader, "GAME OVER", 250.0f,230.0f,2.5f, glm::vec3(1.0f));
        RenderText(textShader, "Score: "+std::to_string(coinsCollected) + " Level: "+std::to_string(level), 450.0f,170.0f,0.6f, glm::vec3(1.0f));
        glDisable(GL_CULL_FACE);

        if((glfwGetTime()-gm_over_start)>=gm_over_time)
        {
            break;
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    //glDeleteVertexArrays(1, &VAO);
    //glDeleteBuffers(1, &EBO);
    //glDeleteProgram(jpShaderProgram);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}





// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    else if(glfwGetKey(window,GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        fly=1;
    }
    else if(glfwGetKey(window,GLFW_KEY_SPACE) == GLFW_RELEASE)
    {
        fly=0;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}




// render line of text
// -------------------
void RenderText(Shader &shader, std::string text, float x, float y, float scale, glm::vec3 color)
{
    // activate corresponding render state	
    shader.use();

    glUniform3f(glGetUniformLocation(shader.ID, "textColor"), color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++) 
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;
        // update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },            
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }           
        };
        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);
        // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
        x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}