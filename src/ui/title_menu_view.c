#define COBJMACROS
#include "ui/title_menu_view.h"
#include "engine/log.h"
#include <d3d9.h>
#include <math.h>
#include <string.h>
#include "title_font.inc"
#include "lobby_font.inc"

enum { GLEAM_FRAMES = 64, BASE_LAYERS = 3, FONT_WIDTH = 512, FONT_HEIGHT = 1024 };
typedef struct TitleVertex { float x, y, z, rhw; DWORD color; float u, v; } TitleVertex;
static IDirect3DTexture9 *layers[BASE_LAYERS + GLEAM_FRAMES], *font, *lobby_font;
static IDirect3DDevice9 *texture_device;
static IDirect3DStateBlock9 *pending_restore;
static unsigned prepared;
static BOOL interpolate_gleam;
static UINT target_width, target_height;
static unsigned pointer_state;

/* Shared by pointer presentation and control hit testing. Both use client
 * pixels, including when Wine/Windows scales the window on another monitor. */
static BOOL owned_client_point(HWND window, POINT point, RECT *client) {
    DWORD pid=0;
    if (!window || !client || GetForegroundWindow()!=window ||
        !GetWindowThreadProcessId(window,&pid) || pid!=GetCurrentProcessId() ||
        !GetClientRect(window,client) || client->right<=0 || client->bottom<=0) return FALSE;
    return point.x>=0 && point.y>=0 && point.x<client->right && point.y<client->bottom;
}

BOOL SudekiMpTitleViewRestore(void) {
    if (!pending_restore) return TRUE;
    if (FAILED(IDirect3DStateBlock9_Apply(pending_restore))) return FALSE;
    IDirect3DStateBlock9_Release(pending_restore);
    pending_restore = NULL;
    return TRUE;
}

BOOL SudekiMpTitleViewRelease(void) {
    if (!SudekiMpTitleViewRestore()) return FALSE;
    for (unsigned i = 0; i < BASE_LAYERS + GLEAM_FRAMES; ++i) {
        if (layers[i]) IDirect3DTexture9_Release(layers[i]);
        layers[i] = NULL;
    }
    if (font) IDirect3DTexture9_Release(font);
    font = NULL;
    if (lobby_font) IDirect3DTexture9_Release(lobby_font);
    lobby_font = NULL;
    texture_device = NULL;
    target_width = target_height = prepared = 0;
    pointer_state = 0;
    return TRUE;
}

static BOOL prepare_font(IDirect3DDevice9 *device, IDirect3DTexture9 **target,
    const uint32_t *runs, unsigned run_count) {
    IDirect3DTexture9 *texture = NULL;
    D3DLOCKED_RECT lock;
    unsigned cursor = 0;
    BOOL valid = TRUE;
    if (FAILED(IDirect3DDevice9_CreateTexture(device, FONT_WIDTH, FONT_HEIGHT, 1,
            0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL))) return FALSE;
    if (FAILED(IDirect3DTexture9_LockRect(texture, 0, &lock, NULL, 0))) goto fail;
    if (!lock.pBits || lock.Pitch < FONT_WIDTH * 4) valid = FALSE;
    for (unsigned i = 0; valid && i < run_count; ++i) {
        uint32_t run = runs[i], count = run >> 16, gray = run & 255u;
        uint32_t pixel = ((run >> 8) & 255u) << 24 | gray * 0x010101u;
        if (!count || count > FONT_WIDTH * FONT_HEIGHT - cursor) { valid = FALSE; break; }
        while (count--) {
            uint32_t *row = (uint32_t *)((char *)lock.pBits + (cursor / FONT_WIDTH) * lock.Pitch);
            row[cursor++ % FONT_WIDTH] = pixel;
        }
    }
    if (FAILED(IDirect3DTexture9_UnlockRect(texture, 0)) || !valid ||
        cursor != FONT_WIDTH * FONT_HEIGHT) goto fail;
    *target = texture;
    return TRUE;
fail:
    IDirect3DTexture9_Release(texture);
    return FALSE;
}

static BOOL prepare_layer(IDirect3DDevice9 *device) {
    D3DLOCKED_RECT lock;
    IDirect3DTexture9 *texture = NULL;
    BOOL gleam = prepared >= BASE_LAYERS;
    unsigned width = gleam ? 320 : 640, height = gleam ? 40 : 80;
    float scale = gleam ? .5f : 1.f;
    BOOL valid;
    if (FAILED(IDirect3DDevice9_CreateTexture(device, width, height, 1, 0,
            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL))) return FALSE;
    if (FAILED(IDirect3DTexture9_LockRect(texture, 0, &lock, NULL, 0))) {
        IDirect3DTexture9_Release(texture); return FALSE;
    }
    valid = SudekiMpTitleButtonRasterPhase(lock.pBits, lock.Pitch, width, height,
        54 * scale, 20 * scale, 586 * scale, 60 * scale,
        gleam ? SUDEKIMP_TITLE_BUTTON_GLEAM : (SudekiMpTitleButtonLayer)prepared,
        gleam ? (float)(prepared - BASE_LAYERS) / GLEAM_FRAMES : 0);
    if (FAILED(IDirect3DTexture9_UnlockRect(texture, 0)) || !valid) {
        IDirect3DTexture9_Release(texture); return FALSE;
    }
    layers[prepared++] = texture;
    return TRUE;
}

