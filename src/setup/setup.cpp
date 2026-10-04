// SZNT Setup - installs, updates and removes SZNT Drive and SZNT View (ETS2 / ATS).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include "installer.h"
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <map>
#include <cmath>

Lang g_lang = LANG_EN;
NoteFn g_note = nullptr;

using namespace inst;

// ------------------------------------------------------------------ state
enum Page { P_LANG, P_MODS, P_GAMES, P_PROGRESS, P_DONE, P_UNINSTALL };
enum Id {
    ID_NONE, ID_CLOSE, ID_MIN, ID_NEXT, ID_BACK, ID_INSTALL, ID_UNINSTALL, ID_FINISH, ID_LAUNCH, ID_ADMIN,
    ID_TOGGLE_DRIVE, ID_TOGGLE_VIEW, ID_ADD_FOLDER, ID_UPDATE, ID_LINK_HOME, ID_LINK_REPO, ID_LINK_OTHER, ID_CONTINUE,
    ID_LANG0 = 100, ID_GAME0 = 200,
};

struct Note { NoteKind kind; std::wstring text; };

static HWND g_wnd;
static Page g_page = P_LANG;
static float g_page_t = 1;
static bool g_uninstall_mode = false;
static bool g_want_drive = HAS_DRIVE, g_want_view = HAS_VIEW;
static std::vector<Game> g_games;
static std::vector<GameStatus> g_status;
static std::vector<Note> g_notes;
static bool g_busy = false, g_finished = false;
static Result g_result;
static std::wstring g_update;
static float g_check_t = 0;

static int g_dpi = 96;
static const float W = 900, H = 600, LEFT = 280;

#define WM_APP_NOTE   (WM_APP + 1)
#define WM_APP_DONE   (WM_APP + 2)
#define WM_APP_UPDATE (WM_APP + 3)

// ------------------------------------------------------------------ Direct2D
static ID2D1Factory *g_d2d;
static IDWriteFactory *g_dw;
static ID2D1HwndRenderTarget *g_rt;
static ID2D1SolidColorBrush *g_brush;
static ID2D1StrokeStyle *g_round;
static IDWriteTextFormat *f_display, *f_title, *f_h2, *f_body, *f_bodyb, *f_small, *f_tiny, *f_path, *f_key, *f_name;

static D2D1_COLOR_F rgb(unsigned c, float a = 1.0f)
{
    return D2D1::ColorF(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, a);
}
static D2D1_COLOR_F mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t)
{
    return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

namespace col {
static const unsigned BG0 = 0x0C0E12, BG1 = 0x12151B, CARD = 0x191D24, CARD_HI = 0x1F242C, BORDER = 0x272C35,
                      TEXT = 0xEDEFF3, MUTED = 0x8D94A0, FAINT = 0x596070, ACCENT = 0xF2A900, ACCENT_HI = 0xFFBE2E,
                      INK = 0x14110A, OK = 0x3DDC84, WARN = 0xF2A900, ERR = 0xFF5D5D, SOFT = 0xC5CAD3;
}

static float g_opacity = 1;
static ID2D1SolidColorBrush *brush(D2D1_COLOR_F c)
{
    c.a *= g_opacity;
    g_brush->SetColor(c);
    return g_brush;
}
static ID2D1SolidColorBrush *brush(unsigned c, float a = 1) { return brush(rgb(c, a)); }

static D2D1_RECT_F R(float x, float y, float w, float h) { return D2D1::RectF(x, y, x + w, y + h); }
static D2D1_POINT_2F P(float x, float y) { return D2D1::Point2F(x, y); }

static void fill_round(D2D1_RECT_F r, float rad, D2D1_COLOR_F c) { g_rt->FillRoundedRectangle(D2D1::RoundedRect(r, rad, rad), brush(c)); }
static void stroke_round(D2D1_RECT_F r, float rad, D2D1_COLOR_F c, float w = 1)
{
    const D2D1_RECT_F in = D2D1::RectF(r.left + w / 2, r.top + w / 2, r.right - w / 2, r.bottom - w / 2);
    g_rt->DrawRoundedRectangle(D2D1::RoundedRect(in, rad, rad), brush(c), w);
}

static IDWriteTextFormat *make_format(const wchar_t *family, float size, DWRITE_FONT_WEIGHT weight)
{
    IDWriteTextFormat *f = nullptr;
    g_dw->CreateTextFormat(family, nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &f);
    return f;
}

static bool font_exists(const wchar_t *family)
{
    IDWriteFontCollection *fc = nullptr;
    UINT32 idx; BOOL found = FALSE;
    if (SUCCEEDED(g_dw->GetSystemFontCollection(&fc, FALSE)) && fc) { fc->FindFamilyName(family, &idx, &found); fc->Release(); }
    return found;
}

static void text(const std::wstring &s, IDWriteTextFormat *f, D2D1_RECT_F r, D2D1_COLOR_F c,
                 DWRITE_TEXT_ALIGNMENT a = DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT p = DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
{
    f->SetTextAlignment(a);
    f->SetParagraphAlignment(p);
    g_rt->DrawText(s.c_str(), (UINT32)s.size(), f, r, brush(c), D2D1_DRAW_TEXT_OPTIONS_NONE);
}

static DWRITE_TEXT_METRICS measure(const std::wstring &s, IDWriteTextFormat *f, float width)
{
    IDWriteTextLayout *l = nullptr;
    DWRITE_TEXT_METRICS m = {};
    f->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    if (SUCCEEDED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), f, width, 4000, &l)) && l) { l->GetMetrics(&m); l->Release(); }
    return m;
}
static float text_width(const std::wstring &s, IDWriteTextFormat *f) { return measure(s, f, 4000).widthIncludingTrailingWhitespace; }
static float text_height(const std::wstring &s, IDWriteTextFormat *f, float width) { return measure(s, f, width).height; }

