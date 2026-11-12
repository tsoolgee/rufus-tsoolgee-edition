/*
 * Rufus TSOOLGEE edition - additions on top of Rufus
 * Copyright © 2026 tsoolgee
 *
 * Based on Rufus: The Reliable USB Formatting Utility
 * Copyright © 2011-2026 Pete Batard <pete@akeo.ie>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <richedit.h>
#include <shellapi.h>

#include "rufus.h"
#include "missing.h"
#include "resource.h"
#include "msapi_utf8.h"
#include "localization.h"
#include "drive.h"
#include "tsoolgee.h"

extern loc_cmd* selected_locale;

BOOL enable_internal_drives = FALSE;

/*
 * The RSA public key modulus of the key that signs the TSOOLGEE update files
 * (Rufus_win.ver and the release executables). Same layout as the upstream one in pki.c:
 * openssl modulus, including the leading 0x00 sign byte.
 */
static const uint8_t ts_rsa_pubkey_modulus[] = {
	0x00, 0xae, 0x0a, 0x81, 0x22, 0x73, 0x0a, 0x46, 0x19, 0xa2, 0x9f, 0x38,
	0x8b, 0x33, 0x5f, 0x69, 0x43, 0xf1, 0x02, 0x38, 0x31, 0x6e, 0x50, 0x9a,
	0x85, 0x5d, 0xda, 0x5a, 0x6f, 0x9a, 0x30, 0xfc, 0xd6, 0x33, 0xeb, 0x77,
	0xb8, 0xdb, 0x46, 0xea, 0xf6, 0xed, 0xa8, 0xc5, 0xc9, 0x75, 0x32, 0x4a,
	0xe8, 0x25, 0x69, 0x6b, 0x77, 0xf9, 0x23, 0xa9, 0x7b, 0x8e, 0x19, 0x76,
	0xa4, 0x91, 0xfa, 0xc6, 0x6f, 0x68, 0xb9, 0x62, 0x45, 0x79, 0x67, 0x8b,
	0x2c, 0x87, 0x87, 0xae, 0xa3, 0x9b, 0xc7, 0x77, 0x46, 0xd4, 0x22, 0x7f,
	0xea, 0xe2, 0x96, 0x08, 0xf1, 0xc9, 0xc1, 0x44, 0x1c, 0xec, 0x6d, 0x5c,
	0xf9, 0x7a, 0x0c, 0x69, 0xa4, 0xb2, 0x21, 0xdf, 0xfd, 0x5e, 0x56, 0x69,
	0x2e, 0x72, 0x93, 0x32, 0xfe, 0x58, 0x29, 0xce, 0xab, 0x0b, 0x6b, 0x70,
	0xcc, 0xea, 0xeb, 0x64, 0x2d, 0x67, 0xe7, 0x66, 0x8b, 0xeb, 0x32, 0xe1,
	0x92, 0x39, 0x03, 0x94, 0x50, 0x3c, 0x94, 0x18, 0xdd, 0x11, 0xdb, 0xb7,
	0xe9, 0xae, 0x31, 0x9c, 0x57, 0x56, 0x2c, 0xa2, 0xe7, 0x33, 0x22, 0xed,
	0x73, 0x6d, 0xd9, 0xc4, 0x76, 0x04, 0x70, 0x91, 0x2d, 0xd8, 0x3c, 0xe4,
	0xa4, 0x24, 0x2f, 0x03, 0x94, 0xf4, 0x9b, 0x84, 0x8f, 0x35, 0x0c, 0x35,
	0xbd, 0xbe, 0x63, 0x89, 0xb0, 0x29, 0x27, 0x28, 0x79, 0x31, 0x0c, 0xd4,
	0x9f, 0x74, 0x71, 0x98, 0xa9, 0x08, 0xea, 0x24, 0x54, 0xf7, 0xf3, 0xc9,
	0x30, 0xa6, 0xf2, 0xb8, 0x43, 0xa4, 0x28, 0x0e, 0x83, 0xd3, 0x15, 0x23,
	0x98, 0x24, 0x20, 0xbf, 0x51, 0x8a, 0x7b, 0x76, 0x5b, 0x21, 0x58, 0x71,
	0x22, 0x43, 0x96, 0x50, 0x5e, 0xe4, 0xf4, 0x31, 0x4a, 0x91, 0x05, 0x2c,
	0x6d, 0xc9, 0xf8, 0xf3, 0x7e, 0xad, 0x93, 0xa1, 0x34, 0x25, 0x70, 0x63,
	0x80, 0x9d, 0x00, 0x6e, 0x7d
};