static BOOL view_ready(IDirect3DDevice9 *device) {
    return device && device == texture_device && prepared >= BASE_LAYERS && font && lobby_font && !pending_restore;
}

BOOL SudekiMpTitleViewPrepare(void *raw) {
    IDirect3DDevice9 *device = raw;
    DWORD start = GetTickCount();
    if (!device || !SudekiMpTitleViewRestore()) return FALSE;
    if (texture_device != device) {
        D3DCAPS9 caps;
        if (!SudekiMpTitleViewRelease() || FAILED(IDirect3DDevice9_GetDeviceCaps(device, &caps)))
            return FALSE;
        if (caps.MaxTextureWidth < 640 || caps.MaxTextureHeight < FONT_HEIGHT) return FALSE;
        texture_device = device;
        interpolate_gleam = caps.MaxTextureBlendStages >= 3 && caps.MaxSimultaneousTextures >= 2 &&
            (caps.TextureOpCaps & D3DTEXOPCAPS_LERP);
    }
    /* Prepare the three button states and lettering together, normally during
     * the native intro. Even if the first callback is already a root frame,
     * finish this small required set before rendering any native rows. */
    while (prepared < BASE_LAYERS)
        if (!prepare_layer(device)) return FALSE;
    if (!font && !prepare_font(device, &font, title_font_runs,
        sizeof(title_font_runs)/sizeof(*title_font_runs))) return FALSE;
    if (!lobby_font && !prepare_font(device, &lobby_font, lobby_font_runs,
        sizeof(lobby_font_runs)/sizeof(*lobby_font_runs))) return FALSE;
    /* The 64-frame decorative cache must not hold the entire menu hostage.
     * Warm it once per render callback within a budget. Until complete, only
     * the gleam is omitted; the buttons, font, bounce and actions are ready.
     * Drawing never advances preparation or generates software noise. */
    while (prepared < BASE_LAYERS + GLEAM_FRAMES && GetTickCount() - start < 4u)
        if (!prepare_layer(device)) break;
    return view_ready(device);
}

static HRESULT quad_uv(IDirect3DDevice9 *device, float cx, float cy, float width,
    float height, DWORD upper, DWORD lower, float u0, float u1, float v0, float v1) {
    float left = cx - width * .5f - .5f, top = cy - height * .5f - .5f;
    TitleVertex vertices[4] = {
        {left, top, 0, 1, upper, u0, v0},
        {left + width, top, 0, 1, upper, u1, v0},
        {left, top + height, 0, 1, lower, u0, v1},
        {left + width, top + height, 0, 1, lower, u1, v1}
    };
    return IDirect3DDevice9_DrawPrimitiveUP(device, D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(*vertices));
}

static HRESULT quad(IDirect3DDevice9 *device, float cx, float cy, float width,
    float height, DWORD upper, DWORD lower, float v0, float v1) {
    return quad_uv(device,cx,cy,width,height,upper,lower,0,1,v0,v1);
}

/* One atlas submission per string, including the panel's small labels. */
static float text_width(const char *text, unsigned capacity) {
    float width=0;
    for (unsigned i=0;i<capacity;++i) {
        unsigned c=(unsigned char)text[i];
        if (!c) return width;
        if (c<32 || c>126) return -1;
        width+=lobby_glyph_advance[c-32];
    }
    return -1;
}
float SudekiMpTitleViewTextWidth(const char *text,unsigned capacity,float size) {
    if(!text || !capacity || !isfinite(size) || size<=0) return -1;
    float width=text_width(text,capacity);
    return width<0?-1:width*size/42.f;
}
static HRESULT draw_text(IDirect3DDevice9 *device, const char *text, unsigned capacity,
    float cx, float cy, float size, float max_width, DWORD upper, DWORD lower) {
    TitleVertex vertices[128*6];
    float width=text_width(text,capacity), factor=size/42.f;
    if (width<0 || capacity>128) return E_INVALIDARG;
    if (!width) return S_OK;
    if (width*factor>max_width) factor=max_width/width;
    float x=cx-width*factor*.5f;
    unsigned count=0;
    for (unsigned i=0;text[i];++i) {
        unsigned c=(unsigned char)text[i]-32;
        float u=(c%8)*64.f/FONT_WIDTH,v=(c/8)*64.f/FONT_HEIGHT;
        float l=x-3*factor-.5f,t=cy-28*factor-.5f,r=l+64*factor,b=t+64*factor;
        TitleVertex q[4]={{l,t,0,1,upper,u,v},{r,t,0,1,upper,u+64.f/FONT_WIDTH,v},
            {l,b,0,1,lower,u,v+64.f/FONT_HEIGHT},{r,b,0,1,lower,u+64.f/FONT_WIDTH,v+64.f/FONT_HEIGHT}};
        static const unsigned order[6]={0,1,2,2,1,3};
        for (unsigned j=0;j<6;++j) vertices[count++]=q[order[j]];
        x+=lobby_glyph_advance[c]*factor;
    }
    HRESULT result=IDirect3DDevice9_SetTexture(device,0,(IDirect3DBaseTexture9 *)lobby_font);
    if (FAILED(result)) return result;
    return IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,count/3,vertices,sizeof(*vertices));
}
static HRESULT text_left(IDirect3DDevice9 *device,const char *text,unsigned capacity,
    float x,float y,float size,float max_width,DWORD color) {
    float width=text_width(text,capacity);
    if (width<0) return E_INVALIDARG;
    width=fminf(width*size/42.f,max_width);
    return draw_text(device,text,capacity,x+width*.5f,y,size,max_width,color,color);
}

