#include "pch.h"
#include "d3dhook.h"
#include "../depend/kiero/kiero.h"
#include "../depend/kiero/minhook/MinHook.h"
#include "../depend/imgui/imgui_impl_dx9.h"
#include "../depend/imgui/imgui_impl_dx11.h"
#include "../depend/imgui/imgui_impl_win32.h"
#include <dinput.h>

#define DIMOUSE ((LPDIRECTINPUTDEVICE8)(RsGlobal.ps->diMouse))
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool D3dHook::GetMouseState()
{
    return mouseShown;
}

void D3dHook::SetMouseState(bool state)
{
    mouseShown = state;
}

LRESULT D3dHook::hkWndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    if (ImGui::GetIO().WantTextInput)
    {
#ifdef GTASA
        Call<0x53F1E0>(); // CPad::ClearKeyboardHistory
#endif
        return 1;
    }

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT D3dHook::hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();

    return oReset(pDevice, pPresentationParameters);
}

void D3dHook::ProcessFrame(void* ptr)
{
    if (!ImGui::GetCurrentContext())
    {
        ImGui::CreateContext();
    }

    ImGuiIO& io = ImGui::GetIO();
    static bool init;
    if (init)
    {
        ProcessMouse();

        // Scale the menu if game resolution changed
        static ImVec2 fScreenSize = ImVec2(-1, -1);
        ImVec2 size(screen::GetScreenWidth(), screen::GetScreenHeight());
        if (fScreenSize.x != size.x && fScreenSize.y != size.y)
        {
            FontMgr::ReloadFonts();

            if (gRenderer == Render_DirectX9)
            {
                ImGui_ImplDX9_InvalidateDeviceObjects();
            }
            else
            {
                ImGui_ImplDX11_InvalidateDeviceObjects();
            }

            ImGuiStyle* style = &ImGui::GetStyle();
            float scaleX = size.x / 1366.0f;
            float scaleY = size.y / 768.0f;

            style->FramePadding = ImVec2(5 * scaleX, 5 * scaleY);
            style->ItemSpacing = ImVec2(8 * scaleX, 4 * scaleY);
            style->ScrollbarSize = 12 * scaleX;
            style->IndentSpacing = 20 * scaleX;
            style->ItemInnerSpacing = ImVec2(5 * scaleX, 5 * scaleY);

            fScreenSize = size;
        }

        ImGui_ImplWin32_NewFrame();
        if (gRenderer == Render_DirectX9)
        {
            ImGui_ImplDX9_NewFrame();
        }
        else
        {
            ImGui_ImplDX11_NewFrame();
        }

        ImGui::NewFrame();

        if (pCallbackFunc != nullptr)
        {
            ((void(*)())pCallbackFunc)();
        }

        ImGui::EndFrame();
        ImGui::Render();

        if (gRenderer == Render_DirectX9)
        {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }
        else
        {
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }
    else
    {
        init = true;
        ImGui_ImplWin32_Init(RsGlobal.ps->window);

#ifdef GTASA
        patch::Nop(0x00531155, 5); // shift trigger fix
#endif

        if (gRenderer == Render_DirectX9)
        {
            ImGui_ImplDX9_Init(reinterpret_cast<IDirect3DDevice9*>(ptr));
        }
        else
        {
            // for dx11 device ptr is swapchain
            reinterpret_cast<IDXGISwapChain*>(ptr)->GetDevice(__uuidof(ID3D11Device), &ptr);
            ID3D11DeviceContext* context;
            reinterpret_cast<ID3D11Device*>(ptr)->GetImmediateContext(&context);

            ImGui_ImplDX11_Init(reinterpret_cast<ID3D11Device*>(ptr), context);
        }

        ImGui_ImplWin32_EnableDpiAwareness();

        // Loading fonts
        io.FontDefault = FontMgr::LoadFont("text", 1.0f);
        FontMgr::LoadFont("title", 2.0f);
        FontMgr::LoadFont("header", 1.25f);

        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        oWndProc = (WNDPROC)SetWindowLongPtr(RsGlobal.ps->window, GWL_WNDPROC, (LRESULT)hkWndProc);
    }
}

HRESULT D3dHook::hkEndScene(IDirect3DDevice9* pDevice)
{
    ProcessFrame(pDevice);
    return oEndScene(pDevice);
}

HRESULT D3dHook::hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    ProcessFrame(pSwapChain);
    return oPresent(pSwapChain, SyncInterval, Flags);
}

