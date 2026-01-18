/*
  TODO: THIS IS NOT A FINAL PLATFORM LAYER!!!

  - Saved game locations
  - Getting a handle to our own executable file
  - Asset loading path
  - Threading (launch a thread)
  - Raw Input (support for multiple keyboards)
  - Sleep/timeBeginPeriod
  - ClipCursor() (for multimonitor support)
  - Fullscreen support
  - WM_SETCURSOR (control cursor visibility)
  - QueryCanceAutoplay
  - WM_ACTIVATEAPP (for whne we are not the active application)
  - Blit speed improvements
  - Hardware acceleration (OpenGL or Direct3d or BOTH??)
  - GetKeyboardLayout (for French keyboards, international WASD support)

  Just a partial list of stuff!!
*/
#include "handmade.cpp"

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <stdio.h>
// TODO: Implement sine ourselves
#include <math.h>

struct win32_offscreen_buffer
{
    // NOTE: Pixels are always 32-bits wide, Memory Order BB GG RR xx
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;    
};

// TODO: This is a global for now
global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// NOTE: XinputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_STATE *)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XinputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD, XINPUT_VIBRATION *)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) \
    HRESULT WINAPI name(LPCGUID, LPDIRECTSOUND *, LPUNKNOWN)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


internal void Win32LoadXInput(void)
{
    // TODO: Test this on Windows 8 (10 or 11 in my case)
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

    // Also try loading xinput9_1_0.dll?

    if(!XInputLibrary)
    {
        // TODO: Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary,
                                                             "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}
        
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary,
                                                             "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}
        
        // TODO: Diagnostic
    }
    else
    {
        // TODO: Diagnostic
    }
}

internal void
Win32InitDSound(HWND Window,
                int32 SamplesPerSecond,
                int32 BufferSize)
{
    // NOTE: Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
        // NOTE: Get a DirectSound object! - cooperative
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
                            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO: Double-check that this works on XP - DirectSound8 or 7??
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16; 
            WaveFormat.nBlockAlign =
                        (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec =
                        WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window,
                                                          DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                // NOTE: "Create" a primary buffer
                // TODO: DSBCAPS_GLOBAL_FOCUS?
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                            &PrimaryBuffer,
                                                            0)))
                {
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // NOTE: We have finally set the format!
                        printf("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO: Diagnostic
                    }
                }
            }
            else
            {
                // TODO: Diagnostic
            }

            // NOTE: "Create" a secondary buffer
            // TODO: DSBCAPS_GETCURRENTPOSITION2?
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription,
                                                        &GlobalSecondaryBuffer,
                                                        0)))
            {
                printf("Secondary buffer format was set succesfully.\n");
            }

            // NOTE: Start it playing!
        }
        else
        {
            // TODO: Diagnostic
        }
    
    }

}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    // Client rectangle is the rectangle of the portion of the window
    // where we can draw, excluding the border and control buttons etc
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