/* Drives that GetDevices() let through only because internal drives are enabled */
static BOOL ts_internal_mark[MAX_DRIVES];

const char* TsText(const char* he, const char* en)
{
	if ((selected_locale != NULL) && (selected_locale->txt[0] != NULL) &&
		(_strnicmp(selected_locale->txt[0], "he", 2) == 0))
		return he;
	return en;
}

void TsClearInternalMarks(void)
{
	memset(ts_internal_mark, 0, sizeof(ts_internal_mark));
}

void TsMarkInternal(DWORD DriveIndex)
{
	if ((DriveIndex >= DRIVE_INDEX_MIN) && (DriveIndex < DRIVE_INDEX_MAX))
		ts_internal_mark[DriveIndex - DRIVE_INDEX_MIN] = TRUE;
}

/* Ask the storage stack which bus the disk sits on */
static int TsGetBusType(DWORD DriveIndex)
{
	STORAGE_PROPERTY_QUERY query = { 0 };
	BYTE buf[1024] = { 0 };
	PSTORAGE_DEVICE_DESCRIPTOR desc = (PSTORAGE_DEVICE_DESCRIPTOR)buf;
	DWORD size = 0;
	int bus = -1;
	HANDLE h = GetPhysicalHandle(DriveIndex, FALSE, FALSE, TRUE);

	if (h == INVALID_HANDLE_VALUE)
		return -1;
	query.PropertyId = StorageDeviceProperty;
	query.QueryType = PropertyStandardQuery;
	if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buf, sizeof(buf), &size, NULL) &&
		(size >= sizeof(STORAGE_DEVICE_DESCRIPTOR)))
		bus = (int)desc->BusType;
	CloseHandle(h);
	return bus;
}

BOOL TsIsInternalDrive(DWORD DriveIndex)
{
	if ((DriveIndex >= DRIVE_INDEX_MIN) && (DriveIndex < DRIVE_INDEX_MAX) &&
		ts_internal_mark[DriveIndex - DRIVE_INDEX_MIN])
		return TRUE;
	switch (TsGetBusType(DriveIndex)) {
	case 2:		// ATAPI
	case 3:		// ATA
	case 8:		// RAID
	case 10:	// SAS
	case 11:	// SATA
	case 17:	// NVMe
	case 18:	// SCM
	case 19:	// UFS
		return TRUE;
	default:	// USB, SD, MMC, 1394, VHD/virtual, SCSI (UAS may report it)...
		return FALSE;
	}
}

/* TRUE if the volume behind 'volume_path' (e.g. "\\.\C:") has an extent on disk 'DriveIndex' */
static BOOL TsVolumeIsOnDisk(const char* volume_path, DWORD DriveIndex)
{
	BYTE buf[sizeof(VOLUME_DISK_EXTENTS) + 32 * sizeof(DISK_EXTENT)] = { 0 };
	PVOLUME_DISK_EXTENTS extents = (PVOLUME_DISK_EXTENTS)buf;
	DWORD i, size = 0;
	BOOL r = FALSE;
	HANDLE h;

	h = CreateFileA(volume_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return FALSE;
	if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, buf, sizeof(buf), &size, NULL)) {
		for (i = 0; (i < extents->NumberOfDiskExtents) && (i < 32); i++) {
			if (extents->Extents[i].DiskNumber == DriveIndex - DRIVE_INDEX_MIN) {
				r = TRUE;
				break;
			}
		}
	}
	CloseHandle(h);
	return r;
}

static BOOL TsPathIsOnDisk(const char* path, DWORD DriveIndex)
{
	char volume_path[] = "\\\\.\\?:";

	if ((path == NULL) || (!isalpha((unsigned char)path[0])) || (path[1] != ':'))
		return FALSE;
	volume_path[4] = (char)toupper((unsigned char)path[0]);
	return TsVolumeIsOnDisk(volume_path, DriveIndex);
}

