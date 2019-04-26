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
struct Intersection{
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
  // char phi, theta; // compressed incident direction   // not sure about this one
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
vec3 DirectLight(const Intersection &i);
vec3 PhotonLight(const Intersection &i, vec4 originalPos);
void EmitPhotons(int number, screen *screen);
void PhotonMap(screen *screen);
vec3 DrawPhotons(Intersection intersection);
float RandomNumber(int max);
vec4 ConvertTo2D(vec4 v);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  EmitPhotons(1000, screen);
  while (Update())
  {
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
void Draw(screen *screen)
{
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));

  Intersection intersection;
  // Intersection intersection2;
  // Intersection intersection3;
  // Intersection intersection4;

  float focalLength = (float)SCREEN_WIDTH;

  for (int x = 0; x < SCREEN_WIDTH; x++)
  {
    for (int y = 0; y < SCREEN_HEIGHT; y++)
    {
      vec4 d(x - SCREEN_WIDTH / 2, y - SCREEN_WIDTH / 2 - 0.25, focalLength, 1);
      // vec4 d2(x - SCREEN_WIDTH / 2 + 0.25, y - SCREEN_WIDTH / 2, focalLength, 1);
      // vec4 d3(x - SCREEN_WIDTH / 2 - 0.25, y - SCREEN_WIDTH / 2, focalLength, 1);
      // vec4 d4(x - SCREEN_WIDTH / 2, y - SCREEN_WIDTH / 2 + +0.25, focalLength, 1);

      d = glm::normalize(d);
      // d2 = glm::normalize(d2);
      // d3 = glm::normalize(d3);
      // d4 = glm::normalize(d4);

      if (closestIntersection(cameraPos, d, triangles, intersection))
      {

        //this is an attempt to average out the 4 rays that are casted per pixel.
        // vec3 finalColour = DirectLight(intersection);
        // PutPixelSDL(screen, x, y, finalColour);
        // finalColour = finalColour * 0.25f;


        if(x<3){
          vec3 colour;
          for (int i = 0; i < photons.size(); i++)
          {
            vec4 distance = (photons[i].position - intersection.position);
            if (glm::length(distance) < 1)
            {
              cout << "yes" << endl;
              colour += photons[i].lightPower;
            }
            else
            {
              cout << "no: distance = " << glm::length(distance) << endl;
            }
            PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1, 0, 0));
          }
        }
      }
    }
  }
}

vec3 DrawPhotons(Intersection intersection){
  vec3 colour;
  for (int i =0;i<photons.size();i++){
    vec4 distance = (photons[i].position - intersection.position);
    if (glm::length(distance) < 0.5){
      // cout << "yes" << endl;
      colour += photons[i].lightPower;
    }
    else{
      // cout << "no: distance = " << glm::length(distance) << endl;
    }
  }
  // cout << "photon lightpower: " << colour.x << "," << colour.y << "," << colour.z << endl;
  return colour;
}

float RandomNumber(int max)
{
  int deno = max * 100;
  float x = rand() % deno;
  x *= 1.0f / deno;
  return x;
}

void EmitPhotons(int number, screen *screen)
{
  int photonCount = 0;
  photons.resize(1);
  float x, y, z;
  Intersection intermediate;
  while (photonCount != number)
  {
    do
    {
      x = RandomNumber(1);
      y = RandomNumber(1);
      z = RandomNumber(1);
    } while (x * x + y * y + z * z > 1);
    vec4 d = vec4(x, y, z, 1);

    vec4 direction = glm::normalize(d);

    if (closestIntersection(lightPositions[0], direction, triangles, intermediate))
    {
      photons.resize(photonCount + 1);
      float russian = RandomNumber(1);
      if (russian <= diffuse)
      { // diffuse reflections
        photons[photonCount].position = intermediate.position;
        photons[photonCount].lightPower = PhotonLight(intermediate, lightPositions[0]);
        photons[photonCount].reflection = 1;
        photons[photonCount].active = true;
        PutPixelSDL(screen, ConvertTo2D(photons[photonCount].position).x, ConvertTo2D(photons[photonCount].position).y, vec3(1, 0, 0));
      }
      else if (russian > diffuse && russian < diffuse + specular)
      { //specular reflections
        // equation: dr = di - 2 * normal * (normal*di)
        photons[photonCount].position = intermediate.position;
        photons[photonCount].lightPower = PhotonLight(intermediate, lightPositions[0]);
        photons[photonCount].reflection = 2;
        photons[photonCount].active = true;
        PutPixelSDL(screen, ConvertTo2D(photons[photonCount].position).x, ConvertTo2D(photons[photonCount].position).y, vec3(1, 0, 0));
      }
      else
      { // absorption
        PutPixelSDL(screen, ConvertTo2D(photons[photonCount].position).x, ConvertTo2D(photons[photonCount].position).y, vec3(1, 0, 0));
        // cout << "absorption: " << photons[photonCount].position.x << "," << photons[photonCount].position.y << "," << photons[photonCount].position.z <<endl;
      }
      // cout << "after: " << photons[photonCount].position.x << "," << photons[photonCount].position.y << "," << photons[photonCount].position.z <<endl;

      // cout<<endl;
    }

    // cout << "photon " << photonCount << endl;
    // cout << "position: " << photons[photonCount].position.x << "," << photons[photonCount].position.y << "," << photons[photonCount].position.z << endl;
    // cout << "lightPower: " << photons[photonCount].lightPower.x << "," << photons[photonCount].lightPower.y << "," << photons[photonCount].lightPower.z << endl;
    // cout << "active: " << photons[photonCount].active << endl <<endl;

    photonCount++;
  }
  // cout << "1. photons.size(): " << photons.size() << endl;
}