static ID2D1PathGeometry *polyline(std::initializer_list<D2D1_POINT_2F> pts, bool closed)
{
    ID2D1PathGeometry *g = nullptr;
    ID2D1GeometrySink *s = nullptr;
    g_d2d->CreatePathGeometry(&g);
    g->Open(&s);
    auto it = pts.begin();
    s->BeginFigure(*it, closed ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
    for (++it; it != pts.end(); ++it) s->AddLine(*it);
    s->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    s->Close();
    s->Release();
    return g;
}

static void stroke_lines(std::initializer_list<D2D1_POINT_2F> pts, D2D1_COLOR_F c, float w)
{
    ID2D1PathGeometry *g = polyline(pts, false);
    g_rt->DrawGeometry(g, brush(c), w, g_round);
    g->Release();
}

static void arc(float cx, float cy, float r, float a0, float a1, D2D1_COLOR_F c, float w)
{
    ID2D1PathGeometry *g = nullptr;
    ID2D1GeometrySink *s = nullptr;
    g_d2d->CreatePathGeometry(&g);
    g->Open(&s);
    s->BeginFigure(P(cx + r * cosf(a0), cy + r * sinf(a0)), D2D1_FIGURE_BEGIN_HOLLOW);
    s->AddArc(D2D1::ArcSegment(P(cx + r * cosf(a1), cy + r * sinf(a1)), D2D1::SizeF(r, r), 0,
                               D2D1_SWEEP_DIRECTION_CLOCKWISE, (a1 - a0) > 3.14159f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
    s->EndFigure(D2D1_FIGURE_END_OPEN);
    s->Close();
    s->Release();
    g_rt->DrawGeometry(g, brush(c), w, g_round);
    g->Release();
}

// ------------------------------------------------------------------ interaction
struct Hit { int id; D2D1_RECT_F r; };
static std::vector<Hit> g_hits;
static std::map<int, float> g_anim;
static int g_hover = ID_NONE, g_pressed = ID_NONE;

static float hover_of(int id) { auto it = g_anim.find(id); return it == g_anim.end() ? 0.f : it->second; }
static void hit(int id, D2D1_RECT_F r) { g_hits.push_back({id, r}); }
static bool inside(D2D1_RECT_F r, float x, float y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

// ------------------------------------------------------------------ drawing
static void draw_wheel(float cx, float cy, float s)
{
    const float R0 = s * 0.35f, ring = s * 0.075f, hub = s * 0.09f;
    g_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), R0, R0), brush(col::TEXT), ring);
    for (float a : {3.14159f, 0.0f, 1.5708f})
        g_rt->DrawLine(P(cx + cosf(a) * hub * 0.6f, cy + sinf(a) * hub * 0.6f),
                       P(cx + cosf(a) * (R0 - ring * 0.4f), cy + sinf(a) * (R0 - ring * 0.4f)), brush(col::TEXT), s * 0.062f, g_round);
    arc(cx, cy, R0, 3.49f, 5.93f, rgb(col::ACCENT), ring);
    g_rt->FillEllipse(D2D1::Ellipse(P(cx, cy), hub, hub), brush(col::ACCENT));
}

static void flag_uk(D2D1_RECT_F r)
{
    const float w = r.right - r.left, h = r.bottom - r.top;
    g_rt->FillRectangle(r, brush(0x012169));
    for (unsigned c : {0xFFFFFFu, 0xC8102Eu}) {
        const float t = c == 0xFFFFFF ? h * 0.2f : h * 0.07f;
        g_rt->DrawLine(P(r.left, r.top), P(r.right, r.bottom), brush(c), t);
        g_rt->DrawLine(P(r.right, r.top), P(r.left, r.bottom), brush(c), t);
    }
    g_rt->FillRectangle(R(r.left, r.top + h / 3, w, h / 3), brush(0xFFFFFF));
    g_rt->FillRectangle(R(r.left + w / 2 - h / 6, r.top, h / 3, h), brush(0xFFFFFF));
    g_rt->FillRectangle(R(r.left, r.top + h * 0.4f, w, h * 0.2f), brush(0xC8102E));
    g_rt->FillRectangle(R(r.left + w / 2 - h * 0.1f, r.top, h * 0.2f, h), brush(0xC8102E));
}

static void flag_br(D2D1_RECT_F r)
{
    const float w = r.right - r.left, h = r.bottom - r.top, cx = r.left + w / 2, cy = r.top + h / 2;
    g_rt->FillRectangle(r, brush(0x009C3B));
    ID2D1PathGeometry *g = polyline({P(cx, r.top + h * 0.12f), P(r.right - w * 0.085f, cy), P(cx, r.bottom - h * 0.12f), P(r.left + w * 0.085f, cy)}, true);
    g_rt->FillGeometry(g, brush(0xFEDF00));
    g->Release();
    const float rr = h * 0.25f;
    g_rt->FillEllipse(D2D1::Ellipse(P(cx, cy), rr, rr), brush(0x002776));
    ID2D1PathGeometry *b = nullptr;
    ID2D1GeometrySink *s = nullptr;
    g_d2d->CreatePathGeometry(&b);
    b->Open(&s);
    s->BeginFigure(P(cx - rr * 0.97f, cy + rr * 0.1f), D2D1_FIGURE_BEGIN_HOLLOW);
    s->AddArc(D2D1::ArcSegment(P(cx + rr * 0.95f, cy + rr * 0.25f), D2D1::SizeF(rr * 1.9f, rr * 1.9f), 0,
                               D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    s->EndFigure(D2D1_FIGURE_END_OPEN);
    s->Close();
    s->Release();
    g_rt->DrawGeometry(b, brush(0xFFFFFF), h * 0.045f);
    b->Release();
}

static void flag_es(D2D1_RECT_F r)
{
    const float h = r.bottom - r.top;
    g_rt->FillRectangle(r, brush(0xAA151B));
    g_rt->FillRectangle(D2D1::RectF(r.left, r.top + h / 4, r.right, r.bottom - h / 4), brush(0xF1BF00));
}

static void flag_de(D2D1_RECT_F r)
{
    const float h = (r.bottom - r.top) / 3;
    g_rt->FillRectangle(D2D1::RectF(r.left, r.top, r.right, r.top + h), brush(0x000000));
    g_rt->FillRectangle(D2D1::RectF(r.left, r.top + h, r.right, r.top + 2 * h), brush(0xDD0000));
    g_rt->FillRectangle(D2D1::RectF(r.left, r.top + 2 * h, r.right, r.bottom), brush(0xFFCE00));
}

static void draw_flag(Lang l, D2D1_RECT_F r)
{
    static void (*const fns[LANG_COUNT])(D2D1_RECT_F) = {flag_uk, flag_br, flag_es, flag_de};
    ID2D1RoundedRectangleGeometry *g = nullptr;
    g_d2d->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r, 6, 6), &g);
    ID2D1Layer *layer = nullptr;
    g_rt->CreateLayer(&layer);
    g_rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), g, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), g_opacity), layer);
    const float keep = g_opacity;
    g_opacity = 1;
    fns[l](r);
    g_opacity = keep;
    g_rt->PopLayer();
    layer->Release();
    g->Release();
    stroke_round(r, 6, rgb(0xFFFFFF, 0.14f));
}

enum BtnStyle { BTN_PRIMARY, BTN_SECONDARY, BTN_DANGER, BTN_GHOST };

static void button(int id, D2D1_RECT_F r, const std::wstring &label, BtnStyle st, bool enabled = true)
{
    const float h = enabled ? hover_of(id) : 0, a = enabled ? 1.f : 0.4f;
    D2D1_RECT_F rr = r;
    if (g_pressed == id && g_hover == id) { rr.top += 1; rr.bottom += 1; }
    const DWRITE_TEXT_ALIGNMENT center = DWRITE_TEXT_ALIGNMENT_CENTER;
    const DWRITE_PARAGRAPH_ALIGNMENT mid = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    switch (st) {
    case BTN_PRIMARY:
        fill_round(rr, 10, mix(rgb(col::ACCENT, a), rgb(col::ACCENT_HI, a), h));
        text(label, f_bodyb, rr, rgb(col::INK, a), center, mid);
        break;
    case BTN_DANGER:
        fill_round(rr, 10, mix(rgb(0xE5484D, a), rgb(0xFF6369, a), h));
        text(label, f_bodyb, rr, rgb(0xFFFFFF, a), center, mid);
        break;
    case BTN_SECONDARY:
        fill_round(rr, 10, mix(rgb(col::CARD, a), rgb(col::CARD_HI, a), h));
        stroke_round(rr, 10, mix(rgb(col::BORDER, a), rgb(0x3A414D, a), h));
        text(label, f_bodyb, rr, rgb(col::TEXT, a), center, mid);
        break;
    case BTN_GHOST:
        text(label, f_bodyb, rr, mix(rgb(col::ACCENT, a), rgb(col::ACCENT_HI, a), h), DWRITE_TEXT_ALIGNMENT_LEADING, mid);
        break;
    }
    if (enabled) hit(id, r);
}