void D3dHook::ProcessMouse()
{
    // Disable player controls for controllers
    bool bMouseDisabled = false;
    bool isController = patch::Get<BYTE>(BY_GAME(0xBA6818, 0x86968B, 0x5F03D8));

#ifdef GTA3
    isController =  !isController;
#endif

    if (isController && (mouseShown || bMouseDisabled))
    {

#ifdef GTASA
        CPlayerPed *player = FindPlayerPed();
        CPad *pad = player ? player->GetPadFromPlayer() : NULL;
#else
        CPad *pad = CPad::GetPad(0);
#endif

        if (pad)
        {
            if (mouseShown)
            {
                bMouseDisabled = true;
#ifdef GTA3
                pad->DisablePlayerControls = true;
#else
                pad->DisablePlayerControls = true;
#endif
            }
            else
            {
                bMouseDisabled = false;
#ifdef GTA3
                pad->DisablePlayerControls = false;
#else
                pad->DisablePlayerControls = false;
#endif
            }
        }
    }

    static bool mouseState;
    if (mouseState != mouseShown)
    {
        ImGui::GetIO().MouseDrawCursor = mouseShown;

        if (mouseShown)
        {

            patch::SetUChar(BY_GAME(0x6194A0, 0x6020A0, 0x580D20), 0xC3); // psSetMousePos
            patch::Nop(BY_GAME(0x541DD7, 0x4AB6CA, 0x49272F), 5); // don't call CPad::UpdateMouse()
        }
        else
        {

            patch::SetUChar(BY_GAME(0x6194A0, 0x6020A0, 0x580D20), BY_GAME(0xE9, 0x53, 0x53));
#ifdef GTASA
            patch::SetRaw(0x541DD7, (char*)"\xE8\xE4\xD5\xFF\xFF", 5);
#elif GTAVC
            patch::SetRaw(0x4AB6CA, (char*)"\xE8\x51\x21\x00\x00", 5);
#else
            patch::SetRaw(0x49272F, (char*)"\xE8\x6C\xF5\xFF\xFF", 5);
#endif
        }

        CPad::NewMouseControllerState.x = 0;
        CPad::NewMouseControllerState.y = 0;
#ifdef GTA3
        CPad::GetPad(0)->ClearMouseHistory();
#else
        CPad::ClearMouseHistory();
#endif
        CPad::UpdatePads();
        mouseState = mouseShown;
    }
}

bool D3dHook::InjectHook(void *pCallback)
{
    static bool hookInjected;
    if (hookInjected)
    {
        return false;
    }

    ImGui::CreateContext();

    /*
        Must check for d3d9 first!
        Seems to crash with nvidia geforce experience overlay
        if anything else is checked before d3d9
    */
    if (init(kiero::RenderType::D3D9) == kiero::Status::Success)
    {
        gRenderer = Render_DirectX9;
        kiero::bind(16, (void**)&oReset, hkReset);
        kiero::bind(42, (void**)&oEndScene, hkEndScene);
        pCallbackFunc = pCallback;
        hookInjected = true;
    }
    else
    {

        if (init(kiero::RenderType::D3D11) == kiero::Status::Success)
        {
            gRenderer = Render_DirectX11;
            kiero::bind(8, (void**)&oPresent, hkPresent);
            pCallbackFunc = pCallback;
            hookInjected = true;
        }
    }

    return hookInjected;
}

void D3dHook::RemoveHook()
{
    pCallbackFunc = nullptr;
    SetWindowLongPtr(RsGlobal.ps->window, GWL_WNDPROC, (LRESULT)oWndProc);
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    kiero::shutdown();
}