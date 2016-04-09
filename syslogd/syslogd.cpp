// syslogd.cpp : コンソール アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "syslogd.h"
#include "tstring.h"
#include "syslog_daemon.hpp"

#include <algorithm>
#include <iostream>

#include <boost/scoped_array.hpp>
#include <boost/foreach.hpp>

SERVICE_STATUS          g_ssService;
SERVICE_STATUS_HANDLE   g_hService			=	NULL;
DWORD                   g_dwError				=	0;
HINSTANCE								g_hInstance			=	NULL;
HANDLE									g_hStopEvent		=	NULL;

void InitGlobal() {
	g_hService			=	NULL;
	g_dwError				=	0;
	g_hInstance			=	NULL;
	g_hStopEvent		=	NULL;
}

const TCHAR* RegRoot = _T("Software\\hunes\\syslogd");
const TCHAR* RegLogDir = _T("logdir");

bool read_reg( tstring& logdir ) {
	HKEY hResult;
	LONG nRes	=	RegOpenKeyEx(
			HKEY_LOCAL_MACHINE,
			RegRoot,
			REG_OPTION_NON_VOLATILE, KEY_READ,
			&hResult
	);
	if( nRes == ERROR_SUCCESS ) {
		DWORD dwLen, dwType = REG_SZ;
		nRes	=	RegQueryValueEx( hResult, RegLogDir, 0, &dwType, NULL, &dwLen );
		if( nRes == ERROR_SUCCESS ) {
			logdir.resize( dwLen );
			nRes	=	RegQueryValueEx( hResult, RegLogDir, 0, &dwType, (LPBYTE)&logdir[0], &dwLen );
		}
		RegCloseKey( hResult );
	}
	if( nRes != ERROR_SUCCESS ) {
		terr << _T("Fatal Error: Can't read from registory. status = ") << GetLastError() << std::endl;
		logdir.clear();
		return false;
	}
	return true;
}

bool write_reg( const TCHAR* logdir ) {
	HKEY hResult;
	DWORD dwResult;
	LONG nRes	=	RegCreateKeyEx( 
			HKEY_LOCAL_MACHINE, 
			RegRoot,
			0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
			&hResult, &dwResult
	);
	if( nRes == ERROR_SUCCESS ) {
		nRes	=	RegSetValueEx( hResult, RegLogDir, 0, REG_SZ, (LPBYTE)logdir, stlen(logdir) * sizeof(TCHAR) );
		RegCloseKey( hResult );
	}
	if( nRes != ERROR_SUCCESS ) {
		terr << _T("Fatal Error: Can't write to registory. status = ") << GetLastError() << std::endl;
		return false;
	}
	return true;
}

const TCHAR* usages[] = {
	_T("syslogd install            : install syslogd service."),
	_T("syslogd uninstall          : uninstall syslogd service."),
	_T("syslogd reg [logdir]       : set log directory."),
	_T("syslogd service            : start service."),
};

void show_usage() {
	BOOST_FOREACH( const TCHAR* msg, usages ) {
		tout << msg << std::endl;
	}
}

int _tmain(int argc, TCHAR* argv[] ) {
	int nRetCode = 0;

	g_hInstance	=	::GetModuleHandle(NULL);
	if( !AfxWinInit( g_hInstance, NULL, ::GetCommandLine(), 0) ) {
		terr << _T("Fatal Error: Can't init MFC.") << std::endl;
		nRetCode	=	1;
	} else {
		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{ SZSERVICENAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
			{ NULL, NULL }
		};

		if( argc > 1 ) {
			if ( sticmp( _T("install"), argv[1] ) == 0 ) {
				if( CmdInstallService() ) {
					tout << _T("Installed service: ") << SZSERVICENAME << std::endl;
				} else {
					terr << _T("Fatal Error: Service ") << SZSERVICENAME << _T(" can't install.") << std::endl;
				}
			} else if( sticmp( _T("uninstall"), argv[1] ) == 0 ) {
				if( CmdRemoveService() ) {
					tout << _T("Removed service: ") << SZSERVICENAME << std::endl;
				} else {
					terr << _T("Fatal Error: Service ") << SZSERVICENAME << _T(" can't remove.") << std::endl;
				}
			} else if( sticmp( _T("reg"), argv[1] ) == 0 && argc == 3 ) {
				if( !write_reg( argv[2] ) )	nRetCode	=	1;
			} else if( sticmp( _T("service"), argv[1] ) == 0 ) {
				StartServiceCtrlDispatcher(dispatchTable);
			} else {
				show_usage();
				nRetCode	=	1;
			}
			return nRetCode;
		}
		show_usage();
		nRetCode	=	1;
	}

	return nRetCode;
}

BOOL ReportServiceStatus(
	DWORD	dwCurrentState,
	DWORD	dwCheckPoint		=	0,
	DWORD	dwWaitHint			=	0
) {
	g_ssService.dwCurrentState						=	dwCurrentState;
	g_ssService.dwWin32ExitCode						=	NO_ERROR;
	g_ssService.dwServiceSpecificExitCode	=	0;
	g_ssService.dwCheckPoint							=	dwCheckPoint;
	g_ssService.dwWaitHint								=	dwWaitHint;
	BOOL result	=	SetServiceStatus( g_hService, &g_ssService );
	if( !result ) {
		g_dwError	=	GetLastError();
	}
	return result;
}

