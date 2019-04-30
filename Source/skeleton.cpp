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


#define N 1000000
#define rand1() (rand() / (double)RAND_MAX)
#define rand_pt(v) { v.x[0] = rand1(); v.x[1] = rand1(); v.x[2] = rand1(); }

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
  vec3 position;
  vec3 lightPower; // power
  int reflection;
  bool active;
  vec3 incidence;
  vec3 normal;
  // short flag;      // flag used in kdtree                  // not sure about this one
};

const int k = 2; 
  
// A structure to represent node of kd tree 
struct Node {
  int point[k]; // To store k dimensional point 
  Node *left, *right; 
};

/* ------------------------------------------------------------STRUCTS--------------------------------------------------------------- */

/* ------------------------------------------------------------GLOBALS--------------------------------------------------------------- */

vector<Triangle> triangles;
struct Node *root = NULL;
vector<Photon> photons;
float photonsToBeEmitted = 2025.0f; //this is the maximum
int hit = 0;
int absorbed = 0;

/* global variable, so sue me */
int visited;

vec4 cameraPos(0.0, 0.0, -3, 1.0);
vector<vec4> lightPositions = {
    vec4(0.0, -0.4, -0.7, 1.0),
};

float theta = 0.0;

float diffuse = 0.7f;
float specular = 0.1f;

vec3 lightColor = 14.0f * vec3(1, 1, 1);
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
vec3 DirectLight(const Intersection &i);
struct Node* newNode(int arr[]);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  EmitPhotons(photonsToBeEmitted, screen); //max
  while (Update()){
    PhotonMap(screen);
    Draw(screen);
    SDL_Renderframe(screen);
  }

  SDL_SaveImage(screen, "screenshot.bmp");

  KillSDL(screen);
  return 0;
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
  int count=0;
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
          vec3 distance;
          for (int i = 0; i < photons.size(); i++){

            distance = (photons[i].position - vec3(intersection.position));
            if (glm::length(distance) < 0.6){
              count++;

              // cout << "photons.position: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z << endl;
              // cout << "yes: distance = " << glm::length(distance) << endl;
              // cout << photons[i].lightPower.x << "," << photons[i].lightPower.y << "," << photons[i].lightPower.z << endl;
              colour +=  photons[i].lightPower/glm::length(distance);
            }
            else{
              // cout << "no: distance = " << glm::length(distance) << endl;
            }
            // PutPixelSDL(screen, ConvertTo2D(photons[i].position).x, ConvertTo2D(photons[i].position).y, vec3(1,1,1));
            // cout << "photons.position: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z << endl;
          }
          // cout << count << " nearest neighbours found" << endl;
          PutPixelSDL(screen, x, y, colour);
      }
    }
  }
}

