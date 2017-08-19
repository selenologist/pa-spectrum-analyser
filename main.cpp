#include <cstdio>
#include <cmath>
#include <cstring>
#include <climits>
#include <memory>
#include <vector>
#include <fstream>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#define GLEW_STATIC
#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <fftw3.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This only works on little-endian machines for now. There's no conversion of sample data."
#endif

#define error(__error_string, ...) fprintf(stderr, __FILE__ ": " __error_string, ##__VA_ARGS__)

using namespace std;

struct Vertex{
    float position[2];
};

GLuint loadShader(const string vshader_file, const string fshader_file)
{
    GLint status;

    ifstream vshader_stream(vshader_file, ios::in);
    string   vshader(istreambuf_iterator<char>(vshader_stream), {}); // read whole file into the string
    ifstream fshader_stream(fshader_file, ios::in);
    string   fshader(istreambuf_iterator<char>(fshader_stream), {}); // no error handling lol

    GLuint vs_ID = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs_ID = glCreateShader(GL_FRAGMENT_SHADER);

    // OpenGL needs a pointer to a pointer for some reason
    const char* vshader_cstr = vshader.c_str();
    const char* fshader_cstr = fshader.c_str();
    glShaderSource(vs_ID, 1, &vshader_cstr, NULL);
    glShaderSource(fs_ID, 1, &fshader_cstr, NULL);

    glCompileShader(vs_ID);

    status = GL_FALSE;
    glGetShaderiv(vs_ID, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        error("Failed to compile vertex shader %s\n", vshader_file.c_str());

        int log_length;
        glGetShaderiv(vs_ID, GL_INFO_LOG_LENGTH, &log_length);
        char* error_message = new char[log_length + 1];
        glGetShaderInfoLog(vs_ID, log_length, NULL, error_message);
        error("%s\n", error_message);
        delete[] error_message;

        exit(EXIT_FAILURE);
    }
    
    glCompileShader(fs_ID);

    status = GL_FALSE;
    glGetShaderiv(fs_ID, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        error("Failed to compile fragment shader %s\n", fshader_file.c_str());

        int log_length;
        glGetShaderiv(fs_ID, GL_INFO_LOG_LENGTH, &log_length);
        char* error_message = new char[log_length + 1];
        glGetShaderInfoLog(fs_ID, log_length, NULL, error_message);
        error("%s\n", error_message);
        delete[] error_message;

        exit(EXIT_FAILURE);
    }

    GLuint program_ID = glCreateProgram();

    glAttachShader(program_ID, vs_ID);
    glAttachShader(program_ID, fs_ID);

    // deallocate the shaders as they won't be used in any other programs
    // (they will be retained until the program is deallocated too)
    glDeleteShader(vs_ID);
    glDeleteShader(fs_ID);

    glLinkProgram(program_ID);

    status = GL_FALSE;
    glGetProgramiv(program_ID, GL_LINK_STATUS, &status);

    if(status == GL_FALSE)
    {
        error("Failed to link program object.\n");
        
        int log_length;
        glGetShaderiv(program_ID, GL_INFO_LOG_LENGTH, &log_length);
        char* error_message = new char[log_length + 1];
        glGetShaderInfoLog(program_ID, log_length, NULL, error_message);
        error("%s\n", error_message);
        delete[] error_message;

        exit(EXIT_FAILURE);
    }

    return program_ID;
}

static void error_callback(int error, const char* description){
    error("GLFW Error: %s\n", description);
}

struct GlContext{
    GLFWwindow* window;
    GLuint      vao;
    GLuint      vbo;
    GLuint      shader;
    int width, height;

