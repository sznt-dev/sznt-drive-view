// SZNT Setup - installs, updates and removes SZNT Drive and SZNT View (ETS2 / ATS).
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include "installer.h"
#include <d2d1.h>
#include <dwrite_3.h>
#include <dwmapi.h>
#include <wincodec.h>
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
    ID_TOGGLE_DRIVE, ID_TOGGLE_VIEW, ID_ADD_FOLDER, ID_UPDATE, ID_LINK_HOME, ID_LINK_OTHER, ID_CONTINUE,
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
static const float W = 900, H = 600, HEADER = 64, PAD = 48, FOOT_Y = 528;
static const float CW = W - 2 * PAD;

#define WM_APP_NOTE   (WM_APP + 1)
#define WM_APP_DONE   (WM_APP + 2)
#define WM_APP_UPDATE (WM_APP + 3)

// ------------------------------------------------------------------ brand (same tokens as mods.sznt.dev, light and dark)
namespace col {
static unsigned PAPER, CARD, WHITE_SOFT, INK, INK2, MUTED, FAINT, LINE, LINE2, ON_INK, SIGNAL, SIGNAL_INK, COBALT, COBALT_INK, ASPHALT, OK;
static const unsigned OK_DARK = 0x3FBF7A, ERR = 0xE5484D;
}
static bool g_dark = true;

static void apply_theme(bool dark)
{
    using namespace col;
    g_dark = dark;
    if (dark) {
        PAPER = 0x101113; CARD = 0x1A1B1F; WHITE_SOFT = 0x1F2025; INK = 0xECEBE6; INK2 = 0xC3C1BB; MUTED = 0x8D8B85; FAINT = 0x5F5D58;
        LINE = 0x2A2B30; LINE2 = 0x3A3B42; ON_INK = 0x111214; SIGNAL = 0xFF5A16; SIGNAL_INK = 0xFF7A42; COBALT = 0x5B7CFF; COBALT_INK = 0x8AA2FF;
        ASPHALT = 0x0B0C0E; OK = 0x3FBF7A;
    } else {
        PAPER = 0xEDEBE6; CARD = 0xF7F6F2; WHITE_SOFT = 0xFBFAF7; INK = 0x141517; INK2 = 0x3D3E42; MUTED = 0x74726B; FAINT = 0x9D9A92;
        LINE = 0xD9D5CC; LINE2 = 0xC8C3B8; ON_INK = 0xFFFFFF; SIGNAL = 0xFF4D00; SIGNAL_INK = 0xC23B00; COBALT = 0x2B59FF; COBALT_INK = 0x1A3FCC;
        ASPHALT = 0x141518; OK = 0x138A4B;
    }
}

static bool windows_dark()
{
    DWORD v = 1, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                     RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS) return true;
    return v == 0;
}

static unsigned accent() { return SZNT_EDITION == 2 ? col::COBALT : col::SIGNAL; }
static unsigned accent_ink() { return SZNT_EDITION == 2 ? col::COBALT_INK : col::SIGNAL_INK; }
static unsigned mod_accent(bool drive) { return drive ? col::SIGNAL : col::COBALT; }

// Wordmark SZNT (Archivo Expanded outlines) + the orange road-stripe signal.
static const char *WORDMARK =
    "M430.6 12Q357.7 12 291.7 3.2Q225.8 -5.6 175 -28.5Q124.3 -51.4 95.3 -93.3Q66.4 -135.3 66.4 -201.1Q66.4 -205.8 66.6 -210Q66.8 -214.1 67.4 -216.1H175.5Q174.5 -211.2 174 -204.8Q173.5 -198.4 173.5 -189.1Q173.5 -152.5 204.2 -128Q234.9 -103.4 292.5 -91.2Q350.1 -79 428.5 -79Q464.9 -79 500.1 -82.4Q535.4 -85.8 566.3 -93.4Q597.2 -100.9 621 -113.3Q644.8 -125.8 658.4 -143.9Q672.1 -162 672.1 -185.9Q672.1 -219.8 647.7 -240.3Q623.2 -260.9 581.4 -273.6Q539.6 -286.4 487.5 -294.8Q435.4 -303.1 380.1 -311.6Q324.8 -320.1 272.7 -333.5Q220.6 -346.8 178.8 -368.2Q136.9 -389.6 112.5 -424.1Q88.1 -458.5 88.1 -510.1Q88.1 -551.2 107.9 -585.5Q127.7 -619.7 169.2 -645.2Q210.8 -670.7 275.4 -684.6Q339.9 -698.5 430 -698.5Q521.5 -698.5 584.1 -682.9Q646.8 -667.3 684.6 -640.6Q722.5 -613.9 739.1 -580.2Q755.7 -546.5 755.7 -510.6V-491.9H649V-510.4Q649 -536.5 623.9 -558.4Q598.7 -580.2 551.8 -593.9Q504.8 -607.5 438.5 -607.5Q358.3 -607.5 305.5 -596.2Q252.6 -584.9 226.4 -564.2Q200.1 -543.4 200.1 -514.3Q200.1 -484.8 224.5 -466.5Q248.9 -448.1 290.8 -436.9Q332.6 -425.6 384.7 -417.4Q436.7 -409.3 492.1 -400.3Q547.4 -391.3 599.5 -378Q651.5 -364.7 693.4 -342.3Q735.2 -320 759.6 -285.4Q784.1 -250.8 784.1 -199.7Q784.1 -121.6 739.4 -74.9Q694.8 -28.2 615.1 -8.1Q535.4 12 430.6 12Z M940.2 0V-53.5L1461.4 -594.5H972.2V-686.5H1645.3V-634.5L1121.7 -92.1H1652.1V0Z M1868.6 0V-686.5H1974.4L2404.6 -250.2Q2416.2 -239.6 2431.2 -223.4Q2446.2 -207.2 2461.8 -191.1Q2477.4 -175 2488.2 -162.2H2495.7Q2495.2 -181 2494.4 -209.7Q2493.6 -238.5 2493.6 -259.8V-686.5H2599.1V0H2499.1L2066.1 -440.8Q2042.7 -464.1 2018.3 -490.7Q1993.8 -517.4 1979.5 -532.7H1972.5Q1973.1 -516.1 1973.6 -484.5Q1974.1 -452.9 1974.1 -421V0Z M3095.4 0V-590.7H2784.8V-686.5H3514.7V-590.7H3204.2V0Z";
static const char *WORDMARK_SIGNAL = "M3614 -109 H3914 L3884 0 H3584 Z";

// ------------------------------------------------------------------ Direct2D / DirectWrite / WIC
static ID2D1Factory *g_d2d;
static IDWriteFactory *g_dw;
static IWICImagingFactory *g_wic;
static IDWriteFontCollection *g_fonts;
static bool g_brand_fonts = false;
static ID2D1HwndRenderTarget *g_rt;
static ID2D1SolidColorBrush *g_brush;
static ID2D1StrokeStyle *g_round;

