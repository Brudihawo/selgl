#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "assert.h"
#include "math.h"

#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"


#include "log.h"

#define RGB_TO_FLOAT(HEX) \
  (float)((HEX >> 24) & 0xff)       / 255.0,\
  (float)((HEX >> 16) & 0x00ff)     / 255.0,\
  (float)((HEX >>  8) & 0x0000ff)   / 255.0,\
  (float)((HEX      ) & 0x000000ff) / 255.0

#define M_PI 3.1415926535897932384626433832795

#define ACTIVE 0xC65333ff
#define INACTIVE 0x697893ff
#define BG 0x00000000

#define WIN_SIZE_DEFAULT 480
#define N_SEGMENTS_DEFAULT 10
#define INNER_RADIUS 0.1
#define OUTER_RADIUS 0.4
#define BORDER_WIDTH 0.01


static int win_size, n_segments;

typedef struct {
  float x, y;
} Vertex;

Vertex vertices[] = {
  { -1.0f, -1.0f },
  {  1.0f, -1.0f },
  {  1.0f,  1.0f },
  {  1.0f,  1.0f },
  { -1.0f, -1.0f },
  { -1.0f,  1.0f },
};

typedef struct {
  GLuint program;
  GLuint vert_shader;
  GLuint frag_shader;
  // uniform locations
  GLint res_loc; 
  GLint mousepos_loc;
  GLint segments_loc; 
  GLint active_seg_loc; 
  GLint active_loc; 
  GLint inactive_loc; 
  GLint bg_loc; 
} Renderer;

const char* shader_type_to_cstr(GLenum shader_type) {
  switch (shader_type) {
    case GL_FRAGMENT_SHADER:
      return "fragment shader";
    case GL_VERTEX_SHADER:
      return "vertex shader";
    default:
      return "other shader";
  }
  assert(false && "unreachable");
}

static inline float length(float x, float y) {
  return sqrt(x * x + y * y);
}

static inline float clampf(float x, float min, float max) {
  if (x < min) return min;
  if (x > max) return max;
  return x;
}

static inline float norm(float x, float min, float max) {
  return ((x - min) / (max - min) - 0.5);
}

float angle(float x, float y) {
  float phi;
  float r = length(x, y);
  if (y < 0) {
    phi = M_PI + acos(-x / r);
  } else {
    phi = acos(x / r);
  }
 return phi;
}

int active_seg(float m_x, float m_y, int n_segments) {
  if (length(m_x, m_y) < INNER_RADIUS) return n_segments + 1;
  float mouse_angle = angle(m_x, m_y);
  return (int)floorf(mouse_angle / (2 * M_PI) * n_segments);
}

static const char* vert_shader_text = 
  "#version 330\n"
  "layout (location = 0) in vec2 position;\n"
  "void main() {\n"
  "  gl_Position = vec4(position, 0.0, 1.0);\n"
  "}\n";

