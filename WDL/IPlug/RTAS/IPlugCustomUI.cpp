#if WINDOWS_VERSION
  #include <windows.h>
#endif

#include "EditorInterface.h"
#include "IPlugCustomUI.h"
#include "IPlugProcess.h"
#include "../IPlugRTAS.h"
#include "../IGraphics.h"
#include "../IControl.h"

IPlugCustomUI* CreateIPlugCustomUI(void *processPtr)
{
  return new IPlugCustomUI(processPtr);
}

#if MAC_VERSION

IPlugCustomUI::IPlugCustomUI(void *processPtr) : EditorInterface(processPtr), 
  mGraphics(0), mLocalWindow(0)
{
  mPlug=((IPlugProcess*)processPtr)->getPlug();
  mGraphics=((IPlugProcess*)processPtr)->getGraphics();
}

IPlugCustomUI::~IPlugCustomUI()
{
  mGraphics = 0;
}
  
void IPlugCustomUI::GetRect(short *left, short *top, short *right, short *bottom)
{
  *left = 0;
  *right = mGraphics->Width();
  *top = 0;
  *bottom = mGraphics->Height();
}

//#define TSSSetRect SetRect
//#undef SetRect
//void IPlugCustomUI::SetRect(short left, short top, short right, short bottom)
//#define SetRect TSSSetRect
//#undef TSSSetRect
//{
//  //mGraphics->Resize(right -left, bottom-top);
//}

bool IPlugCustomUI::Open(void *winPtr)
{
  mLocalWindow = (WindowRef) winPtr;

  if(mGraphics)
  {
    mGraphics->AttachSubWindow(winPtr);
    
    mPlug->OnGUIOpen();
  }

  return true;
}

bool IPlugCustomUI::Close()
{
  if(mLocalWindow && mGraphics)
  {
    mGraphics->RemoveSubWindow();
    mLocalWindow = 0;
  }
  return true;
}

void IPlugCustomUI::SetControlHighlight(long controlIndex, short isHighlighted, short color)
{
  // TODO: implement some highlight method in iplug base etc
}

void IPlugCustomUI::GetControlIndexFromPoint(long x, long y, long *aControlIndex)
{
  *aControlIndex = 0;
}

#pragma mark -
#pragma mark WINDOWS_VERSION

#elif WINDOWS_VERSION

#include <windows.h>

void *hInstance;