static D2D1_COLOR_F rgb(unsigned c, float a = 1.0f)
{
    return D2D1::ColorF(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f, a);
}
static D2D1_COLOR_F mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t)
{
    return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
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

// ------------------------------------------------------------------ fonts
enum Font { F_WIDE, F_DISPLAY, F_SANS, F_SANS_M, F_SANS_SB, F_MONO, F_MONO_SB, F_COUNT };

static void load_brand_fonts()
{
    IDWriteFactory5 *f5 = nullptr;
    if (FAILED(g_dw->QueryInterface(__uuidof(IDWriteFactory5), (void **)&f5)) || !f5) return;
    IDWriteInMemoryFontFileLoader *loader = nullptr;
    IDWriteFontSetBuilder1 *builder = nullptr;
    if (SUCCEEDED(f5->CreateInMemoryFontFileLoader(&loader)) && SUCCEEDED(f5->RegisterFontFileLoader(loader)) &&
        SUCCEEDED(f5->CreateFontSetBuilder(&builder))) {
        int added = 0;
        for (int i = 0; i < IDR_FONT_COUNT; i++) {
            HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_FONT_FIRST + i), (LPCWSTR)RT_RCDATA);
            HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
            const void *p = g ? LockResource(g) : nullptr;
            IDWriteFontFile *file = nullptr;
            if (p && SUCCEEDED(loader->CreateInMemoryFontFileReference(f5, p, SizeofResource(nullptr, r), nullptr, &file))) {
                if (SUCCEEDED(builder->AddFontFile(file))) added++;
                file->Release();
            }
        }
        IDWriteFontSet *set = nullptr;
        IDWriteFontCollection1 *coll = nullptr;
        if (added == IDR_FONT_COUNT && SUCCEEDED(builder->CreateFontSet(&set)) && SUCCEEDED(f5->CreateFontCollectionFromFontSet(set, &coll))) {
            g_fonts = coll;
            g_brand_fonts = true;
        }
        if (set) set->Release();
    }
    if (builder) builder->Release();
    f5->Release();
}

static IDWriteTextFormat *format(Font f, float size)
{
    static std::map<std::pair<int, int>, IDWriteTextFormat *> cache;
    const auto key = std::make_pair((int)f, (int)(size * 10));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    static const struct { const wchar_t *family, *fallback; DWRITE_FONT_WEIGHT weight; } spec[F_COUNT] = {
        {L"SZNT Wide", L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL},
        {L"SZNT Display", L"Segoe UI Light", DWRITE_FONT_WEIGHT_NORMAL},
        {L"SZNT Sans", L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL},
        {L"SZNT Sans", L"Segoe UI", DWRITE_FONT_WEIGHT_MEDIUM},
        {L"SZNT Sans", L"Segoe UI", DWRITE_FONT_WEIGHT_SEMI_BOLD},
        {L"SZNT Mono", L"Consolas", DWRITE_FONT_WEIGHT_MEDIUM},
        {L"SZNT Mono", L"Consolas", DWRITE_FONT_WEIGHT_SEMI_BOLD},
    };
    IDWriteTextFormat *t = nullptr;
    g_dw->CreateTextFormat(g_brand_fonts ? spec[f].family : spec[f].fallback, g_brand_fonts ? g_fonts : nullptr, spec[f].weight,
                           DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &t);
    cache[key] = t;
    return t;
}

static std::wstring upper(std::wstring s)
{
    if (!s.empty()) CharUpperBuffW(&s[0], (DWORD)s.size());
    return s;
}

struct TextOpt {
    DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING;
    DWRITE_PARAGRAPH_ALIGNMENT valign = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    float track = 0;       // em
    float leading = 0;     // multiple of the size, 0 = font default
    bool nowrap = false, ellipsis = false;
};

static IDWriteTextLayout *layout(const std::wstring &s, Font f, float size, float w, float h, const TextOpt &o)
{
    IDWriteTextLayout *l = nullptr;
    if (FAILED(g_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), format(f, size), w, h, &l)) || !l) return nullptr;
    l->SetTextAlignment(o.align);
    l->SetParagraphAlignment(o.valign);
    if (o.nowrap || o.ellipsis) l->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    if (o.ellipsis) {
        IDWriteInlineObject *sign = nullptr;
        g_dw->CreateEllipsisTrimmingSign(format(f, size), &sign);
        const DWRITE_TRIMMING trim = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        l->SetTrimming(&trim, sign);
        if (sign) sign->Release();
    }
    if (o.leading > 0) l->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, size * o.leading, size * o.leading * 0.8f);
    if (o.track != 0) {
        IDWriteTextLayout1 *l1 = nullptr;
        if (SUCCEEDED(l->QueryInterface(__uuidof(IDWriteTextLayout1), (void **)&l1)) && l1) {
            l1->SetCharacterSpacing(0, o.track * size, 0, DWRITE_TEXT_RANGE{0, (UINT32)s.size()});
            l1->Release();
        }
    }
    return l;
}

static DWRITE_TEXT_METRICS text(const std::wstring &s, Font f, float size, D2D1_RECT_F r, D2D1_COLOR_F c, const TextOpt &o = TextOpt())
{
    DWRITE_TEXT_METRICS m = {};
    IDWriteTextLayout *l = layout(s, f, size, r.right - r.left, r.bottom - r.top, o);
    if (!l) return m;
    l->GetMetrics(&m);
    g_rt->DrawTextLayout(P(r.left, r.top), l, brush(c), D2D1_DRAW_TEXT_OPTIONS_NONE);
    l->Release();
    return m;
}

static DWRITE_TEXT_METRICS measure(const std::wstring &s, Font f, float size, float w, const TextOpt &o = TextOpt())
{
    DWRITE_TEXT_METRICS m = {};
    IDWriteTextLayout *l = layout(s, f, size, w, 4000, o);
    if (l) { l->GetMetrics(&m); l->Release(); }
    return m;
}

static TextOpt mid(DWRITE_TEXT_ALIGNMENT a = DWRITE_TEXT_ALIGNMENT_LEADING)
{
    TextOpt o;
    o.align = a;
    o.valign = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    return o;
}

static TextOpt mono_opt(DWRITE_TEXT_ALIGNMENT a = DWRITE_TEXT_ALIGNMENT_LEADING)
{
    TextOpt o = mid(a);
    o.track = 0.12f;
    o.nowrap = true;
    return o;
}