static const char* frag_shader_text =
  "#version 330\n"
  "#define M_PI 3.1415926535897932384626433832795\n"
  "#define INNER_RADIUS 0.1\n"
  "#define OUTER_RADIUS 0.4\n"
  "#define BORDER_WIDTH 0.01\n"
  "uniform vec2 viewport_size;\n"
  "uniform vec4 active_color;\n"
  "uniform vec4 inactive_color;\n"
  "uniform vec4 background_color;\n"
  "uniform vec2 mouse_pos;\n"
  "uniform int n_segments;\n"
  "uniform int active_segment;\n"
  "in vec4 gl_FragCoord;\n"
  "out vec4 color;\n"
  "float angle(float x, float y) {\n"
  "  float phi;\n"
  "  float r = length(vec2(x, y));\n"
  "  if (y < 0) {\n"
  "    phi = M_PI + acos(-x / r);\n"
  "  } else {\n"
  "    phi = acos(x / r);\n"
  "  }\n"
  " return phi;"
  "}\n"
  "\n"
  "int cur_seg(float r, float phi) {\n"
  "  if (r < INNER_RADIUS) return n_segments + 1;\n"
  "  return int(floor(phi / (2 * M_PI) * n_segments));\n"
  "}\n"
  "\n"
  "void main() {\n"
  "  vec2 uv = gl_FragCoord.xy / viewport_size - vec2(0.5, 0.5);\n"
  "  float r = length(uv);\n"
  "  float m_r = length(mouse_pos);\n"
  "  float phi = angle(uv.x, uv.y);\n"
  "  float val = phi / (2 * M_PI);\n"
  "  if (cur_seg(r, phi) == active_segment) {\n"
  "    color = active_color;\n"
  "  } else {\n"
  "    color = inactive_color;\n"
  "  }\n"
  "  for (int i = 0; i < n_segments; i++) {\n"
  "     float mid = (2 * M_PI) / n_segments * i;\n"
  "     float d = r * sin(phi - mid);\n"
  "     if ((abs(d) < BORDER_WIDTH / 2 && r * cos(phi - mid) > 0.0) || r < INNER_RADIUS + BORDER_WIDTH || r > OUTER_RADIUS) {\n"
  "       color = background_color;\n"
  "     }\n"
  "  }\n"
  "  if (r < INNER_RADIUS) {\n"
  "    if (m_r < INNER_RADIUS) color = active_color;\n"
  "    else color = inactive_color;\n"
  "  }\n"
  "}\n";

bool compile_shader(const GLchar *source, GLenum shader_type, GLuint* shader) {
  *shader = glCreateShader(shader_type);
  glShaderSource(*shader, 1, &source, NULL);
  glCompileShader(*shader);

  GLint compiled = 0;
  glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != GL_TRUE) {
    GLchar msg[1024];
    GLsizei msg_size = 0;

    glGetShaderInfoLog(*shader, sizeof(msg), &msg_size, msg);
    log_err("ERROR: Could not compile %s\n    %.*s\n",
            shader_type_to_cstr(shader_type),
            msg_size, msg);
  }

  return compiled;
}

bool link_program(GLuint* program, GLuint vertex_shader, GLuint fragment_shader) {
  *program = glCreateProgram();
  glAttachShader(*program, vertex_shader);
  glAttachShader(*program, fragment_shader);
  glLinkProgram(*program);

  GLint linked = 0;
  glGetProgramiv(*program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    GLchar msg[1024];
    GLsizei msg_size = 0;
    glGetProgramInfoLog(*program, 1024, &msg_size, msg);
    log_err("ERROR: Could not link program.\n    %.*s\n",
            msg_size, msg);
  }
  return linked;
}

void error_callback(int error, const char* description) {
  log_err("GLFW Error: %s (Errno %i)", description, error);
}

static void key_callback(GLFWwindow* window, int key,
                         int scancode, int action, int mods) {
  (void) mods;
  (void) scancode;
  if ((key == GLFW_KEY_Q) && (action == GLFW_PRESS))
    glfwSetWindowShouldClose(window, GLFW_TRUE);

  log_msg("Recieved Keypress");
}

int cur_segment = -1;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
  (void) mods;
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    double xpos_d, ypos_d;
    glfwGetCursorPos(window, &xpos_d, &ypos_d);
    float xpos = norm((float)xpos_d, 0.0, (float)width);
    float ypos = -norm((float)ypos_d, 0.0, (float)height);
    cur_segment = active_seg(xpos, ypos, n_segments); if (length(xpos, ypos) > OUTER_RADIUS) {
      cur_segment = -1;
    }

    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }

  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    cur_segment = -1;
  }
}

void init_gl(void) {
  glfwSetErrorCallback(error_callback);

  if (!glfwInit()) {
    log_err("Could not initialize GLFW. Exiting...");
    exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
}

GLFWwindow* create_win(int win_size) {
  GLFWwindow* window = glfwCreateWindow(win_size, win_size, "Selector", NULL, NULL);
  if (!window) {
    log_err("Could not initialize GLFW window. Exiting...");
    glfwTerminate();
    exit(1);
  }

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
    log_err("Could not initialize OpenGL context! Exiting...");
    exit(1);
  }

  if (glfwGetWindowAttrib(window, GLFW_TRANSPARENT_FRAMEBUFFER) != GLFW_TRUE) {
    log_err("Could not create transparent window");
  }
  // glfwSetWindowOpacity(window, 0.5f);
  glfwSetKeyCallback(window, key_callback);
  glfwSetMouseButtonCallback(window, mouse_button_callback);

  glfwSwapInterval(1);

  int gl_ver_minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
  int gl_ver_major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);

  log_msg("Found OpenGL Version %i.%i.", gl_ver_major, gl_ver_minor);
  return window;
}