static DWORD tint(uint32_t rgb, float opacity) {
    unsigned alpha = (unsigned)(fminf(1, fmaxf(0, opacity)) * 255 + .5f);
    return alpha << 24 | rgb;
}

static HRESULT panel_rect(IDirect3DDevice9 *device,float x,float y,float w,float h,
    DWORD upper,DWORD lower) {
    HRESULT result=IDirect3DDevice9_SetTexture(device,0,NULL);
    if (FAILED(result)) return result;
    return quad(device,x+w*.5f,y+h*.5f,w,h,upper,lower,0,1);
}

static HRESULT draw_avatar_cards(IDirect3DDevice9 *device,const SudekiMpTitleExtras *extras,
    float scale,float hud_x,float hud_y,float opacity) {
    if(extras->avatar_card_count>SUDEKIMP_OVERLAY_AVATAR_CARDS) return E_INVALIDARG;
    for(unsigned i=0;i<extras->avatar_card_count;++i) {
        const SudekiMpOverlayAvatarCard *c=&extras->avatar_cards[i];
        if(!isfinite(c->right) || !isfinite(c->y) || !isfinite(c->width) || !isfinite(c->height) ||
            c->right<0 || c->right>960 || c->y<0 || c->y>720 ||
            c->width<160 || c->width>400 || c->height<76 || c->height>120 ||
            !isfinite(c->hp_fraction) || c->hp_fraction<0 || c->hp_fraction>1 ||
            !isfinite(c->sp_fraction) || c->sp_fraction<0 || c->sp_fraction>1) return E_INVALIDARG;
        float x=c->right*hud_x-c->width*scale,y=c->y*hud_y;
        DWORD edge=tint(c->local?0xc7ad73:0x425764,opacity*.9f);
        DWORD text=tint(0xf3e3bc,opacity),muted=tint(0xaabac2,opacity);
        HRESULT result;
#define CARD(call) do { result=(call); if(FAILED(result)) return result; } while(0)
#define CARD_BOX(a,b,w,h,upper,lower) panel_rect(device,x+(a)*scale,y+(b)*scale,(w)*scale,(h)*scale,upper,lower)
#define CARD_TEXT(value,px,py,size,width,color) text_left(device,value,sizeof(value),x+(px)*scale,y+(py)*scale,(size)*scale,(width)*scale,color)
        CARD(CARD_BOX(0,0,c->width,c->height,edge,edge));
        CARD(CARD_BOX(1,1,c->width-2,c->height-2,tint(0x152a38,opacity*.92f),tint(0x09131d,opacity*.94f)));
        CARD(CARD_BOX(8,11,56,56,tint(0x294657,opacity*.95f),tint(0x102536,opacity*.95f)));
        if(c->portrait) {
            CARD(IDirect3DDevice9_SetTexture(device,0,(IDirect3DBaseTexture9 *)c->portrait));
            CARD(quad(device,x+36*scale,y+39*scale,56*scale,56*scale,
                tint(0xffffff,opacity),tint(0xffffff,opacity),0,1));
        } else CARD(draw_text(device,c->avatar,sizeof(c->avatar),x+36*scale,y+39*scale,
            14*scale,50*scale,text,edge));
        CARD(CARD_TEXT(c->name,76,15,17,c->width-(c->local?118:84),text));
        if(c->local) {
            static const char you[]="YOU";
            CARD(CARD_TEXT(you,c->width-35,15,11,27,edge));
        }
        CARD(CARD_TEXT(c->hp,76,34,13,c->width-84,text));
        CARD(CARD_TEXT(c->sp,76,57,13,c->width-84,muted));
        CARD(CARD_BOX(76,43,c->width-84,4,tint(0x243a41,opacity),tint(0x243a41,opacity)));
        CARD(CARD_BOX(76,66,c->width-84,4,tint(0x233342,opacity),tint(0x233342,opacity)));
        if(c->hp_fraction>0) {
            DWORD health=tint(c->hp_fraction<=.25f?0xd1796e:0x71b899,opacity);
            CARD(CARD_BOX(76,43,(c->width-84)*c->hp_fraction,4,health,health));
        }
        if(c->sp_fraction>0) {
            DWORD spirit=tint(0x79a9d1,opacity);
            CARD(CARD_BOX(76,66,(c->width-84)*c->sp_fraction,4,spirit,spirit));
        }
#undef CARD_TEXT
#undef CARD_BOX
#undef CARD
    }
    return S_OK;
}