void PhotonMap(screen *screen){
  float x, y, z;
  Intersection intermediate;
  // cout << "3. photons.size(): " << photons.size() << endl;
  for (int i = 0; i < photons.size(); i++){ 
    // cout << "4. photons.size(): " << photons.size() << endl;
    if (photons[i].active == true){
      do{
        x = RandomNumber(1);
        y = RandomNumber(1);
        z = RandomNumber(1);
      } while (x * x + y * y + z * z > 1);

      // no longer regarding this pixel
      photons[i].active = false;
      deadCount++;
      // cout << deadCount << endl;

      vec4 d = vec4(x, y, z, 1); // direction for photon

      vec4 direction = glm::normalize(d);

      if (closestIntersection(lightPositions[0], direction, triangles, intermediate)){
        photons.resize(photons.size() + 1);
        float russian = RandomNumber(1);
        if (russian <= diffuse)
        { // diffuse reflections
          photons[i].position = intermediate.position;
          photons[i].lightPower = PhotonLight(intermediate, lightPositions[0]);
          photons[i].reflection = 1;
          photons[i].active = true;
          PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1, 0, 0));
          for (int i =0;i<SCREEN_WIDTH;++i){
            for (int j =0;j<SCREEN_HEIGHT;++j){
              PutPixelSDL(screen, x, y, vec3(1, 0, 0));
          }
        }}
        else if (russian > diffuse && russian < diffuse + specular)
        { //specular reflections
          // equation: dr = di - 2 * normal * (normal*di)
          photons[i].position = intermediate.position;
          photons[i].lightPower = PhotonLight(intermediate, lightPositions[0]);
          photons[i].reflection = 2;
          photons[i].active = true;
          PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1, 0, 0));
        }
        else
        { // absorption
          // PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1, 0, 0));
          // cout << "absorption: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z <<endl;
        }
        // cout << "after: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z <<endl;

        // cout<<endl;
    }
    }
    else
    {
      // cout << "position: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z << endl;
      // cout << "lightPower: " << photons[i].lightPower.x << "," << photons[i].lightPower.y << "," << photons[i].lightPower.z << endl;
      // cout << "active: " << photons[i].active << endl;
    }
  }
}

vec3 PhotonLight(const Intersection &i, vec4 originalPos)
{

  vec3 surfacePower(3);
  vec3 result;

  Intersection intermediate;

  vec3 direction = vec3(originalPos - i.position);
  vec3 normalisedDir = glm::normalize(direction);
  vec3 normal = vec3(triangles[i.triangleIndex].normal);

  float radius = glm::distance(i.position, originalPos);

  float sphereSurfaceArea = 4.0f * M_PI * radius * radius;

  vec4 currentPosition = i.position; // current intersection position

  vec3 power = vec3(lightColor);

  if (closestIntersection(currentPosition, originalPos - currentPosition, triangles, intermediate))
  {
    if (intermediate.distance < glm::distance(currentPosition, originalPos))
    {
      power = vec3(0, 0, 0);
    }
  }

  surfacePower = triangles[i.triangleIndex].color * power * (glm::max(0.0f, glm::dot(normalisedDir, normal)) / (sphereSurfaceArea));

  surfacePower += triangles[i.triangleIndex].color * indirectLight;

  return surfacePower;
}

vec3 DirectLight(const Intersection &i)
{

  vec3 surfacePower(3);
  float count = 0.0f;
  vec3 result;
  vec3 average;

  for (int j = 0; j < lightPositions.size(); j++)
  {

    Intersection intermediate;
    vec3 direction = vec3(lightPositions[j] - i.position);
    vec3 normalisedDir = glm::normalize(direction);
    vec3 normal = vec3(triangles[i.triangleIndex].normal);

    float radius = glm::distance(i.position, lightPositions[j]);

    float sphereSurfaceArea = 4.0f * M_PI * radius * radius;

    vec4 currentPosition = i.position; // current intersection position

    vec3 power = vec3(lightColor);

    if (closestIntersection(currentPosition, lightPositions[j] - currentPosition, triangles, intermediate))
    {
      if (intermediate.distance < glm::distance(currentPosition, lightPositions[j]))
      {
        power = vec3(0, 0, 0);
      }
      else
      {
        count++;
      }
    }
    else
    {
      count++;
    }

    surfacePower = triangles[i.triangleIndex].color * power * (glm::max(0.0f, glm::dot(normalisedDir, normal)) / (sphereSurfaceArea));

    surfacePower.x *= count / lightPositions.size();
    surfacePower.y *= count / lightPositions.size();
    surfacePower.z *= count / lightPositions.size();

    surfacePower += triangles[i.triangleIndex].color * indirectLight;
  }
  return surfacePower;
}

bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection)
{

  //Rotation Matrix
  vec4 d1r(cos(theta), 0, sin(theta), 0);
  vec4 d2r(0, 1, 0, 0);
  vec4 d3r(-sin(theta), 0, cos(theta), 0);
  vec4 d4r(0, 0, 0, 1);
  mat4 yRot(d1r, d2r, d3r, d4r);

  bool found = false;

  float lowest = std::numeric_limits<float>::max();
  for (int i = 0; i < triangles.size(); ++i)
  {

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

    if ((u >= 0) && (v >= 0) && ((u + v) <= 1) && t >= 0)
    {
      if (t < lowest)
      {
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

bool Update()
{
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