    GlContext(){
        glfwSetErrorCallback(error_callback);

        if(glfwInit() != GLFW_TRUE){
            error("GLFW initialisation failed\n");
            exit(EXIT_FAILURE);
        }

        // request OpenGL 3.3
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

        // request 4xMSAA
        glfwWindowHint(GLFW_SAMPLES, 4);

        // requested window dimensions
        #define REQUEST_X 1024
        #define REQUEST_Y 768
        // macro to turn the numbers above into string constants so the title will
        // be a compile-time const char*
        #define _STR(__X) #__X
        #define  STR( _X) _STR(_X)
        window =
            glfwCreateWindow(REQUEST_X, REQUEST_Y,
                             "SpectrumAnalyser - requested " STR(REQUEST_X) "x" STR(REQUEST_Y),
                             NULL, NULL);
        #undef REQUEST_X
        #undef REQUEST_Y
        #undef _STR
        #undef STR
        if(!window){
            error("Failed to create GLFW window\n");
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        // set the current OpenGL context to the new window
        glfwMakeContextCurrent(window);

        // Don't wait for a screen update after glfwSwapBuffers
        // http://www.glfw.org/docs/3.1/group__context.html
        glfwSwapInterval(0);

        if(glewInit() != GLEW_OK){
            error("GLEW initialisation failed\n");
            exit(EXIT_FAILURE);
        }

        // create VBO
        glGenBuffers(1, &vbo); // generate one buffer
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        // Generate a vertex array object
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glVertexAttribPointer(0,        // 0-th attribute (position, hardcoded)
                              2,        // 2d position, so 2 components per attribute
                              GL_FLOAT, // float vertices
                              GL_FALSE, // not normalised
                              sizeof(Vertex), // stride by the size of the Vertex structure
                              reinterpret_cast<void*>(offsetof(Vertex, position))); // offset in the Vertex structure that points to position
        glEnableVertexAttribArray(0);

        // load shader
        shader = loadShader("vertex.vert", "fragment.frag");
    }
    ~GlContext(){
        glDeleteProgram(shader);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    void updateFramebufferSize(){
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
    }
};

static inline double blackman_harris(double input, double current_sample, double total_samples){ // actually needs total_samples - 1
    static const double
        a0 = 0.35875,
        a1 = 0.48829,
        a2 = 0.14128,
        a3 = 0.01168;
    static const double TAU = 6.28318530717958647692528676655900576839433879875021;
    const double offset = TAU * current_sample / total_samples;
    return input *
        (a0 - a1 * cos(      offset)
            - a2 * cos(2.0 * offset)
            - a3 * cos(3.0 * offset));
}

static inline bool power_of_two(const unsigned n){
    if(n && ((n & (n - 1)) == 0)){ // if n is not zero and has only 1 bit set
        return true;
    }
    else{
        return false;
    }
}

int main(int argc, char **argv) {
    enum Mode{
        Mono,
        Stereo
    } mode = Stereo;

    unsigned transform_size = 512;
    unsigned sample_rate    = 44100;

    for(int argi = 1; argi < argc; argi++){
        if(!strcmp("-m", argv[argi])){
            mode = Mono;
        }
        else if(!strncmp("-s", argv[argi], 2) &&
                 strlen(argv[argi]) > 2){
            unsigned long value = // read characters after the s as an unsigned base10 number
                strtoul(&argv[argi][2], NULL, 10);
            if(value > UINT_MAX / 2){
                error("Value %lu too large, limit set at %u for practical reasons. Option ignored.\n",
                      value,
                      UINT_MAX / 2);
            }
            else if(!power_of_two(value)){
                error("Value %lu is not a power of two. Only power of two FFT sizes are supported for now. Option ignored.\n", value);
            }
            else{
                transform_size = value;
            }
        }
        else if(!strncmp("-r", argv[argi], 2) &&
                 strlen(argv[argi]) > 2){
            unsigned long value = // read characters after the r as an unsigned base10 number
                strtoul(&argv[argi][2], NULL, 10);
            if(value > UINT_MAX / 2){
                error("Value %lu too large, limit set at %u for practical reasons. Option ignored.\n",
                      value,
                      UINT_MAX / 2);
            }
            else{
                sample_rate = value;
            }
        }
        else{
            error("Unknown option %s\n", argv[argi]);
        }
    }

    printf("Attempting to open a %s stream at %uHz with a transform window of %u\n",
            (mode == Stereo)? "Stereo" : "Mono",
            sample_rate,
            transform_size);

    const unsigned channels = (mode == Stereo)? 2 : 1;
    static const pa_sample_spec sample_spec = {
        .format   = PA_SAMPLE_S16LE,
        .rate     = sample_rate,
        .channels = (uint8_t) channels
    };

    int error_code = 0;
    pa_simple *stream =
        pa_simple_new(NULL,               // PA server name, use default
                      argv[0],            // client name
                      PA_STREAM_RECORD,   // 'direction'; we want to record
                      NULL,               // source name, use default
                      "Spectrum analyser",// stream name 
                      &sample_spec,       // sample spec...
                      NULL,               // channel map, use default
                      NULL,               // buffering attributes, use default
                      &error_code         // pointer to error code variable if this fails
                     );

    if(!stream){
        error("pa_simple_new() failed: %s\n", pa_strerror(error_code));
        return 1;
    }

    unique_ptr<int16_t[]>      sample_buffer(new int16_t[transform_size*channels]);
    unique_ptr<double[]>       double_buffer(new double[transform_size]);
    unique_ptr<fftw_complex[]> transform_buffer(new fftw_complex[transform_size]);

    fftw_plan plan = fftw_plan_dft_r2c_1d(
            transform_size,         // transform size
            double_buffer.get(),    // input buffer
            transform_buffer.get(), // output buffer
            FFTW_PATIENT |          // plan flags; try many possible algorithms,
            FFTW_DESTROY_INPUT      // and allow destruction of double_buffer
            );
    
    vector<Vertex> vertices;
    vertices.reserve(transform_size);
    GlContext ctx;
    
    glUseProgram(ctx.shader);
    glBindVertexArray(ctx.vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx.vbo);

    while(!glfwWindowShouldClose(ctx.window)){
        if(pa_simple_read(stream,
                          sample_buffer.get(),
                          transform_size * sizeof(int16_t) * channels,
                          &error_code) < 0){
            error("pa_simple_read() failed: %s\n", pa_strerror(error_code));
            return 1;
        }
        pa_usec_t latency = pa_simple_get_latency(stream, &error_code);
        if(latency < 0){
            error("pa_simple_get_latency() failed: %s\n", pa_strerror(error_code));
            // nonfatal
        }
        else{
            printf("Latency: %019lu usec\r", latency);
        }
    
        ctx.updateFramebufferSize();
        glClear(GL_COLOR_BUFFER_BIT);
       
        for(unsigned channel = 0; channel < channels; channel++){
            for(unsigned i = 0; i < transform_size; i++){
                // Convert the s16 input samples into doubles for fftw3
                const double f_sample = sample_buffer.get()[i * channels + channel] / 32767.0; // convert s16 int to [-0.5, +0.5] float
                double_buffer.get()[i] = blackman_harris(f_sample, i, transform_size - 1); // apply blackman-harris window
            }

            fftw_execute(plan);

            const float scale = (mode == Stereo)? 1.0 : 4.0; // increase size when only mono
            const float offset =
                (mode == Mono)? -0.90 : // move down for mono
                    (channel == 0)? 0.05 : -0.95; // move the left channel up and the right channel down
            
            vertices.clear();
            for(unsigned i = 0; i < transform_size/3; i++){
                const float real = (float)transform_buffer.get()[i][0];
                const float imag = (float)transform_buffer.get()[i][1];
                const float magnitude = // get log10'd magnitude
                    log10f(1.0f +                           // make values start at 1 to remove negative logs
                           sqrtf(real * real + imag * imag) // magnitude of complex FFT bin
                           / (float)(transform_size))       // normalised by FFT N
                    * 40.0f;                                // amplify by 40x (obtained through trial and error tbh)

                vertices.push_back(Vertex{
                        (i / (float)(transform_size/3) - 0.5f) * 2.0f, // x position
                        magnitude * scale + offset // y position
                });
            }
      
            glBufferData(GL_ARRAY_BUFFER,
                         vertices.size() * sizeof(Vertex), vertices.data(), 
                         GL_STREAM_DRAW);

            glDrawArrays(GL_LINE_STRIP, 0, vertices.size());
        }
        
        glFlush();
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
    }

    fftw_destroy_plan(plan);
    pa_simple_free(stream);

    return 0;
}