// ------------------------------------------------------------------ SVG paths (logo and icons)
static ID2D1PathGeometry *svg_path(const char *d)
{
    ID2D1PathGeometry *g = nullptr;
    ID2D1GeometrySink *s = nullptr;
    g_d2d->CreatePathGeometry(&g);
    g->Open(&s);
    s->SetFillMode(D2D1_FILL_MODE_WINDING);
    float cx = 0, cy = 0, sx = 0, sy = 0, lx = 0, ly = 0;
    char cmd = 0, prev = 0;
    bool open = false;
    const char *p = d;
    auto skip = [&]() { while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t') p++; };
    auto num = [&]() { skip(); char *e = nullptr; const float v = strtof(p, &e); p = e; return v; };
    auto flag = [&]() { skip(); const bool v = *p == '1'; p++; return v; };
    auto begin = [&](float x, float y) {
        if (open) s->EndFigure(D2D1_FIGURE_END_OPEN);
        s->BeginFigure(P(x, y), D2D1_FIGURE_BEGIN_FILLED);
        open = true;
        sx = x; sy = y;
    };
    while (true) {
        skip();
        if (!*p) break;
        if (isalpha((unsigned char)*p)) cmd = *p++;
        else if (cmd == 'M') cmd = 'L';
        else if (cmd == 'm') cmd = 'l';
        const bool rel = islower((unsigned char)cmd);
        const float ox = rel ? cx : 0, oy = rel ? cy : 0;
        switch (cmd) {
        case 'M': case 'm': { const float x = ox + num(), y = oy + num(); begin(x, y); cx = x; cy = y; break; }
        case 'L': case 'l': { cx = ox + num(); cy = oy + num(); s->AddLine(P(cx, cy)); break; }
        case 'H': case 'h': { cx = ox + num(); s->AddLine(P(cx, cy)); break; }
        case 'V': case 'v': { cy = oy + num(); s->AddLine(P(cx, cy)); break; }
        case 'C': case 'c': {
            const float x1 = ox + num(), y1 = oy + num(), x2 = ox + num(), y2 = oy + num(), x = ox + num(), y = oy + num();
            s->AddBezier(D2D1::BezierSegment(P(x1, y1), P(x2, y2), P(x, y)));
            lx = x2; ly = y2; cx = x; cy = y; break;
        }
        case 'S': case 's': {
            const bool chain = prev == 'C' || prev == 'c' || prev == 'S' || prev == 's';
            const float x1 = chain ? 2 * cx - lx : cx, y1 = chain ? 2 * cy - ly : cy;
            const float x2 = ox + num(), y2 = oy + num(), x = ox + num(), y = oy + num();
            s->AddBezier(D2D1::BezierSegment(P(x1, y1), P(x2, y2), P(x, y)));
            lx = x2; ly = y2; cx = x; cy = y; break;
        }
        case 'Q': case 'q': {
            const float x1 = ox + num(), y1 = oy + num(), x = ox + num(), y = oy + num();
            s->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(x1, y1), P(x, y)));
            lx = x1; ly = y1; cx = x; cy = y; break;
        }
        case 'T': case 't': {
            const bool chain = prev == 'Q' || prev == 'q' || prev == 'T' || prev == 't';
            const float x1 = chain ? 2 * cx - lx : cx, y1 = chain ? 2 * cy - ly : cy, x = ox + num(), y = oy + num();
            s->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(x1, y1), P(x, y)));
            lx = x1; ly = y1; cx = x; cy = y; break;
        }
        case 'A': case 'a': {
            const float rx = num(), ry = num(), rot = num();
            const bool large = flag(), sweep = flag();
            const float x = ox + num(), y = oy + num();
            s->AddArc(D2D1::ArcSegment(P(x, y), D2D1::SizeF(rx, ry), rot, sweep ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                       large ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
            cx = x; cy = y; break;
        }
        case 'Z': case 'z':
            if (open) s->EndFigure(D2D1_FIGURE_END_CLOSED);
            open = false;
            cx = sx; cy = sy;
            break;
        default: p++; break;
        }
        prev = cmd;
    }
    if (open) s->EndFigure(D2D1_FIGURE_END_OPEN);
    s->Close();
    s->Release();
    return g;
}

static ID2D1PathGeometry *cached_path(const std::string &d)
{
    static std::map<std::string, ID2D1PathGeometry *> cache;
    auto it = cache.find(d);
    if (it != cache.end()) return it->second;
    return cache[d] = svg_path(d.c_str());
}

static std::string circ(float cx, float cy, float r)
{
    char b[160];
    snprintf(b, sizeof(b), "M%g %ga%g %g 0 1 0 %g 0a%g %g 0 1 0 %g 0", cx - r, cy, r, r, 2 * r, r, r, -2 * r);
    return b;
}
static std::string quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    char b[200];
    snprintf(b, sizeof(b), "M%g %gL%g %gL%g %gL%g %gZ", x0, y0, x1, y1, x2, y2, x3, y3);
    return b;
}

// Same 24x24 line icons as the website (stroke 1.7, round caps).
static std::string icon_path(const std::string &n)
{
    if (n == "check") return "m5 12.5 4.5 4.5L19 7.5";
    if (n == "arrow") return "M5 12h14M13 6l6 6-6 6";
    if (n == "arrow-left") return "M19 12H5M11 6l-6 6 6 6";
    if (n == "x") return "M6 6l12 12M18 6 6 18";
    if (n == "minus") return "M6 12h12";
    if (n == "plus") return "M12 5v14M5 12h14";
    if (n == "download") return "M12 4v11M7 10.5l5 5 5-5M4.5 19.5h15";
    if (n == "truck") return "M2 6h11v10H2zM13 9h4.5L21 12.5V16h-8" + circ(6, 17.5f, 1.8f) + circ(17, 17.5f, 1.8f);
    return "";
}

static void icon(const char *name, float x, float y, float size, D2D1_COLOR_F c, float stroke = 1.7f)
{
    ID2D1PathGeometry *g = cached_path(icon_path(name));
    D2D1_MATRIX_3X2_F base;
    g_rt->GetTransform(&base);
    g_rt->SetTransform(D2D1::Matrix3x2F::Scale(size / 24, size / 24) * D2D1::Matrix3x2F::Translation(x, y) * base);
    g_rt->DrawGeometry(g, brush(c), stroke, g_round);
    g_rt->SetTransform(base);
}

static void fill_path(const std::string &d, const D2D1_MATRIX_3X2_F &m, D2D1_COLOR_F c)
{
    D2D1_MATRIX_3X2_F base;
    g_rt->GetTransform(&base);
    g_rt->SetTransform(m * base);
    g_rt->FillGeometry(cached_path(d), brush(c));
    g_rt->SetTransform(base);
}

static void logo(float x, float y, float h)
{
    const float s = h / 731.0f;
    const D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(x - 56 * s, y + 709 * s);
    fill_path(WORDMARK, m, rgb(col::INK));
    fill_path(WORDMARK_SIGNAL, m, rgb(col::SIGNAL));
}

// ------------------------------------------------------------------ gameplay clips (same footage as the website)
struct Clip {
    const unsigned char *data = nullptr;
    std::vector<std::pair<unsigned, unsigned>> frames;
    unsigned ms = 83;
    int cur = -1;
    ID2D1Bitmap *bmp = nullptr;
};
static Clip g_clip[2];

static void load_clip(int id, Clip &c)
{
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
    const unsigned char *p = g ? (const unsigned char *)LockResource(g) : nullptr;
    const DWORD size = r ? SizeofResource(nullptr, r) : 0;
    if (!p || size < 12 || memcmp(p, "SZCL", 4)) return;
    unsigned n, ms;
    memcpy(&n, p + 4, 4);
    memcpy(&ms, p + 8, 4);
    size_t off = 12;
    for (unsigned i = 0; i < n && off + 4 <= size; i++) {
        unsigned len;
        memcpy(&len, p + off, 4);
        if (off + 4 + len > size) break;
        c.frames.push_back({(unsigned)off + 4, len});
        off += 4 + len;
    }
    c.data = p;
    c.ms = ms ? ms : 83;
}

