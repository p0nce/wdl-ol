#include "IGraphicsWin.h"
#include "IControl.h"
#include "Log.h"
#include <wininet.h>
#include <Shlobj.h>

#ifdef RTAS_API
#include "PlugInUtils.h"
#endif

#pragma warning(disable:4244) // Pointer size cast mismatch.
#pragma warning(disable:4312) // Pointer size cast mismatch.
#pragma warning(disable:4311) // Pointer size cast mismatch.

static int nWndClassReg = 0;
static const char* wndClassName = "IPlugWndClass";
static double sFPS = 0.0;

#define PARAM_EDIT_ID 99

enum EParamEditMsg {
  kNone,
  kEditing,
  kUpdate,
  kCancel,
  kCommit
};

#define IPLUG_TIMER_ID 2

#ifdef RTAS_API
inline IMouseMod GetMouseMod(WPARAM wParam)
{
  return IMouseMod((wParam & MK_LBUTTON), (wParam & MK_RBUTTON), 
        (wParam & MK_SHIFT), (wParam & MK_CONTROL), IsOptionKeyDown());
}
#else
inline IMouseMod GetMouseMod(WPARAM wParam)
{
  return IMouseMod((wParam & MK_LBUTTON), (wParam & MK_RBUTTON), 
        (wParam & MK_SHIFT), (wParam & MK_CONTROL), GetKeyState(VK_MENU) < 0);
}
#endif
// static
LRESULT CALLBACK IGraphicsWin::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_CREATE) {
    LPCREATESTRUCT lpcs = (LPCREATESTRUCT) lParam;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LPARAM) (lpcs->lpCreateParams));
    int mSec = int(1000.0 / sFPS);
    SetTimer(hWnd, IPLUG_TIMER_ID, mSec, NULL);
    SetFocus(hWnd); // gets scroll wheel working straight away
    return 0;
  }

  IGraphicsWin* pGraphics = (IGraphicsWin*) GetWindowLongPtr(hWnd, GWLP_USERDATA);
  char txt[MAX_PARAM_LEN];
  double v;

  if (!pGraphics || hWnd != pGraphics->mPlugWnd) {
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }
  if (pGraphics->mParamEditWnd && pGraphics->mParamEditMsg == kEditing) {
    if (msg == WM_RBUTTONDOWN || (msg == WM_LBUTTONDOWN)) {
      pGraphics->mParamEditMsg = kCancel;
      return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
  }

  switch (msg) {

    case WM_TIMER: {
      if (wParam == IPLUG_TIMER_ID) {

        if (pGraphics->mParamEditWnd && pGraphics->mParamEditMsg != kNone) {
          switch (pGraphics->mParamEditMsg) {
            //case kUpdate: {
              //pGraphics->mEdParam->GetDisplayForHost(txt);
              //SendMessage(pGraphics->mParamEditWnd, WM_GETTEXT, 0, (LPARAM) txt);
              //char currentText[MAX_PARAM_LEN];
              //SendMessage(pGraphics->mParamEditWnd, WM_GETTEXT, MAX_PARAM_LEN, (LPARAM) currentText);
              //if (strcmp(txt, currentText))
              //{
              //  if (pGraphics->mEdParam->GetNDisplayTexts())
              //    SendMessage(pGraphics->mParamEditWnd, CB_SELECTSTRING, -1, (LPARAM) txt);
              //  else
              //    SendMessage(pGraphics->mParamEditWnd, WM_SETTEXT, 0, (LPARAM) txt);
              //}
            //  break;
            //}
            case kCommit: {
              SendMessage(pGraphics->mParamEditWnd, WM_GETTEXT, MAX_PARAM_LEN, (LPARAM) txt);

              if(pGraphics->mEdParam){
                IParam::EParamType type = pGraphics->mEdParam->Type();
    
                if ( type == IParam::kTypeEnum || type == IParam::kTypeBool){
                  int vi = 0;
                  pGraphics->mEdParam->MapDisplayText(txt, &vi);
                  v = (double) vi;
                }
                else {
                  v = atof(txt);
                  if (pGraphics->mEdParam->DisplayIsNegated()) {
                    v = -v;
                  }
                }
                pGraphics->mEdControl->SetValueFromUserInput(pGraphics->mEdParam->GetNormalized(v));
              }
              else {
                pGraphics->mEdControl->TextFromTextEntry(txt);
              }
              // Fall through.
            }
            case kCancel:
            {
              SetWindowLongPtr(pGraphics->mParamEditWnd, GWLP_WNDPROC, (LPARAM) pGraphics->mDefEditProc);
              DestroyWindow(pGraphics->mParamEditWnd);
              pGraphics->mParamEditWnd = 0;
              pGraphics->mEdParam = 0;
              pGraphics->mEdControl = 0;
              pGraphics->mDefEditProc = 0;
            }
            break;            
          }
          pGraphics->mParamEditMsg = kNone;
          return 0; // TODO: check this!
        }
       
        IRECT dirtyR;
        if (pGraphics->IsDirty(&dirtyR)) {
          RECT r = { dirtyR.L, dirtyR.T, dirtyR.R, dirtyR.B };
          
          InvalidateRect(hWnd, &r, FALSE);
          
          if (pGraphics->mParamEditWnd) {
            IRECT* notDirtyR = pGraphics->mEdControl->GetRECT();
            RECT r2 = { notDirtyR->L, notDirtyR->T, notDirtyR->R, notDirtyR->B };
            ValidateRect(hWnd, &r2); // make sure we dont redraw the edit box area
            UpdateWindow(hWnd);
            pGraphics->mParamEditMsg = kUpdate;
          }
          else 
			      UpdateWindow(hWnd);
        }
      }
      return 0;
    }
    
    case WM_RBUTTONDOWN: 
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
      
      if (pGraphics->mParamEditWnd) 
      {
        SetWindowLongPtr(pGraphics->mParamEditWnd, GWLP_WNDPROC, (LPARAM) pGraphics->mDefEditProc);
        DestroyWindow(pGraphics->mParamEditWnd);
        pGraphics->mParamEditWnd = 0;
        pGraphics->mEdParam = 0;
        pGraphics->mEdControl = 0;
        pGraphics->mDefEditProc = 0;
        pGraphics->mParamEditMsg = kNone;
		//force full redraw when closing text entry
		RECT r = { 0, 0, pGraphics->Width(), pGraphics->Height() };
		InvalidateRect(hWnd, &r, FALSE);
		UpdateWindow(hWnd);
      }
      SetFocus(hWnd); // Added to get keyboard focus again when user clicks in window
      SetCapture(hWnd);
#ifdef RTAS_API
      // pass ctrl-start-alt-click or ctrl-start-click to host window (Pro Tools)
      if ((IsControlKeyDown() && IsOptionKeyDown() && IsCommandKeyDown() ) || (IsControlKeyDown() && IsCommandKeyDown()))
      {
        HWND rootHWnd = GetAncestor( hWnd, GA_ROOT);
  
        union point{
          long lp;
          struct {
            short x;
            short y;
          }s;
        } mousePoint;
 
        // Get global coordinates of local window
        RECT childRect;
        GetWindowRect(hWnd, &childRect);
 
        // Convert global coords to parent window coords
        POINT p;
        p.x = childRect.left;
        p.y = childRect.top;
 
        ScreenToClient(rootHWnd, &p);
 
        // offset the local click-event coordinates to the parent window's values
        mousePoint.lp = lParam;
        mousePoint.s.x += p.x;
        mousePoint.s.y += p.y;
 
        if( pGraphics->GetParamIdxForPTAutomation(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)) > -1)
        {
        // Send converted coords to parent window's event handler for regular processing
          LRESULT result = SendMessage(rootHWnd, msg, wParam, mousePoint.lp);
        }

        return 0;       
      }
#endif
      pGraphics->OnMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &GetMouseMod(wParam));
      return 0;
      
    case WM_MOUSEMOVE: { 
      if (!(wParam & (MK_LBUTTON | MK_RBUTTON))) { 
        if (pGraphics->OnMouseOver(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &GetMouseMod(wParam))) {
          TRACKMOUSEEVENT eventTrack = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, HOVER_DEFAULT };
          TrackMouseEvent(&eventTrack);
        }
      }
      else
      if (GetCapture() == hWnd && !pGraphics->mParamEditWnd) {
        pGraphics->OnMouseDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &GetMouseMod(wParam));
      }

      return 0;
    }
    case WM_MOUSELEAVE: {
      pGraphics->OnMouseOut();
      return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
      ReleaseCapture();
      pGraphics->OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &GetMouseMod(wParam));
      return 0;
    }
    case WM_LBUTTONDBLCLK: {
      if (pGraphics->OnMouseDblClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), &GetMouseMod(wParam))) {
        SetCapture(hWnd);
      }
      return 0;
    }
    case WM_MOUSEWHEEL: {

      if (pGraphics->mParamEditWnd) {
        pGraphics->mParamEditMsg = kCancel;
        return 0;
      }
      else
      {
        int d = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
        RECT r;
        GetWindowRect(hWnd, &r);
        pGraphics->OnMouseWheel(x - r.left, y - r.top, &GetMouseMod(wParam), d);
        return 0;
      }
    }

    case WM_KEYDOWN:
    {
      bool handle = true;
      int key;     
      
      if (wParam == VK_SPACE) key = KEY_SPACE;
      else if (wParam == VK_UP) key = KEY_UPARROW;
      else if (wParam == VK_DOWN) key = KEY_DOWNARROW;
      else if (wParam == VK_LEFT) key = KEY_LEFTARROW;
      else if (wParam == VK_RIGHT) key = KEY_RIGHTARROW;
      else if (wParam >= '0' && wParam <= '9') key = KEY_DIGIT_0+wParam-'0';
      else if (wParam >= 'A' && wParam <= 'Z') key = KEY_ALPHA_A+wParam-'A';
      else if (wParam >= 'a' && wParam <= 'z') key = KEY_ALPHA_A+wParam-'a';
      else handle = false;

      if (handle)
      {
        POINT p;
        GetCursorPos(&p); 
        ScreenToClient(hWnd, &p);
        handle = pGraphics->OnKeyDown(p.x, p.y, key);
      }

      if (!handle) {
        #ifdef RTAS_API
        HWND rootHWnd = GetAncestor( hWnd, GA_ROOT);
        SendMessage(rootHWnd, WM_KEYDOWN, wParam, lParam);
        #endif
        return DefWindowProc(hWnd, msg, wParam, lParam);
      }
      else
        return 0;
    }
    case WM_PAINT: {
      RECT r;
      if (GetUpdateRect(hWnd, &r, FALSE)) {
        IRECT ir(r.left, r.top, r.right, r.bottom);
        pGraphics->Draw(&ir);
      }
      return 0;
    }

  /*  case WM_CTLCOLOREDIT: {
      // An edit control just opened.
      HDC dc = (HDC) wParam;
      SetBkColor (dc, RGB(0, 0, 0));
      SetTextColor(dc, RGB(255, 255, 255));
      SetBkMode(dc,OPAQUE); 
      //SetDCBrushColor(dc, RGB(255, 0, 0));
      return (BOOL)GetStockObject(DC_BRUSH);
      //return 0;
    }
   */
    case WM_CLOSE: {
      pGraphics->CloseWindow();
      return 0;
    }
