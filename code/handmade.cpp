#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

// TODO: This is a global for now
global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitmapWidth;
global_variable int BitmapHeight;
global_variable int BytesPerPixel = 4;

internal void RenderWeirdGradient(int BlueOffset, int GreenOffset)
{
    int Width = BitmapWidth;
    int Height = BitmapHeight;

    int Pitch = Width*BytesPerPixel; // Pitch means row to row distance in bytes in the bitmap
    uint8 *Row = (uint8 *)BitmapMemory;
    for(int Y = 0; Y < BitmapHeight; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = 0; X < BitmapWidth; ++X)
        {
            /*
                What we expect:
                Pixel in memory: RR GG BB xx
                0x xxBBGGRR *LITTLE ENDIAN ARCHITECTURE*

                What happens in Windows since they wanted to see RGB in Registers rather than BGR:
                Pixel in memory: BB GG RR xx
                0x xx RR GG BB
    
                If Pixel was uint8:
                *Pixel = (uint8)(X + XOffset);
                ++Pixel;
                
                *Pixel = (uint8)(Y + YOffset);
                ++Pixel;
                
                *Pixel = 0;
                ++Pixel;
                
                *Pixel = 0;
                ++Pixel;

            */

            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);
            
            /*
            
                Memory:     BB GG RR xx
                Register:   xx RR GG BB

                Blue (8-bits)           => (0-7 bits)
                Green (8-bits) << 8     => (8-15 bits)
                We get Pixel (32-bits)  => 0000GGBB
            */

            *Pixel++ = (Green << 8) | (Blue);
        }
        Row += Pitch;
    }
}

// DIB means device independent bitmap, so this functon is supposed to resize the bitmap that we will blit to the window
internal void Win32ResizeDIBSection(int Width, int Height)
{
    // TODO: Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    // Free the memory for the new allocation of different size
    if(BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapWidth = Width;
    BitmapHeight = Height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitmapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // Negative means bitmap will be top-down, origin at top-left
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32; // RGB + Padding for alignment
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    // NOTE: Thank you to Chris Hecker of Spy Pary fame for clarifying the deal with StretchDIBits and BitBlt!
    // No more DC for us! :)
    // x86 architecture is faster for DWORDS to be aligned in multiples of four
    int BitmapMemorySize = (BitmapWidth*BitmapHeight) * BytesPerPixel;
    // Page allocation!
    BitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    // TODO: Probably clear this to black
}

internal void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height,
        X, Y, Width, Height,
        */
        0, 0, BitmapWidth, BitmapHeight,
        0, 0, WindowWidth, WindowHeight,
        BitmapMemory,
        &BitmapInfo,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
            // Client rectangle is the rectangle of the portion of the window where we can draw, excluding the border and control buttons etc
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int Width = ClientRect.right - ClientRect.left;
            int Height = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(Width, Height);
        } break;
        
        case WM_CLOSE:
        {
            // TODO: Handle this with a message to the user?
            Running = false;
        } break;
        
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_DESTROY:
        {
            // TODO: Hand this as an error - recreate window?
            Running = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            //OutputDebugStringA("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    WNDCLASSA WindowClass = {};

    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero",
                                            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                            0, 0, Instance, 0);
        if(Window)
        {
            int XOffset = 0;
            int YOffset = 0;

            Running = true;
            while(Running)
            {
                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        Running = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                RenderWeirdGradient(XOffset, YOffset);

                HDC DeviceContext = GetDC(Window);
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                int WindowWidth = ClientRect.right - ClientRect.left;
                int WindowHeight = ClientRect.bottom - ClientRect.top;
                Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
                ReleaseDC(Window, DeviceContext);

                ++XOffset;
                YOffset += 2;
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