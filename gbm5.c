#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <assert.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void print_shader_info_log( GLuint shader ) {
        int max_length = 4096;
        int actual_length = 0;
        char slog[4096];
        glGetShaderInfoLog( shader, max_length, &actual_length, slog );
        fprintf( stderr, "shader info log for GL index %u\n%s\n", shader, slog );
}

void check_shader_errors( GLuint shader ) {
        GLint params = -1;
        glGetShaderiv( shader, GL_COMPILE_STATUS, &params );
        if ( GL_TRUE != params ) {
                fprintf( stderr, "ERROR: shader %u did not compile\n", shader );
                print_shader_info_log( shader );
                _exit(1);
        }
}

void check() { GLenum errCode; const GLubyte *errString;
 if ((errCode = glGetError()) !=GL_NO_ERROR) {
    errString = gluErrorString(errCode); fprintf(stderr,"%s\n",errString); _exit(1);
 }
}

int32_t
main (int32_t argc, char* argv[])
{
   bool res;

   int32_t fd = open ("/dev/dri/renderD128", O_RDWR);
   assert (fd > 0);

   struct gbm_device *gbm = gbm_create_device (fd);
   assert (gbm != NULL);

   /* setup EGL from the GBM device */
   EGLDisplay egl_dpy = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, gbm, NULL);
   assert (egl_dpy != NULL);

   res = eglInitialize (egl_dpy, NULL, NULL);
   assert (res);

   const char *egl_extension_st = eglQueryString (egl_dpy, EGL_EXTENSIONS);
   assert (strstr (egl_extension_st, "EGL_KHR_create_context") != NULL);
   assert (strstr (egl_extension_st, "EGL_KHR_surfaceless_context") != NULL);

   static const EGLint config_attribs[] = {
      EGL_NONE
   };
   EGLConfig cfg;
   EGLint count;

   res = eglChooseConfig (egl_dpy, config_attribs, &cfg, 1, &count);
   assert (res);

   res = eglBindAPI (EGL_OPENGL_API);
   assert (res);

   static const EGLint attribs[] = {
      EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 2,
      EGL_NONE
   };
   EGLContext core_ctx = eglCreateContext (egl_dpy,
                                           cfg,
                                           EGL_NO_CONTEXT,
                                           attribs);
   assert (core_ctx != EGL_NO_CONTEXT);

   res = eglMakeCurrent (egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, core_ctx);
   assert (res);

    // Step 9 - create a framebuffer object
    GLuint fboId = 0;
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    GLuint renderBufferWidth = 1024;
    GLuint renderBufferHeight = 768;
    GLuint renderBuffer;
    glGenRenderbuffers(1, &renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, renderBufferWidth, renderBufferHeight);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderBuffer);

GLint m_viewport[4];
glGetIntegerv( GL_VIEWPORT, m_viewport ); printf("%d %d %d %d\n",m_viewport[0],m_viewport[1],m_viewport[2],m_viewport[3]);
glViewport(0,0,renderBufferWidth,renderBufferHeight);
glGetIntegerv( GL_VIEWPORT, m_viewport ); printf("%d %d %d %d\n",m_viewport[0],m_viewport[1],m_viewport[2],m_viewport[3]);
glEnable(GL_FRAMEBUFFER_SRGB);

    glClearColor( 1.0/255.0, 2.0/255.0, 3.0/255.0, 4.0/255.0 );
    glClear(GL_COLOR_BUFFER_BIT );

float points[] = {
   -1.0f,  -1.0f,  0.0f,
   -1.0f,   1.0f,  0.0f,
    1.0f,   1.0f,  0.0f,
    1.0f,  -1.0f,  0.0f,
   -1.0f,  -1.0f,  0.0f
};
GLuint vbo = 0;
glGenBuffers(1, &vbo); check();
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
assert (glGetError () == GL_NO_ERROR);

GLuint vao = 0;
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);
glEnableVertexAttribArray(0);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
assert (glGetError () == GL_NO_ERROR);