#ifdef RTAS_API
    case WM_MEASUREITEM : {
      HWND rootHWnd =  GetAncestor( hWnd, GA_ROOT );
      LRESULT result = SendMessage(rootHWnd, msg, wParam, lParam);
      return result;
    }
    case WM_DRAWITEM : {
      HWND rootHWnd =  GetAncestor( hWnd, GA_ROOT );
      LRESULT result = SendMessage(rootHWnd, msg, wParam, lParam);
      return result;
    }
#endif
	case WM_SETFOCUS: {
        return 0;
    }
    case WM_KILLFOCUS: {
		return 0;
	}
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

// static 
LRESULT CALLBACK IGraphicsWin::ParamEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  IGraphicsWin* pGraphics = (IGraphicsWin*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

  if (pGraphics && pGraphics->mParamEditWnd && pGraphics->mParamEditWnd == hWnd) 
  {
    switch (msg) {
      case WM_CHAR: {
        // limit to numbers for text entry on appropriate parameters
        if(pGraphics->mEdParam) {
	        char c = wParam;

          if(c == 0x08) break; // backspace

	        switch ( pGraphics->mEdParam->Type() ) 
	        {
	          case IParam::kTypeEnum:
	          case IParam::kTypeInt:
            case IParam::kTypeBool:
		          if (c >= '0' && c <= '9') break;
              else if (c == '-') break;
              else if (c == '+') break;
		          else return 0;
	          case IParam::kTypeDouble:
		          if (c >= '0' && c <= '9') break;
              else if (c == '-') break;
              else if (c == '+') break;
		          else if (c == '.') break;
		          else return 0;
	          default:
		          break;
	        }
        }
        break; 
      }
      case WM_KEYDOWN: {
        if (wParam == VK_RETURN) {
          pGraphics->mParamEditMsg = kCommit;
          return 0;
        }
        break;
      }
      case WM_SETFOCUS: {
        pGraphics->mParamEditMsg = kEditing;
        break;
      }
      case WM_KILLFOCUS: {
        pGraphics->mParamEditMsg = kCancel; // when another window is focussed, kill the text edit box
        break;
      }
        // handle WM_GETDLGCODE so that we can say that we want the return key message
        //  (normally single line edit boxes don't get sent return key messages)
      case WM_GETDLGCODE: {
        if (pGraphics->mEdParam) break;
        LPARAM lres;
        // find out if the original control wants it
        lres = CallWindowProc(pGraphics->mDefEditProc, hWnd, WM_GETDLGCODE, wParam, lParam);
        // add in that we want it if it is a return keydown
        if (lParam && ((MSG*)lParam)->message == WM_KEYDOWN  &&  wParam == VK_RETURN) {
          lres |= DLGC_WANTMESSAGE;
        }
        return lres;
      }
      case WM_COMMAND: {
        switch HIWORD(wParam) {
          case CBN_SELCHANGE: {
            if (pGraphics->mParamEditWnd) {
              pGraphics->mParamEditMsg = kCommit;
              return 0;
            }
          }
        
        }
        break;  // Else let the default proc handle it.
      }
    }
    return CallWindowProc(pGraphics->mDefEditProc, hWnd, msg, wParam, lParam);
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

IGraphicsWin::IGraphicsWin(IPlugBase* pPlug, int w, int h, int refreshFPS)
: IGraphics(pPlug, w, h, refreshFPS), mPlugWnd(0), mParamEditWnd(0), 
  mPID(0), mParentWnd(0), mMainWnd(0), mCustomColorStorage(0),
  mEdControl(0), mEdParam(0), mDefEditProc(0), mParamEditMsg(kNone), mIdleTicks(0),
  mHInstance(0)
{}

IGraphicsWin::~IGraphicsWin()
{
  CloseWindow();
  FREE_NULL(mCustomColorStorage);
}

LICE_IBitmap* IGraphicsWin::OSLoadBitmap(int ID, const char* name)
{
  const char* ext = name+strlen(name)-1;
  while (ext > name && *ext != '.') --ext;
  ++ext;

  if (!stricmp(ext, "png")) return _LICE::LICE_LoadPNGFromResource(mHInstance, ID, 0);
#ifdef IPLUG_JPEG_SUPPORT
  if (!stricmp(ext, "jpg") || !stricmp(ext, "jpeg")) return _LICE::LICE_LoadJPGFromResource(mHInstance, ID, 0);
#endif

  return 0;
}

void GetWindowSize(HWND pWnd, int* pW, int* pH)
{
  if (pWnd) {
    RECT r;
    GetWindowRect(pWnd, &r);
    *pW = r.right - r.left;
    *pH = r.bottom - r.top;
  }
  else {
    *pW = *pH = 0;
  }
}

bool IsChildWindow(HWND pWnd)
{
  if (pWnd) {
    int style = GetWindowLong(pWnd, GWL_STYLE);
    int exStyle = GetWindowLong(pWnd, GWL_EXSTYLE);
    return ((style & WS_CHILD) && !(exStyle & WS_EX_MDICHILD));
  }
  return false;
}

void IGraphicsWin::ForceEndUserEdit()
{
  mParamEditMsg = kCancel;
}

#define SETPOS_FLAGS SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE

void IGraphicsWin::Resize(int w, int h)
{
  int dw = w - Width(), dh = h - Height();
  IGraphics::Resize(w, h);
  if (mDrawBitmap) {
    mDrawBitmap->resize(w, h);
  }
  if (WindowIsOpen()) {
    HWND pParent = 0, pGrandparent = 0;
    int w = 0, h = 0, parentW = 0, parentH = 0, grandparentW = 0, grandparentH = 0;
    GetWindowSize(mPlugWnd, &w, &h);
    if (IsChildWindow(mPlugWnd)) {
      pParent = GetParent(mPlugWnd);
      GetWindowSize(pParent, &parentW, &parentH);
      if (IsChildWindow(pParent)) {
        pGrandparent = GetParent(pParent);
        GetWindowSize(pGrandparent, &grandparentW, &grandparentH);
      }
    }
    SetWindowPos(mPlugWnd, 0, 0, 0, w + dw, h + dh, SETPOS_FLAGS);
    if (pParent) {
      SetWindowPos(pParent, 0, 0, 0, parentW + dw, parentH + dh, SETPOS_FLAGS);
    }
    if (pGrandparent) {
      SetWindowPos(pGrandparent, 0, 0, 0, grandparentW + dw, grandparentH + dh, SETPOS_FLAGS);
    }
          
    RECT r = { 0, 0, w, h };
    InvalidateRect(mPlugWnd, &r, FALSE);
  }
}

bool IGraphicsWin::DrawScreen(IRECT* pR)
{
  PAINTSTRUCT ps;
  HWND hWnd = (HWND) GetWindow();
  HDC dc = BeginPaint(hWnd, &ps);
  BitBlt(dc, pR->L, pR->T, pR->W(), pR->H(), mDrawBitmap->getDC(), pR->L, pR->T, SRCCOPY);
  EndPaint(hWnd, &ps);
  return true;
}

void* IGraphicsWin::OpenWindow(void* pParentWnd)
{
  int x = 0, y = 0, w = Width(), h = Height();
  mParentWnd = (HWND) pParentWnd;

  if (mPlugWnd) {
    RECT pR, cR;
    GetWindowRect((HWND) pParentWnd, &pR);
    GetWindowRect(mPlugWnd, &cR);
    CloseWindow();
    x = cR.left - pR.left;
    y = cR.top - pR.top;
    w = cR.right - cR.left;
    h = cR.bottom - cR.top;
  }

  if (nWndClassReg++ == 0) {
    WNDCLASS wndClass = { CS_DBLCLKS, WndProc, 0, 0, mHInstance, 0, LoadCursor(NULL, IDC_ARROW), 0, 0, wndClassName };
    RegisterClass(&wndClass);
  }

  sFPS = FPS();
  mPlugWnd = CreateWindow(wndClassName, "IPlug", WS_CHILD | WS_VISIBLE, // | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
    x, y, w, h, (HWND) pParentWnd, 0, mHInstance, this);
  //SetWindowLong(mPlugWnd, GWL_USERDATA, (LPARAM) this);

  if (!mPlugWnd && --nWndClassReg == 0) {
    UnregisterClass(wndClassName, mHInstance);
  }
  else {
    SetAllControlsDirty();
  }  

  return mPlugWnd;
}

#define MAX_CLASSNAME_LEN 128
void GetWndClassName(HWND hWnd, WDL_String* pStr)
{
    char cStr[MAX_CLASSNAME_LEN];
    cStr[0] = '\0';
    GetClassName(hWnd, cStr, MAX_CLASSNAME_LEN);
    pStr->Set(cStr);
}

BOOL CALLBACK IGraphicsWin::FindMainWindow(HWND hWnd, LPARAM lParam)
{
    IGraphicsWin* pGraphics = (IGraphicsWin*) lParam;
    if (pGraphics) {
        DWORD wPID;
        GetWindowThreadProcessId(hWnd, &wPID);
        WDL_String str;
        GetWndClassName(hWnd, &str);
        if (wPID == pGraphics->mPID && !strcmp(str.Get(), pGraphics->mMainWndClassName.Get())) {
            pGraphics->mMainWnd = hWnd;
            return FALSE;   // Stop enumerating.
        }
    }
    return TRUE;
}

HWND IGraphicsWin::GetMainWnd()
{
    if (!mMainWnd) {
        if (mParentWnd) {
            HWND parentWnd = mParentWnd;
            while (parentWnd) {
                mMainWnd = parentWnd;
                parentWnd = GetParent(mMainWnd);
            }
            GetWndClassName(mMainWnd, &mMainWndClassName);
        }
        else
        if (CSTR_NOT_EMPTY(mMainWndClassName.Get())) {
            mPID = GetCurrentProcessId();
            EnumWindows(FindMainWindow, (LPARAM) this);
        }
    }
    return mMainWnd;
}

#define TOOLWIN_BORDER_W 6
#define TOOLWIN_BORDER_H 23

IRECT IGraphicsWin::GetWindowRECT()
{
    if (mPlugWnd) {
        RECT r;
        GetWindowRect(mPlugWnd, &r);
        r.right -= TOOLWIN_BORDER_W;
        r.bottom -= TOOLWIN_BORDER_H;
        return IRECT(r.left, r.top, r.right, r.bottom);
    }
    return IRECT();
}

void IGraphicsWin::SetWindowTitle(char* str)
{
    SetWindowText(mPlugWnd, str);
}

void IGraphicsWin::CloseWindow()
{
  if (mPlugWnd) {
    DestroyWindow(mPlugWnd);
    mPlugWnd = 0;

    if (--nWndClassReg == 0) {
      UnregisterClass(wndClassName, mHInstance);
    }
  }
}

IPopupMenu* IGraphicsWin::CreateIPopupMenu(IPopupMenu* pMenu, IRECT* pAreaRect)
{
  ReleaseMouseCapture();
  
  int numItems = pMenu->GetNItems();

  POINT cPos;
  
  cPos.x = pAreaRect->L;
  cPos.y = pAreaRect->B;

  ClientToScreen(mPlugWnd, &cPos);

  HMENU hMenu = CreatePopupMenu();

  int flags = 0;
  
  if(numItems && hMenu)
  {
    for(int i = 0; i< numItems;i++)
    {
      IPopupMenuItem* menuItem = pMenu->GetItem(i);

      if (menuItem->GetIsSeparator())
      {
        AppendMenu (hMenu, MF_SEPARATOR, 0, 0);
      }
      else
      {
        const char* str = menuItem->GetText();
        char* titleWithPrefixNumbers = 0;
        
        if (pMenu->GetPrefix())
        {
          titleWithPrefixNumbers = (char*)malloc(strlen(str) + 50);

          switch (pMenu->GetPrefix())
          {
            case 1:
            {
              sprintf(titleWithPrefixNumbers, "%1d: %s", i+1, str); break;
            }
            case 2:
            {
              sprintf(titleWithPrefixNumbers, "%02d: %s", i+1, str); break;
            }
            case 3:
            {
              sprintf(titleWithPrefixNumbers, "%03d: %s", i+1, str); break;
            }
          }
        }

        const char* entryText (titleWithPrefixNumbers ? titleWithPrefixNumbers : str);
        
        flags = MF_STRING;
        //if (nbEntries < 160 && _menu->getNbItemsPerColumn () > 0 && inc && !(inc % _menu->getNbItemsPerColumn ()))
        //  flags |= MF_MENUBARBREAK;

        if (menuItem->GetSubmenu())
        {
        //  HMENU submenu = createMenu (item->getSubmenu (), offsetIdx);
        //  if (submenu)
        //  {
         //   AppendMenu (menu, flags|MF_POPUP|MF_ENABLED, (UINT_PTR)submenu, (const TCHAR*)entryText);
         // }
        }
        else
        {
          if (menuItem->GetEnabled())
            flags |= MF_ENABLED;
          else
            flags |= MF_GRAYED;
          if (menuItem->GetIsTitle())
            flags |= MF_DISABLED;
          //if (multipleCheck && menuItem->GetChecked())
           // flags |= MF_CHECKED;
          if (menuItem->GetChecked())
            flags |= MF_CHECKED;
          else
            flags |= MF_UNCHECKED;
          //if (!(flags & MF_CHECKED))
          //  flags |= MF_UNCHECKED;
          
          AppendMenu(hMenu, flags, i+1, entryText);
          
        }

        if(titleWithPrefixNumbers)
          FREE_NULL(titleWithPrefixNumbers);
      }
    }
    
    int itemChosen = TrackPopupMenu(hMenu, TPM_LEFTALIGN/*|TPM_VCENTERALIGN*/|TPM_NONOTIFY|TPM_RETURNCMD, cPos.x, cPos.y, 0, mPlugWnd, 0);

//    IPopupMenu* chosenMenu;

 

    if (itemChosen > 0)
    {
      pMenu->SetChosenItemIdx(itemChosen - 1);
      DestroyMenu(hMenu);
      return pMenu;
    }
    else 
    {
      DestroyMenu(hMenu);
      return 0;
    }
  }
  else 
  return 0;
}

void IGraphicsWin::CreateTextEntry(IControl* pControl, IText* pText, IRECT* pTextRect, const char* pString, IParam* pParam)
{
  if (!pControl || mParamEditWnd) return;
  
  DWORD editStyle;

  
  switch ( pText->mAlign ) 
  {
    case IText::kAlignNear:   editStyle = ES_LEFT;   break;
    case IText::kAlignFar:    editStyle = ES_RIGHT;  break;
    case IText::kAlignCenter:
    default:                  editStyle = ES_CENTER; break;
  }
  
//  if (!pControl->IsEditable()) editStyle |= ES_READONLY;
//  if (pControl->IsSecure())
//    editStyle |= ES_PASSWORD;
//  else
    editStyle |= ES_MULTILINE;
  
  mParamEditWnd = CreateWindow("EDIT", pString, WS_CHILD | WS_VISIBLE | editStyle , 
                               pTextRect->L, pTextRect->T, pTextRect->W()+1, pTextRect->H()+1, 
                               mPlugWnd, (HMENU) PARAM_EDIT_ID, mHInstance, 0);
  
  //HFONT font = CreateFont(p, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "Verdana");
  HFONT font = CreateFont(pText->mSize, 0, 0, 0, pText->mStyle == IText::kStyleBold ? FW_BOLD : 0, pText->mStyle == IText::kStyleItalic ? TRUE : 0, 0, 0, 0, 0, 0, 0, 0, pText->mFont);

  SendMessage(mParamEditWnd, WM_SETFONT, (WPARAM) font, 0);
  SendMessage(mParamEditWnd, EM_SETSEL, 0, -1);
  Edit_LimitText(mParamEditWnd, pControl->GetTextEntryLength());
  SetFocus(mParamEditWnd);
  
  mDefEditProc = (WNDPROC) SetWindowLongPtr(mParamEditWnd, GWLP_WNDPROC, (LONG_PTR) ParamEditProc);
  SetWindowLong(mParamEditWnd, GWLP_USERDATA, (LPARAM) this);
  
  //DeleteObject(font); 
  
  mEdControl = pControl;
  mEdParam = pParam; // could be 0  
}

#define MAX_PATH_LEN 256

void GetModulePath(HMODULE hModule, WDL_String* pPath) 
{
  pPath->Set("");
  char pathCStr[MAX_PATH_LEN];
  pathCStr[0] = '\0';
  if (GetModuleFileName(hModule, pathCStr, MAX_PATH_LEN)) {
    int s = -1;
    for (int i = 0; i < strlen(pathCStr); ++i) {
      if (pathCStr[i] == '\\') {
        s = i;
      }
    }
    if (s >= 0 && s + 1 < strlen(pathCStr)) {
      pPath->Set(pathCStr, s + 1);
    }
  }
}

void IGraphicsWin::HostPath(WDL_String* pPath)
{
  GetModulePath(0, pPath);
}

void IGraphicsWin::PluginPath(WDL_String* pPath)
{
  GetModulePath(mHInstance, pPath);
}

void IGraphicsWin::DesktopPath(WDL_String* pPath)
{
  TCHAR strPath[MAX_PATH_LEN];
#ifndef __MINGW_H
  // Get the special folder path.
  SHGetSpecialFolderPath( 0,       // Hwnd
                         strPath, // String buffer.
                         CSIDL_DESKTOP, // CSLID of folder
                         FALSE );
#endif
  
  pPath->Set(strPath, MAX_PATH_LEN);
}

void IGraphicsWin::PromptForFile(WDL_String* pFilename, EFileAction action, WDL_String* pDir, char* extensions)
{
  if (!WindowIsOpen()) { 
    pFilename->Set("");
    return;
  }

  char fnCStr[MAX_PATH_LEN];
  char dirCStr[MAX_PATH_LEN];
  
  if (pFilename->GetLength())
    strcpy(fnCStr, pFilename->Get());
  else
    fnCStr[0] = '\0';

  dirCStr[0] = '\0';
  
  if (!pDir->GetLength()) 
  {
    DesktopPath(pDir);
  }

  strcpy(dirCStr, pDir->Get());
  
  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(OPENFILENAME));

  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = mPlugWnd;
  ofn.lpstrFile = fnCStr;
  ofn.nMaxFile = MAX_PATH_LEN - 1;
  ofn.lpstrInitialDir = dirCStr;
  ofn.Flags = OFN_PATHMUSTEXIST;
  
  if (CSTR_NOT_EMPTY(extensions)) 
  {
      char extStr[256];
      char defExtStr[16];
      int i, p, n = strlen(extensions);
      bool seperator = true;

      for (i = 0, p = 0; i < n; ++i) 
      {
        if (seperator) 
        {
          if (p) 
          {
            extStr[p++] = ';';
          }
          seperator = false;
          extStr[p++] = '*';
          extStr[p++] = '.';
        }
        
        if (extensions[i] == ' ') 
        {
          seperator = true;
        }
        else 
        {
          extStr[p++] = extensions[i];
        }
      }
      extStr[p++] = '\0';

      strcpy(&extStr[p], extStr);
      extStr[p + p] = '\0';
      ofn.lpstrFilter = extStr;

      for (i = 0, p = 0; i < n && extensions[i] != ' '; ++i) 
      {
        defExtStr[p++] = extensions[i];
      }

      defExtStr[p++] = '\0';
      ofn.lpstrDefExt = defExtStr;
    }

    bool rc = false;
    switch (action) 
    {
        case kFileSave:
            ofn.Flags |= OFN_OVERWRITEPROMPT;
            rc = GetSaveFileName(&ofn);
            break;

        case kFileOpen:     
        default:
            ofn.Flags |= OFN_FILEMUSTEXIST;
          rc = GetOpenFileName(&ofn);
            break;
    }

    if (rc) 
    {
      char drive[_MAX_DRIVE];
      if(_splitpath_s(ofn.lpstrFile, drive, sizeof(drive), dirCStr, sizeof(dirCStr), NULL, 0, NULL, 0) == 0)
      {
        pDir->SetFormatted(MAX_PATH_LEN, "%s%s", drive, dirCStr);
      }
   
      pFilename->Set(ofn.lpstrFile);
    }
}

UINT_PTR CALLBACK CCHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
{
    if (uiMsg == WM_INITDIALOG && lParam) {
        CHOOSECOLOR* cc = (CHOOSECOLOR*) lParam;
        if (cc && cc->lCustData) {
            char* str = (char*) cc->lCustData;
            SetWindowText(hdlg, str);
        }
    }
    return 0;
}

bool IGraphicsWin::PromptForColor(IColor* pColor, char* prompt)
{
    if (!mPlugWnd) {
        return false;
    }
    if (!mCustomColorStorage) {
        mCustomColorStorage = (COLORREF*) calloc(16, sizeof(COLORREF));
    }
    CHOOSECOLOR cc;
    memset(&cc, 0, sizeof(CHOOSECOLOR));
    cc.lStructSize = sizeof(CHOOSECOLOR);
    cc.hwndOwner = mPlugWnd;
    cc.rgbResult = RGB(pColor->R, pColor->G, pColor->B);
    cc.lpCustColors = mCustomColorStorage;
    cc.lCustData = (LPARAM) prompt;
    cc.lpfnHook = CCHookProc;
    cc.Flags = CC_RGBINIT | CC_ANYCOLOR | CC_FULLOPEN | CC_SOLIDCOLOR | CC_ENABLEHOOK;
    
    if (ChooseColor(&cc)) {
        pColor->R = GetRValue(cc.rgbResult);
        pColor->G = GetGValue(cc.rgbResult);
        pColor->B = GetBValue(cc.rgbResult);
        return true;
    }
    return false;
}

#define MAX_INET_ERR_CODE 32
bool IGraphicsWin::OpenURL(const char* url, 
  const char* msgWindowTitle, const char* confirmMsg, const char* errMsgOnFailure)
{
  if (confirmMsg && MessageBox(mPlugWnd, confirmMsg, msgWindowTitle, MB_YESNO) != IDYES) {
    return false;
  }
  DWORD inetStatus = 0;
  if (InternetGetConnectedState(&inetStatus, 0)) {
    if ((int) ShellExecute(mPlugWnd, "open", url, 0, 0, SW_SHOWNORMAL) > MAX_INET_ERR_CODE) {
      return true;
    }
  }
  if (errMsgOnFailure) {
    MessageBox(mPlugWnd, errMsgOnFailure, msgWindowTitle, MB_OK);
  }
  return false;
}

