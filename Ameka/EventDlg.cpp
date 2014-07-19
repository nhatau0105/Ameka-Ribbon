// EventDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Ameka.h"
#include "EventDlg.h"
#include "afxdialogex.h"


// CEventDlg dialog
CEventDlg::CEventDlg() : CDialogEx(CEventDlg::IDD)
{
	index = -1;
}

int CEventDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	CRect rect;
	event_list.GetClientRect(&rect);
	int nColInterval = rect.Width()/4;

	event_list.InsertColumn(0, _T("Loa�i s�� ki��n"), LVCFMT_LEFT, nColInterval);
	//event_list.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	event_list.InsertColumn(1, _T("T�n s�� ki��n"), LVCFMT_LEFT, nColInterval*3/2);
	//event_list.SetColumnWidth(1, LVSCW_AUTOSIZE_USEHEADER);
	event_list.InsertColumn(2, _T("Th��i gian"), LVCFMT_LEFT, nColInterval*3/2);
	//event_list.SetColumnWidth(2, LVSCW_AUTOSIZE_USEHEADER);
	event_list.InsertColumn(3, _T("ID"), LVCFMT_LEFT, nColInterval);
	//event_list.SetColumnWidth(3, LVSCW_AUTOSIZE_USEHEADER);

	CAmekaDoc* pDoc = CAmekaDoc::GetDoc();
	if (!pDoc)
		return -1;
	int mID = 0;
	for (uint64_t i = 0; i < pDoc->counter; i++)
	{
		if (pDoc->primaryDataArray[i].eventID != 10)
		{
			CString str;
			str.Format(_T("%d"), pDoc->primaryDataArray[i].eventID);
			int nIndex = event_list.InsertItem(mID++, str);

			event_list.SetItemText(nIndex, 1, theApp.evName[pDoc->primaryDataArray[i].eventID]);

			char buff[20];
			time_t now = time(NULL);
			strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&pDoc->primaryDataArray[i].time));
			str = CString(buff, 20);
			event_list.SetItemText(nIndex, 2, str);

			str.Format(_T("%d"), i);
			event_list.SetItemText(nIndex, 3, str);
		}
	}
	//event_list.SetCurSel(0);
	return 0;
}

void CEventDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	//DDX_Control(pDX, evt_list, event_list);
	DDX_Control(pDX, IDC_LIST1, event_list);
}

BEGIN_MESSAGE_MAP(CEventDlg, CDialogEx)
	ON_BN_CLICKED(evt_ok, &CEventDlg::OnBnClickedok)
	ON_BN_CLICKED(evt_rename, &CEventDlg::OnBnClickedrename)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, &CEventDlg::OnLvnItemchangedList1)
END_MESSAGE_MAP()	

void CEventDlg::OnBnClickedrename()
{
	// TODO: Add your control notification handler code here
	if (index == -1)
		return;

	/*CString strID = event_list.GetItemText(index, 3);
	uint64_t numID = _ttoi(strID);

	CAmekaDoc* pDoc = CAmekaDoc::GetDoc();
	if (!pDoc)
		return;

	pDoc->primaryDataArray[numID].eventID = */
}


void CEventDlg::OnLvnItemchangedList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
	index = pNMHDR->idFrom;
}
