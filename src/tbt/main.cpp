// ImGui - standalone example application for DirectX 9
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.

#include <imgui/imgui.h>
#include "imgui_impl_dx9.h"
#include <d3d9.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#include "config.h"

// Data
static LPDIRECT3DDEVICE9        g_pd3dDevice = NULL;
static D3DPRESENT_PARAMETERS    g_d3dpp;

extern LRESULT ImGui_ImplDX9_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT
WINAPI
WndProc ( HWND   hWnd,
          UINT   msg,
          WPARAM wParam,
          LPARAM lParam )
{
  if (ImGui_ImplDX9_WndProcHandler (hWnd, msg, wParam, lParam))
    return TRUE;

  switch (msg)
  {
  case WM_SIZE:
    if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
    {
      ImGui_ImplDX9_InvalidateDeviceObjects  ();
      g_d3dpp.BackBufferWidth       = LOWORD (lParam);
      g_d3dpp.BackBufferHeight      = HIWORD (lParam);

      HRESULT hr =
        g_pd3dDevice->Reset (&g_d3dpp);

      if (hr == D3DERR_INVALIDCALL)
        IM_ASSERT (0);

      ImGui_ImplDX9_CreateDeviceObjects ();
    }
    return 0;

  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
     break;

  case WM_DESTROY:
    PostQuitMessage (0);
    return           0;
  }

  return DefWindowProc (hWnd, msg, wParam, lParam);
}

#include <string>
#include <vector>


bool show_config         = true;
bool show_test_window    = true;
bool show_gamepad_config = false;

ImVec4 clear_col = ImColor(114, 144, 154);

struct {
  std::vector <const char*> array;
  int                       sel   = 0;
} gamepads;

void
TBFix_PauseGame (bool pause)
{
  extern HMODULE hInjectorDLL;

  typedef void (__stdcall *SK_SteamAPI_SetOverlayState_pfn)(bool active);

  static SK_SteamAPI_SetOverlayState_pfn SK_SteamAPI_SetOverlayState =
    (SK_SteamAPI_SetOverlayState_pfn)
      GetProcAddress ( hInjectorDLL,
                         "SK_SteamAPI_SetOverlayState" );

  SK_SteamAPI_SetOverlayState (pause);
}

int cursor_refs = 0;

void
TBFix_ToggleConfigUI (void)
{
  static int cursor_refs = 0;

  if (config.input.ui.visible) {
    while (ShowCursor (FALSE) > cursor_refs)
      ;
    while (ShowCursor (TRUE) < cursor_refs)
      ;
  }
  else {
    cursor_refs = (ShowCursor (FALSE) + 1);

    if (cursor_refs > 0) {
      while (ShowCursor (FALSE) > 0)
        ;
    }
  }

  config.input.ui.visible = (! config.input.ui.visible);

  if (config.input.ui.pause)
    TBFix_PauseGame (config.input.ui.visible);
}


void
TBFix_GamepadConfigDlg (void)
{
  if (gamepads.array.size () == 0)
  {
    if (GetFileAttributesA ("TBFix_Res\\Gamepads\\") != INVALID_FILE_ATTRIBUTES)
    {
      std::vector <std::string> gamepads_;

      WIN32_FIND_DATAA fd;
      HANDLE           hFind  = INVALID_HANDLE_VALUE;
      int              files  = 0;
      LARGE_INTEGER    liSize = { 0 };

      hFind = FindFirstFileA ("TBFix_Res\\Gamepads\\*", &fd);

      if (hFind != INVALID_HANDLE_VALUE)
      {
        do {
          if ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
               fd.cFileName[0]    != '.' )
          {
            gamepads_.push_back (fd.cFileName);
          }
        } while (FindNextFileA (hFind, &fd) != 0);

        FindClose (hFind);
      }

            char current_gamepad [128] = { '\0' };
      snprintf ( current_gamepad, 127,
                   "%ws",
                     config.input.gamepad.texture_set.c_str () );

      for (int i = 0; i < gamepads_.size (); i++)
      {
        gamepads.array.push_back (
          _strdup ( gamepads_ [i].c_str () )
        );

        if (! stricmp (gamepads.array [i], current_gamepad))
          gamepads.sel = i;
      }
    }
  }

  int orig_sel = gamepads.sel;

  ImGui::SetNextWindowSize (ImVec2 (225, 90), ImGuiSetCond_Appearing);

  ImGui::Begin             ("Gamepad Config");
  ImGui::ListBox           ("Gamepad\nIcons", &gamepads.sel, gamepads.array.data (), gamepads.array.size (), 3);
  ImGui::End               ();

  if (orig_sel != gamepads.sel)
  {
    wchar_t pad [128] = { L'\0' };

    swprintf (pad, L"%hs", gamepads.array [gamepads.sel]);

    config.input.gamepad.texture_set = pad;

    //extern void
    //TBFix_ReloadPadButtons (void);

    //TBFix_ReloadPadButtons ();
  }
}

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

