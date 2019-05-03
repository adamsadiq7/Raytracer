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

#define SCREEN_WIDTH 512
#define SCREEN_HEIGHT 512
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
  vec3 position;
  vec3 lightPower;
  vec3 direction;
  // short flag;      // flag used in kdtree                  // not sure about this one
};

struct KDTree{
  Photon *node;
  Photon *left;
  Photon *right;
};

/* ------------------------------------------------------------STRUCTS--------------------------------------------------------------- */

/* ------------------------------------------------------------GLOBALS--------------------------------------------------------------- */

vector<Triangle> triangles;
vec4 cameraPos(0.0, 0.0, -3, 1.0);
vector<vec4> lightPositions = {
    vec4(0.0, -0.2, -0.9, 1.0)
};
vec3 lightColor = 100.0f * vec3(1, 1, 1);
vec3 indirectLight = 0.5f * vec3(1, 1, 1);

float theta = 0.0;

vector<Photon> globalPhotonMap;
vector<Photon*> globalPhotonPointers;
KDTree kdTree;
float photonsToBeEmitted = 1000000.0f;
int currentDimension;

int radianceCount = 100;

float radius = 0.3f;
float diffuse = 0.5f;
float specular = 0.0f;

int maxDepth = 3;
int currentDepth = 0;

/* global variable, so sue me */
int visited;

/* ------------------------------------------------------------GLOBALS--------------------------------------------------------------- */

/* ------------------------------------------------------------FUNCTIONS--------------------------------------------------------------- */
bool Update();
void Draw(screen *screen);
bool closestIntersection(vec4 start, vec4 dir, vector<Triangle> &triangles, Intersection &closestIntersection);
void EmitPhotons(float number);
void PhotonMap(Photon photon);
vec3 DrawPhotons(Intersection intersection);
float RandomNumber(float max);
vec4 ConvertTo2D(vec4 v);
vec3 FindSpecularDirection(vec3 normal, vec3 incidence);
vec4 FindDiffuseDirection();
vec3 DirectLight(const Intersection &i);
KDTree *BalanceTree(vector <Photon*> photonPointers);
void PopulatePointerTree();
void LocatePhotons(Photon* rootNode, vector<Photon*> minHeap, Intersection intersection, float &maxSearchDistance);
int FindMaxDimension();
int GetCurrentMaxDimension();
bool InAreaLight(Intersection intersection);
vec3 FindReflectionColour(Intersection intersection, vec4 direction);
vec3 CastRay(const vec4 orig, const vec4 dir);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  EmitPhotons(photonsToBeEmitted);
  // PopulatePointerTree();
  // BalanceTree(globalPhotonPointers);
  while (Update()){
    Draw(screen);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
}

void LocatePhotons(Photon* rootNode, vector<Photon*> minHeap, Intersection intersection, float &maxSearchDistance){

}

KDTree *BalanceTree(vector <Photon*> photonPointers){

  // cout << "photonPointers size:" << photonPointers.size() << endl;

  // cout << photonPointers[0] << endl;

  int dimension = FindMaxDimension();

  sort(photonPointers.begin(), photonPointers.end(), [](const Photon* lhs, const Photon* rhs) {
        return lhs->position[GetCurrentMaxDimension()] < rhs->position[GetCurrentMaxDimension()];
  });

  int median = floor(photonPointers.size()/2);

  KDTree kdTree;
  vector<Photon*> leftPointers;
  vector<Photon*> rightPointers;

  for (int i =0;i<photonPointers.size();i++){
    if (i<median) {leftPointers.push_back(photonPointers[i]);}
    else if (i> median) {rightPointers.push_back(photonPointers[i]);}
  }

  if (median != 0){
    kdTree.node = photonPointers[median];
  }

  kdTree.node = photonPointers[median];

  if (leftPointers.size() != 0) BalanceTree(leftPointers);
  if (rightPointers.size() != 0) BalanceTree(rightPointers);

  return &kdTree;
}

int GetCurrentMaxDimension(){
  return currentDimension;
}