static HRESULT draw_pointer(IDirect3DDevice9 *device,HWND window,
    UINT width,UINT height,float scale,float opacity) {
    /* Retail's WM_SETFOCUS path calls ShowCursor(FALSE). Draw our pointer in
     * the menu's existing render-state lease; do not change that global
     * display count or take capture/focus away from another game window. */
    POINT point;
    RECT client;
    CURSORINFO cursor={.cbSize=sizeof(cursor)};
    unsigned state=0;
    HRESULT result=S_OK;
    if (GetCursorPos(&point) && ScreenToClient(window,&point) &&
        owned_client_point(window,point,&client)) {
        state=GetCursorInfo(&cursor) && (cursor.flags&CURSOR_SHOWING) && cursor.hCursor ? 1u : 2u;
        if (state==2) {
            /* Outer arrow/stem, then inset fill. Tip (0,0) is the exact hit
             * point. These triangles need no texture or borrowed game asset. */
            static const float shape[][2]={
                {0,0},{0,27},{22,18},
                {6,18},{12,15},{19,30}, {6,18},{19,30},{13,33},
                {2,5},{2,23},{17,18},
                {9,20},{11,19},{16,29}, {9,20},{16,29},{14,30}
            };
            TitleVertex vertices[18];
            float x=(float)point.x*width/client.right-.5f;
            float y=(float)point.y*height/client.bottom-.5f;
            for (unsigned i=0;i<18;++i)
                vertices[i]=(TitleVertex){x+shape[i][0]*scale,y+shape[i][1]*scale,0,1,
                    tint(i<9?0x07131e:0xffe6a5,opacity),0,0};
            result=IDirect3DDevice9_SetTexture(device,0,NULL);
            if (SUCCEEDED(result))
                result=IDirect3DDevice9_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,6,vertices,sizeof(*vertices));
        }
    }
    if (SUCCEEDED(result) && state!=pointer_state) {
        SudekiMpLogFormat("title_multiplayer event=pointer state=%s\r\n",
            state==2?"drawn":state==1?"system_visible":"outside_or_unfocused");
        pointer_state=state;
    }
    return result;
}
static HRESULT draw_panel(IDirect3DDevice9 *device,const SudekiMpTitleExtras *p,
    unsigned count,unsigned selected,unsigned enabled,float scale,float left,float top,
    UINT width,UINT height,float opacity) {
    HRESULT result;
    DWORD gold=tint(0xc7ad73,opacity), pale=tint(0xf3e3bc,opacity), muted=tint(0xaabac2,opacity);
#define PANEL(call) do { result=(call); if (FAILED(result)) return result; } while (0)
#define BOX(x,y,w,h,a,b) panel_rect(device,left+(x)*scale,top+(y)*scale,(w)*scale,(h)*scale,a,b)
#define PANEL_TEXT(s,cap,x,y,size,w,color) text_left(device,s,cap,left+(x)*scale,top+(y)*scale,(size)*scale,(w)*scale,color)
    PANEL(panel_rect(device,0,0,(float)width,(float)height,tint(0x030912,opacity*.95f),tint(0x02060c,opacity*.98f)));
    PANEL(BOX(44,52,872,616,tint(0x152d3d,opacity),tint(0x080f1c,opacity)));
    PANEL(BOX(44,52,872,2,gold,gold)); PANEL(BOX(44,666,872,2,gold,gold));
    PANEL(BOX(44,52,2,616,gold,gold)); PANEL(BOX(914,52,2,616,gold,gold));
    PANEL(BOX(54,62,852,2,tint(0x366575,opacity),tint(0x366575,opacity)));
    PANEL(BOX(64,160,832,1,gold,gold));
    if (!p->full_width)
        PANEL(BOX(245,180,1,425,tint(0x50616c,opacity),tint(0x50616c,opacity)));
    if (p->save_details)
        PANEL(BOX(482,192,1,343,tint(0x50616c,opacity),tint(0x50616c,opacity)));
    PANEL(draw_text(device,p->heading,sizeof(p->heading),left+480*scale,top+103*scale,34*scale,790*scale,pale,gold));
    PANEL(draw_text(device,p->hint,sizeof(p->hint),left+480*scale,top+138*scale,18*scale,790*scale,muted,muted));
    for (unsigned i=0;i<count;++i) {
        const SudekiMpPanelControl *c=&p->controls[i];
        BOOL enabled_row=(enabled&(1u<<i))!=0,focus=enabled_row && i==selected;
        DWORD edge=tint(focus?0x80cfda:c->active?0xc7ad73:0x354856,opacity);
        DWORD ink=enabled_row?pale:muted;
        if (c->kind==SUDEKIMP_PANEL_FIELD) {
            PANEL(PANEL_TEXT(c->label,sizeof(c->label),c->x,c->y-13,17,c->width,muted));
        }
        PANEL(BOX(c->x,c->y,c->width,c->height,edge,edge));
        PANEL(BOX(c->x+1,c->y+1,c->width-2,c->height-2,
            tint(focus?0x24495a:c->active?0x203748:0x111f2d,opacity),tint(0x0a1522,opacity)));
        if (c->active) PANEL(BOX(c->x+1,c->y+1,3,c->height-2,gold,gold));
        if (c->kind==SUDEKIMP_PANEL_SERVER) {
            PANEL(PANEL_TEXT(c->label,sizeof(c->label),c->x+12,c->y+c->height*.5f,18,267,ink));
            PANEL(PANEL_TEXT(c->value,sizeof(c->value),c->x+291,c->y+c->height*.5f,16,174,muted));
            PANEL(PANEL_TEXT(c->detail,sizeof(c->detail),c->x+490,c->y+c->height*.5f,17,61,ink));
        } else if (c->kind==SUDEKIMP_PANEL_SAVE) {
            PANEL(PANEL_TEXT(c->label,sizeof(c->label),c->x+14,c->y+15,19,c->width-28,ink));
            PANEL(PANEL_TEXT(c->detail,sizeof(c->detail),c->x+14,c->y+34,14,c->width-28,muted));
        } else if (c->kind==SUDEKIMP_PANEL_PORTRAIT) {
            if (c->texture) {
                PANEL(IDirect3DDevice9_SetTexture(device,0,(IDirect3DBaseTexture9 *)c->texture));
                PANEL(quad(device,left+(c->x+c->width*.5f)*scale,
                    top+(c->y+81)*scale,(c->width-22)*scale,140*scale,
                    tint(0xffffff,opacity),tint(0xffffff,opacity),0,1));
            } else {
                PANEL(BOX(c->x+11,c->y+13,c->width-22,135,
                    tint(c->active?0x385467:0x203a4f,opacity),tint(0x102334,opacity)));
                PANEL(draw_text(device,c->label,sizeof(c->label),left+(c->x+c->width*.5f)*scale,
                    top+(c->y+80)*scale,28*scale,(c->width-30)*scale,ink,gold));
            }
            PANEL(draw_text(device,c->label,sizeof(c->label),left+(c->x+c->width*.5f)*scale,
                top+(c->y+169)*scale,22*scale,(c->width-22)*scale,ink,ink));
            PANEL(draw_text(device,c->detail,sizeof(c->detail),left+(c->x+c->width*.5f)*scale,
                top+(c->y+193)*scale,15*scale,(c->width-22)*scale,muted,muted));
        } else if (c->kind==SUDEKIMP_PANEL_CHARACTER) {
            PANEL(PANEL_TEXT(c->label,sizeof(c->label),c->x+14,c->y+18,18,c->width-28,ink));
            PANEL(PANEL_TEXT(c->detail,sizeof(c->detail),c->x+14,c->y+44,21,c->width-28,pale));
        } else if (c->kind==SUDEKIMP_PANEL_MEMBER) {
            PANEL(PANEL_TEXT(c->label,sizeof(c->label),c->x+14,c->y+19,21,c->width-28,ink));
            PANEL(PANEL_TEXT(c->detail,sizeof(c->detail),c->x+14,c->y+43,16,c->width-28,muted));
        } else if (c->kind==SUDEKIMP_PANEL_FIELD) {
            PANEL(PANEL_TEXT(c->value,sizeof(c->value),c->x+12,c->y+c->height*.5f,23,c->width-24,ink));
        } else {
            PANEL(draw_text(device,c->label,sizeof(c->label),left+(c->x+c->width*.5f)*scale,
                top+(c->y+c->height*.5f)*scale,21*scale,(c->width-18)*scale,ink,ink));
        }
    }
    if (p->text_count>SUDEKIMP_PANEL_TEXTS) return E_INVALIDARG;
    for (unsigned i=0;i<p->text_count;++i) {
        const SudekiMpPanelText *t=&p->texts[i];
        PANEL(PANEL_TEXT(t->text,sizeof(t->text),t->x,t->y,t->size,t->width,muted));
    }
    PANEL(BOX(64,619,832,1,gold,gold));
    PANEL(draw_text(device,p->status,sizeof(p->status),left+480*scale,top+643*scale,18*scale,810*scale,pale,pale));
#undef PANEL_TEXT
#undef BOX
#undef PANEL
    return S_OK;
}