Renderer create_renderer(GLFWwindow *window, const char* vert_shader_text, const char* frag_shader_text) {
  glEnable(GL_BLEND);
  glEnable(GL_ALPHA);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Compile Shaders
  GLuint vbo, vertex_shader, fragment_shader, program;
  bool vert_compiled = compile_shader(vert_shader_text,
                                        GL_VERTEX_SHADER, &vertex_shader);
  bool frag_compiled = compile_shader(frag_shader_text, 
                                      GL_FRAGMENT_SHADER, &fragment_shader);

  if (!(vert_compiled && frag_compiled)) {
    log_err("Shader compilation error. Exiting...");
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(1);
  }

  bool program_linked = link_program(&program, vertex_shader, fragment_shader);
  if (!program_linked) {
    log_err("Program linking error. Exiting...");
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(1);
  }

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLint pos_attrib = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(pos_attrib);
  glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, 0);

  return (Renderer) {
    .program        = program,
    .vert_shader    = vertex_shader,
    .frag_shader    = fragment_shader,
    .res_loc        = glGetUniformLocation(program, "viewport_size"),
    .mousepos_loc   = glGetUniformLocation(program, "mouse_pos"),
    .segments_loc   = glGetUniformLocation(program, "n_segments"),
    .active_seg_loc = glGetUniformLocation(program, "active_segment"),
    .active_loc     = glGetUniformLocation(program, "active_color"),
    .inactive_loc   = glGetUniformLocation(program, "inactive_color"),
    .bg_loc         = glGetUniformLocation(program, "background_color"),
  };
}

int main(int argc, char** argv) {
  if (argc > 1) {
    win_size = atoi(argv[1]);
  } else {
    win_size = WIN_SIZE_DEFAULT;
  }

  if (argc > 2) {
    n_segments = atoi(argv[2]);
  } else {
    n_segments = N_SEGMENTS_DEFAULT;
  }

  init_gl();
  GLFWwindow* window = create_win(win_size);
  Renderer r = create_renderer(window, vert_shader_text, frag_shader_text);

  // set position to mouse location
  double xpos_d, ypos_d;
  int xpos_w, ypos_w;
  glfwGetCursorPos(window, &xpos_d, &ypos_d);
  glfwGetWindowPos(window, &xpos_w, &ypos_w);
  glfwSetWindowPos(window, (int)xpos_d + xpos_w - win_size / 2, (int)ypos_d + ypos_w - win_size / 2);

  // Render Loop
  while (!glfwWindowShouldClose(window)) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glfwGetCursorPos(window, &xpos_d, &ypos_d);
    float xpos = norm((float)xpos_d, 0.0, (float)width);
    float ypos = -norm((float)ypos_d, 0.0, (float)height);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    int active_segment_shader = active_seg(xpos, ypos, n_segments);

    glUseProgram(r.program);
    glUniform2f(r.res_loc, (float)width, (float)height);
    glUniform1i(r.segments_loc, n_segments);
    glUniform1i(r.active_seg_loc, active_segment_shader);
    glUniform2f(r.mousepos_loc, (float)xpos, (float)ypos);
    glUniform4f(r.active_loc, RGB_TO_FLOAT(ACTIVE));
    glUniform4f(r.inactive_loc, RGB_TO_FLOAT(INACTIVE));
    glUniform4f(r.bg_loc, RGB_TO_FLOAT(BG));

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // TODO: Integrate into pentablet workflow
  printf("%i\n", cur_segment);
  glfwDestroyWindow(window);
  glfwTerminate();
  exit(0);
}