// DIB means device independent bitmap, so this functon is supposed
// to resize the bitmap that we will blit to the window
internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer,
                      int Width,
                      int Height)
{
    // TODO: Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    // Free the memory for the new allocation of different size
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    
    int BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    // Negative means bitmap will be top-down, origin at top-left
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32; // RGB + Padding for alignment
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE: Thank you to Chris Hecker of Spy Pary fame for clarifying the deal
    // with StretchDIBits and BitBlt!
    // No more DC for us! :)
    // x86 architecture is faster for DWORDS to be aligned in multiples of four
    int BitmapMemorySize = (Buffer->Width*Buffer->Height) * BytesPerPixel;
    // Page allocation!
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize,
                                  MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel;
    
    // TODO: Probably clear this to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext,
                           int WindowWidth,
                           int WindowHeight)
{
    // TODO: Aspect ratio correction
    // TODO: Play with stretch modes
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam, 
                        LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {        
        case WM_CLOSE:
        {
            // TODO: Handle this with a message to the user?
            GlobalRunning = false;
        } break;
        
        case WM_ACTIVATEAPP:
        {
            printf("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO: Hand this as an error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool32 WasDown = ((LParam & (1 << 30)) != 0);
            bool32 IsDown = ((LParam & (1 << 31)) == 0);

            if (WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                }
                else if(VKCode == 'A')
                {
                }
                else if(VKCode == 'S')
                {
                }
                else if(VKCode == 'D')
                {
                }
                else if(VKCode == 'Q')
                {
                }
                else if(VKCode == 'E')
                {
                }
                else if(VKCode == VK_UP)
                {
                }
                else if(VKCode == VK_DOWN)
                {
                }
                else if(VKCode == VK_LEFT)
                {
                }
                else if(VKCode == VK_RIGHT)
                {
                }
                else if(VKCode == VK_ESCAPE)
                {
                    printf("ESCAPE: ");
                    if(IsDown)
                    {
                        printf("IsDown");
                    }
                    if(WasDown)
                    {
                        printf("WasDown");
                    }
                    printf("\n");
                }
                else if(VKCode == VK_SPACE)
                {
                }
            }

            bool32 AltKeyWasDown = LParam & (1 << 29);
            if(VKCode == VK_F4 && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
            
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                        Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            //printf("default\n");
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    int16 ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};

internal void
Win32FillSoundBuffer(win32_sound_output* SoundOutput,
                     DWORD ByteToLock,
                     DWORD BytesToWrite)
{
    // TODO: More strenuous test!
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                            &Region1, &Region1Size,
                                            &Region2, &Region2Size,
                                            0)))
    {
        // TODO: assert that Region1Size/Region2Size is valid

        // TODO: Collapse these two loops
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *SampleOut = (int16 *)Region1;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            // TODO: Draw this out for people
            real32 SineValue = sinf(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

            ++SoundOutput->RunningSampleIndex;
            SoundOutput->tSine += 2.0f*Pi32*1.0f/(real32)SoundOutput->ToneHz;
        }

        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        SampleOut = (int16 *)Region2;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            real32 SineValue = sinf(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

            ++SoundOutput->RunningSampleIndex;
            SoundOutput->tSine += 2.0f*Pi32*1.0f/(real32)SoundOutput->ToneHz;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32LoadXInput();

    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0, WindowClass.lpszClassName,
                                     "Handmade Hero",
                                     WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     CW_USEDEFAULT, CW_USEDEFAULT,
                                     0, 0, Instance, 0);
        if(Window)
        {
            // NOTE: Since we specified CS_OWNDC, we can just get one device
            // context and use it forever because we're not sharing it with anyone.
            HDC DeviceContext = GetDC(Window);

            // NOTE: Graphics test
            int XOffset = 0;
            int YOffset = 0;

            // NOTE: Sound test
            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.WavePeriod =
                    SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize =
                    SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond,
                            SoundOutput.SecondaryBufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0,
                            SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            uint64 LastCycleCount = __rdtsc();
            while(GlobalRunning)
            {
                MSG Message;
                
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                // TOOO: Should we poll this more frequently
                for(DWORD ControllerIndex = 0;
                    ControllerIndex < XUSER_MAX_COUNT;
                    ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState)
                                                            == ERROR_SUCCESS)
                    {
                        // NOTE: This controller is plugged in
                        // TODO: See if ControllerState.dwPacketNumber
                        //       increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool32 Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool32 Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool32 LeftShoulder = (Pad->wButtons
                                            & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool32 RightShoulder= (Pad->wButtons
                                            & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool32 AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool32 BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool32 XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool32 YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;

                        // TODO: Proper deadzone handling later using
                        // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                        // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

                        XOffset += StickX / 4096;
                        YOffset += StickY / 4096;

                        SoundOutput.ToneHz = 512
                                + (int)(256.0f*((real32)StickY/30000.0f));
                        SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond
                                / SoundOutput.ToneHz;
                    }
                    else
                    {
                        // NOTE: The controller is not available
                    }
                }

                game_offscreen_buffer Buffer = {};
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.Width = GlobalBackBuffer.Width;
                Buffer.Height = GlobalBackBuffer.Height;
                Buffer.Pitch = GlobalBackBuffer.Pitch;
                GameUpdateAndRender(&Buffer, XOffset, YOffset++);

                // NOTE: DirectSound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if(SUCCEEDED(GlobalSecondaryBuffer->
                                GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock =
                        ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample)
                        % SoundOutput.SecondaryBufferSize);
                    DWORD TargetCursor =
                        (PlayCursor + SoundOutput.LatencySampleCount
                         * SoundOutput.BytesPerSample)
                        % SoundOutput.SecondaryBufferSize;
                    DWORD BytesToWrite;

                    if(ByteToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
                                            Dimension.Width, Dimension.Height);

                uint64 EndCycleCount = __rdtsc();
                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);

                uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                real32 MSPerFrame = (real32)((1000.0f*CounterElapsed) / PerfCountFrequency);
                real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
                real32 MCPF = (real32)(CyclesElapsed / (1000.0f * 1000.0f));
#if 0
                printf("%.2fms/f,  %.2ff/s, %.2fMc/f\n", MSPerFrame, FPS, MCPF); // w[n]sprintf
#endif           
                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;
            }
        }
        else
        {
            // TODO: logging
        }
    }
    else
    {
        // TODO: logging
    }

    return(0);
}
