/*
 * PROJECT:         ReactOS Multimedia Control Panel
 * FILE:            dll/cpl/mmsys/volume.c
 * PURPOSE:         ReactOS Multimedia Control Panel
 * PROGRAMMER:      Thomas Weidenmueller <w3seek@reactos.com>
 *                  Johannes Anderwald <janderwald@reactos.com>
 *                  Dmitry Chapyshev <dmitry@reactos.org>
 */

#include "mmsys.h"

#include <shellapi.h>

#define VOLUME_MIN        0
#define VOLUME_MAX      500
#define VOLUME_TICFREQ   50
#define VOLUME_PAGESIZE 100

typedef struct _IMGINFO
{
    HBITMAP hBitmap;
    INT cxSource;
    INT cySource;
} IMGINFO, *PIMGINFO;


typedef struct _GLOBAL_DATA
{
    HMIXER hMixer;
    HICON hIconMuted;
    HICON hIconUnMuted;
    HICON hIconNoHW;

    LONG muteVal;
    DWORD muteControlID;

    DWORD volumeControlID;
    DWORD volumeMinimum;
    DWORD volumeMaximum;
    DWORD volumeValue;
    DWORD volumeStep;
} GLOBAL_DATA, *PGLOBAL_DATA;


static VOID
InitImageInfo(PIMGINFO ImgInfo)
{
    BITMAP bitmap;

    ZeroMemory(ImgInfo, sizeof(*ImgInfo));

    ImgInfo->hBitmap = LoadImage(hApplet,
                                 MAKEINTRESOURCE(IDB_SPEAKIMG),
                                 IMAGE_BITMAP,
                                 0,
                                 0,
                                 LR_DEFAULTCOLOR);

    if (ImgInfo->hBitmap != NULL)
    {
        GetObject(ImgInfo->hBitmap, sizeof(BITMAP), &bitmap);

        ImgInfo->cxSource = bitmap.bmWidth;
        ImgInfo->cySource = bitmap.bmHeight;
    }
}


VOID
GetMuteControl(PGLOBAL_DATA pGlobalData)
{
    MIXERLINE mxln;
    MIXERCONTROL mxc;
    MIXERLINECONTROLS mxlctrl;

    if (pGlobalData->hMixer == NULL)
        return;

    mxln.cbStruct = sizeof(MIXERLINE);
    mxln.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;

    if (mixerGetLineInfo((HMIXEROBJ)pGlobalData->hMixer, &mxln, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE)
        != MMSYSERR_NOERROR) return;

    mxlctrl.cbStruct = sizeof(MIXERLINECONTROLS);
    mxlctrl.dwLineID = mxln.dwLineID;
    mxlctrl.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
    mxlctrl.cControls = 1;
    mxlctrl.cbmxctrl = sizeof(MIXERCONTROL);
    mxlctrl.pamxctrl = &mxc;

    if (mixerGetLineControls((HMIXEROBJ)pGlobalData->hMixer, &mxlctrl, MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE)
        != MMSYSERR_NOERROR) return;

    pGlobalData->muteControlID = mxc.dwControlID;
}


VOID
GetMuteState(PGLOBAL_DATA pGlobalData)
{
    MIXERCONTROLDETAILS_BOOLEAN mxcdMute;
    MIXERCONTROLDETAILS mxcd;

    if (pGlobalData->hMixer == NULL)
        return;

    mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
    mxcd.dwControlID = pGlobalData->muteControlID;
    mxcd.cChannels = 1;
    mxcd.cMultipleItems = 0;
    mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    mxcd.paDetails = &mxcdMute;

    if (mixerGetControlDetails((HMIXEROBJ)pGlobalData->hMixer, &mxcd, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE)
        != MMSYSERR_NOERROR)
        return;

    pGlobalData->muteVal = mxcdMute.fValue;
}


VOID
SwitchMuteState(PGLOBAL_DATA pGlobalData)
{
    MIXERCONTROLDETAILS_BOOLEAN mxcdMute;
    MIXERCONTROLDETAILS mxcd;

    mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
    mxcd.dwControlID = pGlobalData->muteControlID;
    mxcd.cChannels = 1;
    mxcd.cMultipleItems = 0;
    mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
    mxcd.paDetails = &mxcdMute;

    mxcdMute.fValue = !pGlobalData->muteVal;
    if (mixerSetControlDetails((HMIXEROBJ)pGlobalData->hMixer, &mxcd, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE)
        != MMSYSERR_NOERROR)
        return;

    pGlobalData->muteVal = mxcdMute.fValue;
}


