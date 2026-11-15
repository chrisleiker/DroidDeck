#include "host.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DD_VERSION "0.7.1"

// The "Deck" palette of ui/Theme.kt.
#define C_BACKGROUND 0x0A0B0D
#define C_SURFACE 0x121417
#define C_SURFACE_VARIANT 0x1A1D22
#define C_LINE 0x262A31
#define C_LINE2 0x343A43
#define C_ON_BACKGROUND 0xF2F4F7
#define C_MUTED 0x9AA3AF
#define C_PRIMARY 0x1A9FFF
#define C_ON_PRIMARY 0x03111F
#define C_GOOD 0x4CD37F
#define C_ERROR 0xFF8A80

enum { PAGE_STEAM, PAGE_LIBRARY, PAGE_DESKTOP, PAGE_STORE, PAGE_SETUP, PAGE_COUNT };
static const char *const RAIL[PAGE_COUNT] = { "Steam", "Library", "Desktop", "Store", "Setup" };

static SDL_Renderer *renderer;
static float scale = 1;
static DdHost host;
static SDL_Texture *logo;

static int px(float v) { return (int) (v * scale + 0.5f); }

static void set_color(uint32_t rgb) {
    SDL_SetRenderDrawColor(renderer, rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff, 0xff);
}

static void fill(float x, float y, float w, float h, float radius, uint32_t rgb) {
    int x0 = px(x), y0 = px(y), w0 = px(w), h0 = px(h), r = px(radius);
    if (r * 2 > h0) r = h0 / 2;
    if (r * 2 > w0) r = w0 / 2;
    set_color(rgb);
    for (int row = 0; row < r; row++) {
        float dy = r - row - 0.5f;
        int inset = r - (int) (SDL_sqrtf(r * r - dy * dy) + 0.5f);
        SDL_Rect top = { x0 + inset, y0 + row, w0 - inset * 2, 1 };
        SDL_Rect bottom = { x0 + inset, y0 + h0 - 1 - row, w0 - inset * 2, 1 };
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bottom);
    }
    SDL_Rect middle = { x0, y0 + r, w0, h0 - r * 2 };
    SDL_RenderFillRect(renderer, &middle);
}

static void outline(float x, float y, float w, float h, float radius, float width, uint32_t rgb, uint32_t inner) {
    fill(x - width, y - width, w + width * 2, h + width * 2, radius + width, rgb);
    fill(x, y, w, h, radius, inner);
}

#define FONT_SLOTS 8
static struct { int size; TTF_Font *font; } fonts[FONT_SLOTS];

static TTF_Font *font_for(int size) {
    for (int i = 0; i < FONT_SLOTS; i++)
        if (fonts[i].font && fonts[i].size == size) return fonts[i].font;
    size_t bytes = 0;
    const void *data = dd_host_font(&bytes);
    TTF_Font *font = data ? TTF_OpenFontRW(SDL_RWFromConstMem(data, (int) bytes), 1, size)
                          : TTF_OpenFont("/usr/share/fonts/opentype/inter/Inter-Regular.otf", size);
    if (!font) return NULL;
    static int next;
    int slot = next++ % FONT_SLOTS;
    if (fonts[slot].font) TTF_CloseFont(fonts[slot].font);
    fonts[slot].size = size;
    fonts[slot].font = font;
    return font;
}

#define TEXT_SLOTS 96
static struct { char *text; int size, wrap; uint32_t rgb; SDL_Texture *texture; int w, h; } texts[TEXT_SLOTS];
static int text_count;

static void texts_clear(void) {
    for (int i = 0; i < text_count; i++) {
        free(texts[i].text);
        SDL_DestroyTexture(texts[i].texture);
    }
    text_count = 0;
}

// Draws text with its top left corner at (x, y) and returns its height in layout units.
// align: 0 left, 1 centred on x, 2 right aligned to x.
static float text(const char *string, float x, float y, float size, uint32_t rgb, float wrap, int align) {
    int font_size = px(size), wrap_px = px(wrap);
    int found = -1;
    for (int i = 0; i < text_count && found < 0; i++)
        if (texts[i].size == font_size && texts[i].wrap == wrap_px && texts[i].rgb == rgb && !strcmp(texts[i].text, string)) found = i;
    if (found < 0) {
        TTF_Font *font = font_for(font_size);
        if (!font || !*string) return 0;
        SDL_Color color = { rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff, 0xff };
        SDL_Surface *surface = wrap_px ? TTF_RenderUTF8_Blended_Wrapped(font, string, color, wrap_px)
                                       : TTF_RenderUTF8_Blended(font, string, color);
        if (!surface) return 0;
        if (text_count == TEXT_SLOTS) texts_clear();
        found = text_count++;
        texts[found].text = strdup(string);
        texts[found].size = font_size;
        texts[found].wrap = wrap_px;
        texts[found].rgb = rgb;
        texts[found].texture = SDL_CreateTextureFromSurface(renderer, surface);
        texts[found].w = surface->w;
        texts[found].h = surface->h;
        SDL_FreeSurface(surface);
    }
    SDL_Rect dst = { px(x), px(y), texts[found].w, texts[found].h };
    if (align == 1) dst.x -= dst.w / 2;
    if (align == 2) dst.x -= dst.w;
    SDL_RenderCopy(renderer, texts[found].texture, NULL, &dst);
    return texts[found].h / scale;
}

