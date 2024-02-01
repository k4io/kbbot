#define IMGUI_DEFINE_MATH_OPERATORS

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include "imgui_settings.h"
#include "imgui_freetype.h"
#include <d3d11.h>
#include <D3DX11tex.h>
#pragma comment(lib, "D3DX11.lib")
#include <tchar.h>

#include "image.h"
#include "font.h"
#include "util.hpp"
#include <string>
#include <Windows.h>
#include <thread>
#include <iostream>

#include <codecvt>
#include <iomanip>
#include <tchar.h>
#include "implot/implot.h"
#include "implot/implot_internal.h"

struct nlog {
    int timestamp;
    std::string message;
};

std::vector<nlog> logs = { { (int)time(0), std::string("Initialized") } };

namespace image
{
    ID3D11ShaderResourceView* bg = nullptr;
    ID3D11ShaderResourceView* bg_blurred = nullptr;
    ID3D11ShaderResourceView* bg_decore = nullptr;

    ID3D11ShaderResourceView* logo = nullptr;
    ID3D11ShaderResourceView* logo_cheat = nullptr;

    ID3D11ShaderResourceView* arrow = nullptr;

    ID3D11ShaderResourceView* head = nullptr;
    ID3D11ShaderResourceView* eye = nullptr;
    ID3D11ShaderResourceView* pistol = nullptr;
    ID3D11ShaderResourceView* cog = nullptr;
    ID3D11ShaderResourceView* close = nullptr;

    ID3D11ShaderResourceView* linked = nullptr;
    ID3D11ShaderResourceView* unlinked = nullptr;
}

namespace font
{
    ImFont* poppins_medium = nullptr;
    ImFont* inter_bold_slider = nullptr;

    ImFont* inter_bold = nullptr;
}

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

DWORD picker_flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview;

static int gif_i = 1;
bool gif_state = false;
ID3D11ShaderResourceView* image_gif;

void getGifImage()
{
    const int gif_count = 11;

    D3DX11_IMAGE_LOAD_INFO iInfo;
    ID3DX11ThreadPump* threadPump{ nullptr };

    static ID3D11ShaderResourceView* gif[gif_count];

    std::string image_path;

    if (gif_state == false) {
        for (int i = 0; i <= gif_count; i++) {

            image_path = "C:\\gif\\gif (" + std::to_string(i) + ").jpg";

            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::wstring wide = converter.from_bytes(image_path);
            LPCWSTR result = wide.c_str();

            if (gif[i] == nullptr) D3DX11CreateShaderResourceViewFromFile(g_pd3dDevice, result, &iInfo, threadPump, &gif[i], 0);

            if (i <= (gif_count - 1))
                gif_state = true;
        }
    }

    if (gif_state == true)
    {

        static DWORD dwTickStart = GetTickCount();
        if (GetTickCount() - dwTickStart > 70)
        {

            if (gif_i >= (gif_count - 1)) gif_i = 1; else gif_i++;

            dwTickStart = GetTickCount();
        }

        image_gif = gif[gif_i];
    }
}

template <typename T>
int BinarySearch(const T* arr, int l, int r, T x) {
    if (r >= l) {
        int mid = l + (r - l) / 2;
        if (arr[mid] == x)
            return mid;
        if (arr[mid] > x)
            return BinarySearch(arr, l, mid - 1, x);
        return BinarySearch(arr, mid + 1, r, x);
    }
    return -1;
}