/*
 * The system disk is the one holding the running Windows (the Windows directory), or the
 * one holding the partition Windows booted from (the EFI/System partition), which may be
 * on a different disk.
 */
BOOL TsIsSystemDrive(DWORD DriveIndex)
{
	char path[MAX_PATH] = { 0 }, volume_path[MAX_PATH + 32];
	DWORD size = sizeof(path);

	if ((DriveIndex < DRIVE_INDEX_MIN) || (DriveIndex >= DRIVE_INDEX_MAX))
		return FALSE;
	if ((GetWindowsDirectoryA(path, sizeof(path)) > 0) && TsPathIsOnDisk(path, DriveIndex))
		return TRUE;
	if (TsPathIsOnDisk(system_dir, DriveIndex))
		return TRUE;
	// e.g. "\Device\HarddiskVolume1"
	memset(path, 0, sizeof(path));
	if ((RegGetValueA(HKEY_LOCAL_MACHINE, "SYSTEM\\Setup", "SystemPartition", RRF_RT_REG_SZ, NULL,
		path, &size) == ERROR_SUCCESS) && (_strnicmp(path, "\\Device\\", 8) == 0)) {
		static_sprintf(volume_path, "\\\\?\\GLOBALROOT%s", path);
		if (TsVolumeIsOnDisk(volume_path, DriveIndex))
			return TRUE;
	}
	return FALSE;
}

BOOL TsIsAppDrive(DWORD DriveIndex)
{
	return TsPathIsOnDisk(app_dir, DriveIndex);
}

const char* TsDriveTag(DWORD DriveIndex)
{
	if (TsIsSystemDrive(DriveIndex))
		return TsText("[!! דיסק המערכת !!] ", "[!! SYSTEM DISK !!] ");
	if (TsIsInternalDrive(DriveIndex) || TsIsAppDrive(DriveIndex))
		return TsText("[פנימי] ", "[INTERNAL] ");
	return "";
}

/*
 * The new checkbox shares the row of "List USB Hard Drives": that one is shrunk and ours
 * is right-aligned on the same line, so that no other control of the dialog needs to move.
 */
static void TsLayoutInternalCheckbox(HWND hDlg)
{
	HWND hUsb = GetDlgItem(hDlg, IDC_LIST_USB_HDD), hInt = GetDlgItem(hDlg, IDC_LIST_INTERNAL);
	RECT rc;
	int box, full, w_usb, w_int, h;

	if ((hUsb == NULL) || (hInt == NULL))
		return;
	SendMessage(hInt, WM_SETFONT, SendMessage(hUsb, WM_GETFONT, 0, 0), FALSE);
	GetWindowRect(hUsb, &rc);
	MapWindowPoints(NULL, hDlg, (POINT*)&rc, 2);
	full = rc.right - rc.left;
	h = rc.bottom - rc.top;
	box = GetSystemMetrics(SM_CXMENUCHECK) + GetSystemMetrics(SM_CXEDGE) * 4;
	w_usb = GetTextWidth(hDlg, IDC_LIST_USB_HDD) + box;
	w_int = GetTextWidth(hDlg, IDC_LIST_INTERNAL) + box;
	if (w_usb + w_int > full)
		w_int = max(full - w_usb, full / 3);
	SetWindowPos(hUsb, NULL, rc.left, rc.top, full - w_int, h, SWP_NOZORDER);
	SetWindowPos(hInt, hUsb, rc.left + full - w_int, rc.top, w_int, h, 0);
}

/*
 * "Edited by צול גאה" button, right-aligned on the "Drive Properties" header row (which only
 * holds the header text on the other side), so that nothing else in the dialog has to move.
 */