BOOL SudekiMpTitleViewDraw(void *raw, unsigned count, unsigned selected,
    unsigned enabled_mask, const SudekiMpTitleLabel *labels,
    SudekiMpTitleButtonState state, double seconds, float opacity, HWND *window,
    const SudekiMpTitleExtras *extras) {
    IDirect3DDevice9 *device = raw;
    IDirect3DSurface9 *surface = NULL;
    D3DSURFACE_DESC desc;
    D3DDEVICE_CREATION_PARAMETERS creation;
    D3DVIEWPORT9 viewport;
    HRESULT result;
    float scale, left, top;
    if (!device || !window || (!labels && !(extras && (extras->panel || extras->overlay))) || !count ||
        count > (extras && extras->panel ? SUDEKIMP_PANEL_CONTROLS : SUDEKIMP_TITLE_MAX_ROWS) ||
        selected >= count || !isfinite(seconds) || seconds < 0 || !isfinite(opacity) ||
        opacity < 0 || opacity > 1 || !view_ready(device)) return FALSE;
    for (unsigned row = 0; !(extras && (extras->panel || extras->overlay)) && row < count; ++row)
        if ((unsigned)labels[row] >= SUDEKIMP_TITLE_LABEL_COUNT) return FALSE;
    if (FAILED(IDirect3DDevice9_GetCreationParameters(device, &creation)) || !creation.hFocusWindow ||
        FAILED(IDirect3DDevice9_GetRenderTarget(device, 0, &surface))) return FALSE;
    result = IDirect3DSurface9_GetDesc(surface, &desc);
    IDirect3DSurface9_Release(surface);
    if (FAILED(result) || !desc.Width || !desc.Height) return FALSE;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(device, D3DSBT_ALL, &pending_restore))) return FALSE;
    if (FAILED(IDirect3DStateBlock9_Capture(pending_restore))) {
        IDirect3DStateBlock9_Release(pending_restore); pending_restore = NULL; return FALSE;
    }
    scale = fminf((float)desc.Width / SUDEKIMP_TITLE_CANVAS_WIDTH,
        (float)desc.Height / SUDEKIMP_TITLE_CANVAS_HEIGHT);
    left = ((float)desc.Width - SUDEKIMP_TITLE_CANVAS_WIDTH * scale) * .5f;
    top = ((float)desc.Height - SUDEKIMP_TITLE_CANVAS_HEIGHT * scale) * .5f;