void PlotCandlestick(const char* label_id, const double* xs, const double* opens, const double* closes, const double* lows, const double* highs, int count, bool tooltip, float width_percent, ImVec4 bullCol, ImVec4 bearCol) {

    // get ImGui window DrawList
    ImDrawList* draw_list = ImPlot::GetPlotDrawList();
    // calc real value width
    double half_width = count > 1 ? (xs[1] - xs[0]) * width_percent : width_percent;

    // custom tool
    if (ImPlot::IsPlotHovered() && tooltip) {
        ImPlotPoint mouse = ImPlot::GetPlotMousePos();
        mouse.x = ImPlot::RoundTime(ImPlotTime::FromDouble(mouse.x), ImPlotTimeUnit_Day).ToDouble();
        float  tool_l = ImPlot::PlotToPixels(mouse.x - half_width * 1.5, mouse.y).x;
        float  tool_r = ImPlot::PlotToPixels(mouse.x + half_width * 1.5, mouse.y).x;
        float  tool_t = ImPlot::GetPlotPos().y;
        float  tool_b = tool_t + ImPlot::GetPlotSize().y;
        ImPlot::PushPlotClipRect();
        draw_list->AddRectFilled(ImVec2(tool_l, tool_t), ImVec2(tool_r, tool_b), IM_COL32(128, 128, 128, 64));
        ImPlot::PopPlotClipRect();
        // find mouse location index
        int idx = BinarySearch(xs, 0, count - 1, mouse.x);
        // render tool tip (won't be affected by plot clip rect)
        if (idx != -1) {
            im::BeginTooltip();
            char buff[32];
            ImPlot::FormatDate(ImPlotTime::FromDouble(xs[idx]), buff, 32, ImPlotDateFmt_DayMoYr, ImPlot::GetStyle().UseISO8601);
            im::Text("Day:   %s", buff);
            im::Text("Open:  $%.2f", opens[idx]);
            im::Text("Close: $%.2f", closes[idx]);
            im::Text("Low:   $%.2f", lows[idx]);
            im::Text("High:  $%.2f", highs[idx]);
            im::EndTooltip();
        }
    }

    // begin plot item
    if (ImPlot::BeginItem(label_id)) {
        // override legend icon color
        ImPlot::GetCurrentItem()->Color = IM_COL32(64, 64, 64, 255);
        // fit data if requested
        if (ImPlot::FitThisFrame()) {
            for (int i = 0; i < count; ++i) {
                ImPlot::FitPoint(ImPlotPoint(xs[i], lows[i]));
                ImPlot::FitPoint(ImPlotPoint(xs[i], highs[i]));
            }
        }
        // render data
        for (int i = 0; i < count; ++i) {
            ImVec2 open_pos = ImPlot::PlotToPixels(xs[i] - half_width, opens[i]);
            ImVec2 close_pos = ImPlot::PlotToPixels(xs[i] + half_width, closes[i]);
            ImVec2 low_pos = ImPlot::PlotToPixels(xs[i], lows[i]);
            ImVec2 high_pos = ImPlot::PlotToPixels(xs[i], highs[i]);
            ImU32 color = im::GetColorU32(opens[i] > closes[i] ? bearCol : bullCol);
            draw_list->AddLine(low_pos, high_pos, color);
            draw_list->AddRectFilled(open_pos, close_pos, color);
        }
        // end plot item
        ImPlot::EndItem();
    }
}

static std::vector<double> g_vdDates{};
static std::vector<double> g_vdOpens{};
static std::vector<double> g_vdCloses{};
static std::vector<double> g_vdLows{};
static std::vector<double> g_vdHighs{};

void UpdateChartVectors() {
    /*\
    * example data
    [
        1706796000000, //open time
        "42219.61000000", // open
        "42657.48000000", // high
        "42186.00000000", // low
        "42651.10000000", // close
        "2229.76128000", // volume
        1706799599999, // close time
        "94655481.18064480", // quote asset volume
        110160, // number of trades
        "1262.90750000", // taker buy base asset volume
        "53616609.99320370", // taker buy quote asset volume
        "0" // ignore
    ],
    [
        1706799600000,
        "42651.10000000",
        "42871.10000000",
        "42580.00000000",
        "42836.87000000",
        "1578.61738000",
        1706803199999,
        "67458218.66503330",
        78724,
        "756.99357000",
        "32347588.44046990",
        "0"
    ]
]                           
    */

    do {
        std::vector<double> dates{};
        std::vector<double> opens{};
        std::vector<double> closes{};
        std::vector<double> lows{};
        std::vector<double> highs{};

        nlohmann::json jsondata;

        std::string time_interval;
        switch (g_pUtil->m_settings.m_iChartInterval)
		{
        case 0: //15m
			time_interval = "15m";
			break;
        case 1: //1hr
			time_interval = "1h";
			break;
        case 2: //1day
			time_interval = "1d";
			break;
        }

        switch (g_pUtil->m_settings.m_iTradingPair)
        {
        case 0: // usdt/btc
            jsondata = g_pUtil->binance->getTradingPairInfo("BTCUSDT", time_interval);
            break;
        case 1: // usdt/ltc
            jsondata = g_pUtil->binance->getTradingPairInfo("LTCUSDT", time_interval);
            break;
        case 2: // usdt/ada
            jsondata = g_pUtil->binance->getTradingPairInfo("ADAUSDT", time_interval);
            break;
        case 3: // usdt/eth
            jsondata = g_pUtil->binance->getTradingPairInfo("ETHUSDT", time_interval);
            break;
        default:
            continue;
        }

        //auto s = jsondata.dump(4);
        //printf("%s\n", s.c_str());

        for (int i = 0; i < jsondata.size(); i++) {
            auto date = jsondata[i][0].get<double>();
            dates.push_back(date);

            auto open = jsondata[i][1].get<std::string>();
            opens.push_back(std::stod(open));

            const auto& high = jsondata[i][2].get<std::string>();
            highs.push_back(std::stod(high));

            const auto& low = jsondata[i][3].get<std::string>();
            lows.push_back(std::stod(low));

            const auto& close = jsondata[i][4].get<std::string>();
            closes.push_back(std::stod(close));
        }

        g_vdDates = dates;
        g_vdOpens = opens;
        g_vdCloses = closes;
        g_vdLows = lows;
        g_vdHighs = highs;

        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        //return;
    } while (1);
}

