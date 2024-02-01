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


int main(int, char**)
{
    static int i_screen_x = GetSystemMetrics(SM_CXSCREEN);
    static int i_screen_y = GetSystemMetrics(SM_CYSCREEN);
    if (!GetAsyncKeyState(VK_LSHIFT))
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, /*LoadCursor(NULL, IDC_ARROW)*/ LoadCursor(0, IDC_ARROW), nullptr, nullptr, L"kbbot", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowEx(WS_EX_LAYERED, wc.lpszClassName, L"kbbot", WS_POPUP, 0, 0, i_screen_x, i_screen_y, nullptr, nullptr, wc.hInstance, nullptr);
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

            im::Begin("##kbbot", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
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
                        im::BeginChild("SETTINGS", ImVec2(region.x / 3 - style.ItemSpacing.x - (style.ItemSpacing.x / 3), region.y - (90 + style.ItemSpacing.y * 3)));
                        {
                            if (im::Button(g_pUtil->m_bIsTrading ? "Force-stop" : "Start buy-in", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, 25)))
                                g_pUtil->m_bIsTrading = !g_pUtil->m_bIsTrading;

                            im::Checkbox("Auto-sell", &g_pUtil->m_settings.m_bAutoBuy);
                            im::Checkbox("Auto-buy", &g_pUtil->m_settings.m_bAutosell);
                            im::Combo("Buy Mode", &g_pUtil->m_settings.m_iBuyMode, "Spot\000Cross margin\000Isolated margin");
                            im::SliderFloat("Minimum profit", &g_pUtil->m_settings.m_fMinimumProfitMargin, 5.f, 500.f, "$%.0f");
                            //im::InputTextWithHint("Input buy-in", "Buy-in price..", input, 64, NULL);
                            im::Combo("Trading pair", &g_pUtil->m_settings.m_iTradingPair, "USDT/BTC\000USDT/LTC\000USDT/ADA\000USDT/ETH");

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

                    im::SameLine();

                    im::BeginGroup();
                    {
                        im::BeginChild("OUTPUT", ImVec2(region.x / 1.5f - style.ItemSpacing.x - (style.ItemSpacing.x / 1.5f), region.y - (90 + style.ItemSpacing.y * 3)));
                        {
                            im::BeginChild("", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, im::GetContentRegionMax().y / 2.1f - style.WindowPadding.y / 2.1f), false, ImGuiWindowFlags_HorizontalScrollbar);
                            {
                                im::SetCursorPosY(im::GetCursorPosY() - 10);

                                static auto TimeFromStamp = [&](int timestamp) {
                                    time_t t = timestamp;
                                    tm* timePtr = localtime(&t);
                                    char buffer[80];
                                    strftime(buffer, 80, "%d.%m.%Y %H:%M:%S", timePtr);
                                    return buffer;
                                };

                                for (int i = 0; i < logs.size(); i++) {
                                    im::TextColored(c::text::text_active, (std::string("[") + std::string(TimeFromStamp(logs[i].timestamp)) + std::string("] ") + logs[i].message).c_str());
								}
							}
                            im::EndChild();
                            im::BeginChild(" ", ImVec2(im::GetContentRegionMax().x - style.WindowPadding.x, im::GetContentRegionMax().y / 2.1f - style.WindowPadding.y / 2.1f), false, ImGuiWindowFlags_NoScrollbar);
                            {
                                double dates[] = {
                                    1546300800, 1546387200, 1546473600, 1546560000, 1546819200, 1546905600, 1546992000, 1547078400, 1547164800, 1547424000,
                                    1547510400, 1547596800, 1547683200, 1547769600, 1547942400, 1548028800, 1548115200, 1548201600, 1548288000, 1548374400,
                                    1548633600, 1548720000, 1548806400, 1548892800, 1548979200, 1549238400, 1549324800, 1549411200, 1549497600, 1549584000,
                                    1549843200, 1549929600, 1550016000, 1550102400, 1550188800, 1550361600, 1550448000, 1550534400, 1550620800, 1550707200,
                                    1550793600, 1551052800, 1551139200, 1551225600, 1551312000, 1551398400, 1551657600, 1551744000, 1551830400, 1551916800
                                };

                                double opens[] = {
                                    1284.7, 1319.9, 1318.7, 1328, 1317.6, 1321.6, 1314.3, 1325, 1319.3, 1323.1,
                                    1324.7, 1321.3, 1323.5, 1322, 1281.3, 1281.95, 1311.1, 1315, 1314, 1313.1,
                                    1331.9, 1334.2, 1341.3, 1350.6, 1349.8, 1346.4, 1343.4, 1344.9, 1335.6, 1337.9,
                                    1342.5, 1337, 1338.6, 1337, 1340.4, 1324.65, 1324.35, 1349.5, 1371.3, 1367.9,
                                    1351.3, 1357.8, 1356.1, 1356, 1347.6, 1339.1, 1320.6, 1311.8, 1314, 1312.4
                                };

                                double highs[] = {
                                    1284.75, 1320.6, 1327, 1330.8, 1326.8, 1321.6, 1326, 1328, 1325.8, 1327.1,
                                    1326, 1326, 1323.5, 1322.1, 1282.7, 1282.95, 1315.8, 1316.3, 1314, 1333.2,
                                    1334.7, 1341.7, 1353.2, 1354.6, 1352.2, 1346.4, 1345.7, 1344.9, 1340.7, 1344.2,
                                    1342.7, 1342.1, 1345.2, 1342, 1350, 1324.95, 1330.75, 1369.6, 1374.3, 1368.4,
                                    1359.8, 1359, 1357, 1356, 1353.4, 1340.6, 1322.3, 1314.1, 1316.1, 1312.9
                                };

                                double lows[] = {
                                    1282.85, 1315, 1318.7, 1309.6, 1317.6, 1312.9, 1312.4, 1319.1, 1319, 1321,
                                    1318.1, 1321.3, 1319.9, 1312, 1280.5, 1276.15, 1308, 1309.9, 1308.5, 1312.3,
                                    1329.3, 1333.1, 1340.2, 1347, 1345.9, 1338, 1340.8, 1335, 1332, 1337.9, 1333,
                                    1336.8, 1333.2, 1329.9, 1340.4, 1323.85, 1324.05, 1349, 1366.3, 1351.2, 1349.1,
                                    1352.4, 1350.7, 1344.3, 1338.9, 1316.3, 1308.4, 1306.9, 1309.6, 1306.7, 1312.3
                                };

                                double closes[] = {
                                    1283.35, 1315.3, 1326.1, 1317.4, 1321.5, 1317.4, 1323.5, 1319.2, 1321.3, 1323.3,
                                    1319.7, 1325.1, 1323.6, 1313.8, 1282.05, 1279.05, 1314.2, 1315.2, 1310.8, 1329.1,
                                    1334.5, 1340.2, 1340.5, 1350, 1347.1, 1344.3, 1344.6, 1339.7, 1339.4, 1343.7,
                                    1337, 1338.9, 1340.1, 1338.7, 1346.8, 1324.25, 1329.55, 1369.6, 1372.5, 1352.4,
                                    1357.6, 1354.2, 1353.4, 1346, 1341, 1323.8, 1311.9, 1309.1, 1312.2, 1310.7
                                };

                                static ImVec4 bullCol = ImVec4(g_pUtil->m_settings.m_fHighColor[0], g_pUtil->m_settings.m_fHighColor[1], g_pUtil->m_settings.m_fHighColor[2], 1.000f);
                                static ImVec4 bearCol = ImVec4(g_pUtil->m_settings.m_fLowColor[0], g_pUtil->m_settings.m_fLowColor[1], g_pUtil->m_settings.m_fLowColor[2], 1.000f);
                                ImPlot::GetStyle().UseLocalTime = false;

                                im::SetCursorPosX(im::GetCursorPosX() - 100);
                                im::SetCursorPosY(im::GetCursorPosY() - 50);

                                im::PushStyleColor(ImGuiCol_PopupBg, c::child::background);
                                im::PushStyleColor(ImGuiCol_Text, c::text::text);
                                im::PushStyleColor(ImPlotCol_Crosshairs, c::accent_color);

                                const auto& region = im::GetContentRegionMax();

                                if (ImPlot::BeginPlot("Candlestick Chart", ImVec2(region.x * 1.3f, region.y + 70))) {
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
                                    ImPlot::SetupAxes("", y_axis_title.c_str(), 0, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                                    ImPlot::SetupAxesLimits(1546300800, 1551916800, 1250, 1600);
                                    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
                                    ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 1546300800, 1551916800);
                                    ImPlot::SetupAxisZoomConstraints(ImAxis_X1, 60 * 60 * 24 * 14, 1571961600 - 1546300800);
                                    ImPlot::SetupAxisFormat(ImAxis_Y1, "$%.0f");
                                    PlotCandlestick(y_axis_title.c_str(), dates, opens, closes, lows, highs, 50, g_pUtil->m_settings.m_bChartTooltip, .5f, bullCol, bearCol);
                                    ImPlot::EndPlot();
                                }

                                im::PopStyleColor();
                                im::PopStyleColor();
                                im::PopStyleColor();
							}
                            im::EndChild();
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