vec3 DirectLight(const Intersection &i){

  vec3 surfacePower(3);
  float count = 0.0f;
  vec3 result;
  vec3 average;

  for (int j = 0; j < lightPositions.size(); j++){
  
      Intersection intermediate;
      vec3 direction = vec3(lightPositions[j] - i.position);
      vec3 normalisedDir = glm::normalize(direction);
      vec3 normal = vec3(triangles[i.triangleIndex].normal);

      float radius = glm::distance(i.position, lightPositions[j]);

      float sphereSurfaceArea = 4.0f * M_PI * radius*radius;

      vec4 currentPosition = i.position;  // current intersection position

      vec3 power =  vec3(lightColor);

      if(closestIntersection(currentPosition, lightPositions[j]-currentPosition, triangles, intermediate)){
        if (intermediate.distance < glm::distance(currentPosition, lightPositions[j])){
          power = vec3(0, 0, 0);
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


void EmitPhotons(float number, screen *screen){
  float i = 0.0f;
  float photonCount = 0.0f;
  photons.resize(1);
  float x, y, z;
  Intersection intermediate;
  while (i != number){
    do{
      x = RandomNumber(1.0f);
      y = RandomNumber(1.0f);
      z = RandomNumber(1.0f);
    } while (x * x + y * y + z * z > 1);

    vec4 d = vec4(x, y, z, 1);

    vec4 direction = glm::normalize(d);

    if (closestIntersection(lightPositions[0], direction, triangles, intermediate)){

      float russian = RandomNumber(1);

      // diffuse reflections
      if (russian <= diffuse){
        photons.resize(photonCount + 1);

        photons[photonCount].incidence = vec3(direction);
        photons[photonCount].normal = vec3(triangles[intermediate.triangleIndex].normal);

        // setting photon light to be equal to be fraction of lightpower
        photons[photonCount].lightPower = vec3(lightColor[0]);
        photons[photonCount].lightPower *= 1 / number;

        photons[photonCount].position = vec3(intermediate.position);
        // cout << "incoming: " << photons[photonCount].lightPower.x << "," << photons[photonCount].lightPower.y << "," << photons[photonCount].lightPower.z << endl;
        // cout << "intermediate.position: " << intermediate.position.x << "," << intermediate.position.y << "," << intermediate.position.z << endl;
        photons[photonCount].lightPower *= triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
        // cout << "outgoing: " << photons[photonCount].lightPower.x << "," << photons[photonCount].lightPower.y << "," << photons[photonCount].lightPower.z << endl << endl;
        photons[photonCount].reflection = 1;
        photons[photonCount].active = true;
        photonCount++;
      }

      //specular reflections
      else if (russian > diffuse && russian < diffuse + specular){
        photons.resize(photonCount + 1);

        photons[photonCount].incidence = vec3(direction);
        photons[photonCount].normal = vec3(triangles[intermediate.triangleIndex].normal);

        // setting photon light to be equal to be fraction of lightpower
        photons[photonCount].lightPower = vec3(lightColor[0]);
        photons[photonCount].lightPower *= 1 / number;

        photons[photonCount].position = vec3(intermediate.position);

        // cout << "intermediate.position: " << intermediate.position.x << "," << intermediate.position.y << "," << intermediate.position.z << endl;
        // photons[photonCount].lightPower = triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
        photons[photonCount].reflection = 2;
        photons[photonCount].active = true;
        photonCount++;
      }

      // absorption
      else{ }

      i++;
    }
  }
}


void PhotonMap(screen *screen){
  float x, y, z;
  Intersection intermediate;
  vec3 placeholder;
  vec4 d;
  for (int i = 0; i < photons.size(); i++){
    if (photons[i].active == true){
        vec4 direction;
        vec4 start;
        start.x = photons[i].position.x;
        start.y = photons[i].position.y;
        start.z = photons[i].position.z;
        start.w = 1;

      // cout << "photons[i].position: " << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z << endl;
      if (photons[i].reflection == 2){
        // equation: dr = di - 2 * normal * (normal*di)
        placeholder = photons[i].incidence - (photons[i].normal * 2.0f) * (photons[i].normal*photons[i].incidence);
        d.x = placeholder.x;
        d.y = placeholder.y;
        d.z = placeholder.z;
        d.w = 1;
      }
      else{
        do{
          x = RandomNumber(1);
          y = RandomNumber(1);
          z = RandomNumber(1);

          d = vec4(x, y, z, 1); // direction for photon
          direction = glm::normalize(d);

        } while (x * x + y * y + z * z > 1 && closestIntersection(start, direction, triangles, intermediate));
        // cout << x << "," << y << "," << z << endl;
      }

      // no longer regarding this pixel
      photons[i].active = false;

      float russian = RandomNumber(1);

        // diffuse reflections
      if (russian <= diffuse){
        photons.resize(photons.size() + 1);

        photons[photons.size()-1].incidence = vec3(direction);
        photons[photons.size()-1].normal = vec3(triangles[intermediate.triangleIndex].normal);

        photons[photons.size()-1].position = vec3(intermediate.position);
          // cout << "intermediate.position: " << intermediate.position.x << "," << intermediate.position.y << "," << intermediate.position.z << endl;
        photons[photons.size()-1].lightPower *= triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
        photons[photons.size()-1].reflection = 1;
        photons[photons.size()-1].active = true;
      }

      //specular reflections
      else if (russian > diffuse && russian < diffuse + specular){
        photons.resize(photons.size() + 1);

        photons[photons.size()-1].incidence = vec3(direction);
        photons[photons.size()-1].normal = vec3(triangles[intermediate.triangleIndex].normal);

        photons[photons.size()-1].position = vec3(intermediate.position);
        // cout << "intermediate.position: " << intermediate.position.x << "," << intermediate.position.y << "," << intermediate.position.z << endl;
        // photons[i].lightPower = triangles[intermediate.triangleIndex].color; // multiply the current surface colour by lightpower
        photons[photons.size()-1].reflection = 2;
        photons[photons.size()-1].active = true;
      }
        
      // absorption
      else{ }

    }

    // problem is found here at least, may not originate from here but
    // so many photons have a position of (0,0,0) which is affecting the scene, need to solve before the lab 100%

    else{
      if (photons[i].position.z == 0){
        cout << "photons[i].position: " << i << endl << photons[i].position.x << "," << photons[i].position.y << "," << photons[i].position.z << endl;
      }
      else{ }
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

  // cout << "Render time: " << dt << endl;

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

 
// #define MAX_DIM 3
// struct kd_node_t{
//     double x[MAX_DIM];
//     struct kd_node_t *left, *right;
// };
 
// inline double dist(struct kd_node_t *a, struct kd_node_t *b, int dim){
//     double t, d = 0;
//     while (dim--) {
//         t = a->x[dim] - b->x[dim];
//         d += t * t;
//     }
//     return d;
// }

// inline void swap(struct kd_node_t *x, struct kd_node_t *y) {
//     double tmp[MAX_DIM];
//     memcpy(tmp,  x->x, sizeof(tmp));
//     memcpy(x->x, y->x, sizeof(tmp));
//     memcpy(y->x, tmp,  sizeof(tmp));
// }
 
 
// /* see quickselect method */
// struct kd_node_t* find_median(struct kd_node_t *start, struct kd_node_t *end, int idx){
//     if (end <= start) return NULL;
//     if (end == start + 1)
//         return start;
 
//     struct kd_node_t *p, *store, *md = start + (end - start) / 2;
//     double pivot;
//     while (1) {
//         pivot = md->x[idx];
 
//         swap(md, end - 1);
//         for (store = p = start; p < end; p++) {
//             if (p->x[idx] < pivot) {
//                 if (p != store)
//                     swap(p, store);
//                 store++;
//             }
//         }
//         swap(store, end - 1);
 
//         /* median has duplicate values */
//         if (store->x[idx] == md->x[idx])
//             return md;
 
//         if (store > md) end = store;
//         else        start = store;
//     }
// }
 
// struct kd_node_t* make_tree(struct kd_node_t *t, int len, int i, int dim){
//     struct kd_node_t *n;
 
//     if (!len) return 0;
 
//     if ((n = find_median(t, t + len, i))){
//         i = (i + 1) % dim;
//         n->left  = make_tree(t, n - t, i, dim);
//         n->right = make_tree(n + 1, t + len - (n + 1), i, dim);
//     }
//     return n;
// }
 
// void nearest(struct kd_node_t *root, struct kd_node_t *nd, int i, int dim, struct kd_node_t **best, double *best_dist){
//     double d, dx, dx2;
 
//     if (!root) return;
//     d = dist(root, nd, dim);
//     dx = root->x[i] - nd->x[i];
//     dx2 = dx * dx;
 
//     visited++;
 
//     if (!*best || d < *best_dist) {
//         *best_dist = d;
//         *best = root;
//     }
 
//     /* if chance of exact match is high */
//     if (!*best_dist) return;
 
//     if (++i >= dim)
//       i = 0;
 
//     nearest(dx > 0 ? root->left : root->right, nd, i, dim, best, best_dist);
//     if (dx2 >= *best_dist) return;
//     nearest(dx > 0 ? root->right : root->left, nd, i, dim, best, best_dist);
// }
 

// void test(void){
//     int i;

//     struct kd_node_t wp[] = {
//         {{2.0f, 3.0f, 4}}, {{5, 4, 3}}, {{9, 6, 100}}, {{4, 7, 4}}, {{8, 1, 4}}, {{7, 2, 20}},{{5, 4, 2}},{{5, 5, 2}},{{7, 5, 0}},{{7, 4, 0}},{{2, 4, 3}},{{2, 3, 3}}, {{9.2f,2,0}}
//     };


//     // // how do i do thissss
//     // for (int i = 0; i < photons.size(); i++){
//     //   wp[i] = {{photons[i].position.x, photons[i].position.y, photons[i].position.z}};
//     // }

//     struct kd_node_t testNode = {{9, 2, 1}};
//     struct kd_node_t *root, *found, *million;
//     double best_dist;
 
//     root = make_tree(wp, sizeof(wp) / sizeof(wp[1]), 0, 3);
 
//     visited = 0;
//     found = 0;
//     nearest(root, &testNode, 0, 3, &found, &best_dist);
 
//     printf(">> WP tree\nsearching for (%g, %g, %g)\n"
//             "found (%g, %g, %g) dist %g\nseen %d nodes\n\n",
//             testNode.x[0], testNode.x[1], testNode.x[2],
//             found->x[0], found->x[1], found->x[2], sqrt(best_dist), visited);
 
//     million = (struct kd_node_t*) calloc(N, sizeof(struct kd_node_t));
//     srand(time(0));
//     for (i = 0; i < N; i++)
//       rand_pt(million[i]);
 
//     root = make_tree(million, N, 0, 3);
//     rand_pt(testNode);
 
//     visited = 0;
//     found = 0;
//     nearest(root, &testNode, 0, 3, &found, &best_dist);
 
//     printf(">> Million tree\nsearching for (%g, %g, %g)\n"
//             "found (%g, %g, %g) dist %g\nseen %d nodes\n",
//             testNode.x[0], testNode.x[1], testNode.x[2],
//             found->x[0], found->x[1], found->x[2],
//             sqrt(best_dist), visited);
 
//     /* search many random points in million tree to see average behavior.
//        tree size vs avg nodes visited:
//        10      ~  7
//        100     ~ 16.5
//        1000        ~ 25.5
//        10000       ~ 32.8
//        100000      ~ 38.3
//        1000000     ~ 42.6
//        10000000    ~ 46.7              */
//     int sum = 0, test_runs = 100000;
//     for (i = 0; i < test_runs; i++) {
//         found = 0;
//         visited = 0;
//         rand_pt(testNode);
//         nearest(root, &testNode, 0, 3, &found, &best_dist);
//         sum += visited;
//     }
//       printf("\n>> Million tree\n"
//             "visited %d nodes for %d random findings (%f per lookup)\n",
//             sum, test_runs, sum/(double)test_runs);

// }