#define DRAW_CALL(call) do { result = (call); if (FAILED(result)) goto restore; } while (0)
    viewport = (D3DVIEWPORT9){0, 0, desc.Width, desc.Height, 0, 1};
    DRAW_CALL(IDirect3DDevice9_SetViewport(device, &viewport));
    DRAW_CALL(IDirect3DDevice9_SetVertexShader(device, NULL));
    DRAW_CALL(IDirect3DDevice9_SetPixelShader(device, NULL));
    DRAW_CALL(IDirect3DDevice9_SetFVF(device,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_ZENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHATESTENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_CULLMODE, D3DCULL_NONE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_FOGENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_LIGHTING, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_STENCILENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_FILLMODE, D3DFILL_SOLID));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_SHADEMODE, D3DSHADE_GOURAUD));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_SRGBWRITEENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_SCISSORTESTENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_COLORWRITEENABLE, 15));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE));
    DRAW_CALL(IDirect3DDevice9_SetTexture(device, 0, NULL));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_BLENDOP, D3DBLENDOP_ADD));
    DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_SEPARATEALPHABLENDENABLE, FALSE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE));
    DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE));
    for (unsigned stage = 0; stage < 2; ++stage) {
        DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, stage, D3DTSS_TEXCOORDINDEX, 0));
        DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_SRGBTEXTURE, FALSE));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
        DRAW_CALL(IDirect3DDevice9_SetSamplerState(device, stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
    }
    if (extras && extras->overlay) {
        if(extras->text_count>SUDEKIMP_PANEL_TEXTS ||
            !isfinite(extras->overlay_letterbox) || extras->overlay_letterbox<0 ||
            extras->overlay_letterbox>.4f || !isfinite(extras->overlay_letterbox_bottom) ||
            extras->overlay_letterbox_bottom<0 || extras->overlay_letterbox_bottom>.4f)
            { result=E_INVALIDARG; goto restore; }
        if(extras->overlay_letterbox>0 || extras->overlay_letterbox_bottom>0) {
            float height=extras->overlay_letterbox*desc.Height;
            float bottom=extras->overlay_letterbox_bottom*desc.Height;
            DRAW_CALL(IDirect3DDevice9_SetTexture(device,0,NULL));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG2));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2));
            if(height>0) DRAW_CALL(quad(device,desc.Width*.5f,height*.5f,desc.Width,height,
                tint(0,opacity),tint(0,opacity),0,1));
            if(bottom>0) DRAW_CALL(quad(device,desc.Width*.5f,desc.Height-bottom*.5f,desc.Width,bottom,
                tint(0,opacity),tint(0,opacity),0,1));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE));
        }
        /* Gameplay HUD placement follows the full viewport, not the title
         * panel's centered 4:3 canvas. Keep lettering uniformly scaled. */
        float hud_x=(float)desc.Width/SUDEKIMP_TITLE_CANVAS_WIDTH;
        float hud_y=(float)desc.Height/SUDEKIMP_TITLE_CANVAS_HEIGHT;
        DRAW_CALL(draw_avatar_cards(device,extras,scale,hud_x,hud_y,opacity));
        for(unsigned i=0;i<extras->text_count;++i) {
            const SudekiMpPanelText *t=&extras->texts[i];
            float x=t->x*hud_x,y=t->y*hud_y,width=t->width*hud_x;
            DRAW_CALL(text_left(device,t->text,sizeof(t->text),x+scale,y+scale,
                t->size*scale,width,tint(0x07121d,opacity*.8f)));
            DRAW_CALL(text_left(device,t->text,sizeof(t->text),x,y,
                t->size*scale,width,tint(0xd7d0b8,opacity*.9f)));
        }
        goto restore;
    }
    if (extras && extras->panel) {
        DRAW_CALL(draw_panel(device,extras,count,selected,enabled_mask,scale,left,top,
            desc.Width,desc.Height,opacity));
        DRAW_CALL(draw_pointer(device,creation.hFocusWindow,desc.Width,desc.Height,scale,opacity));
        goto restore;
    }
    for (unsigned row = 0; row < count; ++row) {
        SudekiMpTitleButtonPose pose;
        BOOL focused = row == selected && (enabled_mask & (1u << row));
        SudekiMpTitleButtonState row_state = state == SUDEKIMP_TITLE_BUTTON_RETURN || focused ?
            state : SUDEKIMP_TITLE_BUTTON_REST;
        double elapsed = state == SUDEKIMP_TITLE_BUTTON_RETURN ? fmax(0, seconds - row * .035) : seconds;
        if (row_state == SUDEKIMP_TITLE_BUTTON_RETURN && elapsed >= .8) {
            row_state = focused ? SUDEKIMP_TITLE_BUTTON_FOCUS : SUDEKIMP_TITLE_BUTTON_REST;
            elapsed -= .8;
        }
        if (!SudekiMpTitleButtonSample(row_state, elapsed, &pose)) { result = E_INVALIDARG; goto restore; }
        float cx = left + 480 * scale;
        float cy = top + (SUDEKIMP_TITLE_FIRST_ROW + row * SUDEKIMP_TITLE_ROW_PITCH +
            SUDEKIMP_TITLE_ROW_HEIGHT * (.5f + pose.offset_y)) * scale;
        float art = (SUDEKIMP_TITLE_RIGHT - SUDEKIMP_TITLE_LEFT) / 532.f * scale;
        float alpha = opacity * pose.opacity;
        DWORD white = tint(0xffffffu, alpha);
        unsigned layer = focused ? (pose.confirmation > .01f ? 2 : 1) : 0;
        DRAW_CALL(IDirect3DDevice9_SetTexture(device, 0, (IDirect3DBaseTexture9 *)layers[layer]));
        DRAW_CALL(quad(device, cx, cy, 640 * art * pose.scale_x, 80 * art * pose.scale_y,
            white, white, 0, 1));
        if (focused && pose.gleam_opacity > 0 && prepared == BASE_LAYERS + GLEAM_FRAMES) {
            float phase = pose.gleam_phase * GLEAM_FRAMES;
            unsigned frame = (unsigned)phase % GLEAM_FRAMES;
            DWORD sheen = tint(0xffffffu, alpha * pose.gleam_opacity);
            DRAW_CALL(IDirect3DDevice9_SetTexture(device, 0,
                (IDirect3DBaseTexture9 *)layers[BASE_LAYERS + frame]));
            if (interpolate_gleam) {
                unsigned weight = (unsigned)((phase - floorf(phase)) * 255 + .5f);
                DRAW_CALL(IDirect3DDevice9_SetRenderState(device, D3DRS_TEXTUREFACTOR, weight * 0x01010101u));
                DRAW_CALL(IDirect3DDevice9_SetTexture(device, 1,
                    (IDirect3DBaseTexture9 *)layers[BASE_LAYERS + (frame + 1) % GLEAM_FRAMES]));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_LERP));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAOP, D3DTOP_LERP));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLORARG0, D3DTA_TFACTOR));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAARG0, D3DTA_TFACTOR));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLORARG1, D3DTA_TEXTURE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLORARG2, D3DTA_CURRENT));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_COLOROP, D3DTOP_MODULATE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_ALPHAOP, D3DTOP_MODULATE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_COLORARG1, D3DTA_CURRENT));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_ALPHAARG1, D3DTA_CURRENT));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_COLORARG2, D3DTA_DIFFUSE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 2, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE));
                DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 3, D3DTSS_COLOROP, D3DTOP_DISABLE));
            }
            DRAW_CALL(quad(device, cx, cy, 640 * art * pose.gleam_scale, 80 * art * pose.gleam_scale,
                sheen, sheen, 0, 1));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_COLOROP, D3DTOP_DISABLE));
            DRAW_CALL(IDirect3DDevice9_SetTextureStageState(device, 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE));
            DRAW_CALL(IDirect3DDevice9_SetTexture(device, 1, NULL));
        }
        DWORD upper = tint(focused ? 0xffffffu : 0xebd29au, alpha);
        DWORD lower = tint(focused ? 0xf4f5efu : 0xb7a075u, alpha);
        if (!(enabled_mask & (1u << row)) && !(extras && (extras->informational_rows & (1u << row))))
            upper = lower = tint(0xb2ab9bu, alpha * .9f);
        float label_scale = scale * .70f * pose.label_scale;
        if (extras && extras->rows[row][0]) {
            DRAW_CALL(draw_text(device, extras->rows[row], sizeof(extras->rows[row]),
                cx, cy, 33*scale*pose.label_scale, 475*scale, upper, lower));
        } else {
            DRAW_CALL(IDirect3DDevice9_SetTexture(device, 0, (IDirect3DBaseTexture9 *)font));
            DRAW_CALL(quad(device, cx, cy, 512 * label_scale, 80 * label_scale,
                upper, lower, labels[row] * 80.f / FONT_HEIGHT, (labels[row] + 1) * 80.f / FONT_HEIGHT));
        }
    }
    if (extras) {
        DWORD color=tint(0xf3e1b7u,opacity);
        DRAW_CALL(draw_text(device,extras->heading,sizeof(extras->heading),left+480*scale,
            top+591*scale,27*scale,810*scale,color,color));
        DRAW_CALL(draw_text(device,extras->hint,sizeof(extras->hint),left+480*scale,
            top+626*scale,22*scale,810*scale,color,color));
        DRAW_CALL(draw_text(device,extras->status,sizeof(extras->status),left+480*scale,
            top+660*scale,22*scale,810*scale,color,color));
    }
    DRAW_CALL(draw_pointer(device,creation.hFocusWindow,desc.Width,desc.Height,scale,opacity));
