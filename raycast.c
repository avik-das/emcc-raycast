#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define SCREEN_WIDTH  256
#define SCREEN_HEIGHT 256

// -- 2D VECTOR ----------------------------------------------------------------

struct vec2 {
    float x;
    float y;
};

struct vec2 * vec2_create(float x, float y) {
    struct vec2 *v = (struct vec2 *) malloc(sizeof(struct vec2));
    v->x = x;
    v->y = y;

    return v;
}

struct vec2 * vec2_rotate(struct vec2 *v, float ang) {
    float c = cos(ang);
    float s = sin(ang);
    float x = v->x * c - v->y * s;
    float y = v->x * s + v->y * c;

    return vec2_create(x, y);
}

// -- 2D MATRIX ----------------------------------------------------------------

struct mat2 {
    float a, b;
    float c, d;
};

struct mat2 * mat2_create(float a, float b, float c, float d) {
    struct mat2 *m = (struct mat2 *) malloc(sizeof(struct mat2));
    m->a = a; m->b = b;
    m->c = c; m->d = d;

    return m;
}

float mat2_determinant(struct mat2 *m) {
    return m->a * m->d - m->b * m->c;
}

struct mat2 * mat2_invert(struct mat2 *m) {
    float det = mat2_determinant(m);
    if (det < 1e-10f && det > -1e-10f)
        return NULL;

    return mat2_create( m->d / det, -m->b / det,
                       -m->c / det,  m->a / det);
}

struct vec2 *mat2_left_mult_vec(struct mat2 *m, struct vec2 *v) {
    return vec2_create(m->a * v->x + m->b * v->y,
                       m->c * v->x + m->d * v->y);
}

// -- LINE ---------------------------------------------------------------------

struct line_t {
    struct vec2 start;
    struct vec2 dir;
};

struct line_t * line_create(float startx, float starty,
        float dirx, float diry) {
    struct line_t *l = (struct line_t *) malloc(sizeof(struct line_t));
    l->start.x = startx;
    l->start.y = starty;
    l->dir.x = dirx;
    l->dir.y = diry;

    return l;
}

struct vec2 * line_intersect(struct line_t *l1, struct line_t *l2) {
    struct mat2 *A = mat2_create(l1->dir.x, -l2->dir.x,
                                 l1->dir.y, -l2->dir.y);

    struct mat2 *inv = mat2_invert(A);
    if (A == NULL) {
        free(A);
        return vec2_create(-1.0f, -1.0f);
    }

    struct vec2 *b = vec2_create(l2->start.x - l1->start.x,
                                 l2->start.y - l1->start.y);

    struct vec2 *ts = mat2_left_mult_vec(inv, b);

    free(A);
    free(b);
    free(inv);

    return ts;
}

// -- COLOR --------------------------------------------------------------------

#define SDL_COLOR_SKY(surface) SDL_MapRGB((surface)->format, 128, 128, 255)
#define SDL_COLOR_GROUND(surface) SDL_MapRGB((surface)->format, 255, 128, 0)
#define SDL_COLOR_ERROR(surface) SDL_MapRGB((surface)->format, 255, 0, 255)

struct color_t {
    float r;
    float g;
    float b;
};

void color_intensify_inplace(struct color_t *color, float mult) {
    color->r *= mult;
    color->g *= mult;
    color->b *= mult;
}

int sdl_value_from_color(SDL_Surface *surface, struct color_t *color) {
    char r = round(color->r * 255);
    char g = round(color->g * 255);
    char b = round(color->b * 255);
    return SDL_MapRGB(surface->format, r, g, b);
}

// -- DATA ---------------------------------------------------------------------

struct wall_t {
    struct line_t line;
    struct color_t color;
};

void add_wall(struct wall_t *walls, int i,
        float startx, float starty, float endx, float endy,
        float r, float g, float b) {
    float dirx = endx - startx;
    float diry = endy - starty;

    walls[i] = (struct wall_t) {
        .line = { { startx, starty }, { dirx, diry } },
        .color = { r, g, b }
    };
}

struct map_t {
    struct wall_t *walls;
    size_t num_walls;
};

struct map_t * load_map() {
    struct map_t * map = (struct map_t *) malloc(sizeof(struct map_t));
    map->num_walls = 5;

    map->walls = (struct wall_t *)
        malloc(sizeof(struct wall_t) * map->num_walls);

    add_wall(map->walls, 0, 0.0f, 0.5f, 0.2f, 0.5f, 1.0f, 0.0f, 0.0f);
    add_wall(map->walls, 1, 0.2f, 0.5f, 0.2f, 0.3f, 0.0f, 1.0f, 0.0f);
    add_wall(map->walls, 2, 0.2f, 0.3f, 0.8f, 0.3f, 0.0f, 0.0f, 1.0f);
    add_wall(map->walls, 3, 0.8f, 0.3f, 0.8f, 0.5f, 0.0f, 1.0f, 1.0f);
    add_wall(map->walls, 4, 0.8f, 0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f);