LRESULT CALLBACK IPlugMainWindow(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

IPlugCustomUI::IPlugCustomUI(void *processPtr) : EditorInterface(processPtr), 
  mGraphics(0), mHeaderHeight(0), mHeaderWidth(0),mPlugInWndHINST(0), mLocalPIWin(0), mLocalWindowID(0), mPluginWindow(NULL)
{
  if( mProcess )
    hInstance = ((ProcessInterface*)mProcess)->ProcessGetModuleHandle();  // necessary to load resources from dll

  mPlugInWndHINST = (HINSTANCE) hInstance;
  memset(&mLocalWinClass, 0, sizeof(WNDCLASSEX));

  mPlug=((IPlugProcess*)processPtr)->getPlug();
  mGraphics=((IPlugProcess*)processPtr)->getGraphics();

  Init();
}

IPlugCustomUI::~IPlugCustomUI()
{
  mGraphics = 0;
  mPlugInWndHINST = 0;
  mPluginWindow = NULL;
  mLocalPIWin = 0;
  mLocalWindowID = 0;
}

bool IPlugCustomUI::Init()
{ 
  mLocalWinClass.cbSize     = sizeof(WNDCLASSEX);
  mLocalWinClass.style      = CS_HREDRAW | CS_VREDRAW;
  mLocalWinClass.lpfnWndProc    = IPlugMainWindow;
  mLocalWinClass.hInstance    = mPlugInWndHINST;
  mLocalWinClass.hIcon      = LoadIcon (NULL, IDI_APPLICATION) ;
  mLocalWinClass.hCursor      = LoadCursor (NULL, IDC_ARROW) ;
  mLocalWinClass.lpszMenuName   = NULL ;
  mLocalWinClass.hbrBackground  = CreateSolidBrush(0);
  mLocalWinClass.lpszClassName  = "IPlugLocal";

  mLocalWindowID = RegisterClassEx(&mLocalWinClass);

  if(!mLocalWindowID)
  {
    DWORD err = GetLastError();
    if(err != ERROR_CLASS_ALREADY_EXISTS)
    {
      mPluginWindow = NULL;
      throw err;
    }
  }

  return true;
}

bool IPlugCustomUI::Open(void *winPtr)
{
  mPluginWindow = (HWND)winPtr;
  
  if(!mPluginWindow)
    return false;

  long PIHeight, PITop, PILeft, PIWidth, CLHeight;
  RECT clRect;

  PIHeight = mPIRect.bottom - mPIRect.top;
  GetClientRect(mPluginWindow, &clRect);

  CLHeight = clRect.bottom - clRect.top;

  mHeaderHeight = CLHeight - PIHeight;
  PITop = clRect.top + mHeaderHeight;
  PIHeight = clRect.bottom - mHeaderHeight;

  mHeaderWidth = clRect.right - clRect.left;
  PIWidth = mPIRect.right - mPIRect.left;
  if(PIWidth == mHeaderWidth)
    PILeft = mPIRect.left;
  else
    PILeft = (mHeaderWidth - PIWidth) / 2;

  // Create Window - starts message pump
  mLocalPIWin = CreateWindowEx(
    0,        // extended class style
    mLocalWinClass.lpszClassName, // class name 
    "IPlugCustomUI",        // window name 
    WS_CHILD | WS_VISIBLE,    // window style WS_EX_TRANSPARENT?
    PILeft,     // horizontal position of window
    PITop,      // vertical position of window
    PIWidth,    // window width
    PIHeight,   // window height
    mPluginWindow,  // handle to parent or owner window
    NULL,         // menu handle or child identifier
    NULL, // handle to application instance; ignored in NT/2000/XP
    NULL);  // window-creation data
    
  if(!mLocalPIWin)
  {
    DWORD err = GetLastError();
    mPluginWindow = NULL;
    mPlugInWndHINST = NULL;
    throw err;
    return false;
  }

  #ifdef ShowWindow
    #define tempShowWindow ShowWindow
    #undef ShowWindow
  #endif
      ShowWindow(mLocalPIWin, SW_SHOWNORMAL);
  #ifdef tempShowWindow
    #define ShowWindow tempShowWindow
    #undef tempShowWindow
  #endif

  InvalidateRect(mLocalPIWin,NULL,false);
  UpdateWindow(mLocalPIWin);

  short prevRes = 0;
  
  if(mProcess)
    prevRes = ((ProcessInterface*)mProcess)->ProcessUseResourceFile();

  if( mGraphics )
  {
    mGraphics->OpenWindow((void*)mLocalPIWin);
    mPlug->OnGUIOpen();
  }
    
  if(mProcess)
    ((ProcessInterface*)mProcess)->ProcessRestoreResourceFile(prevRes);

  return true;
}

void IPlugCustomUI::Draw(long left, long top, long right, long bottom)
{
  if (mGraphics)
  { 
    mGraphics->SetAllControlsDirty();
  }
}

bool IPlugCustomUI::Close()
{
  bool result = false;
  
  if( mLocalPIWin )
  {
      // OL - This is nessecary to avoid a collision in the PTSDK
  #ifdef CloseWindow
    #define tempCloseWindow CloseWindow
    #undef CloseWindow
  #endif

  if( mGraphics )
      mGraphics->CloseWindow();

  #ifdef CloseWindow
    #define CloseWindow tempCloseWindow
    #undef CloseWindow
  #endif

    result = DestroyWindow(mLocalPIWin);
  }
  
  return result;
}

void IPlugCustomUI::GetRect(short *left, short *top, short *right, short *bottom)
{
  *left = 0;
  *right = mGraphics->Width();
  *top = 0;
  *bottom = mGraphics->Height();

  mPIRect.left=*left; mPIRect.right=*right; mPIRect.top=*top; mPIRect.bottom=*bottom;
}

//void IPlugCustomUI::SetRect(short left, short top, short right, short bottom)
//{
  //mGraphics->Resize(right -left, bottom-top);
//}

long IPlugCustomUI::UpdateGraphicControl(long index, long value)
{
  return 0; //no error
}

void IPlugCustomUI::SetControlHighlight(long controlIndex, short isHighlighted, short color)
{
}

void IPlugCustomUI::GetControlIndexFromPoint(long x, long y, long *aControlIndex)
{
  *aControlIndex = 0;
}

#endif