restore:
#undef DRAW_CALL
    if (!SudekiMpTitleViewRestore()) {
        SudekiMpLogWrite("title_multiplayer event=render_restore_pending\r\n"); return FALSE;
    }
    if (FAILED(result)) return FALSE;
    target_width = desc.Width; target_height = desc.Height;
    *window = creation.hFocusWindow;
    return TRUE;
}

static BOOL client_canvas_point(HWND window, POINT cursor, float *out_x, float *out_y) {
    RECT client;
    float x, y, scale;
    if (!target_width || !target_height || !owned_client_point(window,cursor,&client)) return FALSE;
    scale = fminf((float)target_width / SUDEKIMP_TITLE_CANVAS_WIDTH,
        (float)target_height / SUDEKIMP_TITLE_CANVAS_HEIGHT);
    x = ((float)cursor.x * target_width / client.right - (target_width - 960 * scale) * .5f) / scale;
    y = ((float)cursor.y * target_height / client.bottom - (target_height - 720 * scale) * .5f) / scale;
    *out_x=x; *out_y=y; return TRUE;
}
static BOOL canvas_point(HWND window,POINT *point,float *out_x,float *out_y) {
    POINT cursor;
    if (!point || !GetCursorPos(&cursor) || !ScreenToClient(window,&cursor)) return FALSE;
    *point=cursor;
    return client_canvas_point(window,cursor,out_x,out_y);
}