int FindMaxDimension(){
  float lowestX = std::numeric_limits<float>::max();
  float largestX = std::numeric_limits<float>::min();
  float rangeX, rangeY, rangeZ = 0.0f;

  float lowestY = std::numeric_limits<float>::max();
  float largestY = std::numeric_limits<float>::min();

  float lowestZ = std::numeric_limits<float>::max();
  float largestZ = std::numeric_limits<float>::min();


  for (int i =0;i<globalPhotonMap.size();i++){
    if (globalPhotonMap[i].position.x < lowestX){
      lowestX = globalPhotonMap[i].position.x;
    }
    else if (globalPhotonMap[i].position.x > largestX){
      largestX = globalPhotonMap[i].position.x;
    }

    if (globalPhotonMap[i].position.y < lowestY){
      lowestY = globalPhotonMap[i].position.y;
    }
    else if (globalPhotonMap[i].position.y > largestY){
      largestY = globalPhotonMap[i].position.y;
    }

    if (globalPhotonMap[i].position.z < lowestZ){
      lowestZ = globalPhotonMap[i].position.z;
    }
    else if (globalPhotonMap[i].position.x > largestZ){
      largestZ = globalPhotonMap[i].position.z;
    }
  }

  rangeX = largestX - lowestX;
  rangeY = largestY - lowestY;
  rangeZ = largestZ - lowestZ;

  if (rangeX >= rangeY && rangeX >= rangeZ){
    currentDimension = 0;
  }
  else if (rangeY >= rangeX && rangeY >= rangeZ){
    currentDimension = 1;
  }
  else{
    currentDimension = 2;
  }
  return currentDimension;
}

void PopulatePointerTree(){
  for (int i =0;i<globalPhotonMap.size();i++){
    globalPhotonPointers.push_back(&globalPhotonMap[i]);
  }
}

vec4 ConvertTo2D(vec3 v){
  //Projection formula to calculate the projected points from a 4d point
  vec4 p;
  p.x = FOCAL_LENGTH * v.x / v.z + SCREEN_WIDTH / 2;
  p.y = FOCAL_LENGTH * v.y / v.z + SCREEN_HEIGHT / 2;
  return p;
}

// / Place your drawing here /
void Draw(screen *screen){
  /* Clear buffer */
  // memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));

  Intersection intersection;
  vec3 directLight;
  float focalLength = (float)SCREEN_WIDTH;

  for (int x = 0; x < SCREEN_WIDTH; x++){
    for (int y = 0; y < SCREEN_HEIGHT; y++){
      vec4 d(x - SCREEN_WIDTH / 2, y - SCREEN_WIDTH / 2 - 0.25, focalLength, 1);

      d = glm::normalize(d);

        if (closestIntersection(cameraPos, d, triangles, intersection)){

          vec3 colour;
          vec3 distance;

          if (InAreaLight(intersection)){
            PutPixelSDL(screen, x, y, vec3(1,1,1));
          }
          else if (triangles[intersection.triangleIndex].material.y > 0){
            colour = CastRay(cameraPos,d);
            PutPixelSDL(screen, x, y, colour);
          }
          else{
            for (int i = 0; i < globalPhotonMap.size(); i++){

              distance = (globalPhotonMap[i].position - vec3(intersection.position));
              if (glm::length(distance) < radius){
                // cout << "neighbour found" << endl;
                colour +=  globalPhotonMap[i].lightPower/*/glm::length(distance)*/;
              }
              // PutPixelSDL(screen, ConvertTo2D(globalPhotonMap[i].position).x, ConvertTo2D(globalPhotonMap[i].position).y, vec3(1,1,1));
            }
            directLight = DirectLight(intersection);

            PutPixelSDL(screen, x, y, colour);
          }
      }
    }
    SDL_Renderframe(screen);
  }
}

vec4 Reflect(const vec4 I, const vec4 N){
  // return glm::reflect(I,N);
  return I - 2.0f * glm::dot(I,N) * N;
}
 
vec3 CastRay(const vec4 orig, const vec4 dir){
  Intersection intersection;
  vec3 currentMaterial;
  vec3 hitColour;
    if (currentDepth > maxDepth){
      return triangles[intersection.triangleIndex].color;
    }
    if (closestIntersection(orig,dir,triangles,intersection)){
      currentMaterial = triangles[intersection.triangleIndex].material;
      if (currentMaterial == vec3(0.5,0.0,0.0)){
        return triangles[intersection.triangleIndex].color;
      }
      else if(currentMaterial == vec3(0.0f,0.6f,0.0f)){
        vec4 R = Reflect(dir,triangles[intersection.triangleIndex].normal);
        hitColour += 0.8f * CastRay(intersection.position + triangles[intersection.triangleIndex].normal * 0.0001f, R);
      }
    }
    return hitColour;
}