const char* vertex_shader =
"#version 420\n"
"in vec3 vp;"
"void main() {"
"  gl_Position = vec4(vp, 1.0);"
"}";
const char* fragment_shader =
"#version 420\n"
"out vec4 frag_colour;"
"uniform ivec2 image_size;"
"int erand_i=1;"
"int erand_j=1;"
"float erand08()"
"{ int k; k=(erand_i+erand_j)%16777216; erand_i=erand_j; erand_j=k; return float(erand_i)/16777216.0; }"
"uint hash( uint x ) {"
"    x += ( x << 10u );"
"    x ^= ( x >>  6u );"
"    x += ( x <<  3u );"
"    x ^= ( x >> 11u );"
"    x += ( x << 15u );"
"    return x;"
"}"
"float floatConstruct( uint m ) {"
"    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask\n"
"    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32\n"
"    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)\n"
"    m |= ieeeOne;                          // Add fractional part to 1.0\n"
"    float  f = uintBitsToFloat( m );       // Range [1:2]\n"
"    return f - 1.0;                        // Range [0:1]\n"
"}"
"float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }"
"struct Ray { vec3 o,d; };"
"int DIFF=0; int SPEC=1; int REFR=2;"
"struct Sphere { float rad; vec3 p, e, c; int refl; };"
"Sphere spheres[] = {//Scene: radius, position, emission, color, material\n"
"  Sphere(1e5, vec3( 1e5+1,40.8,81.6), vec3(0,0,0),vec3(.75,.25,.25),DIFF),//Left\n"
"  Sphere(1e5, vec3(-1e5+99,40.8,81.6),vec3(0,0,0),vec3(.25,.25,.75),DIFF),//Rght\n"
"  Sphere(1e5, vec3(50,40.8, 1e5),     vec3(0,0,0),vec3(.75,.75,.75),DIFF),//Back\n"
"  Sphere(1e5, vec3(50,40.8,-1e5+170), vec3(0,0,0),vec3(0,0,0),      DIFF),//Frnt\n"
"  Sphere(1e5, vec3(50, 1e5, 81.6),    vec3(0,0,0),vec3(.75,.75,.75),DIFF),//Botm\n"
"  Sphere(1e5, vec3(50,-1e5+81.6,81.6),vec3(0,0,0),vec3(.75,.75,.75),DIFF),//Top\n"
"  Sphere(16.5,vec3(27,16.5,47),       vec3(0,0,0),vec3(1,1,1)*.999, SPEC),//Mirr\n"
"  Sphere(16.5,vec3(73,16.5,78),       vec3(0,0,0),vec3(1,1,1)*.999, REFR),//Glas\n"
"  Sphere(600, vec3(50,681.6-.27,81.6),vec3(12,12,12), vec3(0,0,0), DIFF) //Lite\n"
"};"
"float intersect(Sphere s, Ray r) { // returns distance, 0 if nohit\n"
"  vec3 op = s.p - r.o; // Solve t^2*d.d + 2*t*(o-p).d + (o-p).(o-p)-R^2 = 0\n"
"  float t, eps=4e-2, b=dot(op,r.d), det=b*b-dot(op,op)+s.rad*s.rad;"
"  if (det<0) return 0; else det=sqrt(det);"
"  return (t=b-det)>eps ? t : ((t=b+det)>eps ? t : 0);"
"}"
"int numSpheres = 9; // sizeof(spheres)/sizeof(Sphere);\n"
"struct TID { float t; int id; } tid;"
"bool Intersect(Ray r){ // closest intersecting sphere\n"
"  float d, inf=tid.t=1e20; int i;"
"  for(i=0;i<numSpheres;i++) { d=intersect(spheres[i],r);"
"    if((d>0)&&(d<tid.t)) {tid.t=d;tid.id=i;}"
"  }"
"  return tid.t<inf;"
"}"
"float M_PI=3.14159265358979323846;"
"float M_1_PI=0.31830988618379067154;"
"int depth;"
"vec3 radiance(Ray r){"
"  float t;  // distance to intersection\n"
"  int id; // id of intersected object\n"
"  vec3 cl=vec3(0,0,0); // accumulated color\n"
"  vec3 cf=vec3(1,1,1); // accumulated reflectance\n"
"  for (depth=1;depth<=5;depth++) {"
"    if (!Intersect(r)) return cl; // if miss, return black\n"
"    t=tid.t; id=tid.id;"
"    Sphere obj=spheres[id];"
"    vec3 x=r.o+r.d*t, n=normalize(x-obj.p), nl=((dot(n,r.d)<0)?n:n*-1), f=obj.c;"
"    float p = ( ((f.x>f.y) && (f.x>f.z)) ? f.x : ((f.y>f.z) ? f.y : f.z) ); // max refl\n"
"    cl = cl + cf * obj.e;"
"    if (depth>3) {if (erand08()<p) f=f*(1/p); else return cl;} //R.R.\n"
"    cf = cf * f;"
"    if (obj.refl == DIFF){                  // Ideal DIFFUSE reflection\n"
"      float r1=2*M_PI*erand08(), r2=erand08(), r2s=sqrt(r2);"
"      vec3 w=nl, u=normalize(cross( ((abs(w.x)>.1)?vec3(0,1,0):vec3(1,0,0)) ,w )), v=cross(w,u);"
"      vec3 d = normalize( u*cos(r1)*r2s + v*sin(r1)*r2s + w*sqrt(1-r2) );"
"      r = Ray(x,d);"
"      continue;"
"    } else if (obj.refl == SPEC){           // Ideal SPECULAR reflection\n"
"      r = Ray(x,r.d-n*2*dot(n,r.d));"
"      continue;"
"    }"
"    Ray reflRay=Ray(x,r.d-n*2*dot(n,r.d));     // Ideal dielectric REFRACTION\n"
"    bool into = (dot(n,nl)>0);             // Ray from outside going in?\n"
"    float nc=1, nt=1.5, nnt=(into?nc/nt:nt/nc), ddn=dot(r.d,nl), cos2t=1-nnt*nnt*(1-ddn*ddn);"
"    if (cos2t<0){    // Total internal reflection\n"
"      r = reflRay;"
"      continue;"
"    }"
"    vec3 tdir = normalize( r.d*nnt - n*((into?1:-1)*(ddn*nnt+sqrt(cos2t))) );"
"    float a=nt-nc, b=nt+nc, R0=a*a/(b*b), c = 1-(into?-ddn:dot(tdir,n));"
"    float Re=R0+(1-R0)*c*c*c*c*c,Tr=1-Re,P=.25+.5*Re,RP=Re/P,TP=Tr/(1-P);"
"    if (erand08()<P){"
"      cf = cf * RP;"
"      r = reflRay;"
"    } else {"
"      cf = cf * TP;"
"      r = Ray(x,tdir);"
"    }"
"  }"
"  return cl;"
"}"
"float clamp(float x){ return ( (x<0) ? 0 : ( (x>1) ? 1 : x) ); }"
"void main() {"
"  int w=image_size.x, h=image_size.y; int samps=10;                // # samples\n"
"  Ray cam=Ray(vec3(50,52,295.6), normalize(vec3(0,-0.042612,-1))); // cam pos, dir\n"
"  vec3 cx=vec3(w*.5135/h,0,0); vec3 cy=normalize(cross(cx,cam.d))*.5135;"
"  vec3 r=vec3(0,0,0); vec3 c=vec3(0,0,0);"
"  int x,y;"
"  x=int(gl_FragCoord[0]);"
"  y=int(h-gl_FragCoord[1]-1);"
"  erand_i=int(random(y*w+x)*16777216);erand_j=int(random(x*h+y)*16777216);"
"  int sx,sy,s;"
"  for (sy=0; sy<2; sy++) {   // 2x2 subpixel rows\n"
"    for (sx=0; sx<2; sx++) {                // 2x2 subpixel cols\n"
"      for (s=0; s<samps; s++) {"
"            float r1=2*erand08(), dx=( (r1<1) ? sqrt(r1)-1: 1-sqrt(2-r1) );"
"            float r2=2*erand08(), dy=( (r2<1) ? sqrt(r2)-1: 1-sqrt(2-r2) );"
"            vec3 d = cx * (((sx+.5 + dx)/2 + x)/w - .5) +"
"                     cy * (((sy+.5 + dy)/2 + y)/h - .5) + cam.d;"
"            r = r + radiance(Ray(cam.o+d*140,normalize(d)))*(1./samps);"
"      } // Camera rays are pushed ^^^^^ forward to start in interior\n"
"      c=c+vec3(clamp(r.x),clamp(r.y),clamp(r.z))*.25;"
"      r.x=0;r.y=0;r.z=0;"
"    }"
"  }"
"  frag_colour = vec4( c.x, c.y, c.z, 1);"
"}";