static void TsCreateCreditButton(HWND hDlg)
{
	HWND hHdr = GetDlgItem(hDlg, IDS_DRIVE_PROPERTIES_TXT), hRef = GetDlgItem(hDlg, IDS_DEVICE_TXT);
	HWND hSel = GetDlgItem(hDlg, IDC_SELECT), hBtn;
	wchar_t* wtext;
	RECT rh, rr, rs;
	int w, h;

	if ((hHdr == NULL) || (hRef == NULL) || (GetDlgItem(hDlg, IDC_TS_CREDIT) != NULL))
		return;
	wtext = utf8_to_wchar(TsText("נערך ע״י צול גאה ↗", "Edited by צול גאה ↗"));
	hBtn = CreateWindowExW(0, L"BUTTON", (wtext != NULL) ? wtext : L"TSOOLGEE",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, hDlg,
		(HMENU)(UINT_PTR)IDC_TS_CREDIT, GetModuleHandle(NULL), NULL);
	free(wtext);
	if (hBtn == NULL)
		return;
	SendMessage(hBtn, WM_SETFONT, SendMessage(hRef, WM_GETFONT, 0, 0), FALSE);
	GetWindowRect(hHdr, &rh);
	MapWindowPoints(NULL, hDlg, (POINT*)&rh, 2);
	GetWindowRect(hRef, &rr);
	MapWindowPoints(NULL, hDlg, (POINT*)&rr, 2);
	h = rh.bottom - rh.top;
	if (hSel != NULL) {
		GetWindowRect(hSel, &rs);
		h = min(h, rs.bottom - rs.top);
	}
	w = GetTextSize(hBtn, NULL).cx + GetSystemMetrics(SM_CXEDGE) * 8 + 8;
	SetWindowPos(hBtn, NULL, rr.right - w, rh.top + ((rh.bottom - rh.top) - h) / 2, w, h, SWP_NOZORDER);
	CreateTooltip(hBtn, TsText("מהדורה ערוכה של Rufus, זו אינה הגרסה המקורית - " TS_CREDIT_URL,
		"Edited version of Rufus, not the original - " TS_CREDIT_URL), -1);
}

void TsOnCreditClicked(HWND hDlg)
{
	ShellExecuteA(hDlg, "open", TS_CREDIT_URL, NULL, NULL, SW_SHOWNORMAL);
}

/*
 * About box: insert the "edited version" notice right after the upstream issues URL, and turn
 * the words "צול גאה" into a link (the link target is resolved by TsTranslateAboutLink()).
 */
void TsAboutAddCredit(HWND hEdit)
{
	SETTEXTEX st = { ST_SELECTION, CP_UTF8 };
	FINDTEXTEXW ft = { 0 };
	CHARFORMAT2W cf = { 0 };
	char rtf[1024];
	LRESULT pos;

	if (hEdit == NULL)
		return;
	// After the "report bugs" URL of upstream, or at the very end
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = -1;
	ft.lpstrText = L"github.com/pbatard/rufus/issues";
	pos = SendMessageW(hEdit, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&ft);
	if (pos >= 0)
		SendMessage(hEdit, EM_SETSEL, ft.chrgText.cpMax, ft.chrgText.cpMax);
	else
		SendMessage(hEdit, EM_SETSEL, -1, -1);
	static_sprintf(rtf, "{\\rtf1\\ansi\\line\\line\\b\\fs18 %s\\b0\\line\\fs18 %s צול גאה\\line %s\\line "
		TS_REPO_URL "/issues}",
		// Hebrew strings are wrapped in RLE/PDF marks, like Rufus' own RTL translations
		TsText("‫מהדורה ערוכה - זו אינה הגרסה המקורית של התוכנה‬", "TSOOLGEE edition - an edited version, not the original Rufus"),
		TsText("‫נערך על ידי‬", "Edited by"),
		TsText("‫דיווח על בעיות במהדורה הזו:‬", "Report issues with this edition at:"));
	SendMessageA(hEdit, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)rtf);

	// Make the credit name a link
	ft.chrg.cpMin = 0;
	ft.chrg.cpMax = -1;
	ft.lpstrText = TS_CREDIT_NAME_W;
	if (SendMessageW(hEdit, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&ft) >= 0) {
		SendMessage(hEdit, EM_EXSETSEL, 0, (LPARAM)&ft.chrgText);
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_LINK | CFM_UNDERLINE;
		cf.dwEffects = CFE_LINK | CFE_UNDERLINE;
		SendMessageW(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
	}
	SendMessage(hEdit, EM_SETSEL, -1, -1);
}

