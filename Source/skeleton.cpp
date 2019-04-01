#include <iostream>
#include <glm/glm.hpp>
#include <SDL.h>
#include "SDLauxiliary.h"
#include "TestModelH.h"
#include <stdint.h>
#include "limits.h"
#include <math.h>
#include <stdlib.h>

using namespace std;
using glm::mat3;
using glm::mat4;
using glm::vec3;
using glm::vec4;

SDL_Event event;

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 256
#define FULLSCREEN_MODE false

/* --------------------------GLOBALS ---------------------------------*/
struct Intersection{
  vec4 position;
  float distance;
  int triangleIndex;
};

vector<Triangle> triangles;
vec4 cameraPos(0.0, 0.0, -3, 1.0);

float theta = 0.0;

/* --------------------------GLOBALS ---------------------------------*/
vec4 lightPos(0.0, -0.5, -0.7, 1.0);
vec3 lightColor = 14.f * vec3(1, 1, 1);
vec3 indirectLight = 0.5f*vec3(1,1,1);
/* ----------------------------------------------------------------------------*/
//  FUNCTIONS                                                                   */

bool Update();
void Draw(screen *screen);
bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection);
vec3 DirectLight(const Intersection &i);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  while (Update()){
    Draw(screen);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

float toRadian(float x){
  return x * 3.1415 / 180;
}

// / Place your drawing here /
void Draw(screen *screen){
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));

  Intersection intersection;

  float focalLength = (float)SCREEN_WIDTH;


  for (int x = 0; x < SCREEN_WIDTH; x++){
    for (int y = 0; y < SCREEN_HEIGHT; y++){
      vec4 d(x - SCREEN_WIDTH / 2, y - SCREEN_WIDTH / 2, focalLength, 1);
      d = glm::normalize(d);
      if (closestIntersection(cameraPos, d, triangles, intersection)){
        PutPixelSDL(screen, x, y, DirectLight(intersection));
        //PutPixelSDL(screen, x, y, triangles[intersection.triangleIndex].color);
      }
    }
  }
}

vec3 DirectLight(const Intersection &i){

  Intersection intermediate;

  // vec3 R(i.position.x - lightPos.x, i.position.y - lightPos.y, i.position.z - lightPos.z);
  vec3 direction = vec3(lightPos - i.position);
  vec3 normalisedDir = glm::normalize(direction);
  vec3 normal = vec3(triangles[i.triangleIndex].normal);

  // float radius = R.x * R.x + R.y * R.y + R.z * R.z;
  float radius = glm::distance(i.position, lightPos);

  float sphereSurfaceArea = 4.0f * M_PI * radius*radius;

  vec4 currentPosition = i.position;  // current intersection position

  vec3 power =  vec3(lightColor);

  vec4 normalA(normal.x, normal.y,normal.z, 1);

  if(closestIntersection(currentPosition, lightPos-currentPosition, triangles, intermediate)){
    if (intermediate.distance < glm::distance(currentPosition, lightPos)){
      power = vec3(0, 0, 0);
    }
  }
  vec3 surfacePower = triangles[i.triangleIndex].color * power * (glm::max(0.0f, glm::dot(normalisedDir, normal))/( sphereSurfaceArea));

  return surfacePower + triangles[i.triangleIndex].color*indirectLight;
}

// / Place updates of parameters here /
bool Update(){
  static int t = SDL_GetTicks();
  /* Compute frame time */
  int t2 = SDL_GetTicks();
  float dt = float(t2 - t);
  t = t2;

  //---- Translation vectrors-----
  vec4 moveForward(0, 0, 1, 0);
  vec4 moveBackward(0, 0, -1, 0);
  vec4 moveLeft(-1, 0, 0, 0);
  vec4 moveRight(1, 0, -1, 0);

  SDL_Event e;
  while (SDL_PollEvent(&e)){
    if (e.type == SDL_QUIT){
      return false;
    }
    else if (e.type == SDL_KEYDOWN){
      int key_code = e.key.keysym.sym;
      switch (key_code){
      case SDLK_UP:{
        /* Move camera forward */
        cameraPos = cameraPos + moveForward;
        // return true;
        break;
      }
      case SDLK_DOWN:{
        cameraPos = cameraPos + moveBackward;
        // return true;
        break;
      }
      case SDLK_LEFT:{
        theta -= 0.05;
        // return true;
        break;
      }
      case SDLK_RIGHT:{
        theta += 0.05;

        // return true;
        break;
      }
      case SDLK_ESCAPE:{
        /* Move camera quit */
        // return false;
        break;
      }
      case SDLK_w:{
        lightPos += moveForward;
        break;
      }
      case SDLK_s:{
        lightPos += moveBackward;
        break;
      }
      case SDLK_a:{
        lightPos += moveLeft;
        break;
      }
      case SDLK_d:{
        lightPos += moveRight;
        break;
      }
        // case default:
        //   break;
      }
    }
  }
  return true;
}

bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection){

  bool found = false;

  vec4 d1r(cos(theta), 0, sin(theta), 0);
  vec4 d2r(0, 1, 0, 0);
  vec4 d3r(-sin(theta), 0, cos(theta), 0);
  vec4 d4r(0, 0, 0, 1);
  mat4 yRot(d1r, d2r, d3r, d4r);

  float lowest = std::numeric_limits<float>::max();
  for (int i = 0; i < triangles.size(); ++i){
    vec4 v0 = yRot * triangles[i].v0;
    vec4 v1 = yRot * triangles[i].v1;

    vec4 v2 = yRot * triangles[i].v2;

    vec4 e1 = vec4(v1-v0);
    vec4 e2 = vec4(v2-v0);
    vec4 b = vec4(start-v0);

    vec3 d = glm::normalize(vec3(dir));
    //d = glm::normalize(d);

    mat3 A(-d, vec3(e1), vec3(e2));

    vec3 x = glm::inverse(A) * vec3(b);

    float t = x.x;

    float u = x.y;
    float v = x.z;

    if ((u >= 0) && (v >= 0) && ((u + v) <= 1) && t >= 0){
      if (t < lowest){
        closestIntersection.distance = t;
        closestIntersection.position = /*(u * e1 + v * e2 + v0); */ start + (t * dir);
        closestIntersection.triangleIndex = i;
        lowest = t;
        found = true;
      }
    }

  }
  return found;
}