void ErrorServiceStatus(
	DWORD	dwErrorCode
) {
	g_ssService.dwCurrentState						=	SERVICE_STOPPED;
	g_ssService.dwWin32ExitCode						=	ERROR_SERVICE_SPECIFIC_ERROR;
	g_ssService.dwServiceSpecificExitCode	=	dwErrorCode;
	g_ssService.dwCheckPoint							=	0;
	g_ssService.dwWaitHint								=	0;
	SetServiceStatus( g_hService, &g_ssService );
}

void WINAPI ServiceMain(
	DWORD		dwArgc,
	LPSTR*	lpszArgv
) {
	InitGlobal();

	tstring logdir;
	if( !read_reg( logdir ) ) {
		g_dwError	=	GetLastError();
		ErrorServiceStatus( g_dwError );
		return;
	}

	SetCurrentDirectory( logdir.c_str() );

	g_hService	=	RegisterServiceCtrlHandler( SZSERVICENAME, ServiceCtrl );

	if( !g_hService ) {
		g_dwError	=	GetLastError();
		ErrorServiceStatus( g_dwError );
		return;
	}
	g_ssService.dwServiceType							=	SERVICE_WIN32_OWN_PROCESS;
	g_ssService.dwControlsAccepted				=	SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

	DWORD dwProgress	=	0;	// for Finalize
	if( InitializeService() ) {
		boost::asio::io_service	io_service;
		syslog_daemon	sd( io_service );
		if( sd.run() ) {
			if( ReportServiceStatus( SERVICE_RUNNING ) ) {
				LogEventEx( 0, _T("Syslogd started") );
				WaitForSingleObject( g_hStopEvent, INFINITE );
			}
		} else {
			LogEventEx(  0, _T("Failed to start syslogd.") );
		}
		ReportServiceStatus( SERVICE_STOP_PENDING, ++dwProgress, 10 );
		sd.stop();
		ReportServiceStatus( SERVICE_STOP_PENDING, ++dwProgress, 10 );
	}
	CloseHandle( g_hStopEvent );

	g_ssService.dwControlsAccepted				=	0;

	ReportServiceStatus( SERVICE_STOPPED, ++dwProgress, 10 );
	if( g_dwError )	ErrorServiceStatus( g_dwError );
	else		LogEventEx( 0, _T("Syslogd stopped") );

	return;
}


void WINAPI ServiceCtrl(
	DWORD		dwCtrlCode
) {
	switch( dwCtrlCode ) {
	// Notifies a paused service that it should resume. 
	case SERVICE_CONTROL_CONTINUE:				OnServiceContinue();				break;
	// Notifies a service that it should report its current status information to the service control manager. 
	case SERVICE_CONTROL_INTERROGATE:			OnServiceInterrogate();			break;
	// Notifies a network service that there is a new component for binding. 
	// The service should bind to the new component. 
	// However, this control code has been deprecated; use Plug and Play functionality instead.
	// Windows NT:  This value is not supported. 
	case SERVICE_CONTROL_NETBINDADD:			OnServiceNetBindAdd();			break;
	// Notifies a network service that one of its bindings has been disabled. 
	// The service should reread its binding information and remove the binding. 
	// However, this control code has been deprecated; use Plug and Play functionality instead.
	// Windows NT:  This value is not supported. 
	case SERVICE_CONTROL_NETBINDDISABLE:	OnServiceNetBindDisable();	break;
	// Notifies a network service that a disabled binding has been enabled. 
	// The service should reread its binding information and add the new binding. 
	// However, this control code has been deprecated; use Plug and Play functionality instead.
	// Windows NT:  This value is not supported. 
	case SERVICE_CONTROL_NETBINDENABLE:		OnServiceNetBindEnable();		break;
	// Notifies a network service that a component for binding has been removed. 
	// The service should reread its binding information and unbind from the removed component. 
	// However, this control code has been deprecated; use Plug and Play functionality instead.
	// Windows NT:  This value is not supported. 
	case SERVICE_CONTROL_NETBINDREMOVE:		OnServiceNetBindRemove();		break;
	// Notifies a service that its startup parameters have changed. 
	// The service should reread its startup parameters.
	case SERVICE_CONTROL_PARAMCHANGE:			OnServiceParamChange();			break;
	// Notifies a service that it should pause. 
	case SERVICE_CONTROL_PAUSE:						OnServicePause();						break;
	// Notifies a service that the system is shutting down so the service can perform cleanup tasks. 
	case SERVICE_CONTROL_SHUTDOWN:				OnServiceShutdown();				break;
	// Notifies a service that it should stop.
	// If a service accepts this control code, it must stop upon receipt. 
	// After the SCM sends this control code, it will not send other control codes.
	// Windows XP/2000:  If the service returns NO_ERROR and continues to run, 
	// it continues to receive control codes. 
	// This behavior changed starting with Windows Server 2003 and Windows XP SP2.
	case SERVICE_CONTROL_STOP:						OnServiceStop();						break;
	}
}

