#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

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

struct vec2 * line_eval(struct line_t *line, float t) {
    return vec2_create(line->start.x + line->dir.x * t,
                       line->start.y + line->dir.y * t);
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

// -- TEXTURE ------------------------------------------------------------------

#define TEXTURE_NAME(base) "textures/" base

struct texture_t {
    SDL_Surface *surface;
    // in case we need more data in the future
};

// -- DATA ---------------------------------------------------------------------

struct wall_t {
    int has_texture;
    struct line_t line;
    struct color_t color;
    struct texture_t texture;
};

void add_wall(struct wall_t *walls, int i,
        float startx, float starty, float endx, float endy,
        float r, float g, float b) {
    float dirx = endx - startx;
    float diry = endy - starty;

    walls[i] = (struct wall_t) {
        .has_texture = 0,
        .line = { { startx, starty }, { dirx, diry } },
        .color = { r, g, b },
        .texture = { 0 }
    };
}

void add_texture(struct wall_t *wall, const char *texture_name) {
    SDL_Surface *surface = IMG_Load(texture_name);

    wall->has_texture = 1;
    wall->texture = (struct texture_t) {
        .surface = surface
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

    add_wall(map->walls, 0, 0.0f, 0.5f, 0.2f, 0.5f, 0.0f, 0.0f, 0.0f);
    add_wall(map->walls, 1, 0.2f, 0.5f, 0.2f, 0.3f, 0.0f, 0.0f, 0.0f);
    add_wall(map->walls, 2, 0.2f, 0.3f, 0.8f, 0.3f, 0.0f, 0.0f, 0.0f);
    add_wall(map->walls, 3, 0.8f, 0.3f, 0.8f, 0.5f, 1.0f, 0.0f, 0.0f);
    add_wall(map->walls, 4, 0.8f, 0.5f, 1.0f, 0.5f, 0.0f, 0.0f, 1.0f);

    add_texture(map->walls    , TEXTURE_NAME("brick.jpg"));
    add_texture(map->walls + 1, TEXTURE_NAME("brick.jpg"));
    add_texture(map->walls + 2, TEXTURE_NAME("orange-damascus.png"));

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

void player_rotate(struct player_t *player, float ang) {
    struct vec2 *new_dir = vec2_rotate(&player->dir, ang);
    player->dir = (struct vec2) {
        .x = new_dir->x,
        .y = new_dir->y
    };
    free(new_dir);
}

int player_attempt_move(struct player_t *player, struct map_t *map,
        float dist) {
    struct line_t ray = (struct line_t) {
        .start = {
            .x = player->pos.x,
            .y = player->pos.y
        },
        .dir = {
            .x = player->dir.x * dist,
            .y = player->dir.y * dist
        }
    };

    for (int i = 0; i < map->num_walls; i++) {
        struct wall_t *wall = map->walls + i;

        struct vec2 *ts = line_intersect(&ray, &wall->line);

        if (ts->x < 0 || ts->x > 0 ||
            ts->y < 0 || ts->y > 1) {
            free(ts);
            continue;
        }

        free(ts);
        return 0;
    }

    struct vec2 *new_pos = line_eval(&ray, 1.0f);
    player->pos = (struct vec2) {
        .x = new_pos->x,
        .y = new_pos->y
    };

    free(new_pos);
    return 1;
}

struct canvas_t {
    SDL_Surface *surface;
    int w;
    int h;
    int has_rendered;
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

    int num_pixels =
        4 * canvas->surface->w * canvas->surface->h;
    char *sw_rendered = (char *) malloc(num_pixels);
    memset(sw_rendered, 0, num_pixels);

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

        if (hit->wall->has_texture) {
            SDL_Surface *texsurface = hit->wall->texture.surface;
            int texw = texsurface->w;
            int texh = texsurface->h;
            int srcx = ((int) round(texw * hit->wall_ts)) % texw;
            int dsty = (canvas->h - h) / 2;
            for (int dsty_offset = 0; dsty_offset < h; dsty_offset++) {
                dsty++;

                int srcy = (int) (((float) dsty_offset) / h * texh);

                int dst_offset = srcy * texsurface->w + srcx;
                *((Uint32 *) sw_rendered + dsty * surface->w + x) =
                    ((Uint32 *) texsurface->pixels)[dst_offset];
            }
        } else {
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

    SDL_Surface *overlay = SDL_CreateRGBSurfaceFrom(
            sw_rendered,
            surface->w,
            surface->h,
            32,
            surface->w * 4,
            0x0000FF,
            0x00FF00,
            0xFF0000,
            0
            );

    SDL_Rect srcrect = (SDL_Rect) {
        .x = 0,
        .y = 0,
        .w = surface->w,
        .h = surface->h
    };

    SDL_Rect dstrect = (SDL_Rect) {
        .x = 0,
        .y = 0,
        .w = 0,
        .h = 0
    };

    SDL_BlitSurface(
            overlay,
            &srcrect,
            surface,
            &dstrect
            );

    SDL_FreeSurface(overlay);
    free(sw_rendered);
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

struct map_t *map;
struct player_t *player;
struct canvas_t *canvas;

int handle_keypress(SDL_KeyboardEvent *key) {
    switch (key->keysym.sym) {
    case SDLK_LEFT:
        player_rotate(player, -0.087);
        return 1;

    case SDLK_RIGHT:
        player_rotate(player, 0.087);
        return 1;

    case SDLK_UP:
        return player_attempt_move(player, map, 0.025);

    case SDLK_DOWN:
        return player_attempt_move(player, map, -0.025);

    default:
        return 0;
    }
}

int handle_events() {
    SDL_Event event;

    int rerender = 0;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            rerender = handle_keypress(&event.key) || rerender;
            break;

        default:
            break;
        }
    }

    return rerender;
}

void iterate() {
    if (handle_events() || !canvas->has_rendered) {
        canvas->has_rendered = 1;

        struct hit_t **hits = cast_rays(map, player, canvas->w);
        draw_colums(canvas, player, hits);
        free(hits);

        SDL_Flip(canvas->surface);
    }
}

int main(int argc, char** argv) {
  SDL_Init(SDL_INIT_VIDEO);
  SDL_Surface *surface = SDL_SetVideoMode(
          SCREEN_WIDTH,
          SCREEN_HEIGHT,
          32,
          SDL_HWSURFACE
          );

  canvas = (struct canvas_t *) malloc(sizeof(struct canvas_t));
  *canvas = (struct canvas_t) {
      .surface = surface,
      .w = SCREEN_WIDTH,
      .h = SCREEN_HEIGHT,
      .has_rendered = 0
  };

  player = player_create();
  map = load_map();

#ifdef TEST_SDL_LOCK_OPTS
  EM_ASM(
          "SDL.defaults.copyOnLock = false;"
          "SDL.defaults.discardOnLock = true;"
          "SDL.defaults.opaqueFrontBuffer = false;"
        );
#endif

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(iterate, 0, 1);
#endif
  // If we wanted to support non-asm.js targets, we would support a manual
  // main loop.

  SDL_Quit();

  return 0;
}