void TsTranslateAboutLink(wchar_t* link, size_t link_size)
{
	if ((link != NULL) && (wcscmp(link, TS_CREDIT_NAME_W) == 0))
		MultiByteToWideChar(CP_UTF8, 0, TS_CREDIT_URL, -1, link, (int)link_size);
}

void TsInitInternalCheckbox(HWND hDlg)
{
	HWND hInt = GetDlgItem(hDlg, IDC_LIST_INTERNAL);

	TsCreateCreditButton(hDlg);

	if (hInt == NULL)
		return;
	TsLayoutInternalCheckbox(hDlg);
	CheckDlgButton(hDlg, IDC_LIST_INTERNAL, enable_internal_drives ? BST_CHECKED : BST_UNCHECKED);
	CreateTooltip(hInt, TsText(
		"מציג גם את הכוננים הפנימיים של המחשב (SATA / NVMe / IDE), כולל הדיסק שעליו Windows מותקן. "
		"צריבה לכונן פנימי מוחקת את כל המידע שעליו! השתמשו רק אם אתם בטוחים.",
		"Also list the computer's INTERNAL drives (SATA / NVMe / IDE), including the disk Windows is "
		"installed on. Writing to an internal drive erases everything on it! USE AT YOUR OWN RISK."), 30000);
}

void TsEnableInternalCheckbox(HWND hDlg, BOOL enable)
{
	EnableWindow(GetDlgItem(hDlg, IDC_LIST_INTERNAL), enable);
}

void TsOnInternalCheckboxClicked(HWND hDlg)
{
	if (IsDlgButtonChecked(hDlg, IDC_LIST_INTERNAL) == BST_CHECKED) {
		if (MessageBoxExU(hDlg, TsText(
			"אתם עומדים להציג ברשימה את הכוננים הפנימיים של המחשב.\n\n"
			"• כונן פנימי הוא דיסק קשיח/SSD שנמצא בתוך המחשב - לא דיסק און קי.\n"
			"• בחירה בכונן כזה וצריבה אליו ימחקו את כל הקבצים, התוכנות והמערכת שעליו.\n"
			"• הדיסק שממנו Windows פועל יסומן ב-[!! דיסק המערכת !!], ולפני צריבה אליו יוצגו אזהרות נוספות.\n\n"
			"להציג את הכוננים הפנימיים?",
			"You are about to list the computer's INTERNAL drives.\n\n"
			"- An internal drive is a hard disk / SSD inside the computer - not a USB flash drive.\n"
			"- Selecting such a drive and writing to it erases ALL the files, programs and OS on it.\n"
			"- The disk Windows is running from is tagged [!! SYSTEM DISK !!] and requires additional "
			"confirmations before anything is written to it.\n\n"
			"List internal drives?"),
			TsText("אזהרה - כוננים פנימיים", "WARNING - Internal drives"),
			MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_IS_RTL, selected_langid) != IDYES) {
			CheckDlgButton(hDlg, IDC_LIST_INTERNAL, BST_UNCHECKED);
			return;
		}
		enable_internal_drives = TRUE;
	} else {
		enable_internal_drives = FALSE;
	}
	uprintf("%sListing of internal (non-USB) drives %s", enable_internal_drives ? "CAUTION: " : "",
		enable_internal_drives ? "enabled" : "disabled");
	GetDevices(0);
}

/*
 * Extra confirmations, on top of the regular "ALL DATA WILL BE DESTROYED" one.
 * Every box defaults to "No". Returns TRUE if the operation may proceed.
 */