static void toggle(int id, float x, float y)
{
    const float t = hover_of(id + 1000);
    fill_round(R(x, y, 44, 24), 12, mix(rgb(0x2B313B), rgb(col::ACCENT), t));
    g_rt->FillEllipse(D2D1::Ellipse(P(x + 12 + t * 20, y + 12), 9, 9), brush(mix(rgb(col::TEXT), rgb(col::INK), t)));
    hit(id, R(x - 6, y - 6, 56, 36));
}

static void checkbox(float x, float y, bool on, float hover)
{
    const D2D1_RECT_F r = R(x, y, 22, 22);
    if (on) {
        fill_round(r, 6, rgb(col::ACCENT));
        stroke_lines({P(x + 5.5f, y + 11.5f), P(x + 9.5f, y + 15.5f), P(x + 16.5f, y + 7)}, rgb(col::INK), 2.4f);
    } else {
        stroke_round(r, 6, mix(rgb(0x3A414D), rgb(col::ACCENT), hover * 0.6f), 1.6f);
    }
}

static void chip(float right, float cy, const std::wstring &s, unsigned c)
{
    const float w = text_width(s, f_small) + 22;
    const D2D1_RECT_F r = R(right - w, cy - 12, w, 24);
    fill_round(r, 12, rgb(c, 0.13f));
    text(s, f_small, r, rgb(c), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

static float keycap(float x, float y, const std::wstring &label)
{
    const float w = label.size() > 1 ? text_width(label, f_key) + 22 : 32;
    fill_round(R(x, y + 2, w, 30), 7, rgb(0x08090C));
    fill_round(R(x, y, w, 30), 7, rgb(0x262B34));
    stroke_round(R(x, y, w, 30), 7, rgb(0x363C47));
    text(label, f_key, R(x, y, w, 30), rgb(col::TEXT), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    return w;
}

static float bullet(float x, float y, float w, const std::wstring &s)
{
    g_rt->FillEllipse(D2D1::Ellipse(P(x + 3, y + 9), 3, 3), brush(col::ACCENT));
    const float h = text_height(s, f_small, w - 16);
    text(s, f_small, R(x + 16, y, w - 16, h + 2), rgb(col::SOFT));
    return h;
}

// ------------------------------------------------------------------ pages
static float content_x() { return LEFT + 48; }
static float content_w() { return W - LEFT - 96; }
static const float BTN_Y = H - 84;

static void heading(const Str &title, const Str &sub, float y = 84)
{
    const float x = content_x(), w = content_w();
    text(tr(title), f_display, R(x, y, w, 46), rgb(col::TEXT));
    text(tr(sub), f_body, R(x, y + 50, w, 44), rgb(col::MUTED));
}

static std::wstring status_text(size_t i, unsigned &c)
{
    if (process_running(g_games[i].exe)) { c = col::ERR; return tr(txt::st_running); }
    const GameStatus &s = g_status[i];
    std::wstring v;
    if (HAS_DRIVE && HAS_VIEW && !s.drive.empty() && s.drive == s.view) v = s.drive;
    else if (HAS_DRIVE && !s.drive.empty()) v = L"Drive " + s.drive;
    if (HAS_VIEW && !s.view.empty() && v != s.view) v += (v.empty() ? L"" : L"  ·  ") + std::wstring(L"View ") + s.view;
    if (!v.empty()) { c = col::OK; return std::wstring(tr(txt::st_installed)) + L"  " + v; }
    if (s.legacy) { c = col::WARN; return tr(txt::st_old); }
    c = col::FAINT;
    return tr(txt::st_not_installed);
}

static void page_lang()
{
    const float x = content_x(), w = content_w();
    heading(txt::choose_lang, txt::choose_lang_sub, 96);
    const float cw = (w - 16) / 2, ch = 96;
    for (int i = 0; i < LANG_COUNT; i++) {
        const float cx = x + (i % 2) * (cw + 16), cy = 214 + (i / 2) * (ch + 16);
        const D2D1_RECT_F r = R(cx, cy, cw, ch);
        const float h = hover_of(ID_LANG0 + i);
        const bool cur = g_lang == (Lang)i;
        fill_round(r, 16, mix(rgb(col::CARD), rgb(col::CARD_HI), h));
        stroke_round(r, 16, cur ? rgb(col::ACCENT) : mix(rgb(col::BORDER), rgb(0x3A414D), h), cur ? 1.6f : 1.0f);
        draw_flag((Lang)i, R(cx + 24, cy + (ch - 40) / 2, 60, 40));
        text(tr(txt::lang_name[i]), f_h2, R(cx + 104, cy, cw - 140, ch), rgb(col::TEXT), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        text(L"›", f_title, R(cx, cy, cw - 22, ch - 4), mix(rgb(col::FAINT), rgb(col::ACCENT), h), DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        hit(ID_LANG0 + i, r);
    }
}

static void mod_card(D2D1_RECT_F r, bool drive, bool with_toggle)
{
    const bool on = drive ? g_want_drive : g_want_view;
    fill_round(r, 16, rgb(col::CARD));
    stroke_round(r, 16, on ? rgb(col::ACCENT, 0.55f) : rgb(col::BORDER), on ? 1.4f : 1.0f);
    const float x = r.left + 22, w = r.right - r.left - 44;
    text(drive ? L"SZNT Drive" : L"SZNT View", f_title, R(x, r.top + 20, w, 30), rgb(col::TEXT));
    const float tag_w = w - (with_toggle ? 56 : 0);
    const float th = text_height(tr(drive ? txt::drive_tag : txt::view_tag), f_small, tag_w);
    text(tr(drive ? txt::drive_tag : txt::view_tag), f_small, R(x, r.top + 54, tag_w, th + 2), rgb(col::ACCENT));
    float y = r.top + 66 + th;
    g_rt->FillRectangle(R(x, y, w, 1), brush(col::BORDER));
    y += 14;
    const Str *b[3] = {drive ? &txt::drive_b1 : &txt::view_b1, drive ? &txt::drive_b2 : &txt::view_b2, drive ? &txt::drive_b3 : &txt::view_b3};
    for (auto *s : b) y += bullet(x, y, w, tr(*s)) + 10;
    if (with_toggle) toggle(drive ? ID_TOGGLE_DRIVE : ID_TOGGLE_VIEW, r.right - 66, r.top + 24);
}

static void page_mods()
{
    const float x = content_x(), w = content_w();
    text(tr(txt::hero_title), f_display, R(x, 84, w, 46), rgb(col::TEXT));
    const float sh = text_height(tr(txt::hero_sub), f_body, w);
    text(tr(txt::hero_sub), f_body, R(x, 134, w, sh + 4), rgb(col::MUTED));
    const float top = 154 + sh;
    if (SZNT_EDITION == 0) {
        const float cw = (w - 16) / 2;
        mod_card(R(x, top, cw, BTN_Y - top - 24), true, true);
        mod_card(R(x + cw + 16, top, cw, BTN_Y - top - 24), false, true);
    } else {
        const bool drive = SZNT_EDITION == 1;
        mod_card(R(x, top, w, 220), drive, false);
        const D2D1_RECT_F r = R(x, top + 234, w, 56);
        const float h = hover_of(ID_LINK_OTHER);
        fill_round(r, 14, mix(rgb(col::BG1), rgb(col::CARD), 0.5f + h * 0.5f));
        stroke_round(r, 14, rgb(col::BORDER));
        const std::wstring other = drive ? L"SZNT View" : L"SZNT Drive";
        text(std::wstring(tr(txt::also_available)) + L":  ", f_small, R(r.left + 18, r.top, 400, 56), rgb(col::MUTED), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        const float lw = text_width(std::wstring(tr(txt::also_available)) + L":  ", f_small);
        text(other, f_bodyb, R(r.left + 18 + lw, r.top, 200, 56), rgb(col::TEXT), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        text(std::wstring(tr(txt::get_it)) + L"  ›", f_small, R(r.left, r.top, w - 18, 56), mix(rgb(col::ACCENT), rgb(col::ACCENT_HI), h),
             DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        hit(ID_LINK_OTHER, r);
    }
    button(ID_BACK, R(x, BTN_Y, 120, 44), tr(txt::btn_back), BTN_SECONDARY);
    button(ID_NEXT, R(x + w - 160, BTN_Y, 160, 44), tr(txt::btn_next), BTN_PRIMARY, g_want_drive || g_want_view);
}

static bool any_selected()
{
    for (auto &g : g_games) if (g.selected) return true;
    return false;
}

static bool any_selected_installed()
{
    for (size_t i = 0; i < g_games.size(); i++)
        if (g_games[i].selected && ((g_want_drive && !g_status[i].drive.empty()) || (g_want_view && !g_status[i].view.empty()) || g_status[i].legacy)) return true;
    return false;
}

static void game_list(float x, float y, float w)
{
    if (g_games.empty()) {
        fill_round(R(x, y, w, 64), 14, rgb(col::CARD));
        stroke_round(R(x, y, w, 64), 14, rgb(col::BORDER));
        text(tr(txt::no_games), f_body, R(x + 20, y, w - 40, 64), rgb(col::MUTED), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        y += 76;
    }
    for (size_t i = 0; i < g_games.size() && i < 4; i++) {
        const D2D1_RECT_F r = R(x, y, w, 70);
        const float h = hover_of(ID_GAME0 + (int)i);
        fill_round(r, 14, mix(rgb(col::CARD), rgb(col::CARD_HI), h));
        stroke_round(r, 14, g_games[i].selected ? rgb(col::ACCENT, 0.45f) : rgb(col::BORDER));
        checkbox(x + 20, y + 24, g_games[i].selected, h);
        unsigned c;
        const std::wstring st = status_text(i, c);
        const float chip_w = text_width(st, f_small) + 22;
        text(g_games[i].name, f_name, R(x + 58, y + 14, w - chip_w - 90, 24), rgb(col::TEXT));
        text(g_games[i].root, f_path, R(x + 58, y + 38, w - chip_w - 90, 20), rgb(col::FAINT));
        chip(r.right - 18, y + 35, st, c);
        hit(ID_GAME0 + (int)i, r);
        y += 82;
    }
    button(ID_ADD_FOLDER, R(x + 4, y, 280, 30), tr(txt::add_folder), BTN_GHOST);
}

static void page_games()
{
    const float x = content_x(), w = content_w();
    heading(txt::games_title, txt::games_sub);
    game_list(x, 190, w);
    button(ID_BACK, R(x, BTN_Y, 120, 44), tr(txt::btn_back), BTN_SECONDARY);
    button(ID_INSTALL, R(x + w - 180, BTN_Y, 180, 44), tr(any_selected_installed() ? txt::btn_update : txt::btn_install), BTN_PRIMARY, any_selected());
}

static float now_s() { return (float)(GetTickCount64() % 1000000) / 1000.0f; }

static void spinner(float cx, float cy, float r)
{
    g_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), r, r), brush(0x2B313B), 3);
    const float a = now_s() * 5.5f;
    arc(cx, cy, r, a, a + 1.6f, rgb(col::ACCENT), 3);
}

static void note_icon(NoteKind k, float x, float y)
{
    if (k == NOTE_OK) stroke_lines({P(x + 2, y + 9), P(x + 6, y + 13), P(x + 13, y + 4)}, rgb(col::OK), 2.2f);
    else if (k == NOTE_WARN || k == NOTE_ERROR) {
        const unsigned c = k == NOTE_WARN ? col::WARN : col::ERR;
        g_rt->FillEllipse(D2D1::Ellipse(P(x + 7.5f, y + 8.5f), 8, 8), brush(c, 0.18f));
        text(L"!", f_key, R(x - 1, y, 17, 17), rgb(c), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    } else g_rt->FillEllipse(D2D1::Ellipse(P(x + 7.5f, y + 8.5f), 2.5f, 2.5f), brush(col::FAINT));
}

static void page_progress()
{
    const float x = content_x(), w = content_w();
    const bool warn = g_finished && !g_result.ok;
    std::wstring title = g_busy ? tr(g_uninstall_mode ? txt::removing : txt::installing) : tr(warn ? txt::done_warn_title : txt::done_title);
    if (g_finished && g_result.need_admin) title = tr(txt::admin_needed);
    if (g_busy) spinner(x + 14, 108, 12);
    text(title, f_display, R(x + (g_busy ? 42 : 0), 84, w - 42, 46), rgb(col::TEXT));

    const D2D1_RECT_F bar = R(x, 148, w, 6);
    fill_round(bar, 3, rgb(0x232831));
    if (g_busy) {
        const float t = fmodf(now_s(), 1.4f) / 1.4f, seg = w * 0.32f, px = x - seg + (w + seg) * t;
        g_rt->PushAxisAlignedClip(bar, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        fill_round(R(px, 148, seg, 6), 3, rgb(col::ACCENT));
        g_rt->PopAxisAlignedClip();
    } else fill_round(bar, 3, rgb(warn ? col::WARN : col::OK));

    const D2D1_RECT_F box = R(x, 174, w, BTN_Y - 174 - 22);
    fill_round(box, 14, rgb(col::BG0));
    stroke_round(box, 14, rgb(col::BORDER));
    std::vector<float> hs;
    float total = 0;
    for (auto &n : g_notes) {
        hs.push_back(text_height(n.text, n.kind == NOTE_HEADER ? f_bodyb : f_small, w - 66) + (n.kind == NOTE_HEADER ? 12 : 8));
        total += hs.back();
    }
    const float room = box.bottom - box.top - 32;
    float y = box.top + 16 - (total > room ? total - room : 0);
    g_rt->PushAxisAlignedClip(D2D1::RectF(box.left, box.top + 8, box.right, box.bottom - 8), D2D1_ANTIALIAS_MODE_ALIASED);
    for (size_t i = 0; i < g_notes.size(); i++) {
        const Note &n = g_notes[i];
        if (n.kind == NOTE_HEADER) text(n.text, f_bodyb, R(box.left + 20, y, w - 40, hs[i]), rgb(col::TEXT));
        else {
            note_icon(n.kind, box.left + 22, y + 1);
            text(n.text, f_small, R(box.left + 46, y, w - 66, hs[i]), rgb(n.kind == NOTE_ERROR ? 0xFFB3B3 : col::SOFT));
        }
        y += hs[i];
    }
    g_rt->PopAxisAlignedClip();

    if (g_finished) {
        button(ID_BACK, R(x, BTN_Y, 120, 44), tr(txt::btn_back), BTN_SECONDARY);
        if (g_result.need_admin) button(ID_ADMIN, R(x + w - 270, BTN_Y, 270, 44), tr(txt::btn_retry_admin), BTN_PRIMARY);
        else button(ID_CONTINUE, R(x + w - 160, BTN_Y, 160, 44), tr(txt::btn_next), BTN_PRIMARY);
    }
}

static const Game *launchable()
{
    for (auto &g : g_games) if (g.selected && g.steam_app) return &g;
    return nullptr;
}

static void page_done()
{
    const float x = content_x(), w = content_w();
    const float t = g_check_t, e = 1 - (1 - t) * (1 - t) * (1 - t);
    const float cx = x + 28, cy = 108;
    g_rt->FillEllipse(D2D1::Ellipse(P(cx, cy), 26 * e, 26 * e), brush(col::OK, 0.15f));
    g_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), 26 * e, 26 * e), brush(col::OK), 2);
    if (t > 0.4f) stroke_lines({P(cx - 10, cy + 1), P(cx - 3, cy + 8), P(cx + 11, cy - 7)}, rgb(col::OK, (t - 0.4f) / 0.6f), 3.2f);
    const bool removed = g_uninstall_mode;
    text(tr(removed ? txt::removed_title : txt::done_title), f_display, R(x + 72, 80, w - 72, 46), rgb(col::TEXT));
    text(tr(removed ? txt::removed_sub : txt::done_sub), f_body, R(x + 72, 126, w - 72, 48), rgb(col::MUTED));

    if (!removed) {
        const D2D1_RECT_F card = R(x, 196, w, BTN_Y - 196 - 22);
        fill_round(card, 16, rgb(col::CARD));
        stroke_round(card, 16, rgb(col::BORDER));
        text(tr(txt::controls_title), f_bodyb, R(x + 24, 214, w - 48, 24), rgb(col::TEXT));
        float y = 250;
        auto row = [&](std::initializer_list<const wchar_t *> keys, const std::wstring &desc) {
            float kx = x + 24;
            for (auto k : keys) kx += keycap(kx, y, k) + 6;
            text(desc, f_small, R(x + 200, y, w - 224, 30), rgb(col::SOFT), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            y += 40;
        };
        if (g_want_drive) {
            row({L"W", L"S"}, tr(txt::ctl_ws));
            row({L"A", L"D"}, tr(txt::ctl_ad));
            row({L"2×  W", L"2×  S"}, tr(txt::ctl_double));
        }
        if (g_want_view) {
            row({L"Mouse"}, tr(txt::ctl_mouse));
            row({L"Mouse 3"}, tr(txt::ctl_middle));
        }
        text(tr(txt::ctl_ini), f_tiny, R(x + 24, card.bottom - 40, w - 48, 30), rgb(col::FAINT), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (launchable()) button(ID_LAUNCH, R(x, BTN_Y, 180, 44), tr(txt::btn_launch), BTN_SECONDARY);
    }
    button(ID_FINISH, R(x + w - 160, BTN_Y, 160, 44), tr(txt::btn_finish), BTN_PRIMARY);
}

static void page_uninstall()
{
    const float x = content_x(), w = content_w();
    heading(txt::uninstall_title, txt::uninstall_sub);
    float y = 190;
    if (SZNT_EDITION == 0) {
        for (int i = 0; i < 2; i++) {
            const bool drive = i == 0, on = drive ? g_want_drive : g_want_view;
            const D2D1_RECT_F r = R(x + i * (w / 2 + 8), y, w / 2 - 8, 58);
            fill_round(r, 14, rgb(col::CARD));
            stroke_round(r, 14, on ? rgb(col::ACCENT, 0.5f) : rgb(col::BORDER));
            text(drive ? L"SZNT Drive" : L"SZNT View", f_bodyb, R(r.left + 20, r.top, 200, 58), rgb(col::TEXT), DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            toggle(drive ? ID_TOGGLE_DRIVE : ID_TOGGLE_VIEW, r.right - 64, r.top + 17);
        }
        y += 76;
    }
    game_list(x, y, w);
    button(ID_UNINSTALL, R(x + w - 180, BTN_Y, 180, 44), tr(txt::btn_uninstall), BTN_DANGER, any_selected() && (g_want_drive || g_want_view));
}

static void draw_sidebar()
{
    g_rt->FillRectangle(R(0, 0, LEFT, H), brush(col::BG0));
    ID2D1RadialGradientBrush *glow = nullptr;
    ID2D1GradientStopCollection *stops = nullptr;
    const D2D1_GRADIENT_STOP gs[2] = {{0, rgb(col::ACCENT, 0.17f)}, {1, rgb(col::ACCENT, 0)}};
    g_rt->CreateGradientStopCollection(gs, 2, &stops);
    g_rt->CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(P(-20, H + 30), P(0, 0), 340, 340), stops, &glow);
    g_rt->FillRectangle(R(0, 0, LEFT, H), glow);
    glow->Release();
    stops->Release();
    g_rt->FillRectangle(R(LEFT - 1, 0, 1, H), brush(col::BORDER));

    draw_wheel(56, 80, 54);
    text(L"SZNT", f_tiny, R(96, 58, 160, 16), rgb(col::ACCENT));
    text(EDITION_NAME, f_h2, R(96, 72, 180, 26), rgb(col::TEXT));
    text(widen(SZNT_VERSION), f_tiny, R(96, 97, 160, 16), rgb(col::FAINT));

    if (!g_uninstall_mode) {
        const Str *steps[4] = {&txt::step_lang, &txt::step_mods, &txt::step_games, &txt::step_install};
        const int cur = g_page == P_LANG ? 0 : g_page == P_MODS ? 1 : g_page == P_GAMES ? 2 : 3;
        for (int i = 0; i < 4; i++) {
            const float y = 170 + i * 48;
            const bool done = i < cur || g_page == P_DONE, active = i == cur && g_page != P_DONE;
            if (i < 3) g_rt->FillRectangle(R(39.25f, y + 27, 1.5f, 20), brush(done ? col::ACCENT : 0x2B313B));
            if (active) {
                g_rt->FillEllipse(D2D1::Ellipse(P(40, y + 13), 13, 13), brush(col::ACCENT));
                text(std::to_wstring(i + 1), f_key, R(27, y, 26, 26), rgb(col::INK), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            } else if (done) {
                g_rt->DrawEllipse(D2D1::Ellipse(P(40, y + 13), 12, 12), brush(col::ACCENT), 1.5f);
                stroke_lines({P(34.5f, y + 13.5f), P(38.5f, y + 17.5f), P(45.5f, y + 9)}, rgb(col::ACCENT), 2);
            } else {
                g_rt->DrawEllipse(D2D1::Ellipse(P(40, y + 13), 12, 12), brush(0x343A45), 1.5f);
                text(std::to_wstring(i + 1), f_key, R(27, y, 26, 26), rgb(col::FAINT), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
            text(tr(*steps[i]), active ? f_bodyb : f_body, R(66, y, 190, 26), rgb(active || done ? col::TEXT : col::FAINT),
                 DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
    }

    const float h1 = hover_of(ID_LINK_HOME), h2 = hover_of(ID_LINK_REPO);
    text(L"mods.sznt.dev", f_small, R(28, H - 72, 200, 20), mix(rgb(col::MUTED), rgb(col::ACCENT), h1));
    hit(ID_LINK_HOME, R(28, H - 74, 110, 22));
    text(L"Open source  ·  GPL-3.0", f_tiny, R(28, H - 48, 220, 18), mix(rgb(col::FAINT), rgb(col::MUTED), h2));
    hit(ID_LINK_REPO, R(28, H - 50, 150, 20));
}

static void draw_titlebar()
{
    for (int i = 0; i < 2; i++) {
        const int id = i == 0 ? ID_MIN : ID_CLOSE;
        const D2D1_RECT_F r = R(W - 92 + i * 46, 0, 46, 36);
        const float h = hover_of(id), cx = r.left + 23, cy = 18;
        if (h > 0.01f) g_rt->FillRectangle(r, brush(id == ID_CLOSE ? rgb(0xE5484D, h) : rgb(0xFFFFFF, 0.08f * h)));
        const D2D1_COLOR_F c = mix(rgb(col::MUTED), rgb(0xFFFFFF), h);
        if (id == ID_MIN) g_rt->DrawLine(P(cx - 5, cy), P(cx + 5, cy), brush(c), 1.2f);
        else {
            g_rt->DrawLine(P(cx - 5, cy - 5), P(cx + 5, cy + 5), brush(c), 1.2f);
            g_rt->DrawLine(P(cx + 5, cy - 5), P(cx - 5, cy + 5), brush(c), 1.2f);
        }
        hit(id, r);
    }
    if (!g_update.empty()) {
        const std::wstring s = fmt(tr(txt::update_available), g_update.c_str()) + L"   ·   " + tr(txt::btn_download) + L"  ›";
        const float w = text_width(s, f_small) + 30, h = hover_of(ID_UPDATE);
        const D2D1_RECT_F r = R(W - 108 - w, 8, w, 26);
        fill_round(r, 13, rgb(col::ACCENT, 0.14f + 0.1f * h));
        text(s, f_small, r, rgb(col::ACCENT_HI), DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        hit(ID_UPDATE, r);
    }
}

static void draw_page()
{
    switch (g_page) {
    case P_LANG: page_lang(); break;
    case P_MODS: page_mods(); break;
    case P_GAMES: page_games(); break;
    case P_PROGRESS: page_progress(); break;
    case P_DONE: page_done(); break;
    case P_UNINSTALL: page_uninstall(); break;
    }
}

// ------------------------------------------------------------------ render
static void create_target()
{
    if (g_rt) return;
    RECT rc; GetClientRect(g_wnd, &rc);
    g_d2d->CreateHwndRenderTarget(D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), (float)g_dpi, (float)g_dpi),
                                  D2D1::HwndRenderTargetProperties(g_wnd, D2D1::SizeU(rc.right, rc.bottom)), &g_rt);
    g_rt->CreateSolidColorBrush(rgb(0), &g_brush);
    g_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

static void release_target()
{
    if (g_brush) { g_brush->Release(); g_brush = nullptr; }
    if (g_rt) { g_rt->Release(); g_rt = nullptr; }
}

static void render()
{
    create_target();
    g_rt->BeginDraw();
    g_rt->Clear(rgb(col::BG1));
    g_hits.clear();
    g_opacity = 1;
    draw_sidebar();
    draw_titlebar();
    const float e = 1 - (1 - g_page_t) * (1 - g_page_t) * (1 - g_page_t);
    g_opacity = e;
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation((1 - e) * 18, 0));
    draw_page();
    g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g_opacity = 1;
    if (g_rt->EndDraw() == (HRESULT)D2DERR_RECREATE_TARGET) release_target();
}

static float anim_target(int id)
{
    if (id == ID_TOGGLE_DRIVE + 1000) return g_want_drive ? 1.f : 0.f;
    if (id == ID_TOGGLE_VIEW + 1000) return g_want_view ? 1.f : 0.f;
    return id == g_hover ? 1.f : 0.f;
}

static bool animating()
{
    if (g_page_t < 1 || g_busy || (g_page == P_DONE && g_check_t < 1)) return true;
    for (auto &kv : g_anim) if (fabsf(kv.second - anim_target(kv.first)) > 0.002f) return true;
    return false;
}

static void tick(float dt)
{
    g_page_t = fminf(1.f, g_page_t + dt / 0.3f);
    if (g_page == P_DONE) g_check_t = fminf(1.f, g_check_t + dt / 0.6f);
    if (g_hover != ID_NONE) g_anim[g_hover];
    const float k = 1 - expf(-dt * 16);
    for (auto &kv : g_anim) kv.second += (anim_target(kv.first) - kv.second) * k;
}

static LARGE_INTEGER g_qpf, g_last;
static void kick_anim() { QueryPerformanceCounter(&g_last); SetTimer(g_wnd, 1, 15, nullptr); }

static void go(Page p)
{
    g_page = p;
    g_page_t = 0;
    if (p == P_DONE) g_check_t = 0;
    kick_anim();
    InvalidateRect(g_wnd, nullptr, FALSE);
}

// ------------------------------------------------------------------ actions
static void refresh_status()
{
    g_status.clear();
    for (auto &g : g_games) g_status.push_back(status_of(g));
}

static void save_lang()
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SZNT", 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        reg_set(k, L"Language", widen(txt::lang_code[g_lang]));
        RegCloseKey(k);
    }
}

static void load_lang()
{
    const std::wstring saved = reg_string(HKEY_CURRENT_USER, L"Software\\SZNT", L"Language");
    for (int i = 0; i < LANG_COUNT; i++) if (saved == widen(txt::lang_code[i])) { g_lang = (Lang)i; return; }
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_PORTUGUESE: g_lang = LANG_PT; break;
    case LANG_SPANISH: g_lang = LANG_ES; break;
    case LANG_GERMAN: g_lang = LANG_DE; break;
    default: g_lang = LANG_EN;
    }
}

static void post_note(NoteKind k, const std::wstring &s) { PostMessageW(g_wnd, WM_APP_NOTE, (WPARAM)k, (LPARAM)new std::wstring(s)); }

static DWORD WINAPI worker(LPVOID)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Result *res = new Result;
    Sleep(400);
    for (auto &g : g_games) {
        if (!g.selected) continue;
        if (g_uninstall_mode) uninstall_game(g, g_want_drive, g_want_view, *res);
        else install_game(g, g_want_drive, g_want_view, *res);
        if (res->need_admin) break;
    }
    if (!res->need_admin) {
        if (g_uninstall_mode) unregister_app(g_want_drive && !installed_anywhere(g_games, true), g_want_view && !installed_anywhere(g_games, false));
        else if (res->ok) register_app(g_want_drive, g_want_view);
    }
    Sleep(300);
    PostMessageW(g_wnd, WM_APP_DONE, 0, (LPARAM)res);
    CoUninitialize();
    return 0;
}

static void start_work()
{
    g_notes.clear();
    g_busy = true;
    g_finished = false;
    g_result = Result();
    go(P_PROGRESS);
    CloseHandle(CreateThread(nullptr, 0, worker, nullptr, 0, nullptr));
}

static void open_url(const char *u) { ShellExecuteW(g_wnd, L"open", widen(u).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

static HANDLE g_single;

static void restart_as_admin()
{
    SHELLEXECUTEINFOW ei; memset(&ei, 0, sizeof(ei));
    ei.cbSize = sizeof(ei);
    ei.lpVerb = L"runas";
    const std::wstring me = self_path();
    std::wstring args = L"--lang " + widen(txt::lang_code[g_lang]);
    if (g_uninstall_mode) args += L" --uninstall";
    if (!g_want_drive) args += L" --only-view";
    if (!g_want_view) args += L" --only-drive";
    ei.lpFile = me.c_str();
    ei.lpParameters = args.c_str();
    ei.nShow = SW_SHOWNORMAL;
    if (g_single) { CloseHandle(g_single); g_single = nullptr; }
    if (ShellExecuteExW(&ei)) DestroyWindow(g_wnd);
    else g_single = CreateMutexW(nullptr, TRUE, L"SZNT-Setup-Running");
}

static void add_folder()
{
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void **)&dlg))) return;
    DWORD opt = 0;
    dlg->GetOptions(&opt);
    dlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(tr(txt::folder_title));
    if (SUCCEEDED(dlg->Show(g_wnd))) {
        IShellItem *it = nullptr;
        if (SUCCEEDED(dlg->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                if (!add_game(g_games, p)) MessageBoxW(g_wnd, tr(txt::folder_bad), EDITION_NAME, MB_ICONWARNING);
                refresh_status();
                CoTaskMemFree(p);
            }
            it->Release();
        }
    }
    dlg->Release();
}

static void click(int id)
{
    if (id >= ID_LANG0 && id < ID_LANG0 + LANG_COUNT) {
        g_lang = (Lang)(id - ID_LANG0);
        save_lang();
        go(P_MODS);
        return;
    }
    if (id >= ID_GAME0 && id < ID_GAME0 + 16) {
        g_games[id - ID_GAME0].selected = !g_games[id - ID_GAME0].selected;
        return;
    }
    switch (id) {
    case ID_CLOSE: if (!g_busy) DestroyWindow(g_wnd); break;
    case ID_MIN: ShowWindow(g_wnd, SW_MINIMIZE); break;
    case ID_NEXT: go(P_GAMES); break;
    case ID_BACK:
        if (g_page == P_MODS) go(P_LANG);
        else if (g_page == P_GAMES) go(P_MODS);
        else if (g_page == P_PROGRESS) go(g_uninstall_mode ? P_UNINSTALL : P_GAMES);
        break;
    case ID_TOGGLE_DRIVE: g_want_drive = !g_want_drive; break;
    case ID_TOGGLE_VIEW: g_want_view = !g_want_view; break;
    case ID_INSTALL: case ID_UNINSTALL: start_work(); break;
    case ID_CONTINUE: go(P_DONE); break;
    case ID_ADMIN: restart_as_admin(); break;
    case ID_FINISH: DestroyWindow(g_wnd); break;
    case ID_LAUNCH:
        if (const Game *g = launchable()) ShellExecuteW(g_wnd, L"open", fmt(L"steam://rungameid/%u", g->steam_app).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        DestroyWindow(g_wnd);
        break;
    case ID_ADD_FOLDER: add_folder(); break;
    case ID_UPDATE: open_url(download_page()); break;
    case ID_LINK_HOME: open_url(SZNT_URL_HOME); break;
    case ID_LINK_REPO: open_url(SZNT_REPO_URL); break;
    case ID_LINK_OTHER: open_url(SZNT_EDITION == 1 ? SZNT_URL_VIEW : SZNT_URL_DRIVE); break;
    }
}

static int hit_at(float x, float y)
{
    for (auto it = g_hits.rbegin(); it != g_hits.rend(); ++it) if (inside(it->r, x, y)) return it->id;
    return ID_NONE;
}

// ------------------------------------------------------------------ window
static float dip(int px) { return px * 96.0f / g_dpi; }

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_NCCALCSIZE:
        if (wp) return 0;
        break;
    case WM_NCHITTEST: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(h, &p);
        if (dip(p.y) < 40 && hit_at(dip(p.x), dip(p.y)) == ID_NONE) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        render();
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER: {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        const float dt = fminf(0.1f, (float)(now.QuadPart - g_last.QuadPart) / g_qpf.QuadPart);
        g_last = now;
        tick(dt);
        InvalidateRect(h, nullptr, FALSE);
        if (!animating()) KillTimer(h, 1);
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int id = hit_at(dip(GET_X_LPARAM(lp)), dip(GET_Y_LPARAM(lp)));
        if (id != g_hover) { g_hover = id; kick_anim(); }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, h, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: g_hover = ID_NONE; kick_anim(); return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, g_hover != ID_NONE ? IDC_HAND : IDC_ARROW)); return TRUE; }
        break;
    case WM_LBUTTONDOWN:
        g_pressed = hit_at(dip(GET_X_LPARAM(lp)), dip(GET_Y_LPARAM(lp)));
        SetCapture(h);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        const int id = hit_at(dip(GET_X_LPARAM(lp)), dip(GET_Y_LPARAM(lp))), was = g_pressed;
        g_pressed = ID_NONE;
        if (id != ID_NONE && id == was) click(id);
        kick_anim();
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE && !g_busy) DestroyWindow(h);
        return 0;
    case WM_APP_NOTE: {
        std::wstring *s = (std::wstring *)lp;
        g_notes.push_back({(NoteKind)wp, *s});
        delete s;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_APP_DONE: {
        Result *r = (Result *)lp;
        g_result = *r;
        delete r;
        g_busy = false;
        g_finished = true;
        refresh_status();
        if (g_result.ok && !g_result.need_admin) go(P_DONE);
        else { kick_anim(); InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_APP_UPDATE: {
        std::wstring *s = (std::wstring *)lp;
        g_update = *s;
        delete s;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_SIZE:
        if (g_rt) g_rt->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
        return 0;
    case WM_DPICHANGED: {
        g_dpi = HIWORD(wp);
        release_target();
        const RECT *r = (const RECT *)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_CLOSE:
        if (g_busy) return 0;
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static DWORD WINAPI update_check(LPVOID)
{
    const std::wstring v = newer_version();
    if (!v.empty()) PostMessageW(g_wnd, WM_APP_UPDATE, 0, (LPARAM)new std::wstring(v));
    return 0;
}

// ------------------------------------------------------------------ silent mode
static std::wstring g_silent_log;
static void silent_note(NoteKind k, const std::wstring &s)
{
    static const wchar_t *const tags[] = {L"   ", L" ok ", L" !! ", L" XX ", L""};
    g_silent_log += (k == NOTE_HEADER ? L"\r\n" + s : std::wstring(tags[k]) + s) + L"\r\n";
}

// SZNT-Setup.exe --silent [--uninstall] [--only-drive|--only-view] [--lang en|pt|es|de] [--game "folder"]...
// Report in %TEMP%\sznt-setup.log.
static int run_silent(const std::vector<std::wstring> &only)
{
    g_note = silent_note;
    (void)only;
    Result res;
    if (g_games.empty()) { silent_note(NOTE_ERROR, tr(txt::m_no_games)); res.ok = false; }
    for (auto &g : g_games) {
        if (g_uninstall_mode) uninstall_game(g, g_want_drive, g_want_view, res);
        else install_game(g, g_want_drive, g_want_view, res);
    }
    if (res.need_admin) silent_note(NOTE_ERROR, tr(txt::admin_needed));
    if (g_uninstall_mode) unregister_app(g_want_drive && !installed_anywhere(g_games, true), g_want_view && !installed_anywhere(g_games, false));
    else if (res.ok) register_app(g_want_drive, g_want_view);
    wchar_t tmp[MAX_PATH] = {0};
    GetTempPathW(MAX_PATH, tmp);
    write_file(std::wstring(tmp) + L"sznt-setup.log", "\xEF\xBB\xBF" + narrow(g_silent_log));
    return res.ok ? 0 : 1;
}

// ------------------------------------------------------------------ entry point
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    load_lang();
    bool silent = false;
    std::vector<std::wstring> only;
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; i++) {
        if (!wcscmp(argv[i], L"--silent")) silent = true;
        else if (!wcscmp(argv[i], L"--uninstall")) g_uninstall_mode = true;
        else if (!wcscmp(argv[i], L"--only-drive") && HAS_DRIVE) g_want_view = false;
        else if (!wcscmp(argv[i], L"--only-view") && HAS_VIEW) g_want_drive = false;
        else if (!wcscmp(argv[i], L"--game") && i + 1 < argc) only.push_back(argv[++i]);
        else if (!wcscmp(argv[i], L"--lang") && i + 1 < argc) {
            const std::string c = narrow(argv[++i]);
            for (int k = 0; k < LANG_COUNT; k++) if (c == txt::lang_code[k]) g_lang = (Lang)k;
        }
    }
    if (argv) LocalFree(argv);
    g_games = find_games();
    if (!only.empty()) { g_games.clear(); for (auto &p : only) add_game(g_games, p); }
    if (silent) {
        const int rc = run_silent(only);
        CoUninitialize();
        return rc;
    }

    g_single = CreateMutexW(nullptr, TRUE, L"SZNT-Setup-Running");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(L"SZNTSetup", nullptr)) SetForegroundWindow(other);
        return 0;
    }

    g_note = post_note;
    if (g_uninstall_mode)
        for (auto &g : g_games) {
            const GameStatus s = status_of(g);
            g.selected = (g_want_drive && !s.drive.empty()) || (g_want_view && !s.view.empty());
        }
    refresh_status();
    g_anim[ID_TOGGLE_DRIVE + 1000] = g_want_drive ? 1.f : 0.f;
    g_anim[ID_TOGGLE_VIEW + 1000] = g_want_view ? 1.f : 0.f;

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, (void **)&g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_dw);
    g_d2d->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND),
                             nullptr, 0, &g_round);
    const wchar_t *disp = font_exists(L"Segoe UI Variable Display") ? L"Segoe UI Variable Display" : L"Segoe UI";
    const wchar_t *body = font_exists(L"Segoe UI Variable Text") ? L"Segoe UI Variable Text" : L"Segoe UI";
    f_display = make_format(disp, 30, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_title = make_format(disp, 22, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_h2 = make_format(disp, 17, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_body = make_format(body, 14.5f, DWRITE_FONT_WEIGHT_NORMAL);
    f_bodyb = make_format(body, 14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_small = make_format(body, 13, DWRITE_FONT_WEIGHT_NORMAL);
    f_tiny = make_format(body, 11.5f, DWRITE_FONT_WEIGHT_NORMAL);
    f_key = make_format(body, 12.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_path = make_format(body, 12, DWRITE_FONT_WEIGHT_NORMAL);
    f_path->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteInlineObject *ellipsis = nullptr;
    g_dw->CreateEllipsisTrimmingSign(f_path, &ellipsis);
    const DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
    f_path->SetTrimming(&trim, ellipsis);
    f_name = make_format(body, 14.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    f_name->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteInlineObject *ellipsis2 = nullptr;
    g_dw->CreateEllipsisTrimmingSign(f_name, &ellipsis2);
    f_name->SetTrimming(&trim, ellipsis2);

    WNDCLASSEXW wc; memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, 0);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"SZNTSetup";
    RegisterClassExW(&wc);

    POINT pt; GetCursorPos(&pt);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi);
    g_wnd = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, EDITION_NAME, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            mi.rcWork.left, mi.rcWork.top, 100, 100, nullptr, nullptr, inst, nullptr);
    g_dpi = GetDpiForWindow(g_wnd);
    const int ww = MulDiv((int)W, g_dpi, 96), wh = MulDiv((int)H, g_dpi, 96);
    SetWindowPos(g_wnd, nullptr, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2,
                 mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2, ww, wh, SWP_NOZORDER | SWP_FRAMECHANGED);
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(g_wnd, 20, &dark, sizeof(dark));
    const int round = 2;
    DwmSetWindowAttribute(g_wnd, 33, &round, sizeof(round));
    const MARGINS margins = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(g_wnd, &margins);

    QueryPerformanceFrequency(&g_qpf);
    g_page = g_uninstall_mode ? P_UNINSTALL : P_LANG;
    g_page_t = 0;
    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);
    kick_anim();
    CloseHandle(CreateThread(nullptr, 0, update_check, nullptr, 0, nullptr));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    release_target();
    if (g_single) CloseHandle(g_single);
    CoUninitialize();
    return 0;
}