vec3 DirectLight(const Intersection &i){

  vec3 surfacePower(3);
  float count = 0.0f;
  vec3 result;

  for (int j = 0; j < lightPositions.size(); j++){
  
      Intersection intermediate;
      vec3 direction = vec3(lightPositions[j] - i.position);
      vec3 normalisedDir = glm::normalize(direction);
      vec3 normal = vec3(triangles[i.triangleIndex].normal);

      float radius = glm::distance(i.position, lightPositions[j]);

      float sphereSurfaceArea = 4.0f * M_PI * radius*radius;

      vec4 currentPosition = i.position;  // current intersection position

      vec3 power = vec3(lightColor);

      if(closestIntersection(currentPosition, lightPositions[j]-currentPosition, triangles, intermediate)){
        if (intermediate.distance < glm::distance(currentPosition, lightPositions[j])){
          // power = vec3(0, 0, 0);
        }
        else{
          count++;
        }
      }
      else{
        count++;
      }
      
      surfacePower = triangles[i.triangleIndex].color * power * (glm::max(0.0f, glm::dot(normalisedDir, normal)) / (sphereSurfaceArea));

      surfacePower.x *= count/lightPositions.size();
      surfacePower.y *= count/lightPositions.size();
      surfacePower.z *= count/lightPositions.size();

      surfacePower += triangles[i.triangleIndex].color * indirectLight; 
      
    }  
  return surfacePower;
}

bool InAreaLight(Intersection intersection){
  // cout << intersection.position.y << endl;
  if (intersection.position.x >= -0.2f && intersection.position.x <= 0.2f
  && intersection.position.y >= -1.0f && intersection.position.y <= -0.8f && intersection.position.z >= -0.7f &&intersection.position.z <= -0.3f){
    // cout << intersection.position.y << endl;
    return true;
  }
  return false;
}

void EmitPhotons(float photonCount){
  Photon photon;
  vec4 direction;

  for (int i = 0; i < photonCount; i++){

    photon.position = vec3(0.2*RandomNumber(1.0f), -0.9, lightPositions[0].z + 0.2*RandomNumber(1.0f));


    float u = fabs(RandomNumber(1.0f));
    float v = 2.0f * M_PI * fabs(RandomNumber(1.0f));
    vec3 d = vec3(cos(v)*sqrt(u), sqrt(1.0f-u), sin(v)*sqrt(u));

    // cout << "position:" << photon.position.x << ","<< photon.position.y << ","<<photon.position.z<<endl;
    // cout << "d:" << d.x << "," << d.y << "," << d.z << endl << endl;

    photon.direction = d;
    photon.lightPower = lightColor / photonCount;
    PhotonMap(photon);
  }
}

vec4 FindDiffuseDirection(){
  float x, y, z;
  do{
    x = RandomNumber(1.0f);
    y = RandomNumber(1.0f);
    z = RandomNumber(1.0f);
  } while (x * x + y * y + z * z > 1);

  vec4 d = vec4(x, y, z, 1);

  vec4 direction = glm::normalize(d);

  return direction;
}

vec3 FindSpecularDirection(vec3 normal, vec3 incidence){
  vec3 direction = incidence - (normal * 2.0f) * (normal*incidence);
  return direction;
}

void PhotonMap(Photon photon){
  Intersection intermediate;
  vec4 d;
  bool absorbed = false;

  while (!absorbed){

    if (closestIntersection(vec4(photon.position, 1), vec4(photon.direction, 1), triangles, intermediate)){
      float nigerian = fabs(RandomNumber(1)); // russian roulette

      // diffuse reflections
      if (nigerian <= triangles[intermediate.triangleIndex].material.x){
        photon.position = vec3(intermediate.position);
        photon.direction = vec3(FindDiffuseDirection());

        // if(!closestIntersection(vec4(photon.position, 1), vec4(photon.direction, 1), triangles, intermediate)){
        //   break;
        // }
        do{
          photon.direction = vec3(FindDiffuseDirection());
        } while(!closestIntersection(vec4(photon.position, 1), vec4(photon.direction, 1), triangles, intermediate));

        photon.lightPower *= triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower

        globalPhotonMap.push_back(photon); // add photon to global list;
      }

      //specular reflections
      else if (nigerian > triangles[intermediate.triangleIndex].material.x && nigerian < triangles[intermediate.triangleIndex].material.x + triangles[intermediate.triangleIndex].material.y){
        photon.position = vec3(intermediate.position);
        photon.direction = FindSpecularDirection(vec3(triangles[intermediate.triangleIndex].normal), photon.direction);
        if (!closestIntersection(vec4(photon.position, 1), vec4(photon.direction, 1), triangles, intermediate)){
          break;
        }
      }

      // absorption
      else{
        absorbed = true;
      }
    }
    else{
      break;
    }
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

  cout << "Render time: " << dt << endl;

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