BOOL SudekiMpTitleViewHit(HWND window, unsigned count, unsigned *row, POINT *point) {
    float x,y;
    if (!row || !count || count>SUDEKIMP_TITLE_MAX_ROWS || !canvas_point(window,point,&x,&y)) return FALSE;
    if (x < SUDEKIMP_TITLE_LEFT || x >= SUDEKIMP_TITLE_RIGHT || y < SUDEKIMP_TITLE_FIRST_ROW) return FALSE;
    *row = (unsigned)((y - SUDEKIMP_TITLE_FIRST_ROW) / SUDEKIMP_TITLE_ROW_PITCH);
    if (*row >= count) return FALSE;
    /* Reject the rounded transparent ends as well as the gaps between rows. */
    y -= SUDEKIMP_TITLE_FIRST_ROW + *row * SUDEKIMP_TITLE_ROW_PITCH;
    if (y >= SUDEKIMP_TITLE_ROW_HEIGHT) return FALSE;
    float radius = SUDEKIMP_TITLE_ROW_HEIGHT * .5f;
    float dx = fmaxf(fabsf(x - 480) - ((SUDEKIMP_TITLE_RIGHT - SUDEKIMP_TITLE_LEFT) * .5f - radius), 0);
    return dx * dx + (y - radius) * (y - radius) <= radius * radius;
}

BOOL SudekiMpTitlePanelHit(HWND window,unsigned count,const SudekiMpTitleExtras *panel,
    unsigned *row,POINT *point) {
    if (!point || !GetCursorPos(point) || !ScreenToClient(window,point)) return FALSE;
    return SudekiMpTitlePanelHitPoint(window,count,panel,*point,row);
}
BOOL SudekiMpTitlePanelHitPoint(HWND window,unsigned count,const SudekiMpTitleExtras *panel,
    POINT point,unsigned *row) {
    float x,y;
    if (!panel || !panel->panel || !row || !count || count>SUDEKIMP_PANEL_CONTROLS ||
        !client_canvas_point(window,point,&x,&y)) return FALSE;
    for (unsigned i=0;i<count;++i) {
        const SudekiMpPanelControl *c=&panel->controls[i];
        if (x>=c->x && x<c->x+c->width && y>=c->y && y<c->y+c->height) { *row=i; return TRUE; }
    }
    return FALSE;
}

BOOL SudekiMpTitleViewFade(void *raw,float opacity) {
    IDirect3DDevice9 *device=raw;
    if (!view_ready(device) || !isfinite(opacity) || opacity<0 || opacity>1) return FALSE;
    if (!opacity) return TRUE;
    if (FAILED(IDirect3DDevice9_CreateStateBlock(device,D3DSBT_ALL,&pending_restore))) return FALSE;
    if (FAILED(IDirect3DStateBlock9_Capture(pending_restore))) {
        IDirect3DStateBlock9_Release(pending_restore); pending_restore=NULL; return FALSE;
    }
    HRESULT result=S_OK;
#define FADE(call) do { result=(call); if (FAILED(result)) goto restore_fade; } while (0)
    FADE(IDirect3DDevice9_SetVertexShader(device,NULL));
    FADE(IDirect3DDevice9_SetPixelShader(device,NULL));
    FADE(IDirect3DDevice9_SetFVF(device,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));
    D3DVIEWPORT9 viewport={0,0,target_width,target_height,0,1};
    FADE(IDirect3DDevice9_SetViewport(device,&viewport));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_ZENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHATESTENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_STENCILENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_FOGENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_LIGHTING,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_FILLMODE,D3DFILL_SOLID));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_SCISSORTESTENABLE,FALSE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_COLORWRITEENABLE,15));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_BLENDOP,D3DBLENDOP_ADD));
    FADE(IDirect3DDevice9_SetRenderState(device,D3DRS_SEPARATEALPHABLENDENABLE,FALSE));
    FADE(IDirect3DDevice9_SetTexture(device,0,NULL));
    FADE(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));
    FADE(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
    FADE(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));
    FADE(IDirect3DDevice9_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));
    FADE(IDirect3DDevice9_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    FADE(quad(device,target_width*.5f,target_height*.5f,(float)target_width,(float)target_height,
        tint(0x030812,opacity),tint(0x030812,opacity),0,1));
restore_fade:
#undef FADE
    if (!SudekiMpTitleViewRestore()) return FALSE;
    return SUCCEEDED(result);
}