static ID2D1Bitmap *decode_jpeg(const unsigned char *p, unsigned len)
{
    if (!g_wic) return nullptr;
    IWICStream *stream = nullptr;
    IWICBitmapDecoder *dec = nullptr;
    IWICBitmapFrameDecode *frame = nullptr;
    IWICFormatConverter *conv = nullptr;
    ID2D1Bitmap *bmp = nullptr;
    if (SUCCEEDED(g_wic->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromMemory((BYTE *)p, len)) &&
        SUCCEEDED(g_wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(g_wic->CreateFormatConverter(&conv)) &&
        SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)))
        g_rt->CreateBitmapFromWicBitmap(conv, nullptr, &bmp);
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    if (stream) stream->Release();
    return bmp;
}

static void draw_clip(Clip &c, D2D1_RECT_F r, float radius, float dim = 0)
{
    fill_round(r, radius, rgb(col::ASPHALT));
    if (c.frames.empty()) return;
    const int idx = (int)((GetTickCount64() / c.ms) % c.frames.size());
    if (idx != c.cur || !c.bmp) {
        ID2D1Bitmap *b = decode_jpeg(c.data + c.frames[idx].first, c.frames[idx].second);
        if (b) {
            if (c.bmp) c.bmp->Release();
            c.bmp = b;
            c.cur = idx;
        }
    }
    if (!c.bmp) return;
    const D2D1_SIZE_F sz = c.bmp->GetSize();
    const float rw = r.right - r.left, rh = r.bottom - r.top, s = fmaxf(rw / sz.width, rh / sz.height);
    const float dw = sz.width * s, dh = sz.height * s;
    ID2D1BitmapBrush *bb = nullptr;
    if (SUCCEEDED(g_rt->CreateBitmapBrush(c.bmp, &bb)) && bb) {
        bb->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        bb->SetExtendModeX(D2D1_EXTEND_MODE_CLAMP);
        bb->SetExtendModeY(D2D1_EXTEND_MODE_CLAMP);
        bb->SetTransform(D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(r.left + (rw - dw) / 2, r.top + (rh - dh) / 2));
        bb->SetOpacity(g_opacity);
        g_rt->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), bb);
        bb->Release();
    }
    if (dim > 0) fill_round(r, radius, rgb(col::PAPER, dim));
    stroke_round(r, radius, rgb(col::LINE));
}

static void release_clip_bitmaps()
{
    for (auto &c : g_clip) { if (c.bmp) c.bmp->Release(); c.bmp = nullptr; c.cur = -1; }
}

// ------------------------------------------------------------------ interaction
struct Hit { int id; D2D1_RECT_F r; };
static std::vector<Hit> g_hits;
static std::map<int, float> g_anim;
static int g_hover = ID_NONE, g_pressed = ID_NONE;

static float hover_of(int id) { auto it = g_anim.find(id); return it == g_anim.end() ? 0.f : it->second; }
static void hit(int id, D2D1_RECT_F r) { g_hits.push_back({id, r}); }
static bool inside(D2D1_RECT_F r, float x, float y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }

// ------------------------------------------------------------------ components
static void eyebrow(float x, float y, const std::wstring &s)
{
    fill_path("M1.5 0H11L9.5 7H0Z", D2D1::Matrix3x2F::Translation(x, y + 4), rgb(col::SIGNAL));
    text(upper(s), F_MONO, 11.5f, R(x + 19, y - 2, 700, 18), rgb(col::INK2), mono_opt());
}

static void mod_label(float x, float y, bool drive, float size = 13)
{
    g_rt->FillEllipse(D2D1::Ellipse(P(x + 4, y + size * 0.62f), 4, 4), brush(mod_accent(drive)));
    TextOpt o;
    o.track = 0.04f;
    o.nowrap = true;
    text(drive ? L"SZNT DRIVE" : L"SZNT VIEW", F_WIDE, size, R(x + 16, y, 300, size * 1.3f), rgb(col::INK), o);
}

static float pill_width(const std::wstring &s, bool dot) { return measure(upper(s), F_MONO, 10.5f, 600, mono_opt()).widthIncludingTrailingWhitespace + (dot ? 34 : 22); }

static float pill(float x, float y, const std::wstring &s, unsigned dot = 0, bool right_align = false)
{
    const std::wstring u = upper(s);
    const float w = pill_width(s, dot != 0);
    if (right_align) x -= w;
    const D2D1_RECT_F r = R(x, y, w, 24);
    fill_round(r, 12, rgb(col::WHITE_SOFT));
    stroke_round(r, 12, rgb(col::LINE2));
    if (dot) g_rt->FillEllipse(D2D1::Ellipse(P(x + 14, y + 12), 3, 3), brush(dot));
    text(u, F_MONO, 10.5f, R(x + (dot ? 23 : 11), y, w, 24), rgb(col::INK2), mono_opt());
    return w;
}

enum BtnStyle { BTN_INK, BTN_ACCENT, BTN_GHOST };

static void button(int id, D2D1_RECT_F r, const std::wstring &label, BtnStyle st, const char *ic = "arrow", bool enabled = true, bool icon_left = false)
{
    const float h = enabled ? hover_of(id) : 0, a = enabled ? 1.f : 0.35f;
    D2D1_RECT_F rr = r;
    if (g_pressed == id && g_hover == id) { rr.top += 1; rr.bottom += 1; }
    D2D1_COLOR_F fg = rgb(0xFFFFFF, a);
    switch (st) {
    case BTN_INK:
        fill_round(rr, 14, mix(rgb(col::INK, a), rgb(col::INK2, a), h * 0.5f));
        fg = rgb(col::ON_INK, a);
        break;
    case BTN_ACCENT: fill_round(rr, 14, mix(rgb(accent(), a), rgb(accent_ink(), a), h * 0.3f)); break;
    case BTN_GHOST:
        fill_round(rr, 14, rgb(col::INK, 0.05f * h));
        stroke_round(rr, 14, rgb(col::LINE2, a));
        fg = rgb(col::INK, a);
        break;
    }
    const float iw = ic ? 18.f : 0.f, gap = ic ? 8.f : 0.f;
    const float tw = measure(label, F_SANS_M, 15, 600, mid()).widthIncludingTrailingWhitespace;
    float x = (rr.left + rr.right - (tw + iw + gap)) / 2;
    const float cy = (rr.top + rr.bottom) / 2;
    if (ic && icon_left) { icon(ic, x - h * 2, cy - 9, 18, fg, 1.9f); x += iw + gap; }
    text(label, F_SANS_M, 15, D2D1::RectF(x, rr.top, x + tw + 4, rr.bottom), fg, mid());
    if (ic && !icon_left) icon(ic, x + tw + gap + h * 2, cy - 9, 18, fg, 1.9f);
    if (enabled) hit(id, r);
}

static void checkbox(float x, float y, bool on, float hover, unsigned c = 0)
{
    const D2D1_RECT_F r = R(x, y, 22, 22);
    if (on) {
        fill_round(r, 7, rgb(c ? c : col::SIGNAL));
        icon("check", x + 2, y + 2, 18, rgb(0xFFFFFF), 2.4f);
    } else {
        fill_round(r, 7, rgb(col::WHITE_SOFT));
        stroke_round(r, 7, mix(rgb(col::LINE2), rgb(col::INK2), hover * 0.6f), 1.5f);
    }
}