    return map;
}

struct player_t {
    struct vec2 pos;
    struct vec2 dir;
    float fov;
};

struct player_t * player_create() {
    struct player_t *p = (struct player_t *) malloc(sizeof(struct player_t));
    p->pos = (struct vec2) { 0.5,  1.0};
    p->dir = (struct vec2) { 0.0, -1.0};
    p->fov = M_PI / 2.0f;

    return p;
}

struct canvas_t {
    SDL_Surface *surface;
    int w;
    int h;
};

// -- RENDERING ----------------------------------------------------------------

struct hit_t {
    float t;
    struct wall_t * wall;
    float wall_ts;
};

struct hit_t * hit_create(float t, struct wall_t *wall, float wall_ts) {
    struct hit_t *h = (struct hit_t *) malloc(sizeof(struct hit_t));
    h->t = t;
    h->wall = wall;
    h->wall_ts = wall_ts;

    return h;
}

void draw_colums(struct canvas_t *canvas, struct player_t * player,
        struct hit_t **hits) {
    SDL_Surface *surface = canvas->surface;
    SDL_Rect rect;

    float d_ang = player->fov / canvas->w;

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        float ang = d_ang * (x - canvas->w / 2);

        // 1. draw sky
        rect = (SDL_Rect) {
            .x = x,
            .y = 0,
            .w = 1,
            .h = canvas->h / 2
        };
        SDL_FillRect(surface, &rect, SDL_COLOR_SKY(surface));

        // 2. draw ground
        rect = (SDL_Rect) {
            .x = x,
            .y = canvas->h / 2,
            .w = 1,
            .h = canvas->h / 2
        };
        SDL_FillRect(surface, &rect, SDL_COLOR_GROUND(surface));

        // 3. draw wall if necessary
        struct hit_t *hit = hits[x];
        if (hit == NULL)
            continue;

        float h = canvas->h * 0.25 / (hit->t * cos(ang));
        if (h <= 0.0f)
            continue;

        rect = (SDL_Rect) {
            .x = x,
            .y = (canvas->h - h) / 2,
            .w = 1,
            .h = h
        };

        struct color_t color = hit->wall->color;

        if (hit->t > 1.0f) {
            color_intensify_inplace(&color, 1.0f / hit->t);
        }

        int sdl_color = sdl_value_from_color(surface, &color);
        SDL_FillRect(surface, &rect, sdl_color);
    }
}

void cast_one_ray(struct map_t *map, struct hit_t **hits, int x,
        struct line_t *ray) {
    float min_ts = INFINITY;
    struct wall_t *closest_wall = NULL;
    float wall_ts = INFINITY;

    for (int i = 0; i < map->num_walls; i++) {
        struct wall_t *wall = map->walls + i;

        struct vec2 *ts = line_intersect(ray, &wall->line);

        if (ts->x > 0.0f &&
                ts->y > 0.0f &&
                ts->y < 1.0f &&
                ts->x < min_ts) {
            min_ts = ts->x;
            closest_wall = wall;
            wall_ts = ts->y;
        }

        free(ts);
    }

    if (closest_wall != NULL) {
        hits[x] = hit_create(min_ts, closest_wall, wall_ts);
    } else {
        hits[x] = NULL;
    }
}

struct hit_t ** cast_rays(struct map_t *map, struct player_t *player,
        int nrays) {
    float d_ang = player->fov / nrays;
    struct hit_t ** hits = (struct hit_t **)
        malloc(sizeof(struct hit_t *) * nrays);

    for (int x = 0; x < nrays; x++) {
        float ang = -player->fov / 2 + d_ang * x + d_ang / 2;
        struct vec2 *dir = vec2_rotate(&player->dir, ang);
        struct line_t *ray = line_create(player->pos.x, player->pos.y,
                dir->x, dir->y);

        cast_one_ray(map, hits, x, ray);

        free(dir);
        free(ray);
    }

    return hits;
}

// -- MAIN LOOP ----------------------------------------------------------------

int main(int argc, char** argv) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Surface *surface = SDL_SetVideoMode(
          SCREEN_WIDTH,
          SCREEN_HEIGHT,
          32,
          SDL_HWSURFACE
          );

  struct canvas_t canvas = {
      .surface = surface,
      .w = SCREEN_WIDTH,
      .h = SCREEN_HEIGHT
  };

  struct player_t *player = player_create();
  struct map_t * map = load_map();

#ifdef TEST_SDL_LOCK_OPTS
  EM_ASM(
          "SDL.defaults.copyOnLock = false;"
          "SDL.defaults.discardOnLock = true;"
          "SDL.defaults.opaqueFrontBuffer = false;"
        );
#endif

  struct hit_t **hits = cast_rays(map, player, canvas.w);
  draw_colums(&canvas, player, hits);
  free(hits);

  SDL_Flip(surface);

  SDL_Quit();

  return 0;
}