void
TBFix_DrawConfigUI (LPDIRECT3DDEVICE9 pDev = nullptr)
{
  if (pDev != nullptr)
    g_pd3dDevice = pDev;

  ImGui_ImplDX9_NewFrame ();

  ImGui::SetNextWindowPos (ImVec2 (640, 360), ImGuiSetCond_FirstUseEver);

  // 1. Show a simple window
  // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
    ImGui::Begin ("Tales of Berseria \"Fix\" Control Panel", &show_config);

    ImGui::PushItemWidth (ImGui::GetWindowWidth () * 0.65f);

    if (ImGui::CollapsingHeader ("Framerate Control"))
    {
      ImGui::Checkbox ("Disable Namco's Limiter (requires restart)", &config.framerate.replace_limiter);

      static float values [120]  = { 0 };
      static int   values_offset = 0;
      static float refresh_time  = ImGui::GetTime ();

      values [values_offset] = 1000.0f * ImGui::GetIO ().DeltaTime;
      values_offset = (values_offset + 1) % IM_ARRAYSIZE (values);

      ImGui::PlotLines ( "Frametimes",
                           values,
                             IM_ARRAYSIZE (values),
                               values_offset,
                                 "Milliseconds per-frame",
                                   0.0f,
                                     33.0f,
                                       ImVec2 (0, 80) );

      ImGui::Text ( "Application average %.3f ms/frame (%.1f FPS)",
                      1000.0f / ImGui::GetIO ().Framerate,
                                ImGui::GetIO ().Framerate );
    }

    if (ImGui::CollapsingHeader ("Texture Options"))
    {
      ImGui::Checkbox ("Dump Textures",    &config.textures.dump);
      ImGui::Checkbox ("Generate Mipmaps", &config.textures.remaster);

      if (ImGui::IsItemHovered ())
        ImGui::SetTooltip ("Eliminates distant texture aliasing");

      if (config.textures.remaster) {
        ImGui::SameLine (150); 
        ImGui::Checkbox ("(Uncompressed)", &config.textures.uncompressed);

        if (ImGui::IsItemHovered ())
          ImGui::SetTooltip ("Uses more VRAM, but avoids texture compression artifacts on generated mipmaps.");
      }

      ImGui::SliderFloat ("LOD Bias", &config.textures.lod_bias, -3.0f, config.textures.uncompressed ? 16.0f : 3.0f);

      if (ImGui::IsItemHovered ())
      {
        ImGui::BeginTooltip ();
        ImGui::Text         ("Controls texture sharpness;  -3 = Sharpest (WILL shimmer),  0 = Neutral,  16 = Laughably blurry");
        ImGui::EndTooltip   ();
      }
    }

#if 0
    if (ImGui::CollapsingHeader ("Shader Options"))
    {
      ImGui::Checkbox ("Dump Shaders", &config.render.dump_shaders);
    }
#endif

    if (ImGui::CollapsingHeader ("Shadow Quality"))
    {
      struct shadow_imp_s
      {
        shadow_imp_s (int scale)
        {
          scale = std::abs (scale);

          if (scale > 3)
            scale = 3;

          radio = scale;
        }

        int radio    = 0;
        int last_sel = 0;
      };

      static shadow_imp_s shadows     (config.render.shadow_rescale);
      static shadow_imp_s env_shadows (config.render.env_shadow_rescale);

      ImGui::Combo ("Character Shadow Resolution",     &shadows.radio,     "Normal\0Enhanced\0High\0Ultra\0\0");
      ImGui::Combo ("Environmental Shadow Resolution", &env_shadows.radio, "Normal\0High\0Ultra\0\0");

      if (env_shadows.radio != env_shadows.last_sel) {
        config.render.env_shadow_rescale = env_shadows.radio;
        env_shadows.last_sel             = env_shadows.radio;
      }

      if (shadows.radio != shadows.last_sel) {
        config.render.shadow_rescale = -shadows.radio;
        shadows.last_sel             =  shadows.radio;
      }
    }

    if (ImGui::Button ("Gamepad Config"))
      show_gamepad_config ^= 1;

    ImGui::SameLine ();

    //if (ImGui::Button ("Special K Config"))
      //show_gamepad_config ^= 1;

    if ( ImGui::Checkbox ("Pause Game While This Menu Is Open", &config.input.ui.pause) )
      TBFix_PauseGame (config.input.ui.pause);

    ImGui::End ();

  if (show_gamepad_config)
  {
    TBFix_GamepadConfigDlg ();
  }

  //if (show_test_window)
  //{
    //ImGui::SetNextWindowPos (ImVec2 (650, 20), ImGuiSetCond_FirstUseEver);
    //ImGui::ShowTestWindow   (&show_test_window);
  //}

  // Rendering
  g_pd3dDevice->SetRenderState (D3DRS_ZENABLE,           false);
  g_pd3dDevice->SetRenderState (D3DRS_ALPHABLENDENABLE,  false);
  g_pd3dDevice->SetRenderState (D3DRS_SCISSORTESTENABLE, false);

  if (pDev == nullptr) {
    D3DCOLOR clear_col_dx =
      D3DCOLOR_RGBA ( (int)(clear_col.x * 255.0f),
                      (int)(clear_col.y * 255.0f),
                      (int)(clear_col.z * 255.0f),
                      (int)(clear_col.w * 255.0f) );

    g_pd3dDevice->Clear (
      0,
        NULL,
          D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
            clear_col_dx,
              1.0f,
                0 );

    
  }

  if (SUCCEEDED (g_pd3dDevice->BeginScene ())) {
    ImGui::Render          ();
    g_pd3dDevice->EndScene ();
  }
}

