#pragma once

// internal name of the service
#define SZSERVICENAME        _T("syslogd")
// displayed name of the service
#define SZSERVICEDISPLAYNAME _T("syslogd")
// list of service dependencies - "dep1\0dep2\0\0"
#define SZDEPENDENCIES       _T("\0\0")

// Service Controller Function 
void WINAPI ServiceCtrl(DWORD dwCtrlCode);
// Service Main Function
void WINAPI ServiceMain(DWORD dwArgc, LPSTR *lpszArgv);

// サービス・インストール
BOOL CmdInstallService();
// サービス・アンインストール
BOOL CmdRemoveService();

// サービス初期化処理
bool InitializeService();
// サービスクリーン処理
void FinalizeService();

void OnServiceStart(DWORD dwArgc, LPSTR *lpszArgv);
void OnServiceContinue();
void OnServiceInterrogate();
void OnServiceNetBindAdd();
void OnServiceNetBindDisable();
void OnServiceNetBindEnable();
void OnServiceNetBindRemove();
void OnServiceParamChange();
void OnServicePause();
void OnServiceShutdown();
void OnServiceStop();

BOOL ReportStatusToSCMgr(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

void LogEventEx(int id, LPCTSTR pszMessage = NULL, WORD type = EVENTLOG_INFORMATION_TYPE);

