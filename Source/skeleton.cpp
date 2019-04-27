#include <iostream>
#include <random>
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
#define FOCAL_LENGTH SCREEN_HEIGHT

/* ------------------------------------------------------------STRUCTS--------------------------------------------------------------- */
struct Intersection
{
  vec4 position;
  float distance;
  int triangleIndex;
  vec3 surfacePower;
};

struct Photon{
  vec4 position;
  vec3 lightPower; // power
  int reflection;
  bool active;
  vec4 incidence;
  vec4 normal;
  // char phi, theta; // compressed incident direction        // not sure about this one
  // short flag;      // flag used in kdtree                  // not sure about this one
};

/* ------------------------------------------------------------STRUCTS--------------------------------------------------------------- */

/* ------------------------------------------------------------GLOBALS--------------------------------------------------------------- */

vector<Triangle> triangles;
vector<Photon> photons;

vec4 cameraPos(0.0, 0.0, -3, 1.0);
vector<vec4> lightPositions = {
    vec4(0.0, -0.4, -0.7, 1.0),
};

float theta = 0.0;

float diffuse = 0.6f;
float specular = 0.2f;
int deadCount = 0;

vec3 lightColor = 14.f * vec3(1, 1, 1);
vec3 indirectLight = 0.5f * vec3(1, 1, 1);

/* ------------------------------------------------------------GLOBALS--------------------------------------------------------------- */

/* ------------------------------------------------------------FUNCTIONS--------------------------------------------------------------- */
bool Update();
void Draw(screen *screen);
bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection);
void EmitPhotons(float number, screen *screen);
void PhotonMap(screen *screen);
vec3 DrawPhotons(Intersection intersection);
float RandomNumber(float max);
vec4 ConvertTo2D(vec4 v);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  EmitPhotons(1000.0f, screen);
  while (Update()){
    PhotonMap(screen);
    Draw(screen);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

vec4 ConvertTo2D(vec4 v){
  //Projection formula to calculate the projected points from a 4d point
  vec4 p;
  p.x = FOCAL_LENGTH * v.x / v.z + SCREEN_WIDTH / 2;
  p.y = FOCAL_LENGTH * v.y / v.z + SCREEN_HEIGHT / 2;
  return p;
}

// / Place your drawing here /
void Draw(screen *screen){
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));

  Intersection intersection;

  float focalLength = (float)SCREEN_WIDTH;

  for (int x = 0; x < SCREEN_WIDTH; x++){
    for (int y = 0; y < SCREEN_HEIGHT; y++){
      vec4 d(x - SCREEN_WIDTH / 2, y - SCREEN_WIDTH / 2 - 0.25, focalLength, 1);

      d = glm::normalize(d);

      if (closestIntersection(cameraPos, d, triangles, intersection)){

          vec3 colour;
          for (int i = 0; i < photons.size(); i++){
            PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1,1,1));
            // cout << photons[i].lightPower.x << "," << photons[i].lightPower.y << "," << photons[i].lightPower.z << endl;
          }

          //   vec4 distance = (photons[i].position - intersection.position);
          //   if (glm::length(distance) < 1){
          //     // cout << "yes: distance = " << glm::length(distance) << endl;
          //     // cout << photons[i].lightPower.x << "," << photons[i].lightPower.y << "," << photons[i].lightPower.z << endl;
          //     colour += photons[i].lightPower;
          //   }
          //   else{
          //     // cout << "no: distance = " << glm::length(distance) << endl;
          //   }
          //   PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, colour);
          // }
          // PutPixelSDL(screen, x, y, colour);
      }
    }
  }
}

vec3 DrawPhotons(Intersection intersection){
  vec3 colour;
  for (int i = 0; i < photons.size(); i++){
    vec4 distance = (photons[i].position - intersection.position);
    if (glm::length(distance) < 0.5){
      colour += photons[i].lightPower;
    }
    else{ }
  }
  return colour;
}

void EmitPhotons(float number, screen *screen){
  float photonCount = 0.0f;
  photons.resize(1);
  float x, y, z;
  Intersection intermediate;
  while (photonCount != number){
    do{
      x = RandomNumber(1.0f);
      y = RandomNumber(1.0f);
      z = RandomNumber(1.0f);
    } while (x * x + y * y + z * z > 1);
    vec4 d = vec4(x, y, z, 1);

    vec4 direction = glm::normalize(d);

    if (closestIntersection(lightPositions[0], direction, triangles, intermediate)){
      photons[photonCount].incidence = direction;
      photons[photonCount].normal = triangles[intermediate.triangleIndex].normal;

      photons.resize(photonCount + 1);
      // setting photon light to be equal to be fraction of lightpower
      photons[photonCount].lightPower = vec3(lightColor[0]);
      photons[photonCount].lightPower *= 1 / number;

      float russian = RandomNumber(1);

      // diffuse reflections
      if (russian <= diffuse){
        photons[photonCount].position = intermediate.position;
        // photons[photonCount].lightPower *= triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
        // photons[photonCount].reflection = 1;
        photons[photonCount].active = true;
      }

      //specular reflections
      else if (russian > diffuse && russian < diffuse + specular){
        photons[photonCount].position = intermediate.position;
        // photons[photonCount].reflection = 2;
        photons[photonCount].active = true;
      }

      // absorption
      else{
        photons[photonCount].position = intermediate.position;
      }
    }
    photonCount++;
  }
}