GLuint vs = glCreateShader(GL_VERTEX_SHADER);
glShaderSource(vs, 1, &vertex_shader, NULL);
glCompileShader(vs); check_shader_errors( vs ); assert (glGetError () == GL_NO_ERROR);
GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(fs, 1, &fragment_shader, NULL);
glCompileShader(fs); check_shader_errors( fs ); assert (glGetError () == GL_NO_ERROR);

GLuint shader_programme = glCreateProgram();
glAttachShader(shader_programme, fs);           assert (glGetError () == GL_NO_ERROR);
glAttachShader(shader_programme, vs);           assert (glGetError () == GL_NO_ERROR);
glLinkProgram(shader_programme);                assert (glGetError () == GL_NO_ERROR);

int uniform_image_size = glGetUniformLocation(shader_programme,"image_size");
  glUseProgram(shader_programme);               assert (glGetError () == GL_NO_ERROR);
glUniform2i(uniform_image_size, renderBufferWidth, renderBufferHeight);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glDrawArrays(GL_TRIANGLES, 2, 3);
  glFinish();                                   assert (glGetError () == GL_NO_ERROR);

   GLvoid * data = malloc(4 * renderBufferWidth * renderBufferHeight);
   if( data ) {
     glReadPixels(0, 0, renderBufferWidth, renderBufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, data);
     glFinish();assert (glGetError () == GL_NO_ERROR);
   }

  FILE *f = fopen("t.ppm", "w");         // Write image to PPM file.
  fprintf(f, "P3\n%d %d\n%d\n", renderBufferWidth, renderBufferHeight, 255); unsigned char* d; d=data;
  int i; for (i=0; i<(renderBufferWidth*renderBufferHeight); i++) { unsigned x,y,z; x=d[i*4]; y=d[i*4+1]; z=d[i*4+2];
    fprintf(f,"%d %d %d ", x,y,z); } fclose(f);

   eglDestroyContext (egl_dpy, core_ctx);
   eglTerminate (egl_dpy);
   gbm_device_destroy (gbm);
   close (fd);

   return 0;
}
