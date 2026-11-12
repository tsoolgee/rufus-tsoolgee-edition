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

#include <windows.h>
#include <stdint.h>

#pragma once

#define TS_EDITION_NAME             "TSOOLGEE"
#define TS_REPO_URL                 "https://github.com/tsoolgee/rufus-tsoolgee-edition"

/*
 * Update check: the fork's own (signed) version files are assets of its latest release.
 * NB: not raw.githubusercontent.com - filtered networks (e.g. NetFree) block the .sig files there.
 */
#define TS_UPDATE_SERVER            "https://github.com"
#define TS_UPDATE_PATH              "tsoolgee/rufus-tsoolgee-edition/releases/latest/download/"

/* New checkbox, sitting on the same row as "List USB Hard Drives" */
#define IDC_LIST_INTERNAL           1990
/* Credit button, on the "Drive Properties" header row */
#define IDC_TS_CREDIT               1991

/* Credit: this is an edited version of Rufus, by tsoolgee */
#define TS_CREDIT_URL               "https://tsoolgee.uk"
#define TS_CREDIT_NAME_W            L"צול גאה"
#define TS_TITLE_SUFFIX             "- TSOOLGEE edition"	// IsAlphaOrBeta() ends with a space

/* Internal (non USB) drives are only listed when the user explicitly enabled them */
extern BOOL enable_internal_drives;
#define TS_LIST_INTERNAL_DRIVES     (enable_internal_drives)

/* Hebrew if the UI is in Hebrew, English otherwise */
const char* TsText(const char* he, const char* en);

/* Drive classification */
void TsClearInternalMarks(void);
void TsMarkInternal(DWORD DriveIndex);
BOOL TsIsInternalDrive(DWORD DriveIndex);
BOOL TsIsSystemDrive(DWORD DriveIndex);
BOOL TsIsAppDrive(DWORD DriveIndex);
const char* TsDriveTag(DWORD DriveIndex);

/* UI */
void TsInitInternalCheckbox(HWND hDlg);
void TsOnInternalCheckboxClicked(HWND hDlg);
void TsEnableInternalCheckbox(HWND hDlg, BOOL enable);
void TsOnCreditClicked(HWND hDlg);
void TsAboutAddCredit(HWND hEdit);
void TsTranslateAboutLink(wchar_t* link, size_t link_size);
BOOL TsConfirmTargetDrive(HWND hDlg, DWORD DriveIndex, const char* drive_name);

/* Signatures: upstream files are signed with the upstream key, TSOOLGEE update files with ours */
BOOL TsValidateOpensslSignature(BYTE* pbBuffer, DWORD dwBufferLen, BYTE* pbSignature, DWORD dwSigLen,
	const uint8_t* upstream_modulus, size_t upstream_modulus_size);