void PhotonMap(screen *screen){
  float x, y, z;
  Intersection intermediate;
  vec4 d;
  for (int i = 0; i < photons.size(); i++){

    if (photons[i].active == true){
      // if (photons[i].reflection == 2){
        // equation: dr = di - 2 * normal * (normal*di)
        // d = photons[i].incidence - (photons[i].normal * 2.0f) * (photons[i].normal*photons[i].incidence); 
      // }
      // else{
        do{
          x = RandomNumber(1);
          y = RandomNumber(1);
          z = RandomNumber(1);
        } while (x * x + y * y + z * z > 1);
        cout << x << "," << y << "," << z << endl;
        d = vec4(x, y, z, 1); // direction for photon
      // }

      // no longer regarding this pixel
      photons[i].active = false;
      deadCount++;

      vec4 direction = glm::normalize(d);

      if (closestIntersection(lightPositions[0], direction, triangles, intermediate)){

        photons.resize(photons.size() + 1);

        // photons[i].incidence = direction;
        // photons[i].normal = triangles[intermediate.triangleIndex].normal;

        float russian = RandomNumber(1);

        photons[i].position = intermediate.position;

        // diffuse reflections
        if (russian <= diffuse){
          photons[i].position = intermediate.position;
          // photons[i].lightPower *= triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
          // photons[i].reflection = 1;
          photons[i].active = true;
        }

        //specular reflections
        else if (russian > diffuse && russian < diffuse + specular){
          photons[i].position = intermediate.position;
          // photons[i].reflection = 2;
          photons[i].active = true;
        }
        
        // absorption
        else{ 
          photons[i].position = intermediate.position;
        }
      }
      else{
        // cout << "rejection" << endl;/
      }
    }

    else { }
  }
}

float RandomNumber(float max){
  int deno = max * 200;
  float x = rand() % deno;
  x *= max * 2.0f / deno;
  return x-1.0f;
}

bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection){

  //Rotation Matrix
  vec4 d1r(cos(theta), 0, sin(theta), 0);
  vec4 d2r(0, 1, 0, 0);
  vec4 d3r(-sin(theta), 0, cos(theta), 0);
  vec4 d4r(0, 0, 0, 1);
  mat4 yRot(d1r, d2r, d3r, d4r);

  bool found = false;

  float lowest = std::numeric_limits<float>::max();
  for (int i = 0; i < triangles.size(); ++i){

    // rotation
    vec4 v0 = yRot * triangles[i].v0;
    vec4 v1 = yRot * triangles[i].v1;
    vec4 v2 = yRot * triangles[i].v2;
    vec4 e1 = vec4(v1 - v0);
    vec4 e2 = vec4(v2 - v0);
    vec4 b = vec4(start - v0);
    vec3 d = glm::normalize(vec3(dir));
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
  while (SDL_PollEvent(&e))
  {
    if (e.type == SDL_QUIT)
    {
      return false;
    }
    else if (e.type == SDL_KEYDOWN)
    {
      int key_code = e.key.keysym.sym;
      switch (key_code)
      {
      case SDLK_UP:
      {
        /* Move camera forward */
        cameraPos = cameraPos + moveForward;
        break;
      }
      case SDLK_DOWN:
      {
        cameraPos = cameraPos + moveBackward;
        break;
      }
      case SDLK_LEFT:
      {
        theta += 0.05;
        break;
      }
      case SDLK_RIGHT:
      {
        theta -= 0.05;
        break;
      }
      case SDLK_ESCAPE:
      {
        break;
      }
      case SDLK_w:
      {
        for (int i = 0; i < lightPositions.size(); i++)
        {
          lightPositions[i] += moveForward;
        }
        break;
      }
      case SDLK_s:
      {
        for (int i = 0; i < lightPositions.size(); i++)
        {
          lightPositions[i] += moveBackward;
        }
        break;
      }
      case SDLK_a:
      {
        for (int i = 0; i < lightPositions.size(); i++)
        {
          lightPositions[i] += moveLeft;
        }
        break;
      }
      case SDLK_d:
      {
        for (int i = 0; i < lightPositions.size(); i++)
        {
          lightPositions[i] += moveRight;
        }
        break;
      }
      }
    }
  }
  return true;
}