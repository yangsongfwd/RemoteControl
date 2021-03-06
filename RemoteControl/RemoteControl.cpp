///////////////////////////////////////////////////////////////
//
// FileName	: RemoteControl.cpp
// Creator	: 杨松
// Date		: 2013年2月27日，20:10:26
// Comment	: 服务器应用程序类实现
//
//////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "RemoteControl.h"
#include "MainFrm.h"

#include "RemoteControlDoc.h"
#include "RemoteControlView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CRemoteControlApp

BEGIN_MESSAGE_MAP(CRemoteControlApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CRemoteControlApp::OnAppAbout)
	// 基于文件的标准文档命令
// 	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
// 	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
ON_COMMAND(ID_ABOUT, &CRemoteControlApp::OnAbout)
END_MESSAGE_MAP()


// CRemoteControlApp 构造

CRemoteControlApp::CRemoteControlApp()
:m_hSingleInsMutex(NULL)
{
	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的一个 CRemoteControlApp 对象

CRemoteControlApp theApp;


// CRemoteControlApp 初始化

BOOL CRemoteControlApp::InitInstance()
{
	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);


	m_hSingleInsMutex = CreateMutex(NULL , FALSE , _T("__REMOUTE_CTRL_SERVER_SINGLE_INS_METEX__"));
	if ( NULL != m_hSingleInsMutex)
	{
		if (ERROR_ALREADY_EXISTS == GetLastError())
		{//已经存在了

			CString strMsg;
			CString strCapture;
			strCapture.LoadString(IDS_NOTIFY);
			strMsg.LoadString(IDS_APP_IS_RUNNING);
			MessageBox(NULL , strMsg , strCapture , MB_OK|MB_ICONWARNING);
			
			CloseHandle(m_hSingleInsMutex);
			return FALSE;
		}
	}
	else
	{//直接创建互斥量失败？！
		return FALSE;
	}



	CWinApp::InitInstance();

	if (!AfxSocketInit())
	{
		AfxMessageBox(IDP_SOCKETS_INIT_FAILED);
		return FALSE;
	}

	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_lgdiplusToken, &gdiplusStartupInput, NULL);

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));
	LoadStdProfileSettings(0);  // 加载标准 INI 文件选项(包括 MRU)
	// 注册应用程序的文档模板。文档模板
	// 将用作文档、框架窗口和视图之间的连接
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CRemoteControlDoc),
		RUNTIME_CLASS(CMainFrame),       // 主 SDI 框架窗口
		RUNTIME_CLASS(CRemoteControlView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);



	// 分析标准外壳命令、DDE、打开文件操作的命令行
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);


	// 调度在命令行中指定的命令。如果
	// 用 /RegServer、/Register、/Unregserver 或 /Unregister 启动应用程序，则返回 FALSE。
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// 唯一的一个窗口已初始化，因此显示它并对其进行更新
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	// 仅当具有后缀时才调用 DragAcceptFiles
	//  在 SDI 应用程序中，这应在 ProcessShellCommand 之后发生
	return TRUE;
}



// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// 用于运行对话框的应用程序命令
void CRemoteControlApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}


// CRemoteControlApp 消息处理程序


int CRemoteControlApp::ExitInstance()
{
	//清理GPI+库
	GdiplusShutdown(m_lgdiplusToken);

	return CWinApp::ExitInstance();
}

void CRemoteControlApp::OnAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}
