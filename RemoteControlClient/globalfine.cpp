///////////////////////////////////////////////////////////////
//
// FileName	: globalfine.cpp 
// Creator	: 杨松
// Date		: 2013年3月13日, 16:19:16
// Comment	: 客户端全局函数定义代码
//
//////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "globalfine.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void InitSockaddr( sockaddr_in &serverAddr, USHORT serverPort, CString strServerIP ) 
{
	char cServerIP[20] = {0};
	if (strServerIP.GetLength())
	{
		::wcstombs( cServerIP , strServerIP , 20);
	}

	memset(&serverAddr , 0 , sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = serverPort?htons(serverPort):0;
	serverAddr.sin_addr.s_addr = strServerIP.GetLength()?inet_addr(cServerIP):htonl(INADDR_ANY);
}

DWORD WINAPI CaptureScreenThread(LPVOID wParam)
{
	PCaptureScreenThreadContext pContext = (PCaptureScreenThreadContext)wParam;
	ASSERT(pContext != NULL);

	sockaddr_in	serverAddr = {0};
	long		threadState = 0;
	LARGE_INTEGER move = {0};
	HDC			hDesktopDC	= ::GetDC(GetDesktopWindow());
	AutoSizeStream* imgData = AutoSizeStream::CreateAutoSizeStream();
	char*		buf = NULL;

	pContext->sSocket = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
	if(pContext->sSocket == INVALID_SOCKET) //创建套接字失败
	{//创建套接字失败
		pContext->pWnd->PostMessage(WM_SCREEN_SOCKET_ERR	, 0 ,0);
		pContext->sSocket = 0;	
		goto exit_print_screnn; 
	}
	else
	{
		sockaddr_in local_addr = {0};
		int sin_size = sizeof(local_addr);
		InitSockaddr(local_addr , 0 , 0);

		if ((bind(pContext->sSocket , (struct sockaddr *) (&local_addr) , sin_size )) == -1)
		{//绑定失败
			pContext->pWnd->PostMessage(WM_SCREEN_SOCKET_ERR	, 0 ,0);
			goto exit_print_screnn;
		}
	}

	//初始化Socket
	InitSockaddr(serverAddr, pContext->uScreenPort, pContext->strServerIP);

	CLSID				encoderClsid;
	EncoderParameters	encoderParameters;
	ULONG				quality;
	buf					= new char[SEND_BUF_SIZE];

	//获取编码器clsid
	GetEncoderClsid(L"image/jpeg" , &encoderClsid);
	//初始化编码器参数
	InitEncoderParameter(encoderParameters , quality);

	//开始抓屏了
	while(threadState = pContext->lThreadState)
	{
		if ( 2 == threadState )
		{
			ResetEvent(pContext->hEvent);
			WaitForSingleObject(pContext->hEvent , INFINITE);
			if (0 == pContext->lThreadState)
			{//要退出线程了
				goto exit_print_screnn;
			}
		}
		static const int DATALEN_MSG = sizeof(RCMSG_BUFF) - 1 + sizeof(UINT);

		//获取图像质量
		quality = pContext->lScreenQuality;
		//抓屏
		CaptureDesktop(pContext->lScreenW , pContext->lScreenH
			, hDesktopDC , imgData , encoderClsid , encoderParameters);

		//将数据发出去
		UINT imgDataSize	= (UINT)imgData->GetValidSize().QuadPart;
		UINT sendTotal		= 0;	//总共发了的数据大小
		ULONG sendOne		= 0;	//一次发送的大小
		PRCMSG_BUFF pMsg	= PRCMSG_BUFF(buf);
		
		{//发送一下数据的长度
		pMsg->msgHead.size	= DATALEN_MSG;
		pMsg->msgHead.type	= MT_SCREEN_DATA_SIZE_C;
		memcpy(pMsg->buf , &imgDataSize , sizeof(UINT));
		sendto(pContext->sSocket , (char*)pMsg , DATALEN_MSG , 0 , (sockaddr*)&serverAddr , sizeof(serverAddr));
		}

		//循环发送数据
		while(sendTotal < imgDataSize)
		{
			::memset(buf , 0 , SEND_BUF_SIZE);//初始化缓存
			imgData->Read(buf , SEND_BUF_SIZE , &sendOne);
			if( sendOne == 0)
				break;
			else if (DATALEN_MSG == sendOne)//避开发送一个数据报时大小刚好是数据大小消息大小
				++sendOne;

			sendOne = sendto(pContext->sSocket , buf , sendOne , 0 , (sockaddr*)&serverAddr , sizeof(serverAddr));
			if(sendOne == SOCKET_ERROR)
			{//发送时套接字错误
				pContext->pWnd->PostMessage(WM_SCREEN_SOCKET_ERR , 0 , 0);
				goto exit_print_screnn;
			}
			sendTotal += sendOne;
		}
		
		imgData->CleanValidData();//清理有效数据，避免每次读取数据是都来分配空间
		Sleep(pContext->lFluency);
	}

exit_print_screnn:
	closesocket(pContext->sSocket);
	pContext->sSocket = 0;
	ReleaseDC(GetDesktopWindow() , hDesktopDC);
	imgData->Release();
	if (NULL != buf)
		delete[] buf;
	return 0;
}

DWORD WINAPI RecvServerPushedDesktopThread( LPVOID wParam )
{
	PRecvPushedDesktopThreadContext pContext = (PRecvPushedDesktopThreadContext)wParam;
	ASSERT(NULL != pContext);

	//CMainFrame* pFram = (CMainFrame*)wParam;
	SOCKET		localSock;
	int			err = 0;
	HDC			hDC = ::GetDC(pContext->pScreenView->GetSafeHwnd());
	CRect		viewRect;
	char*		buf = NULL;
	AutoSizeStream* screenData = AutoSizeStream::CreateAutoSizeStream();

	localSock = ::socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
	if(localSock == INVALID_SOCKET) 
	{//创建套接字失败
		//pFram->PostMessage(WM_SCREEN_SOCKET_ERR	, 0 ,0);
		localSock = 0;	
		goto recv_push_exit; 
	}
	else
	{
		// 用来绑定套接字
		SOCKADDR_IN sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(SERVER_PUSH_DESKTOP_PORT);
		sin.sin_addr.s_addr = 0;

		// 绑定套接字  
		err = bind(localSock, (SOCKADDR*)&sin, sizeof(SOCKADDR));  
		if(SOCKET_ERROR == err)  
		{//绑定失败 
			goto recv_push_exit;
		} 
	}

	//缓存
	buf = new char[RECV_BUF_SIZE];

	while(pContext->lThreadState)
	{
		static const int DATALEN_MSG = sizeof(RCMSG_BUFF) - 1 + sizeof(UINT);
		//文件的
		UINT nScreenDataSize = 0;
		UINT nRcvTotal		 = 0;
		UINT nRcvOne		 = 0; 
		LARGE_INTEGER move	 = {0};

		//接收此次要传输的数据的大小
		if((nRcvOne = recvfrom(localSock , buf , RECV_BUF_SIZE , 0 , NULL , NULL )) == SOCKET_ERROR)
		{//接收此次数据的大小，失败
			pContext->pFrame->PostMessage(WM_SCREEN_SOCKET_ERR);
			int err = WSAGetLastError();
			goto recv_push_exit; 
		}else if (nRcvOne == 0)	
		{//连接已经断开
			//pFram->PostMessage(WM_SCREEN_SOCK_UNCON);
			goto recv_push_exit;
		}else if(nRcvOne != DATALEN_MSG) 
			continue; //接到了一次无效数据
		else{//获得了数据长度
			if(MT_SCREEN_DATA_SIZE_C == PRCMSG_BUFF(buf)->msgHead.type)
				nScreenDataSize = *((UINT*)(PRCMSG_BUFF(buf)->buf));
			else //无效数据
				continue;
		}

		while(nRcvTotal < nScreenDataSize)
		{
			nRcvOne = recvfrom( localSock , buf , RECV_BUF_SIZE , 0 , NULL , NULL );
			if(SOCKET_ERROR == nRcvOne) 
			{//接受数据时  套接字错误
				//pFram->PostMessage(WM_SCREEN_SOCK_UNCON);
				goto recv_push_exit; 
			} 

			if (nRcvOne == DATALEN_MSG)
			{//在有数据包丢失的情况可能会进入这里,在客户端应避免发送大小为DATALEN_MSG的数据
				if(MT_SCREEN_DATA_SIZE_C == PRCMSG_BUFF(buf)->msgHead.type)
				{
					nScreenDataSize = *((UINT*)(PRCMSG_BUFF(buf)->buf));
					nRcvTotal = 0;
					screenData->Seek(move , STREAM_SEEK_SET , NULL);
					continue;
				}
			}
			if ((nRcvTotal + nRcvOne) == 1 + nScreenDataSize)
			{//一个数据包只有DATALEN_MSG 所以多发了一个字节
				--nRcvOne;
			}
			screenData->Write(buf , nRcvOne , NULL );
			nRcvTotal += nRcvOne;
		}

		//获取绘图区域
		pContext->pScreenView->GetWindowRect(&viewRect);
		viewRect.MoveToXY(0 , 0);
		
		screenData->Seek(move , STREAM_SEEK_SET , NULL);

		//使用GDI+绘图
		DrawPictureWithGDIPP(screenData, hDC, viewRect);
		//使用OLE
		//DrawPictureWithOle(screenData, hDC, viewRect);

		screenData->CleanValidData();
	}

recv_push_exit:
	screenData->Release();
	if (hDC)
		ReleaseDC(pContext->pScreenView->GetSafeHwnd() , hDC);
	//pFram->PostMessage(WM_SCREEN_THREAD_EXIT , 0 , 0);
	closesocket(localSock);
	if (NULL != buf)
		delete[] buf;

	return 1;
}

BOOL Shutdown( SHUTDOWN sd )
{
	HANDLE       hToken;
	TOKEN_PRIVILEGES tkp;
	//这个函数的作用是打开一个进程的访问令牌
	if(!OpenProcessToken(GetCurrentProcess() , TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY , &hToken))
	{//获取当前进程访问令牌失败
		return FALSE;
	}
	//修改进程的权限
	LookupPrivilegeValue(NULL,SE_SHUTDOWN_NAME , &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1; //赋给本进程特权
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	//通知Windows NT修改本进程的权利
	AdjustTokenPrivileges(hToken , FALSE , &tkp , 0 ,(PTOKEN_PRIVILEGES)NULL , 0 );
	if(GetLastError() != ERROR_SUCCESS)
	{//操作失败
		CloseHandle(hToken);
		return FALSE;
	}
	CloseHandle(hToken);

	if(sd == SD_SHUTDOWN)
	{//关机
		return InitiateSystemShutdown(NULL , _T("计算机将在30秒后关闭") , 30 , TRUE , FALSE);
	}
	else if(sd == SD_RESTART)
	{//重启
		return InitiateSystemShutdown(NULL , _T("计算机在30秒之后重启") , 30 , TRUE , TRUE);
	}
	else if( sd == SD_LOGIN_OUT)
	{//注销
		return ExitWindowsEx(EWX_LOGOFF , 0);
	}
	else
		ASSERT(FALSE);
	return TRUE;
}

DWORD WINAPI ReadCMDThread(LPVOID wParam)
{//读取会先消息

	PCMDContext pContext = (PCMDContext)wParam;
	ASSERT(NULL != pContext);

	PRCMSG_BUFF pMsg	= NULL;
	char*		buf		= NULL;

	//用来发送命令行的回显数据的套接字
	//为了避免同步的麻烦，所以在这里直接发送
	//而不是由主消息套接字来发送
	SOCKET		localSock  = {0};
	sockaddr_in serverAddr = {0};

	localSock = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
	if(localSock == INVALID_SOCKET) 
	{//创建套接字失败
		//TODO 需要做错误处理
		return 0; 
	}
	else
	{//创建套接字成功
		InitSockaddr(serverAddr , SERVER_MSG_PORT , pContext->strIP);
	}

	//回显信息缓存
	buf = new char[SEND_BUF_SIZE];
	pMsg = (PRCMSG_BUFF)buf;
	pMsg->msgHead.type = MT_CMD_LINE_DATA_C;

	//由主线程决定是否要 继续运行
	while(0 != pContext->lReadCMDThreadState)
	{
		//一次读取的数据的大小
		DWORD dwSize = 0;

		//判断是否有数据可以读取
		if(0 == ::GetFileSize(pContext->hRead , 0 ))
		{//没有数据可以读取
			//ReleaseMutex(hMutex);
			Sleep(100);
			continue;
		}

		dwSize = 0;

		//读取cmd回显数据
		if(!::ReadFile(pContext->hRead , pMsg->buf , SEND_BUF_SIZE - sizeof(RCMSG_BUFF) - 2 , &dwSize , 0))
		{//读取数据失败
			closesocket(localSock);
			localSock = 0;
			//TODO  需要做错误处理
			goto exit_send_cmd_data_thread;
		}

		pMsg->buf[dwSize] = 0;
		//需要将数据发出去
		pMsg->msgHead.size = (WORD)(sizeof(RCMSG_BUFF) - 1 + dwSize + 1);
		sendto(localSock , (const char*)pMsg , pMsg->msgHead.size , 0 ,
			(sockaddr*)&serverAddr , sizeof(serverAddr));
	}
exit_send_cmd_data_thread:
	closesocket(localSock);
	if (NULL != buf)
		delete[] buf;
	return 0;
}

DWORD WINAPI EnumFileThread( LPVOID param )
{
	CMainFrame* pFram	= (CMainFrame*)param;
	ASSERT(NULL != pFram);
	CString		strServerIP = pFram->GetServerIP();
	CString		strPath = pFram->GetEnumPath();//需要枚举的文件夹路径
	
	PRCMSG_BUFF pMsg	= NULL;
	SOCKET		localSock  = {0};
	sockaddr_in serverAddr = {0};

	//0我的电脑
	//1桌面
	//2我的文档
	UINT preDir = 0;
	
	strPath = RevertPath( strPath, preDir);

	localSock = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
	if(localSock == INVALID_SOCKET) 
	{//创建套接字失败
		//TODO 需要做错误处理
		return 0; 
	}
	else
	{//创建套接字成功
		InitSockaddr(serverAddr , SERVER_MSG_PORT , strServerIP);
	}

	char buf[MAX_PATH + sizeof(RCMSG_BUFF)] = {0};
	pMsg = (PRCMSG_BUFF)buf;
	pMsg->msgHead.type = MT_FILE_PATH_C;

	if (strPath.Right(1) != _T("\\"))
	{
		strPath += _T("\\");
	}
	CFileFind ff;
	BOOL res = ff.FindFile(strPath + _T("*.*"));
	while(res)
	{
		res = ff.FindNextFile();
		if(ff.IsDots())//"."和“..”就不要了吧
			continue;
		else
		{//是文件  发送出去
	
			CString file = ff.GetFilePath().Trim();
			if(ff.IsDirectory())
				file += _T("\\");//是文件夹是就在后面添加一个字符   \
			
			if (preDir != 0)
			{
				file = ReplacePath(file , preDir);
			}

			{//字符编码转换
			::setlocale(LC_ALL , "");
			::wcstombs(pMsg->buf , file , MAX_PATH);
			::setlocale(LC_ALL , "C");
			}//字符编码转换

			//需要将数据发出去
			pMsg->msgHead.size = (WORD)(sizeof(RCMSG_BUFF) - 1 + strlen(pMsg->buf) + 1);
			sendto(localSock , (const char*)pMsg , pMsg->msgHead.size , 0 ,
				(sockaddr*)&serverAddr , sizeof(serverAddr));
		}
	}
	ff.Close();

	closesocket(localSock);
	return 0;
}

BOOL DeleteFileOrDir( CString path )
{
	CFileFind tempFind;
	CString temp;
	if (path.Right(1) != _T('\\'))
	{//需要删除的是一个文件
		return DeleteFile(path);
	}
	temp = path + _T("*.*");
	BOOL IsFinded = (BOOL)tempFind.FindFile(temp);
	while(IsFinded)
	{
		IsFinded = (BOOL)tempFind.FindNextFile();
		if(!tempFind.IsDots())
		{
			CString foundFileName = tempFind.GetFileName();
			if(tempFind.IsDirectory())
			{//找到一个子目录
				CString inner = path + _T("\\") + foundFileName + _T("\\");
				if( FALSE == DeleteFileOrDir(inner))
				{//删除目录失败
					tempFind.Close();
					return FALSE;
				}
			}
			else
			{//找到的是文件
				CString inner = path + _T("\\") + foundFileName;
				if(FALSE == DeleteFile(inner))
				{//删除文件失败
					tempFind.Close();
					return FALSE;
				}
			}
		}
	}
	tempFind.Close();
	if(!RemoveDirectory(path))
	{//删除当前目录失败
		return FALSE;
	}
	return TRUE;
}