static float keycap(float x, float y, const std::wstring &label)
{
    const float w = label.size() > 1 ? measure(label, F_MONO, 12, 400, mono_opt()).widthIncludingTrailingWhitespace + 18 : 30;
    fill_round(R(x, y, w, 30), 9, rgb(col::LINE2));
    fill_round(R(x, y, w, 27), 9, rgb(col::WHITE_SOFT));
    stroke_round(R(x, y, w, 27), 9, rgb(col::LINE2));
    TextOpt o = mid(DWRITE_TEXT_ALIGNMENT_CENTER);
    o.track = 0.02f;
    text(label, F_MONO, 12, R(x, y, w, 27), rgb(col::INK), o);
    return w;
}

static void grid_lines(D2D1_RECT_F r, unsigned c, float a, float step)
{
    for (float x = r.left + step; x < r.right; x += step) g_rt->DrawLine(P(x, r.top), P(x, r.bottom), brush(c, a), 1);
    for (float y = r.top + step; y < r.bottom; y += step) g_rt->DrawLine(P(r.left, y), P(r.right, y), brush(c, a), 1);
}

// ------------------------------------------------------------------ flags
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
    g_rt->FillGeometry(cached_path(quad(cx, r.top + h * 0.12f, r.right - w * 0.085f, cy, cx, r.bottom - h * 0.12f, r.left + w * 0.085f, cy)), brush(0xFEDF00));
    const float rr = h * 0.25f;
    g_rt->FillEllipse(D2D1::Ellipse(P(cx, cy), rr, rr), brush(0x002776));
    char buf[160];
    snprintf(buf, sizeof(buf), "M%g %gA%g %g 0 0 1 %g %g", cx - rr * 0.97f, cy + rr * 0.1f, rr * 1.9f, rr * 1.9f, cx + rr * 0.95f, cy + rr * 0.25f);
    g_rt->DrawGeometry(cached_path(buf), brush(0xFFFFFF), h * 0.045f);
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
    g_d2d->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r, 7, 7), &g);
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
    stroke_round(r, 7, rgb(0x000000, 0.15f));
}

// ------------------------------------------------------------------ pages
static void title(float y, const std::wstring &s, float size = 44, float w = CW)
{
    TextOpt o;
    o.track = -0.03f;
    o.leading = 1.04f;
    text(s, F_DISPLAY, size, R(PAD, y, w, size * 2.4f), rgb(col::INK), o);
}

static void subtitle(float y, const std::wstring &s, float w = CW * 0.82f) { text(s, F_SANS, 15, R(PAD, y, w, 48), rgb(col::MUTED)); }

static std::wstring status_text(size_t i, unsigned &c)
{
    if (game_running(g_games[i])) { c = col::ERR; return tr(txt::st_running); }
    const GameStatus &s = g_status[i];
    std::wstring v;
    if (HAS_DRIVE && HAS_VIEW && !s.drive.empty() && s.drive == s.view) v = s.drive;
    else if (HAS_DRIVE && !s.drive.empty()) v = L"Drive " + s.drive;
    if (HAS_VIEW && !s.view.empty() && v != s.view) v += (v.empty() ? L"" : L" · ") + std::wstring(L"View ") + s.view;
    if (!v.empty()) { c = col::OK; return std::wstring(tr(txt::st_installed)) + L" · " + v; }
    if (s.legacy) { c = col::SIGNAL; return tr(txt::st_old); }
    c = col::FAINT;
    return tr(txt::st_not_installed);
}

static void page_lang()
{
    eyebrow(PAD, 96, std::wstring(tr(txt::installer)) + L" · " + EDITION_NAME);
    title(118, tr(txt::choose_lang));
    const float cw = (CW - 16) / 2, ch = 96;
    for (int i = 0; i < LANG_COUNT; i++) {
        const float x = PAD + (i % 2) * (cw + 16), y = 214 + (i / 2) * (ch + 16);
        const D2D1_RECT_F r = R(x, y, cw, ch);
        const float h = hover_of(ID_LANG0 + i);
        const bool cur = g_lang == (Lang)i;
        fill_round(r, 22, mix(rgb(col::CARD), rgb(col::WHITE_SOFT), h));
        stroke_round(r, 22, cur ? rgb(col::SIGNAL) : mix(rgb(col::LINE), rgb(col::LINE2), h), cur ? 1.5f : 1.0f);
        draw_flag((Lang)i, R(x + 24, y + (ch - 40) / 2, 60, 40));
        text(tr(txt::lang_name[i]), F_SANS_M, 18, R(x + 104, y, cw - 160, ch), rgb(col::INK), mid());
        icon("arrow", r.right - 46 + h * 3, y + ch / 2 - 10, 20, mix(rgb(col::FAINT), rgb(col::INK), h), 1.8f);
        hit(ID_LANG0 + i, r);
    }
}

static void mod_choice(D2D1_RECT_F r, bool drive, int id)
{
    const bool on = drive ? g_want_drive : g_want_view;
    const float h = hover_of(id);
    fill_round(r, 24, mix(rgb(col::CARD), rgb(col::WHITE_SOFT), h * 0.6f));
    stroke_round(r, 24, on ? rgb(mod_accent(drive), 0.9f) : mix(rgb(col::LINE), rgb(col::LINE2), h), on ? 1.5f : 1.0f);
    const float x = r.left + 12, w = r.right - r.left - 24;
    const float vh = r.bottom - r.top - 140;
    draw_clip(g_clip[drive ? 0 : 1], R(x, r.top + 12, w, vh), 16, on ? 0.f : 0.6f);
    const float keep = g_opacity;
    g_opacity *= on ? 1.f : 0.5f;
    const float ly = r.top + 12 + vh + 18;
    mod_label(r.left + 22, ly, drive);
    TextOpt t;
    t.track = -0.02f;
    t.leading = 1.12f;
    text(tr(drive ? txt::drive_tag : txt::view_tag), F_DISPLAY, 22, R(r.left + 22, ly + 30, w - 30, 60), rgb(col::INK), t);
    g_opacity = keep;
    checkbox(r.right - 46, ly - 3, on, h, mod_accent(drive));
    hit(id, r);
}