VOID
GetVolumeControl(PGLOBAL_DATA pGlobalData)
{
    MIXERLINE mxln;
    MIXERCONTROL mxc;
    MIXERLINECONTROLS mxlc;

    if (pGlobalData->hMixer == NULL)
        return;

    mxln.cbStruct = sizeof(MIXERLINE);
    mxln.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
    if (mixerGetLineInfo((HMIXEROBJ)pGlobalData->hMixer, &mxln, MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE)
        != MMSYSERR_NOERROR)
        return;

    mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
    mxlc.dwLineID = mxln.dwLineID;
    mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
    mxlc.cControls = 1;
    mxlc.cbmxctrl = sizeof(MIXERCONTROL);
    mxlc.pamxctrl = &mxc;
    if (mixerGetLineControls((HMIXEROBJ)pGlobalData->hMixer, &mxlc, MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE)
        != MMSYSERR_NOERROR)
        return;

    pGlobalData->volumeMinimum = mxc.Bounds.dwMinimum;
    pGlobalData->volumeMaximum = mxc.Bounds.dwMaximum;
    pGlobalData->volumeControlID = mxc.dwControlID;
    pGlobalData->volumeStep = (pGlobalData->volumeMaximum - pGlobalData->volumeMinimum) / (VOLUME_MAX - VOLUME_MIN);
}


VOID
GetVolumeValue(PGLOBAL_DATA pGlobalData)
{
    MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
    MIXERCONTROLDETAILS mxcd;

    if (pGlobalData->hMixer == NULL)
        return;

    mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
    mxcd.dwControlID = pGlobalData->volumeControlID;
    mxcd.cChannels = 1;
    mxcd.cMultipleItems = 0;
    mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
    mxcd.paDetails = &mxcdVolume;

    if (mixerGetControlDetails((HMIXEROBJ)pGlobalData->hMixer, &mxcd, MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE)
        != MMSYSERR_NOERROR)
        return;

    pGlobalData->volumeValue = mxcdVolume.dwValue;
}


VOID
SetVolumeValue(PGLOBAL_DATA pGlobalData){
    MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
    MIXERCONTROLDETAILS mxcd;

    if (pGlobalData->hMixer == NULL)
        return;

    mxcdVolume.dwValue = pGlobalData->volumeValue;
    mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
    mxcd.dwControlID = pGlobalData->volumeControlID;
    mxcd.cChannels = 1;
    mxcd.cMultipleItems = 0;
    mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
    mxcd.paDetails = &mxcdVolume;

    if (mixerSetControlDetails((HMIXEROBJ)pGlobalData->hMixer, &mxcd, MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE)
        != MMSYSERR_NOERROR)
        return;

    pGlobalData->volumeValue = mxcdVolume.dwValue;
}


static
VOID
SetSystrayVolumeIconState(BOOL bEnabled)
{
    HWND hwndTaskBar;

    hwndTaskBar = FindWindowW(L"SystemTray_Main", NULL);
    if (hwndTaskBar == NULL)
        return;

    SendMessageW(hwndTaskBar, WM_USER + 220, 4, bEnabled);
}

static
BOOL
GetSystrayVolumeIconState(VOID)
{
    HWND hwndTaskBar;

    hwndTaskBar = FindWindowW(L"SystemTray_Main", NULL);
    if (hwndTaskBar == NULL)
    {
        return FALSE;
    }

    return (BOOL)SendMessageW(hwndTaskBar, WM_USER + 221, 4, 0);
}

