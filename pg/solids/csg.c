#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// constants
#define COPLANAR 0
#define FRONT 1
#define BACK 2
#define BOTH 3
#define EPS 1e-5f

// macros
#define FOR(type, name, list) \
    { \
        type *name; \
        for (int i = 0; i < (list)->count; i++) { \
            name = list_ptr(list, i);

#define END_FOR }}

// data structures
typedef struct {
    int capacity;
    int count;
    int size;
    void *data;
} List;

typedef struct {
    float x;
    float y;
    float z;
} Vector;

typedef struct {
    Vector position;
    Vector normal;
    Vector uv;
} Vertex;

typedef struct {
    Vector normal;
    float w;
} Plane;

typedef struct {
    Plane plane;
    Vertex vertices[3];
} Polygon;

typedef struct Node Node;

struct Node {
    Plane plane;
    List polygons;
    Node *front;
    Node *back;
};

// list functions
void list_alloc(List *list, int size) {
    list->capacity = 4;
    list->count = 0;
    list->size = size;
    list->data = malloc(list->size * list->capacity);
}

void list_free(List *list) {
    free(list->data);
    // memset(list, 0, sizeof(List));
}

void list_copy(List *dst, List *src) {
    dst->capacity = src->capacity;
    dst->count = src->count;
    dst->size = src->size;
    dst->data = malloc(dst->size * dst->capacity);
    memcpy(dst->data, src->data, dst->size * dst->capacity);
}

void list_replace(List *dst, List *src) {
    list_free(dst);
    list_copy(dst, src);
    list_free(src);
}

void list_grow(List *list) {
    int capacity = list->capacity * 2;
    void *data = malloc(list->size * capacity);
    memcpy(data, list->data, list->size * list->capacity);
    free(list->data);
    list->capacity = capacity;
    list->data = data;
}

void *list_ptr(List *list, int index) {
    return (char *)list->data + list->size * index;
}

void list_get(List *list, int index, void *item) {
    memcpy(item, list_ptr(list, index), list->size);
}

void list_set(List *list, int index, void *item) {
    memcpy(list_ptr(list, index), item, list->size);
}

void list_append(List *list, void *item) {
    if (list->count == list->capacity) {
        list_grow(list);
    }
    list_set(list, list->count, item);
    list->count++;
}

void list_extend(List *dst, List *src) {
    FOR (void, item, src) {
        list_append(dst, item);
    } END_FOR;
}

void list_clear(List *list) {
    list->count = 0;
}

void list_reverse(List *list) {
    void *a = malloc(list->size);
    void *b = malloc(list->size);
    int n = list->count;
    for (int i = 0; i < n / 2; i++) {
        int j = n - i - 1;
        list_get(list, i, a);
        list_get(list, j, b);
        list_set(list, i, b);
        list_set(list, j, a);
    }
    free(a);
    free(b);
}

// vector functions
void vec_neg(Vector *out, Vector *a) {
    out->x = -a->x;
    out->y = -a->y;
    out->z = -a->z;
}

void vec_add(Vector *out, Vector *a, Vector *b) {
    out->x = a->x + b->x;
    out->y = a->y + b->y;
    out->z = a->z + b->z;
}

void vec_sub(Vector *out, Vector *a, Vector *b) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
    out->z = a->z - b->z;
}

void vec_mul(Vector *out, Vector *a, float b) {
    out->x = a->x * b;
    out->y = a->y * b;
    out->z = a->z * b;
}

void vec_div(Vector *out, Vector *a, float b) {
    out->x = a->x / b;
    out->y = a->y / b;
    out->z = a->z / b;
}

float vec_dot(Vector *a, Vector *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

float vec_len(Vector *a) {
    return sqrt(vec_dot(a, a));
}

void vec_cross(Vector *out, Vector *a, Vector *b) {
    float x = a->y * b->z - a->z * b->y;
    float y = a->z * b->x - a->x * b->z;
    float z = a->x * b->y - a->y * b->x;
    out->x = x;
    out->y = y;
    out->z = z;
}

void vec_unit(Vector *out, Vector *a) {
    vec_div(out, a, vec_len(a));
}

void vec_lerp(Vector *out, Vector *a, Vector *b, float t) {
    out->x = a->x + (b->x - a->x) * t;
    out->y = a->y + (b->y - a->y) * t;
    out->z = a->z + (b->z - a->z) * t;
}

// vertex functions
void vertex_flip(Vertex *a) {
    vec_neg(&a->normal, &a->normal);
}

void vertex_lerp(Vertex *out, Vertex *a, Vertex *b, float t) {
    vec_lerp(&out->position, &a->position, &b->position, t);
    vec_lerp(&out->normal, &a->normal, &b->normal, t);
    vec_lerp(&out->uv, &a->uv, &b->uv, t);
}

// plane functions
void plane_copy(Plane *dst, Plane *src) {
    memcpy(dst, src, sizeof(Plane));
}

void plane_from_points(Plane *out, Vector *a, Vector *b, Vector *c) {
    Vector ab;
    Vector ac;
    vec_sub(&ab, b, a);
    vec_sub(&ac, c, a);
    vec_cross(&out->normal, &ab, &ac);
    vec_unit(&out->normal, &out->normal);
    out->w = vec_dot(&out->normal, a);
}

void plane_flip(Plane *a) {
    vec_neg(&a->normal, &a->normal);
    a->w = -a->w;
}

void plane_split(
    Plane *plane, Polygon *polygon,
    List *co_front, List *co_back, List *front, List *back)
{
    Plane *p;
    p = plane;
    p = &polygon->plane;
    int polygon_type = 0;
    int vertex_types[3];
    for (int i = 0; i < 3; i++) {
        Vertex *vertex = &polygon->vertices[i];
        float w = vec_dot(&plane->normal, &vertex->position) - plane->w;
        int t = COPLANAR;
        if (w < -EPS) {
            t = BACK;
        }
        if (w > EPS) {
            t = FRONT;
        }
        polygon_type |= t;
        vertex_types[i] = t;
    }
    if (polygon_type == COPLANAR) {
        if (vec_dot(&plane->normal, &polygon->plane.normal) > 0) {
            list_append(co_front, polygon);
        }
        else {
            list_append(co_back, polygon);
        }
    }
    else if (polygon_type == FRONT) {
        list_append(front, polygon);
    }
    else if (polygon_type == BACK) {
        list_append(back, polygon);
    }
    else {
        List f;
        List b;
        list_alloc(&f, sizeof(Vertex));
        list_alloc(&b, sizeof(Vertex));
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            Vertex *v1 = &polygon->vertices[i];
            Vertex *v2 = &polygon->vertices[j];
            int t1 = vertex_types[i];
            int t2 = vertex_types[j];
            if (t1 != BACK) {
                list_append(&f, v1);
            }
            if (t1 != FRONT) {
                list_append(&b, v1);
            }
            if ((t1 | t2) == BOTH) {
                Vector d;
                vec_sub(&d, &v2->position, &v1->position);
                float ta = plane->w - vec_dot(&plane->normal, &v1->position);
                float tb = vec_dot(&plane->normal, &d);
                Vertex v;
                vertex_lerp(&v, v1, v2, ta / tb);
                list_append(&f, &v);
                list_append(&b, &v);
            }
        }
        if (f.count >= 3) {
            Polygon p;
            for (int i = 2; i < f.count; i++) {
                list_get(&f, 0, &p.vertices[0]);
                list_get(&f, i - 1, &p.vertices[1]);
                list_get(&f, i, &p.vertices[2]);
                plane_from_points(
                    &p.plane,
                    &p.vertices[0].position,
                    &p.vertices[1].position,
                    &p.vertices[2].position);
                list_append(front, &p);
            }
        }
        if (b.count >= 3) {
            Polygon p;
            for (int i = 2; i < b.count; i++) {
                list_get(&b, 0, &p.vertices[0]);
                list_get(&b, i - 1, &p.vertices[1]);
                list_get(&b, i, &p.vertices[2]);
                plane_from_points(
                    &p.plane,
                    &p.vertices[0].position,
                    &p.vertices[1].position,
                    &p.vertices[2].position);
                list_append(back, &p);
            }
        }
        list_free(&f);
        list_free(&b);
    }
}

// polygon functions
void polygon_flip(Polygon *a) {
    plane_flip(&a->plane);
    Vertex temp = a->vertices[0];
    a->vertices[0] = a->vertices[2];
    a->vertices[2] = temp;
    for (int i = 0; i < 3; i++) {
        vertex_flip(&a->vertices[i]);
    }
}

// node functions
void node_alloc(Node *node) {
    list_alloc(&node->polygons, sizeof(Polygon));
    node->front = 0;
    node->back = 0;
}

void node_free(Node *node) {
    if (node->front) {
        node_free(node->front);
    }
    if (node->back) {
        node_free(node->back);
    }
    list_free(&node->polygons);
    free(node);
}

void node_polygons(Node *node, List *list) {
    FOR (Polygon, polygon, &node->polygons) {
        list_append(list, polygon);
    } END_FOR;
    if (node->front) {
        node_polygons(node->front, list);
    }
    if (node->back) {
        node_polygons(node->back, list);
    }
}

void node_build(Node *node, List *polygons) {
    if (polygons->count == 0) {
        return;
    }
    if (node->polygons.count == 0) {
        Polygon *p = list_ptr(polygons, 0);
        plane_copy(&node->plane, &p->plane);
    }
    List front;
    List back;
    list_alloc(&front, sizeof(Polygon));
    list_alloc(&back, sizeof(Polygon));
    FOR (Polygon, polygon, polygons) {
        plane_split(
            &node->plane, polygon,
            &node->polygons, &node->polygons, &front, &back);
    } END_FOR;
    if (front.count) {
        if (node->front == 0) {
            node->front = malloc(sizeof(Node));
            node_alloc(node->front);
        }
        node_build(node->front, &front);
    }
    if (back.count) {
        if (node->back == 0) {
            node->back = malloc(sizeof(Node));
            node_alloc(node->back);
        }
        node_build(node->back, &back);
    }
    list_free(&front);
    list_free(&back);
}

void node_build_from(Node *node, Node *other) {
    List polygons;
    list_alloc(&polygons, sizeof(Polygon));
    node_polygons(other, &polygons);
    node_build(node, &polygons);
    list_free(&polygons);
}

void node_from_polygons(Node *node, List *polygons) {
    node_alloc(node);
    node_build(node, polygons);
}

void node_invert(Node *node) {
    FOR (Polygon, polygon, &node->polygons) {
        polygon_flip(polygon);
    } END_FOR;
    plane_flip(&node->plane);
    if (node->front) {
        node_invert(node->front);
    }
    if (node->back) {
        node_invert(node->back);
    }
    Node *temp = node->front;
    node->front = node->back;
    node->back = temp;
}

void node_clip_polygons(Node *node, List *polygons, List *out) {
    List front;
    List back;
    list_alloc(&front, sizeof(Polygon));
    list_alloc(&back, sizeof(Polygon));
    FOR (Polygon, polygon, polygons) {
        plane_split(&node->plane, polygon, &front, &back, &front, &back);
    } END_FOR;
    if (node->front) {
        List new_front;
        list_alloc(&new_front, sizeof(Polygon));
        node_clip_polygons(node->front, &front, &new_front);
        list_replace(&front, &new_front);
    }
    if (node->back) {
        List new_back;
        list_alloc(&new_back, sizeof(Polygon));
        node_clip_polygons(node->back, &back, &new_back);
        list_replace(&back, &new_back);
    }
    else {
        list_clear(&back);
    }
    list_extend(out, &front);
    list_extend(out, &back);
    list_free(&front);
    list_free(&back);
}

void node_clip_to(Node *node, Node *other) {
    List polygons;
    list_alloc(&polygons, sizeof(Polygon));
    node_clip_polygons(other, &node->polygons, &polygons);
    list_replace(&node->polygons, &polygons);
    if (node->front) {
        node_clip_to(node->front, other);
    }
    if (node->back) {
        node_clip_to(node->back, other);
    }
}

// csg functions
void csg_union(List *out, List *m1, List *m2) {
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    node_from_polygons(a, m1);
    node_from_polygons(b, m2);
    node_clip_to(a, b);
    node_clip_to(b, a);
    node_invert(b);
    node_clip_to(b, a);
    node_invert(b);
    node_build_from(a, b);
    node_polygons(a, out);
    node_free(a);
    node_free(b);
}

void csg_difference(List *out, List *m1, List *m2) {
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    node_from_polygons(a, m1);
    node_from_polygons(b, m2);
    node_invert(a);
    node_clip_to(a, b);
    node_clip_to(b, a);
    node_invert(b);
    node_clip_to(b, a);
    node_invert(b);
    node_build_from(a, b);
    node_invert(a);
    node_polygons(a, out);
    node_free(a);
    node_free(b);
}

void csg_intersection(List *out, List *m1, List *m2) {
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    node_from_polygons(a, m1);
    node_from_polygons(b, m2);
    node_invert(a);
    node_clip_to(b, a);
    node_invert(b);
    node_clip_to(a, b);
    node_clip_to(b, a);
    node_build_from(a, b);
    node_invert(a);
    node_polygons(a, out);
    node_free(a);
    node_free(b);
}

void csg_inverse(List *out, List *m1) {
    Node *a = malloc(sizeof(Node));
    node_from_polygons(a, m1);
    node_invert(a);
    node_polygons(a, out);
    node_free(a);
}

// interface
void triangles(List *out, float *data, int count) {
    float *d = data;
    for (int i = 0; i < count; i++) {
        Polygon polygon;
        for (int j = 0; j < 3; j++) {
            float x = *(d++);
            float y = *(d++);
            float z = *(d++);
            float nx = *(d++);
            float ny = *(d++);
            float nz = *(d++);
            float u = *(d++);
            float v = *(d++);
            polygon.vertices[j].position.x = x;
            polygon.vertices[j].position.y = y;
            polygon.vertices[j].position.z = z;
            polygon.vertices[j].normal.x = nx;
            polygon.vertices[j].normal.y = ny;
            polygon.vertices[j].normal.z = nz;
            polygon.vertices[j].uv.x = u;
            polygon.vertices[j].uv.y = v;
            polygon.vertices[j].uv.z = 0;
        }
        plane_from_points(
            &polygon.plane,
            &polygon.vertices[0].position,
            &polygon.vertices[1].position,
            &polygon.vertices[2].position);
        list_append(out, &polygon);
    }
}

void triangulate(List *polygons, float *data) {
    float *d = data;
    FOR (Polygon, polygon, polygons) {
        for (int i = 0; i < 3; i++) {
            Vertex *vertex = &polygon->vertices[i];
            *(d++) = vertex->position.x;
            *(d++) = vertex->position.y;
            *(d++) = vertex->position.z;
            *(d++) = vertex->normal.x;
            *(d++) = vertex->normal.y;
            *(d++) = vertex->normal.z;
            *(d++) = vertex->uv.x;
            *(d++) = vertex->uv.y;
        }
    } END_FOR;
}