// サービス初期化処理
bool InitializeService() {
	DWORD dwProgress	=	0;
	if( !ReportServiceStatus( SERVICE_START_PENDING, ++dwProgress, 100 ) )	return false;
	g_hStopEvent	=	CreateEvent( NULL, TRUE, FALSE, NULL );
	if( !g_hStopEvent ) {
		g_dwError	=	GetLastError();
		return false;
	}
	if( !ReportServiceStatus( SERVICE_START_PENDING, ++dwProgress, 100 ) )	return false;
	return true;
}

void OnServiceContinue() {}
void OnServiceInterrogate() {}
void OnServiceNetBindAdd() {}
void OnServiceNetBindDisable() {}
void OnServiceNetBindEnable() {}
void OnServiceNetBindRemove() {}
void OnServiceParamChange() {}
void OnServicePause() {}
void OnServiceShutdown() {
	//LogEventEx( 0, _T("OnServiceShutdown") );
	OnServiceStop();
}

void OnServiceStop() {
	//LogEventEx( 0, _T("OnServiceStop") );
	BOOST_ASSERT( g_hStopEvent );
	SetEvent( g_hStopEvent );
}

//! サービスをインストール
BOOL CmdInstallService() {
	BOOL	result	=	TRUE;
	SC_HANDLE	schService;
	SC_HANDLE	schManager;

	TCHAR	szPath[ 600 ];
	if( GetModuleFileName( NULL, szPath, 512 ) == 0 ) {
		terr << _T("Fatal Error: Can't get module file name.") << std::endl;
		return FALSE;
	}

	tstring	command_path = szPath;
	command_path +=	_T(" service");

	schManager	=	OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if( !schManager ) {
		terr << _T("Fatal Error: Can't open service control manager.") << std::endl;
		return FALSE;
	}

	schService	=	CreateService(
		schManager,									// SCManager database
		SZSERVICENAME,							// name of service
		SZSERVICEDISPLAYNAME,				// name to display
		SERVICE_ALL_ACCESS,         // desired access
		SERVICE_WIN32_OWN_PROCESS,  // service type
		SERVICE_AUTO_START,					// start type
		SERVICE_ERROR_NORMAL,				// error control type
		command_path.c_str(), 			// service's binary
		NULL,												// no load ordering group
		NULL,												// no tag identifier
		SZDEPENDENCIES,							// dependencies
		NULL,												// LocalSystem account
		NULL												// no password
	);

	{	// イベントＩＤの登録
		CRegKey key;
		LONG lRes = key.Create( 
			HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\syslogd") 
		);
		if( lRes == ERROR_SUCCESS ) {
			lRes = key.SetStringValue( _T("EventMessageFile"), szPath );
			if( lRes == ERROR_SUCCESS ) {
				DWORD dwTypes	=	EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
				key.SetDWORDValue( _T("TypesSupported"), dwTypes );
			}
		}
	}


	if( !schService )		result	=	FALSE;
	else								CloseServiceHandle( schService );

	CloseServiceHandle( schManager);
	return result;	
}

//! サービスを削除
BOOL CmdRemoveService() {
	SC_HANDLE	schService;
	SC_HANDLE	schManager;

	schManager	=	OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	if( !schManager ) {
		terr << _T("Fatal Error: Can't open service control manager.") << std::endl;
		return FALSE;
	}

	schService	=	OpenService( schManager, SZSERVICENAME, SERVICE_ALL_ACCESS );
	if( !schService ) {
		terr << _T("Fatal Error: Can't open service.") << std::endl;
		CloseServiceHandle( schManager );
		return FALSE;
	}

	ControlService( schService, SERVICE_CONTROL_STOP, &g_ssService );

	if( QueryServiceStatus( schService, &g_ssService ) ) {
		if( g_ssService.dwCurrentState != SERVICE_STOPPED ) {
			terr << _T("Fatal Error: Service is running.") << std::endl;
			CloseServiceHandle( schService );
			CloseServiceHandle( schManager );
			return FALSE;
		}
	} else {
		terr << _T("Fatal Error: Can't get service status.") << std::endl;
	}

	BOOL result	=	DeleteService( schService );

	CloseServiceHandle( schService );
	CloseServiceHandle( schManager );

	return result;
}

void LogEventEx(int id, LPCTSTR pszMessage, WORD type ) {
	HANDLE hEventSource	=	RegisterEventSource( NULL, SZSERVICENAME );
	if( hEventSource ) {
		ReportEvent(
			hEventSource, 
			type,
			(WORD)0,
			id,
			NULL,
			(WORD)(pszMessage != NULL ? 1 : 0),
			0,
			pszMessage != NULL ? &pszMessage : NULL,
			NULL
		);
		DeregisterEventSource(hEventSource);
	}
}