__declspec (dllexport)
void
CALLBACK
tbt_main (HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
  // Create application window
  WNDCLASSEX wc =
  { 
    sizeof (WNDCLASSEX),
    CS_CLASSDC,
    WndProc,
    0L, 0L,
    GetModuleHandle (nullptr),
    nullptr,
    LoadCursor      (nullptr, IDC_ARROW),
    nullptr, nullptr,
    L"Tales of Berseria Tweak",
    nullptr
  };

  RegisterClassEx (&wc);
  hwnd =
    CreateWindow ( L"Tales of Berseria Tweak", L"Tales of Berseria Tweak",
                   WS_OVERLAPPEDWINDOW,
                   100,  100,
                   1280, 800,
                   NULL, NULL,
                   wc.hInstance,
                   NULL );

  typedef IDirect3D9*
    (STDMETHODCALLTYPE *Direct3DCreate9_pfn)(UINT SDKVersion);

  extern HMODULE hInjectorDLL; // Handle to Special K

  hInjectorDLL = LoadLibrary (L"d3d9.dll");

  Direct3DCreate9_pfn SK_Direct3DCreate9 =
    (Direct3DCreate9_pfn)
      GetProcAddress (hInjectorDLL, "Direct3DCreate9");

  // Initialize Direct3D
  LPDIRECT3D9 pD3D;
  if ((pD3D = SK_Direct3DCreate9 (D3D_SDK_VERSION)) == NULL)
    UnregisterClass (L"Tales of Berseria Tweak", wc.hInstance);

  ZeroMemory (&g_d3dpp, sizeof (g_d3dpp));
  g_d3dpp.Windowed               = TRUE;
  g_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
  g_d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
  g_d3dpp.EnableAutoDepthStencil = TRUE;
  g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
  g_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

  // Create the D3DDevice
  if ( pD3D->CreateDevice ( D3DADAPTER_DEFAULT,
                              D3DDEVTYPE_HAL,
                                hwnd,
                                  D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &g_d3dpp,
                                      &g_pd3dDevice ) < 0 )
  {
    pD3D->Release   ();
    UnregisterClass (L"Tales of Berseria Tweak", wc.hInstance);
  }

  // Setup ImGui binding
  ImGui_ImplDX9_Init (hwnd, g_pd3dDevice);

  // Load Fonts
  // (there is a default font, this is only if you want to change it. see extra_fonts/README.txt for more details)
  //ImGuiIO& io = ImGui::GetIO();
  //io.Fonts->AddFontDefault();
  //io.Fonts->AddFontFromFileTTF("../../extra_fonts/Cousine-Regular.ttf", 15.0f);
  //io.Fonts->AddFontFromFileTTF("../../extra_fonts/DroidSans.ttf", 16.0f);
  //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyClean.ttf", 13.0f);
  //io.Fonts->AddFontFromFileTTF("../../extra_fonts/ProggyTiny.ttf", 10.0f);
  //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

  // Main loop
  MSG         msg;
  ZeroMemory (&msg, sizeof (msg));

  ShowWindow   (hwnd, SW_SHOWDEFAULT);
  UpdateWindow (hwnd);

  //TBF_LoadConfig();

  while (msg.message != WM_QUIT)
  {
    if (PeekMessage (&msg, NULL, 0U, 0U, PM_REMOVE))
    {
      TranslateMessage (&msg);
      DispatchMessage  (&msg);
      continue;
    }

    TBFix_DrawConfigUI ();

    g_pd3dDevice->Present (NULL, NULL, NULL, NULL);
  }

  ImGui_ImplDX9_Shutdown ();

  if (g_pd3dDevice)
    g_pd3dDevice->Release ();

  if (pD3D)
    pD3D->Release ();

  UnregisterClass (L"Tales of Berseria Tweak", wc.hInstance);
}