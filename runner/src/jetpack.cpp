#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 500;

// variables to keep track of -------------------------------------

// for jetpack 
float upAcc=0;
float last_frame_time_ref=0;
float cur_frame_time_ref=0;
float current_speed=0;
int fly=0;
float curr_y=-0.55;

// for lazers
const int lazerCount=3; // formula for this is (int)2/speed*spacingTime + 1 // max number of lazers that can fit
float speed=0.2;
glm::mat4 configs[lazerCount];
const int spacingTime=4;  // spacingTime*speed >= length of lazer
float renderTime; // time when the last lazer render happened
int render_till=0; // keeps track of which lazer to render till initially, when all lazers are not configured
int destroyLazer=0; // keeps track of which lazer to reuse and generate a new lazer config in its place





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
//common frag shader for jetpack,lazer,coins etc
const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(0.0,1.0,0.0,1.0);\n"
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
void calculateJetpackY(float maxHeight,int upA)
{
    float timeDelta = (cur_frame_time_ref - last_frame_time_ref);
    if (fly)
    {
        upAcc = upA;
    }
    else
    {
        upAcc = 0;
    }
    current_speed += (upAcc - 1) * timeDelta;
    curr_y += (current_speed * timeDelta);

    if (curr_y > maxHeight)
    {
        curr_y = 0.85;
        current_speed = 0;
    }
    else if (curr_y < -1*maxHeight)
    {
        curr_y = -0.85;
        current_speed = 0;
    }
}




//generates a random configuration for a lazer and return the config matrix
glm::mat4 generateRandomConfig()
{
    glm::mat4 randomConfig;
    randomConfig = glm::mat4(1.0f);
    float vertical_loc = (((float)rand()*0.6) / (RAND_MAX / 2) - 0.6);
    float rot_angle = glm::radians((float)rand());
    randomConfig = glm::translate(randomConfig, glm::vec3(1.0f, 1.0f * vertical_loc, 0.0f));
    randomConfig = glm::rotate(randomConfig, rot_angle, glm::vec3(0.0f, 0.0f, 1.0f));
    return randomConfig;
}



// keeps the config array updated so the lazer render loop renders the correct lazers
void updateConfigArray()
{
    // taking care of getting the lazerCount lazers on the screen intitially
    if ((render_till < lazerCount) && ((glfwGetTime() - renderTime) >= spacingTime))
    {
        // making the config matrix
        configs[render_till] = generateRandomConfig();
        renderTime = glfwGetTime();
        render_till++;
    }
    // the rest of the game
    else if ((render_till >= lazerCount) && ((glfwGetTime() - renderTime) >= spacingTime))
    {
        destroyLazer = (destroyLazer % (lazerCount));
        configs[destroyLazer] = generateRandomConfig();
        renderTime = glfwGetTime();
        destroyLazer++;
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
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
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







    // shaderProgram config
    unsigned int jpShaderProgram=createShadProgram(jetpackVertexShaderSource,fragmentShaderSource);
    unsigned int lazerShaderProgram=createShadProgram(lazerVertexShaderSource,fragmentShaderSource);




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
    float lazerVertices[] = {
     0.05f,  0.0f, 0.0f,  // right
     -0.05f, 0.0f, 0.0f,  // left
    0.0f, 0.4f, 0.0f,  // top
    0.0f,  -0.4f, 0.0f   // bottom 
    };
    unsigned int lazerIndices[] = {  // note that we start from 0!
        0, 1, 3,   // first triangle
        0, 1, 2    // second triangle
    };  




    // creating a VAO
    //jetpack
    unsigned int jpVAO=createVAO(jetpackVertices,jetpackIndices,0,sizeof(jetpackVertices),sizeof(jetpackIndices));
    //lazer
    unsigned int lazerVAO=createVAO(lazerVertices,lazerIndices,0,sizeof(lazerVertices),sizeof(lazerIndices));


    // setting some meta data
    last_frame_time_ref=glfwGetTime();
    srand(10);
    renderTime=glfwGetTime();


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        cur_frame_time_ref=glfwGetTime();


        // rendering the jetpack-----------------------------------------------------
        glUseProgram(jpShaderProgram);
        

        float flyFactor;
        calculateJetpackY(0.85,4);
        flyFactor=(curr_y-(-0.55));

        // setting the flyTrans matrix
        glm::mat4 flyTrans=glm::mat4(1.0f);
        flyTrans=glm::translate(flyTrans,glm::vec3(0.0f,1.0f*flyFactor,0.0f));
        unsigned int flyTransLoc=glGetUniformLocation(jpShaderProgram,"flyTrans");
        glUniformMatrix4fv(flyTransLoc,1,GL_FALSE,glm::value_ptr(flyTrans));


        glBindVertexArray(jpVAO); 
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);



        // rendering the lazers----------------------------------------------------------
        glUseProgram(lazerShaderProgram);

        updateConfigArray();
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

 

        last_frame_time_ref=cur_frame_time_ref;
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