int main(int, char**)
{
    static int l_iScreenX = GetSystemMetrics(SM_CXSCREEN);
    static int l_iScreenY = GetSystemMetrics(SM_CYSCREEN);
    if (!GetAsyncKeyState(VK_LSHIFT))
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, /*LoadCursor(NULL, IDC_ARROW)*/ LoadCursor(0, IDC_ARROW), nullptr, nullptr, L"kbbot", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowEx(WS_EX_LAYERED, wc.lpszClassName, L"kbbot", WS_POPUP, 0, 0, l_iScreenX, l_iScreenY, nullptr, nullptr, wc.hInstance, nullptr);
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, ULW_COLORKEY);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    im::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = im::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImFontConfig cfg;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;

    font::poppins_medium = io.Fonts->AddFontFromMemoryTTF(poppins, sizeof(poppins), 18.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::inter_bold_slider = io.Fonts->AddFontFromMemoryTTF(inter_bold, sizeof(inter_bold), 13.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    font::inter_bold = io.Fonts->AddFontFromMemoryTTF(inter_bold, sizeof(inter_bold), 18.f, &cfg, io.Fonts->GetGlyphRangesCyrillic());

    D3DX11_IMAGE_LOAD_INFO info; ID3DX11ThreadPump* pump{ nullptr };
    //if (image::bg == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, background, sizeof(background), &info, pump, &image::bg, 0);
    if (image::bg_blurred == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, bg_blurred, sizeof(bg_blurred), &info, pump, &image::bg_blurred, 0);
    if (image::bg_decore == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, background_decore, sizeof(background_decore), &info, pump, &image::bg_decore, 0);

    if (image::logo_cheat == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo_cheat, sizeof(logo_cheat), &info, pump, &image::logo_cheat, 0);
    if (image::logo == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, logo, sizeof(logo), &info, pump, &image::logo, 0);
    if (image::arrow == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, arrow, sizeof(arrow), &info, pump, &image::arrow, 0);

    if (image::head == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, head_icon, sizeof(head_icon), &info, pump, &image::head, 0);
    if (image::eye == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, eye_icon, sizeof(eye_icon), &info, pump, &image::eye, 0);
    if (image::pistol == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, pistol_icon, sizeof(pistol_icon), &info, pump, &image::pistol, 0);
    if (image::cog == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, cog_icon, sizeof(cog_icon), &info, pump, &image::cog, 0);
    if (image::close == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, closebutton , sizeof(closebutton), &info, pump, &image::close, 0);

    if (image::linked == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, link, sizeof(link), &info, pump, &image::linked, 0);
    if (image::unlinked == nullptr) D3DX11CreateShaderResourceViewFromMemory(g_pd3dDevice, zunlink, sizeof(zunlink), &info, pump, &image::unlinked, 0);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);


    g_pUtil = new CUtil();


    bool done = false;
    while (!done)
    {

        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        im::NewFrame();
        {
            ImGuiContext& g = *GImGui;
            const ImGuiStyle& style = g.Style;

            getGifImage();

            static float accentcol[4] = { 196.f / 255.f, 156.f / 255.f, 29.f / 255.f, 1.f };
            c::accent_color = { accentcol[0], accentcol[1], accentcol[2], 1.f };
            c::checkbox::background_active = { accentcol[0], accentcol[1], accentcol[2], 0.5f };

            im::GetStyle().WindowPadding = ImVec2(0, 0);
            im::GetStyle().WindowBorderSize = 0;

            im::GetStyle().ScrollbarSize = 7.f;

            im::GetStyle().ItemSpacing = ImVec2(10, 10);

            //im::GetBackgroundDrawList()->AddImage(image::bg, ImVec2(0, 0), ImVec2(1920, 1080), ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32(c::other::background_color));

            im::SetNextWindowSizeConstraints(ImVec2(c::bg::size.x, c::bg::size.y), im::GetIO().DisplaySize);

            im::Begin("##kbbot", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            {
                const ImVec2& pos = im::GetWindowPos();
                const ImVec2& region = im::GetContentRegionMax();

                //im::GetWindowDrawList()->AddImage(image::bg_blurred, ImVec2(0, 0), ImVec2(1920, 1080), ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32(c::other::background_color));
                im::GetWindowDrawList()->AddRectFilled(pos, { pos + region }, im::GetColorU32(c::bg::background), c::bg::rounding);

                //im::GetWindowDrawList()->AddImage(image::bg_decore, pos, { pos + region }, ImVec2(0, 0), ImVec2(1, 1), ImColor(255, 255, 255, 255));

                im::GetWindowDrawList()->AddRectFilled(pos + style.ItemSpacing, pos + ImVec2(region.x - 200 - style.ItemSpacing.x * 2, 100), im::GetColorU32(c::child::background), c::child::rounding);
                im::GetWindowDrawList()->AddRectFilled(pos + ImVec2(region.x - style.ItemSpacing.x - 100, 0) + style.ItemSpacing, pos + ImVec2(region.x - style.ItemSpacing.x, 100), im::GetColorU32(c::child::background), c::child::rounding);

                im::GetWindowDrawList()->AddRectFilled(pos + ImVec2(region.x - style.ItemSpacing.x - (style.ItemSpacing.x / 2) - 200, 0) + style.ItemSpacing, pos + ImVec2(region.x - 100 - style.ItemSpacing.x - style.ItemSpacing.x / 2, 100), im::GetColorU32({ 0, 0, 0, 0 }), c::child::rounding);

                im::GetWindowDrawList()->AddRectFilled(pos + ImVec2(region.x - style.ItemSpacing.x - (style.ItemSpacing.x / 2) - 200, 0) + style.ItemSpacing, pos + ImVec2(region.x - 100 - style.ItemSpacing.x - style.ItemSpacing.x / 2, 100), im::GetColorU32(c::child::background), c::child::rounding);

                //im::GetWindowDrawList()->AddImage(image::close, pos + ImVec2(region.x - style.ItemSpacing.x - 100, 0) + ImVec2(30, 30) + style.ItemSpacing, pos + ImVec2(region.x - style.ItemSpacing.x, 100) - ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32(c::accent_color));

                im::SetCursorPos(ImVec2(region.x - style.ItemSpacing.x - 100, 0) + style.ItemSpacing + ImVec2(30, 30) + ImVec2(0, 0));

                if (im::ImageButton(image::close, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1))) {
                    done = true;
                }

                im::GetWindowDrawList()->AddImage(image::logo_cheat, pos + ImVec2(region.x - style.ItemSpacing.x - (style.ItemSpacing.x / 2) - 200, 0) + style.ItemSpacing + ImVec2(20, 20), pos + ImVec2(region.x - 100 - style.ItemSpacing.x - style.ItemSpacing.x / 2, 100) - ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32(c::accent_color));

                static int page = 0;

                im::SetCursorPos(ImVec2(style.ItemSpacing.x + 30, style.ItemSpacing.y));
                im::BeginGroup();
                {
                    if (im::Tab(1, 0 == page, image::cog, ImVec2(30, 85))) page = 0;
                    im::SameLine(0, 35);
                    if (im::Tab(2, 1 == page, image::eye, ImVec2(30, 85))) page = 1;
                    im::SameLine(0, 35);
                }
                im::EndGroup();

                static float tab_alpha = 0.f; /* */ static float tab_add; /* */ static int active_tab = 0;

                tab_alpha = ImClamp(tab_alpha + (4.f * im::GetIO().DeltaTime * (page == active_tab ? 1.f : -1.f)), 0.f, 1.f);
                tab_add = ImClamp(tab_add + (std::round(350.f) * im::GetIO().DeltaTime * (page == active_tab ? 1.f : -1.f)), 0.f, 1.f);

                if (tab_alpha == 0.f && tab_add == 0.f) active_tab = page;

                im::SetCursorPos(ImVec2(style.ItemSpacing) + ImVec2(0, 90 + style.ItemSpacing.y));

                im::PushStyleVar(ImGuiStyleVar_Alpha, tab_alpha * style.Alpha);

                if (active_tab == 0) {
                    im::BeginGroup();
                    {
                        im::BeginChild("SETTINGS", ImVec2(region.x / 3 - style.ItemSpacing.x - (style.ItemSpacing.x / 3), region.y - (90 + style.ItemSpacing.y * 3)), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                        {
                            im::GetStyle().ItemSpacing = ImVec2(10, 10);
                            if (im::Button(g_pUtil->m_bIsTrading ? "Force-stop" : "Start buy-in", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, 25)))
                                g_pUtil->m_bIsTrading = !g_pUtil->m_bIsTrading;

                            im::Checkbox("Auto-sell", &g_pUtil->m_settings.m_bAutoBuy);
                            im::Checkbox("Auto-buy", &g_pUtil->m_settings.m_bAutosell);
                            im::Combo("Buy Mode", &g_pUtil->m_settings.m_iBuyMode, "Spot\000Cross margin\000Isolated margin");
                            im::SliderFloat("Minimum profit", &g_pUtil->m_settings.m_fMinimumProfitMargin, 5.f, 500.f, "$%.0f");
                            //im::InputTextWithHint("Input buy-in", "Buy-in price..", input, 64, NULL);
                            im::Combo("Trading pair", &g_pUtil->m_settings.m_iTradingPair, "USDT/BTC\000USDT/LTC\000USDT/ADA\000USDT/ETH");
                            im::Combo("Chart intervals", &g_pUtil->m_settings.m_iTradingPair, "15m\0001hr\0001d");
                            im::Checkbox("Chart tooltip", &g_pUtil->m_settings.m_bChartTooltip);
                            im::ColorEdit4("Accent color", accentcol, picker_flags);
                            im::ColorEdit4("High color", g_pUtil->m_settings.m_fHighColor, picker_flags);
                            im::ColorEdit4("Low color", g_pUtil->m_settings.m_fLowColor, picker_flags);

                            if (im::Button("Load config", ImVec2(im::GetContentRegionMax().x / 2 - style.WindowPadding.x, 25)))
                                g_pUtil->LoadSettings();

                            im::SameLine();

                            if (im::Button("Save config", ImVec2(im::GetContentRegionMax().x / 2 - style.WindowPadding.x, 25)))
                                g_pUtil->SaveSettings();

                            im::SetCursorPosX(0);
                        }
                        im::EndChild();
                    }
                    im::EndGroup();
                    im::GetStyle().ItemSpacing = ImVec2(10, 10);
                    im::SameLine();
                    im::BeginGroup();
                    {
                        im::BeginChild("OUTPUT", ImVec2(region.x / 1.5f - style.ItemSpacing.x - (style.ItemSpacing.x / 1.5f), region.y - (90 + style.ItemSpacing.y * 3)));
                        {
                            im::BeginChild("LOG", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, im::GetContentRegionMax().y / 2.1f - style.WindowPadding.y / 2.1f), false, ImGuiWindowFlags_HorizontalScrollbar);
                            {
                                im::SetCursorPosY(im::GetCursorPosY() - 10);

                                static auto TimeFromStamp = [&](int timestamp) {
                                    time_t t = timestamp;
                                    tm* timePtr = localtime(&t);
                                    char buffer[80];
                                    strftime(buffer, 80, "%d.%m.%Y %H:%M:%S", timePtr);
                                    return buffer;
                                    };

                                im::GetStyle().ItemSpacing.y = 5;
                                for (int i = 0; i < logs.size(); i++) {
                                    im::TextColored(c::text::text_active, (std::string("[") + std::string(TimeFromStamp(logs[i].timestamp)) + std::string("] ") + logs[i].message).c_str());
                                }

                                im::GetStyle().ItemSpacing.y = 10;
                            }
                            im::EndChild();

                            const auto& image_min = pos + ImVec2(region.x - style.ItemSpacing.x - (style.ItemSpacing.x / 2) - 75, 0) + style.ItemSpacing + ImVec2(20, 105);
                            const auto& image_max = image_min + ImVec2(30, 30);
                            if (g_pUtil->m_bIsConnected) {
                                im::GetForegroundDrawList()->AddImage(image::linked, image_min, image_max, ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32({ 1, 1, 1, 1 }));
                            } else {
                                im::GetForegroundDrawList()->AddImage(image::unlinked, image_min, image_max, ImVec2(0, 0), ImVec2(1, 1), im::GetColorU32({ 1, 1, 1, 1 }));
                            }

                            auto tb4 = im::GetStyle().FramePadding;
                            im::GetStyle().FramePadding = ImVec2(5, 5);

                            im::BeginChild("CHART", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, im::GetContentRegionMax().y / 2.1f - style.WindowPadding.y / 2.1f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                            {
                                static bool g_bChartUpdateThreadCreated = false;
                                if (!g_bChartUpdateThreadCreated) {
                                    CreateThread(0,0,(LPTHREAD_START_ROUTINE)UpdateChartVectors,0,0,0);
                                    g_bChartUpdateThreadCreated = true;
                                }

                                const ImVec4 bullCol = ImVec4(g_pUtil->m_settings.m_fHighColor[0], g_pUtil->m_settings.m_fHighColor[1], g_pUtil->m_settings.m_fHighColor[2], 1.000f);
                                const ImVec4 bearCol = ImVec4(g_pUtil->m_settings.m_fLowColor[0], g_pUtil->m_settings.m_fLowColor[1], g_pUtil->m_settings.m_fLowColor[2], 1.000f);

                                ImPlot::GetStyle().UseLocalTime = false;
                                ImPlot::GetStyle().Use24HourClock = true;
                                ImPlot::GetStyle().LineWeight = 2.f;
                                ImPlot::GetStyle().MarkerWeight = 2.f;

                                im::SetCursorPosX(im::GetCursorPosX() - 20);
                                im::SetCursorPosY(im::GetCursorPosY() - 25);

                                im::PushStyleColor(ImGuiCol_PopupBg, c::child::background);
                                im::PushStyleColor(ImGuiCol_Text, c::text::text);
                                im::PushStyleColor(ImPlotCol_Crosshairs, c::accent_color);

                                im::PushStyleColor(ImPlotCol_AxisText, c::text::text_active);
                                im::PushStyleColor(ImPlotCol_LegendText, c::text::text_active);
                                im::PushStyleColor(ImPlotCol_TitleText, c::text::text_active);


                                const auto& region = im::GetContentRegionMax();

                                if (g_vdDates.size() > 49 && ImPlot::BeginPlot("###PRICEAS", ImVec2(region.x * 1.f, region.y + 70), ImPlotFlags_NoLegend)) {
                                    std::string y_axis_title = "Price";
                                    switch (g_pUtil->m_settings.m_iTradingPair) {
                                    case 0:
                                        y_axis_title = "USDT/BTC";
                                        break;
                                    case 1:
                                        y_axis_title = "USDT/LTC";
                                        break;
                                    case 2:
                                        y_axis_title = "USDT/ADA";
                                        break;
                                    case 3:
                                        y_axis_title = "USDT/ETH";
                                        break;
                                    }

                                    ImPlot::SetupAxes(nullptr, y_axis_title.c_str(), 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);

                                    const double x_axis_begin = g_vdDates[0];
                                    const double x_axis_limit = g_vdDates[g_vdDates.size() - 1];

                                    const double y_axis_begin = (*std::min_element(g_vdLows.begin(), g_vdLows.end())) - 5000.0;
                                    const double y_axis_limit = (*std::max_element(g_vdHighs.begin(), g_vdHighs.end())) + 5000.0;

                                    ImPlot::SetupAxesLimits(x_axis_begin, x_axis_limit, y_axis_begin, y_axis_limit);
                                    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, x_axis_begin, x_axis_limit);
                                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 60 * 60 * 24 * 14, x_axis_limit - ((x_axis_begin - x_axis_limit) * .25f));
                                    ImPlot::SetupAxisFormat(ImAxis_X1, (char*)nullptr);
                                    ImPlot::SetupAxisFormat(ImAxis_Y1, "$%.2f");

                                    ImPlot::SetupFinish();

                                    double dates[51] = { 0 };
                                    double opens[51] = { 0 };
                                    double closes[51] = { 0 };
                                    double lows[51] = { 0 };
                                    double highs[51] = { 0 };

                                    for (int i = 0; i < 50; i++) {
										dates[i] = g_vdDates[i];
										opens[i] = g_vdOpens[i];
										closes[i] = g_vdCloses[i];
										lows[i] = g_vdLows[i];
										highs[i] = g_vdHighs[i];
									}

                                    PlotCandlestick(y_axis_title.c_str(), dates, opens, closes, lows, highs, 51, g_pUtil->m_settings.m_bChartTooltip, .35f, bullCol, bearCol);
                                    ImPlot::EndPlot();
                                }

                                im::PopStyleColor();
                                im::PopStyleColor();
                                im::PopStyleColor();
                                im::PopStyleColor();
                                im::PopStyleColor();
                                im::PopStyleColor();
                            }
                            im::EndChild();

                            im::GetStyle().FramePadding = tb4;
                        }
                        im::EndChild();
                    }
                    im::EndGroup();
                }
                else if (active_tab == 1) {
                    im::BeginGroup();
                    {
                        im::BeginChild("ACCOUNT", ImVec2(region.x / 2.f - style.ItemSpacing.x - (style.ItemSpacing.x / 2.f), region.y - (90 + style.ItemSpacing.y * 3)));
                        {
                            im::InputTextWithHint("API-key", "API-key..", g_pUtil->m_settings.m_cApiKey, 256, NULL);
                            im::InputTextWithHint("Secret-key", "Secret-key..", g_pUtil->m_settings.m_cSecretKey, 256, NULL);
                            if (im::Button("Check keys", ImVec2(im::GetContentRegionMax().x / 2 - style.WindowPadding.x, 25))) {
                                const auto& account_code = g_pUtil->CheckBinanceKeys();
                                if (account_code) {
                                    MessageBoxA(0, "Successfully connected to Binance!", "Success", MB_OK | MB_ICONINFORMATION);

                                    g_pUtil->m_bIsConnected = true;
                                } else {
                                    char buffer[64];
                                    sprintf(buffer, "Failed to connect to Binance! Error code: %d", account_code);
                                    MessageBoxA(0, buffer, "Error", MB_OK | MB_ICONERROR);
                                }
                            }
                        }
                        im::EndChild();
                    }
                    im::EndGroup();

                    im::SameLine();

                    im::BeginGroup();
                    {
                        im::BeginChild("HELP", ImVec2(region.x / 2.f - style.ItemSpacing.x - (style.ItemSpacing.x / 2.f), region.y - (90 + style.ItemSpacing.y * 3)));
                        {

                        }
                        im::EndChild();
                    }
                    im::EndGroup();
                }


                static bool g_bStarted = false;


                if (g_pUtil->m_bIsTrading) {
                    std::string apikey = g_pUtil->m_settings.m_cApiKey;
                    std::string secretkey = g_pUtil->m_settings.m_cSecretKey;

                    if (apikey.empty()) {
                        logs.push_back({ (int)time(0), "API-key is empty!" });
                        g_pUtil->m_bIsTrading = false;
                    }
                    else if (secretkey.empty()) {
                        logs.push_back({ (int)time(0), "Secret key is empty!" });
                        g_pUtil->m_bIsTrading = false;
                    }
                    else if (!g_bStarted) {
                        logs.push_back({ (int)time(0), "Starting binance bot..." });
						g_bStarted = true;
                        logs.push_back({ (int)time(0), "Checking keys..." });

                        const auto& result = g_pUtil->CheckBinanceKeys();

                        if (result) {
                            logs.push_back({ (int)time(0), "Keys confirmed valid!" });
                            g_pUtil->m_bIsConnected = true;
                        }
                        else {
                            char buffer[128];
                            sprintf(buffer, "Failed to connect to Binance! Error code: %d", result);
                            logs.push_back({ (int)time(0), buffer });
							g_pUtil->m_bIsTrading = false;
                        }
                    }
                }
                else if (g_bStarted) {
					logs.push_back({ (int)time(0), "Stopping binance bot..." });
					g_bStarted = false;
                    g_pUtil->m_bIsConnected = false;
				}


                im::PopStyleVar();

            }
            im::End();
        }
        im::Render();
        const float clear_color_with_alpha[4] = { 0.f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(im::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    im::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}


bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