VOID
InitVolumeControls(HWND hwndDlg, PGLOBAL_DATA pGlobalData)
{
    UINT NumMixers;
    MIXERCAPS mxc;
    TCHAR szNoDevices[256];

    CheckDlgButton(hwndDlg,
                   IDC_ICON_IN_TASKBAR,
                   GetSystrayVolumeIconState() ? BST_CHECKED : BST_UNCHECKED);

    LoadString(hApplet, IDS_NO_DEVICES, szNoDevices, _countof(szNoDevices));

    NumMixers = mixerGetNumDevs();
    if (!NumMixers)
    {
        EnableWindow(GetDlgItem(hwndDlg, IDC_VOLUME_TRACKBAR), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_MUTE_CHECKBOX),   FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_ADVANCED_BTN),    FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_SPEAKER_VOL_BTN), FALSE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_ADVANCED2_BTN),   FALSE);
        SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconNoHW);
        SetDlgItemText(hwndDlg, IDC_DEVICE_NAME, szNoDevices);
        return;
    }

    if (mixerOpen(&pGlobalData->hMixer, 0, PtrToUlong(hwndDlg), 0, MIXER_OBJECTF_MIXER | CALLBACK_WINDOW) != MMSYSERR_NOERROR)
    {
        MessageBox(hwndDlg, _T("Cannot open mixer"), NULL, MB_OK);
        return;
    }

    ZeroMemory(&mxc, sizeof(MIXERCAPS));
    if (mixerGetDevCaps(PtrToUint(pGlobalData->hMixer), &mxc, sizeof(MIXERCAPS)) != MMSYSERR_NOERROR)
    {
        MessageBox(hwndDlg, _T("mixerGetDevCaps failed"), NULL, MB_OK);
        return;
    }

    GetMuteControl(pGlobalData);
    GetMuteState(pGlobalData);
    if (pGlobalData->muteVal)
    {
        SendDlgItemMessage(hwndDlg, IDC_MUTE_CHECKBOX, BM_SETCHECK, (WPARAM)BST_CHECKED, (LPARAM)0);
        SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconMuted);
    }
    else
    {
        SendDlgItemMessage(hwndDlg, IDC_MUTE_CHECKBOX, BM_SETCHECK, (WPARAM)BST_UNCHECKED, (LPARAM)0);
        SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconUnMuted);
    }

    GetVolumeControl(pGlobalData);
    GetVolumeValue(pGlobalData);

    SendDlgItemMessage(hwndDlg, IDC_DEVICE_NAME, WM_SETTEXT, 0, (LPARAM)mxc.szPname);
    SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_SETRANGE, (WPARAM)TRUE, (LPARAM)MAKELONG(VOLUME_MIN, VOLUME_MAX));
    SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_SETTICFREQ, VOLUME_TICFREQ, 0);
    SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_SETPAGESIZE, 0, VOLUME_PAGESIZE);
    SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(pGlobalData->volumeValue - pGlobalData->volumeMinimum) / pGlobalData->volumeStep);
}

VOID
SaveData(HWND hwndDlg)
{
    BOOL bShowIcon;

    bShowIcon = (IsDlgButtonChecked(hwndDlg, IDC_ICON_IN_TASKBAR) == BST_CHECKED);

    SetSystrayVolumeIconState(!bShowIcon);
}

VOID
LaunchSoundControl(HWND hwndDlg)
{
    if ((INT_PTR)ShellExecuteW(NULL, L"open", L"sndvol32.exe", NULL, NULL, SW_SHOWNORMAL) > 32)
        return;
    MessageBox(hwndDlg, _T("Cannot run sndvol32.exe"), NULL, MB_OK);
}

