///////////////////////////////////////////////////////////////
//
// FileName	: SettingDlg.h
// Creator	: ����
// Date		: 2013��4��9�գ�10:34:00
// Comment	: ���öԻ����������
//
//////////////////////////////////////////////////////////////

#pragma once
#include "afxcmn.h"

class CPushSettingPane;
class CCMDSettingPane;
class CRemoteControlDoc;

// CSettingDlg �Ի���

class CSettingDlg : public CDialog
{
	DECLARE_DYNAMIC(CSettingDlg)

public:
	CSettingDlg( PUSH_CFG_V pushValue , CMD_CFG_V cmdValue ,CRemoteControlDoc* doc, CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~CSettingDlg();

// �Ի�������
	enum { IDD = IDD_SETTING };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()

private:
	//���͵�����ֵ
	PUSH_CFG_V m_pushValue;
	//CMD������ֵ
	CMD_CFG_V m_cmdValue;

	CTabCtrl m_wndTab;
	virtual BOOL OnInitDialog();

	//��������ҳ
	CPushSettingPane* m_pPushPane;
	//cmd��������ҳ
	CCMDSettingPane* m_pCmdPane;

	//�ĵ���
	CRemoteControlDoc* m_pDoc;

public:

	afx_msg void OnTcnSelchangeTab(NMHDR *pNMHDR, LRESULT *pResult);
	virtual BOOL DestroyWindow();
	void ValueChanged();
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedOk();
};