static uint32_t state_color(DdState state) {
    return state == DD_PASS ? C_GOOD : state == DD_FAIL ? C_ERROR : C_MUTED;
}

#define RAIL_W 232
#define CONTENT_X (RAIL_W + 48)
#define CONTENT_W (1280 - CONTENT_X - 48)

static void draw_rail(int page, bool focused) {
    fill(0, 0, RAIL_W, 720, 0, C_SURFACE);
    fill(RAIL_W, 0, 1, 720, 0, C_LINE);
    if (logo) {
        SDL_Rect dst = { px(24), px(28), px(40), px(40) };
        SDL_RenderCopy(renderer, logo, NULL, &dst);
    }
    text("DroidDeck", 76, 34, 22, C_ON_BACKGROUND, 0, 0);
    for (int i = 0; i < PAGE_COUNT; i++) {
        float y = 108 + i * 56;
        bool selected = i == page;
        if (selected && focused) outline(16, y, RAIL_W - 32, 48, 10, 2, C_PRIMARY, C_SURFACE_VARIANT);
        else if (selected) fill(16, y, RAIL_W - 32, 48, 10, C_SURFACE_VARIANT);
        if (selected) fill(16, y + 12, 4, 24, 2, C_PRIMARY);
        text(RAIL[i], 40, y + 12, 19, selected ? C_ON_BACKGROUND : C_MUTED, 0, 0);
    }
    text(DD_VERSION " for Switch 2", 24, 676, 13, C_MUTED, 0, 0);
}

static void draw_header(const char *eyebrow, const char *title, const char *subtitle) {
    text(eyebrow, CONTENT_X, 44, 13, C_PRIMARY, 0, 0);
    text(title, CONTENT_X, 64, 36, C_ON_BACKGROUND, 0, 0);
    if (subtitle) text(subtitle, CONTENT_X, 116, 17, C_MUTED, CONTENT_W, 0);
}

static void draw_runtime_status(float y) {
    fill(CONTENT_X, y, CONTENT_W, 76, 14, C_SURFACE);
    fill(CONTENT_X + 24, y + 32, 12, 12, 6, host.runtime_can_start ? C_GOOD : C_ERROR);
    text("Linux runtime", CONTENT_X + 52, y + 14, 18, C_ON_BACKGROUND, 0, 0);
    text(host.runtime_can_start ? "Ready" : "Cannot run on Horizon yet. Open Setup for the device report.",
         CONTENT_X + 52, y + 42, 15, C_MUTED, 0, 0);
}

static void draw_steam(bool focused) {
    draw_header("STEAM", "Play Steam", "Steam Gaming Mode, your library and the Linux desktop.");
    if (focused) outline(CONTENT_X, 184, 260, 64, 14, 3, C_ON_BACKGROUND, C_PRIMARY);
    else fill(CONTENT_X, 184, 260, 64, 14, C_PRIMARY);
    text("Play Steam", CONTENT_X + 130, 203, 22, C_ON_PRIMARY, 0, 1);
    draw_runtime_status(288);
}

static void draw_needs_runtime(const char *eyebrow, const char *title) {
    draw_header(eyebrow, title, NULL);
    fill(CONTENT_X, 150, CONTENT_W, 180, 14, C_SURFACE);
    text("Install the Linux runtime from Setup first.", CONTENT_X + CONTENT_W / 2, 214, 20, C_ON_BACKGROUND, 0, 1);
    text("The Linux runtime cannot run on Horizon yet.", CONTENT_X + CONTENT_W / 2, 250, 15, C_MUTED, 0, 1);
}

static void draw_setup(void) {
    char subtitle[256];
    snprintf(subtitle, sizeof(subtitle), "System %s", host.firmware[0] ? host.firmware : "unknown");
    draw_header("SETUP", "Device report", subtitle);
    float y = 158;
    for (int i = 0; i < host.check_count; i++) {
        const DdCheck *check = &host.checks[i];
        float h = 78;
        fill(CONTENT_X, y, CONTENT_W, h, 12, C_SURFACE);
        fill(CONTENT_X + 22, y + 22, 12, 12, 6, state_color(check->state));
        text(check->title, CONTENT_X + 50, y + 12, 18, C_ON_BACKGROUND, 0, 0);
        text(check->state == DD_PASS ? "OK" : check->state == DD_FAIL ? "Blocked" : "Not checked",
             CONTENT_X + CONTENT_W - 22, y + 14, 14, state_color(check->state), 0, 2);
        text(check->detail, CONTENT_X + 50, y + 40, 14, C_MUTED, CONTENT_W - 180, 0);
        y += h + 8;
    }
}

