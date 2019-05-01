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
  vec3 lightPower;
  vec3 direction;
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
vec4 cameraPos(0.0, 0.0, -3, 1.0);
vector<vec4> lightPositions = {
    vec4(0.0, -0.2, -0.9, 1.0)
};
vec3 lightColor = 50.0f * vec3(1, 1, 1);
vec3 indirectLight = 0.5f * vec3(1, 1, 1);

float theta = 0.0;

vector<Photon> globalPhotonMap;
float photonsToBeEmitted = 200000.0f;

float radius = 0.3f;
float diffuse = 0.5f;
float specular = 0.0f;

struct Node *root = NULL;
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
struct Node* newNode(int arr[]);
// void test(void);

int main(int argc, char *argv[]){

  screen *screen = InitializeSDL(SCREEN_WIDTH, SCREEN_HEIGHT, FULLSCREEN_MODE);

  LoadTestModel(triangles);

  EmitPhotons(photonsToBeEmitted); //max
  while (Update()){
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
  /* Clear buffer */
  memset(screen->buffer, 0, screen->height * screen->width * sizeof(uint32_t));

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
          for (int i = 0; i < globalPhotonMap.size(); i++){

            distance = (globalPhotonMap[i].position - vec3(intersection.position));
            if (glm::length(distance) < radius){
              colour +=  globalPhotonMap[i].lightPower/*/glm::length(distance)*/;
            }
            // PutPixelSDL(screen, ConvertTo2D(globalPhotonMap[i].position).x, ConvertTo2D(globalPhotonMap[i].position).y, vec3(1,1,1));
          }
          directLight = DirectLight(intersection);
          // if (directLight != vec3(0,0,0)){
          // colour *= directLight;
          // }
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

      vec3 power = vec3(lightColor);

      // if(closestIntersection(currentPosition, lightPositions[j]-currentPosition, triangles, intermediate)){
      //   if (intermediate.distance < glm::distance(currentPosition, lightPositions[j])){
      //     // power = vec3(0, 0, 0);
      //   }
      //   else{
      //     count++;
      //   }
      // }
      // else{
      //   count++;
      // }
      
      surfacePower = triangles[i.triangleIndex].color * power * (glm::max(0.0f, glm::dot(normalisedDir, normal)) / (sphereSurfaceArea));

      surfacePower.x *= count/lightPositions.size();
      surfacePower.y *= count/lightPositions.size();
      surfacePower.z *= count/lightPositions.size();

      surfacePower += triangles[i.triangleIndex].color * indirectLight; 
      
    }  
  return surfacePower;
}

void EmitPhotons(float photonCount){
  Intersection intermediate;
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
 

// // void test(void){
// //     int i;

// //     struct kd_node_t wp[] = {
// //         {{2.0f, 3.0f, 4}}, {{5, 4, 3}}, {{9, 6, 100}}, {{4, 7, 4}}, {{8, 1, 4}}, {{7, 2, 20}},{{5, 4, 2}},{{5, 5, 2}},{{7, 5, 0}},{{7, 4, 0}},{{2, 4, 3}},{{2, 3, 3}}, {{9.2f,2,0}}
// //     };


// //     // // how do i do thissss
// //     // for (int i = 0; i < photons.size(); i++){
// //     //   wp[i] = {{photons[i].position.x, photons[i].position.y, photons[i].position.z}};
// //     // }

// //     struct kd_node_t testNode = {{9, 2, 1}};
// //     struct kd_node_t *root, *found, *million;
// //     double best_dist;
 
// //     root = make_tree(wp, sizeof(wp) / sizeof(wp[1]), 0, 3);
 
// //     visited = 0;
// //     found = 0;
// //     nearest(root, &testNode, 0, 3, &found, &best_dist);
 
// //     printf(">> WP tree\nsearching for (%g, %g, %g)\n"
// //             "found (%g, %g, %g) dist %g\nseen %d nodes\n\n",
// //             testNode.x[0], testNode.x[1], testNode.x[2],
// //             found->x[0], found->x[1], found->x[2], sqrt(best_dist), visited);
 
// //     million = (struct kd_node_t*) calloc(N, sizeof(struct kd_node_t));
// //     srand(time(0));
// //     for (i = 0; i < N; i++)
// //       rand_pt(million[i]);
 
// //     root = make_tree(million, N, 0, 3);
// //     rand_pt(testNode);
 
// //     visited = 0;
// //     found = 0;
// //     nearest(root, &testNode, 0, 3, &found, &best_dist);
 
// //     printf(">> Million tree\nsearching for (%g, %g, %g)\n"
// //             "found (%g, %g, %g) dist %g\nseen %d nodes\n",
// //             testNode.x[0], testNode.x[1], testNode.x[2],
// //             found->x[0], found->x[1], found->x[2],
// //             sqrt(best_dist), visited);
 
// //     /* search many random points in million tree to see average behavior.
// //        tree size vs avg nodes visited:
// //        10      ~  7
// //        100     ~ 16.5
// //        1000        ~ 25.5
// //        10000       ~ 32.8
// //        100000      ~ 38.3
// //        1000000     ~ 42.6
// //        10000000    ~ 46.7              */
// //     int sum = 0, test_runs = 100000;
// //     for (i = 0; i < test_runs; i++) {
// //         found = 0;
// //         visited = 0;
// //         rand_pt(testNode);
// //         nearest(root, &testNode, 0, 3, &found, &best_dist);
// //         sum += visited;
// //     }
// //       printf("\n>> Million tree\n"
// //             "visited %d nodes for %d random findings (%f per lookup)\n",
// //             sum, test_runs, sum/(double)test_runs);

// // }