/* Volume property page dialog callback */
//static INT_PTR CALLBACK
INT_PTR CALLBACK
VolumeDlgProc(HWND hwndDlg,
              UINT uMsg,
              WPARAM wParam,
              LPARAM lParam)
{
    static IMGINFO ImgInfo;
    PGLOBAL_DATA pGlobalData;
    UNREFERENCED_PARAMETER(lParam);
    UNREFERENCED_PARAMETER(wParam);

    pGlobalData = (PGLOBAL_DATA)GetWindowLongPtr(hwndDlg, DWLP_USER);

    switch(uMsg)
    {
        case MM_MIXM_LINE_CHANGE:
        {
            GetMuteState(pGlobalData);
            if (pGlobalData->muteVal)
            {
                SendDlgItemMessage(hwndDlg, IDC_MUTE_CHECKBOX, BM_SETCHECK, (WPARAM)BST_CHECKED, (LPARAM)0);
                SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconMuted);
            }
            else
            {
                SendDlgItemMessage(hwndDlg, IDC_MUTE_CHECKBOX, BM_SETCHECK, (WPARAM)BST_UNCHECKED, (LPARAM)0);
                SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconUnMuted);
            }
            break;
        }
        case MM_MIXM_CONTROL_CHANGE:
        {
            GetVolumeValue(pGlobalData);
            SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_SETPOS, (WPARAM)TRUE, (LPARAM)(pGlobalData->volumeValue - pGlobalData->volumeMinimum) / pGlobalData->volumeStep);
            break;
        }
        case WM_INITDIALOG:
        {
            pGlobalData = (GLOBAL_DATA*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(GLOBAL_DATA));
            SetWindowLongPtr(hwndDlg, DWLP_USER, (LONG_PTR)pGlobalData);

            pGlobalData->hIconUnMuted = LoadImage(hApplet, MAKEINTRESOURCE(IDI_CPLICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
            pGlobalData->hIconMuted = LoadImage(hApplet, MAKEINTRESOURCE(IDI_MUTED_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
            pGlobalData->hIconNoHW = LoadImage(hApplet, MAKEINTRESOURCE(IDI_NO_HW), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);

            InitImageInfo(&ImgInfo);
            InitVolumeControls(hwndDlg, pGlobalData);
            break;
        }

        case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT lpDrawItem;
            lpDrawItem = (LPDRAWITEMSTRUCT) lParam;
            if(lpDrawItem->CtlID == IDC_SPEAKIMG)
            {
                HDC hdcMem;
                LONG left;

                /* Position image in centre of dialog */
                left = (lpDrawItem->rcItem.right - ImgInfo.cxSource) / 2;

                hdcMem = CreateCompatibleDC(lpDrawItem->hDC);
                if (hdcMem != NULL)
                {
                    SelectObject(hdcMem, ImgInfo.hBitmap);
                    BitBlt(lpDrawItem->hDC,
                           left,
                           lpDrawItem->rcItem.top,
                           lpDrawItem->rcItem.right - lpDrawItem->rcItem.left,
                           lpDrawItem->rcItem.bottom - lpDrawItem->rcItem.top,
                           hdcMem,
                           0,
                           0,
                           SRCCOPY);
                    DeleteDC(hdcMem);
                }
            }
            break;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case IDC_MUTE_CHECKBOX:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        SwitchMuteState(pGlobalData);
                        if (pGlobalData->muteVal)
                        {
                            SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconMuted);
                        }
                        else
                        {
                            SendDlgItemMessage(hwndDlg, IDC_MUTE_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)pGlobalData->hIconUnMuted);
                        }

                        PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
                    }
                    break;

                case IDC_ICON_IN_TASKBAR:
                    if (HIWORD(wParam) == BN_CLICKED)
                    {
                        PropSheet_Changed(GetParent(hwndDlg), hwndDlg);
                    }
                    break;

                case IDC_ADVANCED_BTN:
                    LaunchSoundControl(hwndDlg);
                    break;
            }
            break;
        }

        case WM_HSCROLL:
        {
            HWND hVolumeTrackbar = GetDlgItem(hwndDlg, IDC_VOLUME_TRACKBAR);
            if (hVolumeTrackbar == (HWND)lParam)
            {
                pGlobalData->volumeValue = ((DWORD)SendDlgItemMessage(hwndDlg, IDC_VOLUME_TRACKBAR, TBM_GETPOS, 0, 0) * pGlobalData->volumeStep) + pGlobalData->volumeMinimum;
                SetVolumeValue(pGlobalData);

                if (LOWORD(wParam) == TB_ENDTRACK)
                    PlaySound((LPCTSTR)SND_ALIAS_SYSTEMDEFAULT, NULL, SND_ALIAS_ID | SND_ASYNC);
            }
            break;
        }

        case WM_DESTROY:
            mixerClose(pGlobalData->hMixer);
            DestroyIcon(pGlobalData->hIconMuted);
            DestroyIcon(pGlobalData->hIconUnMuted);
            DestroyIcon(pGlobalData->hIconNoHW);
            HeapFree(GetProcessHeap(), 0, pGlobalData);
            break;

        case WM_NOTIFY:
            if (((LPNMHDR)lParam)->code == (UINT)PSN_APPLY)
            {
                SaveData(hwndDlg);
            }
            return TRUE;
    }

    return FALSE;
}