BOOL TsConfirmTargetDrive(HWND hDlg, DWORD DriveIndex, const char* drive_name)
{
	char msg[2048];
	BOOL is_system = TsIsSystemDrive(DriveIndex);
	BOOL is_app = TsIsAppDrive(DriveIndex);
	BOOL is_internal = TsIsInternalDrive(DriveIndex);
	const char* name = (drive_name != NULL) ? drive_name : "?";

	if (!is_system && !is_app && !is_internal)
		return TRUE;
	uprintf("TSOOLGEE: target is %s%s%s- asking for extra confirmation", is_system ? "the SYSTEM disk " : "",
		is_app ? "the disk Rufus runs from " : "", is_internal ? "an internal drive " : "");

	if (is_internal || is_app) {
		static_sprintf(msg, TsText(
			"הכונן שנבחר הוא כונן פנימי של המחשב - לא דיסק און קי:\n\n%s\n\n"
			"כל המחיצות והקבצים בכונן הזה יימחקו לצמיתות.%s\n\n"
			"האם זה באמת הכונן שאתם רוצים למחוק?",
			"The selected drive is an INTERNAL drive of this computer - not a USB flash drive:\n\n%s\n\n"
			"All the partitions and files on this drive will be permanently erased.%s\n\n"
			"Is this really the drive you want to erase?"), name,
			is_app ? TsText("\nשימו לב: Rufus עצמו פועל מתוך הדיסק הזה.", "\nNote: Rufus itself is running from this disk.") : "");
		if (MessageBoxExU(hDlg, msg, TsText("אזהרה - כונן פנימי", "WARNING - Internal drive"),
			MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2 | MB_IS_RTL, selected_langid) != IDYES)
			return FALSE;
	}

	if (is_system) {
		static_sprintf(msg, TsText(
			"!!! זהו הדיסק שממנו מערכת ההפעלה (Windows) פועלת כרגע !!!\n\n%s\n\n"
			"המשך הפעולה ימחק את Windows, את כל התוכנות ואת כל הקבצים שלכם בדיסק הזה.\n"
			"סביר שהמחשב לא יעלה יותר, והפעולה גם עלולה להיכשל באמצע ולהשאיר את הדיסק הרוס.\n\n"
			"להמשיך בכל זאת?",
			"!!! THIS IS THE DISK THE OPERATING SYSTEM (Windows) IS RUNNING FROM !!!\n\n%s\n\n"
			"Continuing will erase Windows, all your programs and all your files on this disk.\n"
			"The computer will most likely no longer boot, and the operation may even fail half-way, "
			"leaving the disk damaged.\n\nContinue anyway?"), name);
		if (MessageBoxExU(hDlg, msg, TsText("סכנה - דיסק המערכת", "DANGER - System disk"),
			MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2 | MB_IS_RTL, selected_langid) != IDYES)
			return FALSE;
		if (MessageBoxExU(hDlg, TsText(
			"אישור אחרון: למחוק את הדיסק שממנו Windows פועל עכשיו?\n\n"
			"אחרי שהפעולה מתחילה אין דרך לבטל אותה.",
			"Last confirmation: erase the disk Windows is running from right now?\n\n"
			"There is no way to undo this once it starts."),
			TsText("אישור אחרון - דיסק המערכת", "Last confirmation - System disk"),
			MB_YESNO | MB_ICONERROR | MB_DEFBUTTON2 | MB_IS_RTL, selected_langid) != IDYES)
			return FALSE;
		uprintf("TSOOLGEE: user confirmed writing to the SYSTEM disk!");
	}
	return TRUE;
}

/*
 * Accept a file signed with the upstream key (DBX, GRUB, Syslinux, Fido... from rufus.ie) or with
 * the TSOOLGEE key (the update files and executables of this edition). The validator wipes the
 * buffer when a signature does not match, so the first attempt is made on a copy.
 */
BOOL TsValidateOpensslSignature(BYTE* pbBuffer, DWORD dwBufferLen, BYTE* pbSignature, DWORD dwSigLen,
	const uint8_t* upstream_modulus, size_t upstream_modulus_size)
{
	BYTE* copy = (dwBufferLen != 0) ? (BYTE*)malloc(dwBufferLen) : NULL;
	BOOL r;

	if (copy == NULL)
		return FALSE;
	memcpy(copy, pbBuffer, dwBufferLen);
	r = ValidateOpensslSignatureWithKey(copy, dwBufferLen, pbSignature, dwSigLen,
		upstream_modulus, upstream_modulus_size);
	free(copy);
	if (r)
		return TRUE;
	r = ValidateOpensslSignatureWithKey(pbBuffer, dwBufferLen, pbSignature, dwSigLen,
		ts_rsa_pubkey_modulus, sizeof(ts_rsa_pubkey_modulus));
	if (r)
		uprintf("(signature matches the " TS_EDITION_NAME " key)");
	return r;
}