static void draw_dialog(void) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xb0);
    SDL_RenderFillRect(renderer, NULL);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    float w = 640, h = 256, x = (1280 - w) / 2, y = (720 - h) / 2;
    outline(x, y, w, h, 18, 1, C_LINE2, C_SURFACE_VARIANT);
    text("Steam can't start on Switch 2 yet", x + 32, y + 28, 24, C_ON_BACKGROUND, 0, 0);
    text("DroidDeck runs Steam, Proton and FEX inside a Linux runtime. That runtime needs Linux system calls, "
         "which Horizon does not have. This build has the launcher and the device report only.",
         x + 32, y + 78, 16, C_MUTED, w - 64, 0);
    outline(x + w - 32 - 140, y + h - 32 - 48, 140, 48, 12, 2, C_ON_BACKGROUND, C_PRIMARY);
    text("OK", x + w - 32 - 70, y + h - 32 - 36, 18, C_ON_PRIMARY, 0, 1);
}

static void draw_hints(int page, bool in_content, bool dialog) {
    const char *hints = dialog ? "A  OK" : in_content ? "A  Select      B  Back      +  Exit"
                      : page == PAGE_STEAM ? "A  Open      L / R  Switch page      +  Exit" : "L / R  Switch page      +  Exit";
    text(hints, 1280 - 48, 680, 14, C_MUTED, 0, 2);
}

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;
    dd_host_init();
    dd_host_probe(&host);

    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) return 1;
    IMG_Init(IMG_INIT_PNG);
    SDL_Window *window = SDL_CreateWindow("DroidDeck", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, 0);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (!renderer) return 1;
    char path[256];
    logo = IMG_LoadTexture(renderer, dd_host_asset("logo.png", path, sizeof(path)));

    const char *shot = getenv("DD_SCREENSHOT");
    const char *start = getenv("DD_START");
    int page = start ? atoi(start) % PAGE_COUNT : PAGE_STEAM;
    bool in_content = getenv("DD_CONTENT") != NULL, dialog = getenv("DD_DIALOG") != NULL;
    int last_h = 0;

    while (dd_host_running()) {
        uint32_t keys = dd_host_keys_down();
        if (keys & DD_KEY_QUIT) break;
        if (dialog) {
            if (keys & (DD_KEY_A | DD_KEY_B)) dialog = false;
        } else {
            if (keys & DD_KEY_L) page = (page + PAGE_COUNT - 1) % PAGE_COUNT;
            if (keys & DD_KEY_R) page = (page + 1) % PAGE_COUNT;
            if (keys & (DD_KEY_L | DD_KEY_R)) in_content = false;
            if (!in_content) {
                if ((keys & DD_KEY_UP) && page > 0) page--;
                if ((keys & DD_KEY_DOWN) && page < PAGE_COUNT - 1) page++;
                if ((keys & (DD_KEY_A | DD_KEY_RIGHT)) && page == PAGE_STEAM) in_content = true;
            } else {
                if (keys & (DD_KEY_B | DD_KEY_LEFT)) in_content = false;
                else if ((keys & DD_KEY_A) && page == PAGE_STEAM && !host.runtime_can_start) dialog = true;
            }
        }

        int out_w, out_h;
        SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
        if (out_h != last_h) {
            texts_clear();
            last_h = out_h;
        }
        scale = out_h / 720.0f;

        set_color(C_BACKGROUND);
        SDL_RenderClear(renderer);
        draw_rail(page, !in_content && !dialog);
        switch (page) {
            case PAGE_STEAM: draw_steam(in_content && !dialog); break;
            case PAGE_LIBRARY: draw_needs_runtime("LIBRARY", "Library"); break;
            case PAGE_DESKTOP: draw_needs_runtime("DESKTOP", "Desktop"); break;
            case PAGE_STORE: draw_needs_runtime("STORE", "Store"); break;
            case PAGE_SETUP: draw_setup(); break;
        }
        draw_hints(page, in_content, dialog);
        if (dialog) draw_dialog();

        if (shot) {
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, out_w, out_h, 32, SDL_PIXELFORMAT_RGBA32);
            SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch);
            IMG_SavePNG(surface, shot);
            SDL_FreeSurface(surface);
            break;
        }
        SDL_RenderPresent(renderer);
    }

    texts_clear();
    for (int i = 0; i < FONT_SLOTS; i++)
        if (fonts[i].font) TTF_CloseFont(fonts[i].font);
    SDL_DestroyTexture(logo);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    dd_host_exit();
    return 0;
}