static void page_mods()
{
    if (SZNT_EDITION == 0) {
        eyebrow(PAD, 92, tr(txt::step_mods));
        title(112, tr(txt::mods_title), 36);
        subtitle(158, tr(txt::mods_sub));
        const float cw = (CW - 16) / 2, top = 196;
        mod_choice(R(PAD, top, cw, FOOT_Y - top - 16), true, ID_TOGGLE_DRIVE);
        mod_choice(R(PAD + cw + 16, top, cw, FOOT_Y - top - 16), false, ID_TOGGLE_VIEW);
        return;
    }
    const bool drive = SZNT_EDITION == 1;
    draw_clip(g_clip[drive ? 0 : 1], R(PAD, 88, CW, 250), 24);
    mod_label(PAD, 360, drive, 14);
    pill(W - PAD, 358, std::wstring(L"Beta · v") + widen(SZNT_VERSION), 0, true);
    title(388, tr(drive ? txt::drive_tag : txt::view_tag), 32);
    const float h = hover_of(ID_LINK_OTHER);
    const std::wstring other = std::wstring(tr(txt::also)) + L" " + (drive ? L"SZNT View" : L"SZNT Drive") + L" · mods.sznt.dev";
    const float tw = measure(other, F_SANS_M, 13.5f, 700, mid()).widthIncludingTrailingWhitespace;
    g_rt->FillEllipse(D2D1::Ellipse(P(PAD + 4, 482), 4, 4), brush(drive ? col::COBALT : col::SIGNAL));
    text(other, F_SANS_M, 13.5f, R(PAD + 16, 472, tw + 4, 20), mix(rgb(col::MUTED), rgb(col::INK), h), mid());
    icon("arrow", PAD + 22 + tw + h * 2, 473, 17, mix(rgb(col::MUTED), rgb(col::INK), h), 1.8f);
    hit(ID_LINK_OTHER, R(PAD, 466, tw + 48, 28));
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

static void game_list(float y)
{
    if (g_games.empty()) {
        fill_round(R(PAD, y, CW, 72), 22, rgb(col::CARD));
        stroke_round(R(PAD, y, CW, 72), 22, rgb(col::LINE));
        icon("truck", PAD + 22, y + 24, 24, rgb(col::MUTED));
        text(tr(txt::no_games), F_SANS, 15, R(PAD + 60, y, CW - 80, 72), rgb(col::INK2), mid());
        y += 84;
    }
    for (size_t i = 0; i < g_games.size() && i < 3; i++) {
        const D2D1_RECT_F r = R(PAD, y, CW, 72);
        const float h = hover_of(ID_GAME0 + (int)i);
        fill_round(r, 22, mix(rgb(col::CARD), rgb(col::WHITE_SOFT), h));
        stroke_round(r, 22, g_games[i].selected ? rgb(col::SIGNAL, 0.9f) : mix(rgb(col::LINE), rgb(col::LINE2), h), g_games[i].selected ? 1.5f : 1.f);
        checkbox(PAD + 22, y + 25, g_games[i].selected, h);
        unsigned c;
        const std::wstring st = status_text(i, c);
        const float chip = pill_width(st, true);
        TextOpt e;
        e.ellipsis = true;
        text(g_games[i].name, F_SANS_M, 16, R(PAD + 62, y + 15, CW - chip - 100, 22), rgb(col::INK), e);
        e.track = 0.02f;
        text(g_games[i].root, F_MONO, 11, R(PAD + 62, y + 41, CW - chip - 100, 18), rgb(col::MUTED), e);
        pill(r.right - 20, y + 24, st, c, true);
        hit(ID_GAME0 + (int)i, r);
        y += 84;
    }
    const float h = hover_of(ID_ADD_FOLDER);
    icon("plus", PAD + 2, y + 6, 18, mix(rgb(col::INK2), rgb(col::INK), h), 1.9f);
    const std::wstring s = tr(txt::add_folder);
    const float tw = measure(s, F_SANS_M, 14, 600, mid()).widthIncludingTrailingWhitespace;
    text(s, F_SANS_M, 14, R(PAD + 28, y, tw + 4, 30), mix(rgb(col::INK2), rgb(col::INK), h), mid());
    hit(ID_ADD_FOLDER, R(PAD, y, tw + 40, 30));
}

static void page_games()
{
    eyebrow(PAD, 96, tr(txt::step_games));
    title(118, tr(txt::games_title), 40);
    subtitle(168, tr(txt::games_sub));
    game_list(212);
}

static float now_s() { return (float)(GetTickCount64() % 1000000) / 1000.0f; }

static void road_stripe(D2D1_RECT_F r, bool moving, unsigned c)
{
    fill_round(r, 3, rgb(col::LINE));
    g_rt->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    const float off = moving ? fmodf(now_s() * 90.0f, 48.0f) : 0, sy = (r.bottom - r.top) / 6;
    for (float x = r.left - 48 + off; x < r.right; x += 48)
        fill_path("M3 0H31L28 6H0Z", D2D1::Matrix3x2F::Scale(1, sy) * D2D1::Matrix3x2F::Translation(x, r.top), rgb(c));
    g_rt->PopAxisAlignedClip();
}

static void page_progress()
{
    const bool warn = g_finished && !g_result.ok;
    std::wstring t = g_busy ? std::wstring(tr(g_uninstall_mode ? txt::removing : txt::installing)) : tr(warn ? txt::done_warn_title : txt::done_title);
    if (g_finished && g_result.need_admin) t = tr(txt::admin_needed);
    if (g_busy) t += std::wstring(L"...").substr(0, 1 + ((int)(now_s() * 2.5f) % 3));
    eyebrow(PAD, 96, tr(g_uninstall_mode ? txt::uninstall_title : txt::step_install));
    title(118, t, 40);
    road_stripe(R(PAD, 178, CW, 6), g_busy, g_finished ? (warn ? col::SIGNAL : col::OK) : col::SIGNAL);

    const D2D1_RECT_F box = R(PAD, 202, CW, FOOT_Y - 202 - 16);
    fill_round(box, 22, rgb(col::ASPHALT));
    stroke_round(box, 22, rgb(col::LINE));
    std::vector<float> hs;
    float total = 0;
    for (auto &n : g_notes) {
        hs.push_back(measure(n.text, n.kind == NOTE_HEADER ? F_SANS_M : F_MONO, n.kind == NOTE_HEADER ? 14 : 12, CW - 70).height + (n.kind == NOTE_HEADER ? 12 : 8));
        total += hs.back();
    }
    const float room = box.bottom - box.top - 36;
    float y = box.top + 18 - (total > room ? total - room : 0);
    g_rt->PushAxisAlignedClip(D2D1::RectF(box.left, box.top + 10, box.right, box.bottom - 10), D2D1_ANTIALIAS_MODE_ALIASED);
    for (size_t i = 0; i < g_notes.size(); i++) {
        const Note &n = g_notes[i];
        if (n.kind == NOTE_HEADER) {
            text(n.text, F_SANS_M, 14, R(box.left + 22, y, CW - 44, hs[i]), rgb(0xECEBE6));
        } else {
            if (n.kind == NOTE_OK) icon("check", box.left + 22, y, 16, rgb(col::OK_DARK), 2.2f);
            else if (n.kind == NOTE_WARN || n.kind == NOTE_ERROR) {
                const unsigned c = n.kind == NOTE_WARN ? 0xFF7A42 : 0xFF6B6B;
                g_rt->FillEllipse(D2D1::Ellipse(P(box.left + 30, y + 8), 7, 7), brush(c, 0.22f));
                text(L"!", F_MONO_SB, 11, R(box.left + 23, y + 1, 14, 14), rgb(c), mid(DWRITE_TEXT_ALIGNMENT_CENTER));
            } else g_rt->FillEllipse(D2D1::Ellipse(P(box.left + 30, y + 8), 2.5f, 2.5f), brush(0xFFFFFF, 0.4f));
            text(n.text, F_MONO, 12, R(box.left + 48, y, CW - 70, hs[i]), rgb(n.kind == NOTE_ERROR ? 0xFFB3B3 : 0xC3C1BB));
        }
        y += hs[i];
    }
    g_rt->PopAxisAlignedClip();
}

static const Game *launchable()
{
    for (auto &g : g_games) if (g.selected && g.steam_app) return &g;
    return nullptr;
}

static void page_done()
{
    const float t = g_check_t, e = 1 - (1 - t) * (1 - t) * (1 - t);
    const bool removed = g_uninstall_mode;
    const float s = 48 * (0.85f + 0.15f * e);
    fill_round(R(PAD + (48 - s) / 2, 88 + (48 - s) / 2, s, s), 15, rgb(col::SIGNAL, e));
    if (t > 0.35f) icon("check", PAD + 11, 99, 26, rgb(0xFFFFFF, (t - 0.35f) / 0.65f), 2.4f);
    title(146, tr(removed ? txt::removed_title : txt::done_title), 40);
    subtitle(194, tr(removed ? txt::removed_sub : txt::done_sub));
    if (removed) return;

    const D2D1_RECT_F card = R(PAD, 250, CW, FOOT_Y - 250 - 16);
    fill_round(card, 24, rgb(col::CARD));
    stroke_round(card, 24, rgb(col::LINE));
    eyebrow(PAD + 26, 270, tr(txt::controls_title));
    text(tr(txt::controls_head), F_DISPLAY, 18, R(PAD + 26, 288, CW - 52, 28), rgb(col::INK));
    struct Row { std::vector<std::wstring> keys; const Str *action, *detail; };
    std::vector<Row> rows;
    if (g_want_drive) {
        rows.push_back({{L"A", L"D"}, &txt::ctl_steer, &txt::ctl_steer_d});
        rows.push_back({{L"W"}, &txt::ctl_thr, &txt::ctl_thr_d});
        rows.push_back({{L"S"}, &txt::ctl_brk, &txt::ctl_brk_d});
    }
    if (g_want_view) {
        rows.push_back({{L"Mouse"}, &txt::ctl_look, &txt::ctl_look_d});
        rows.push_back({{tr(txt::ctl_middle)}, &txt::ctl_zoom, &txt::ctl_zoom_d});
    }
    const float top = 326, rh = rows.size() > 3 ? 31.f : 40.f;
    for (size_t i = 0; i < rows.size(); i++) {
        const float y = top + i * rh;
        if (i) g_rt->FillRectangle(R(PAD + 26, y - 3, CW - 52, 1), brush(col::LINE));
        float kx = PAD + 26;
        for (auto &k : rows[i].keys) kx += keycap(kx, y, k) + 6;
        text(tr(*rows[i].action), F_SANS_M, 14, R(PAD + 190, y, 150, 28), rgb(col::INK), mid());
        text(tr(*rows[i].detail), F_SANS, 13, R(PAD + 330, y, CW - 356, 28), rgb(col::MUTED), mid());
    }
}

static void page_uninstall()
{
    eyebrow(PAD, 96, tr(txt::uninstall_title));
    title(118, EDITION_NAME, 40);
    subtitle(168, tr(txt::uninstall_sub));
    float y = 212;
    if (SZNT_EDITION == 0) {
        for (int i = 0; i < 2; i++) {
            const bool drive = i == 0, on = drive ? g_want_drive : g_want_view;
            const int id = drive ? ID_TOGGLE_DRIVE : ID_TOGGLE_VIEW;
            const D2D1_RECT_F r = R(PAD + i * (CW / 2 + 8), y, CW / 2 - 8, 60);
            const float h = hover_of(id);
            fill_round(r, 22, mix(rgb(col::CARD), rgb(col::WHITE_SOFT), h));
            stroke_round(r, 22, on ? rgb(mod_accent(drive), 0.9f) : rgb(col::LINE), on ? 1.5f : 1.f);
            mod_label(r.left + 22, r.top + 21, drive);
            checkbox(r.right - 44, r.top + 19, on, h, mod_accent(drive));
            hit(id, r);
        }
        y += 76;
    }
    game_list(y);
}

static void draw_header()
{
    g_rt->FillRectangle(R(0, 0, W, HEADER), brush(col::PAPER));
    g_rt->FillRectangle(R(0, HEADER - 1, W, 1), brush(col::LINE));
    logo(28, 24, 15);

    if (!g_uninstall_mode) {
        const Str *steps[4] = {&txt::step_lang, &txt::step_mods, &txt::step_games, &txt::step_install};
        const int cur = g_page == P_LANG ? 0 : g_page == P_MODS ? 1 : g_page == P_GAMES ? 2 : 3;
        float widths[4], total = 0;
        std::wstring labels[4];
        for (int i = 0; i < 4; i++) {
            wchar_t n[8];
            swprintf(n, 8, L"0%d  ", i + 1);
            labels[i] = upper(std::wstring(n) + tr(*steps[i]));
            widths[i] = measure(labels[i], F_MONO, 11, 400, mono_opt()).widthIncludingTrailingWhitespace;
            total += widths[i] + (i < 3 ? 36 : 0);
        }
        float x = (W - total) / 2 - 20;
        for (int i = 0; i < 4; i++) {
            const bool done = i < cur || g_page == P_DONE, active = i == cur && g_page != P_DONE;
            const unsigned c = active ? col::INK : done ? col::INK2 : col::FAINT;
            if (active) g_rt->FillRectangle(R(x, HEADER - 2, widths[i] - 8, 2), brush(col::SIGNAL));
            text(labels[i], F_MONO, 11, R(x, 20, widths[i] + 4, 24), rgb(c), mono_opt());
            x += widths[i];
            if (i < 3) { g_rt->FillRectangle(R(x + 8, 32, 14, 1), brush(col::LINE2)); x += 36; }
        }
    }

    if (!g_update.empty()) {
        const std::wstring s = upper(fmt(tr(txt::update_available), g_update.c_str()) + L" · " + tr(txt::btn_download));
        const float w = measure(s, F_MONO, 10.5f, 600, mono_opt()).widthIncludingTrailingWhitespace + 36, h = hover_of(ID_UPDATE);
        const D2D1_RECT_F r = R(W - 100 - w, 20, w, 24);
        fill_round(r, 12, mix(rgb(col::SIGNAL), rgb(col::SIGNAL_INK), h * 0.4f));
        icon("download", r.left + 10, r.top + 5, 14, rgb(0xFFFFFF), 2);
        text(s, F_MONO, 10.5f, R(r.left + 28, r.top, w, 24), rgb(0xFFFFFF), mono_opt());
        hit(ID_UPDATE, r);
    }

    for (int i = 0; i < 2; i++) {
        const int id = i == 0 ? ID_MIN : ID_CLOSE;
        const D2D1_RECT_F r = R(W - 86 + i * 40, 14, 36, 36);
        const float h = hover_of(id);
        if (h > 0.01f) fill_round(r, 10, id == ID_CLOSE ? rgb(col::SIGNAL, h) : rgb(col::INK, 0.08f * h));
        icon(id == ID_MIN ? "minus" : "x", r.left + 9, r.top + 9, 18, id == ID_CLOSE ? mix(rgb(col::INK2), rgb(0xFFFFFF), h) : rgb(col::INK2), 1.7f);
        hit(id, r);
    }
}

static void draw_footer()
{
    const float y = FOOT_Y + 10, bh = 46;
    switch (g_page) {
    case P_LANG: {
        const float h = hover_of(ID_LINK_HOME);
        text(L"MODS.SZNT.DEV", F_MONO, 11, R(PAD, y, 200, bh), mix(rgb(col::MUTED), rgb(col::INK), h), mono_opt());
        hit(ID_LINK_HOME, R(PAD, y + 12, 120, 24));
        text(L"v" + widen(SZNT_VERSION), F_MONO, 11, R(W - PAD - 200, y, 200, bh), rgb(col::FAINT), mono_opt(DWRITE_TEXT_ALIGNMENT_TRAILING));
        break;
    }
    case P_MODS:
        button(ID_BACK, R(PAD, y, 130, bh), tr(txt::btn_back), BTN_GHOST, "arrow-left", true, true);
        button(ID_NEXT, R(W - PAD - 190, y, 190, bh), tr(txt::btn_next), BTN_INK, "arrow", g_want_drive || g_want_view);
        break;
    case P_GAMES:
        button(ID_BACK, R(PAD, y, 130, bh), tr(txt::btn_back), BTN_GHOST, "arrow-left", true, true);
        button(ID_INSTALL, R(W - PAD - 210, y, 210, bh), tr(any_selected_installed() ? txt::btn_update : txt::btn_install), BTN_ACCENT, "download", any_selected());
        break;
    case P_PROGRESS:
        if (g_finished) {
            button(ID_BACK, R(PAD, y, 130, bh), tr(txt::btn_back), BTN_GHOST, "arrow-left", true, true);
            if (g_result.need_admin) button(ID_ADMIN, R(W - PAD - 290, y, 290, bh), tr(txt::btn_retry_admin), BTN_INK);
            else button(ID_CONTINUE, R(W - PAD - 190, y, 190, bh), tr(txt::btn_next), BTN_INK);
        }
        break;
    case P_DONE:
        if (!g_uninstall_mode && launchable()) button(ID_LAUNCH, R(PAD, y, 210, bh), tr(txt::btn_launch), BTN_GHOST, "truck", true, true);
        button(ID_FINISH, R(W - PAD - 190, y, 190, bh), tr(txt::btn_finish), BTN_INK, "check");
        break;
    case P_UNINSTALL:
        button(ID_UNINSTALL, R(W - PAD - 210, y, 210, bh), tr(txt::btn_uninstall), BTN_INK, "x", any_selected() && (g_want_drive || g_want_view));
        break;
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
    draw_footer();
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
    release_clip_bitmaps();
    if (g_brush) { g_brush->Release(); g_brush = nullptr; }
    if (g_rt) { g_rt->Release(); g_rt = nullptr; }
}

static void render()
{
    create_target();
    g_rt->BeginDraw();
    g_rt->Clear(rgb(col::PAPER));
    g_hits.clear();
    g_opacity = 1;
    grid_lines(R(0, HEADER, W, H - HEADER), col::INK, 0.035f, 56);
    const float e = 1 - (1 - g_page_t) * (1 - g_page_t) * (1 - g_page_t);
    g_opacity = e;
    g_rt->SetTransform(D2D1::Matrix3x2F::Translation(0, (1 - e) * 14));
    draw_page();
    g_rt->SetTransform(D2D1::Matrix3x2F::Identity());
    g_opacity = 1;
    draw_header();
    if (g_rt->EndDraw() == (HRESULT)D2DERR_RECREATE_TARGET) release_target();
}

static float anim_target(int id) { return id == g_hover ? 1.f : 0.f; }

static bool animating()
{
    if (g_page_t < 1 || g_busy || g_page == P_MODS || (g_page == P_DONE && g_check_t < 1)) return true;
    for (auto &kv : g_anim) if (fabsf(kv.second - anim_target(kv.first)) > 0.002f) return true;
    return false;
}

static void tick(float dt)
{
    g_page_t = fminf(1.f, g_page_t + dt / 0.32f);
    if (g_page == P_DONE) g_check_t = fminf(1.f, g_check_t + dt / 0.5f);
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

static void set_window_theme()
{
    const BOOL dark = g_dark;
    DwmSetWindowAttribute(g_wnd, 20, &dark, sizeof(dark));
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
    Sleep(500);
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
    Sleep(400);
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
    case ID_UPDATE: case ID_LINK_HOME: case ID_LINK_OTHER: open_url(SZNT_URL_HOME); break;
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
        if (dip(p.y) < HEADER && hit_at(dip(p.x), dip(p.y)) == ID_NONE) return HTCAPTION;
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
    case WM_SETTINGCHANGE:
        if (lp && !wcscmp((const wchar_t *)lp, L"ImmersiveColorSet") && windows_dark() != g_dark) {
            apply_theme(!g_dark);
            set_window_theme();
            InvalidateRect(h, nullptr, FALSE);
        }
        break;
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

// SZNT-Drive-View-Setup.exe --silent [--uninstall] [--only-drive|--only-view] [--lang en|pt|es|de] [--game "folder"]...
// Report in %TEMP%\sznt-setup.log.
static int run_silent()
{
    g_note = silent_note;
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
    int theme = -1;
    std::vector<std::wstring> only;
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; i++) {
        if (!wcscmp(argv[i], L"--silent")) silent = true;
        else if (!wcscmp(argv[i], L"--uninstall")) g_uninstall_mode = true;
        else if (!wcscmp(argv[i], L"--only-drive") && HAS_DRIVE) g_want_view = false;
        else if (!wcscmp(argv[i], L"--only-view") && HAS_VIEW) g_want_drive = false;
        else if (!wcscmp(argv[i], L"--game") && i + 1 < argc) only.push_back(argv[++i]);
        else if (!wcscmp(argv[i], L"--light")) theme = 0;
        else if (!wcscmp(argv[i], L"--dark")) theme = 1;
        else if (!wcscmp(argv[i], L"--lang") && i + 1 < argc) {
            const std::string c = narrow(argv[++i]);
            for (int k = 0; k < LANG_COUNT; k++) if (c == txt::lang_code[k]) g_lang = (Lang)k;
        }
    }
    if (argv) LocalFree(argv);
    g_games = find_games();
    if (!only.empty()) { g_games.clear(); for (auto &p : only) add_game(g_games, p); }
    if (silent) {
        const int rc = run_silent();
        CoUninitialize();
        return rc;
    }

    g_single = CreateMutexW(nullptr, TRUE, L"SZNT-Setup-Running");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(L"SZNTSetup", nullptr)) SetForegroundWindow(other);
        return 0;
    }

    apply_theme(theme < 0 ? windows_dark() : theme == 1);
    g_note = post_note;
    if (g_uninstall_mode)
        for (auto &g : g_games) {
            const GameStatus s = status_of(g);
            g.selected = (g_want_drive && !s.drive.empty()) || (g_want_view && !s.view.empty());
        }
    refresh_status();

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), nullptr, (void **)&g_d2d);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&g_dw);
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void **)&g_wic);
    load_brand_fonts();
    if (HAS_DRIVE) load_clip(IDR_CLIP_DRIVE, g_clip[0]);
    if (HAS_VIEW) load_clip(IDR_CLIP_VIEW, g_clip[1]);
    g_d2d->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND),
                             nullptr, 0, &g_round);

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
    set_window_theme();
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
