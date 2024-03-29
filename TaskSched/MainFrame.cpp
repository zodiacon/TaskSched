#include "pch.h"
#include "Resource.h"
#include "MainFrame.h"
#include "SecurityHelper.h"
#include "AboutDlg.h"
#include "IconHelper.h"
#include "ComHelper.h"
#include "SortHelper.h"

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return FALSE;
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return 0;
}

CString CMainFrame::GetColumnText(HWND h, int row, int col) const {
	if (h == m_Details)
		return GetDetailsColumnText(h, row, col);
	auto& item = m_Items[row];
	switch (static_cast<ColumnType>(GetColumnManager(h)->GetColumnTag(col))) {
		case ColumnType::Name: return item.Name;
		case ColumnType::Path: return item.Path;
		case ColumnType::Triggers: return TaskHelper::GetTaskTriggers(item.spTask);
		case ColumnType::Status: return TaskHelper::TaskStateToString(GetTaskState(item));
		case ColumnType::InstanceGuid:
		{
			CComBSTR guid;
			item.spRunningTask->get_InstanceGuid(&guid);
			return CString(guid);
		}
		case ColumnType::RunningPid:
		{
			DWORD pid;
			item.spRunningTask->get_EnginePID(&pid);
			CString text;
			text.Format(L"%u (%s)", pid, (PCWSTR)TaskHelper::GetProcessName(pid));
			return text;
		}
		case ColumnType::LastRun:
		{
			DATE date;
			item.spTask->get_LastRunTime(&date);

			if (date == 0)
				return L"";

			CComVariant v(date, VT_DATE);
			v.ChangeType(VT_BSTR);
			return CString(v.bstrVal);
		}
		case ColumnType::NextRun:
		{
			if (item.NextRun == 0)
				item.spTask->get_NextRunTime(&item.NextRun);

			if (item.NextRun == 0)
				return L"";

			CComVariant v(item.NextRun, VT_DATE);
			v.ChangeType(VT_BSTR);
			return CString(v.bstrVal);
		}
		case ColumnType::RunResults:
		{
			LONG result;
			item.spTask->get_LastTaskResult(&result);
			auto msg = TaskHelper::FormatMessage(result);
			msg.Format(L"%s (0x%X)", (PCWSTR)msg, result);
			return msg;
		}
		case ColumnType::Created: return GetTaskCreateDate(item);
		case ColumnType::Author: return GetTaskAuthor(item);
		case ColumnType::CurrentAction:
		{
			CComBSTR action;
			item.spRunningTask->get_CurrentAction(&action);
			return CString(action);
		}
	}
	return L"";
}

CString CMainFrame::GetDetailsColumnText(HWND, int row, int col) const {
	return col == 0 ? m_ItemDetails[row].Name : m_ItemDetails[row].Details;
}

int CMainFrame::GetRowImage(HWND h, int row, int col) const {
	if (h == m_Details)
		return static_cast<int>(m_ItemDetails[row].Image);

	auto& item = m_Items[row];
	switch (GetTaskState(item)) {
		case TASK_STATE_DISABLED: return 1;
		case TASK_STATE_RUNNING: return 2;
	}
	return 0;
}

void CMainFrame::DoSort(const SortInfo* si) {
	if (si == nullptr || si->SortColumn < 0)
		return;

	if (si->hWnd == m_Details)
		return DoSortDetails(si);

	auto cm = GetColumnManager(m_List);
	auto asc = si->SortAscending;
	auto col = cm->GetColumnTag<ColumnType>(si->SortColumn);
	auto compare = [&](auto& item1, auto& item2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(item1.Name, item2.Name, asc);
			case ColumnType::Path: return SortHelper::Sort(item1.Path, item2.Path, asc);
			case ColumnType::Status: return SortHelper::Sort(GetTaskState(item1), GetTaskState(item2), asc);
			case ColumnType::RunningPid: 
			{
				DWORD p1, p2;
				item1.spRunningTask->get_EnginePID(&p1);
				item2.spRunningTask->get_EnginePID(&p2);
				return SortHelper::Sort(p1, p2, asc);
			}
			case ColumnType::RunResults:
			{
				LONG r1, r2;
				item1.spTask->get_LastTaskResult(&r1);
				item1.spTask->get_LastTaskResult(&r2);
				return SortHelper::Sort(r1, r2, asc);
			}
			case ColumnType::Author: return SortHelper::Sort(GetTaskAuthor(item1), GetTaskAuthor(item2), asc);
			case ColumnType::Created: return SortHelper::Sort(GetTaskCreateDate(item1), GetTaskCreateDate(item2), asc);
			case ColumnType::Triggers: return SortHelper::Sort(TaskHelper::GetTaskTriggers(item1.spTask), TaskHelper::GetTaskTriggers(item2.spTask), asc);
			case ColumnType::LastRun:
			{
				DATE d1, d2;
				item1.spTask->get_LastRunTime(&d1);
				item2.spTask->get_LastRunTime(&d2);
				return SortHelper::Sort(d1, d2, asc);
			}
			case ColumnType::NextRun:
			{
				DATE d1 = item1.NextRun, d2 = item2.NextRun;
				if (d1 == 0) {
					item1.spTask->get_NextRunTime(&d1);
					item1.NextRun = d1;
				}
				if (d2 == 0) {
					item2.spTask->get_NextRunTime(&d2);
					item2.NextRun = d2;
				}
				return SortHelper::Sort(d1, d2, asc);
			}
		}
		return false;
	};

	CWaitCursor wait(false);
	if (col == ColumnType::Triggers && m_Items.size() > 30)
		wait.Set();
	std::sort(m_Items.begin(), m_Items.end(), compare);
}

void CMainFrame::DoSortDetails(const SortInfo* si) {
	std::sort(m_ItemDetails.begin(), m_ItemDetails.end(), [&](const auto& d1, const auto& d2) {
		switch (si->SortColumn) {
			case 0: return SortHelper::Sort(d1.Name, d2.Name, si->SortAscending);
			case 1: return SortHelper::Sort(d1.Details, d2.Details, si->SortAscending);
		}
		return false;
		});
}

BOOL CMainFrame::OnRightClickList(HWND h, int row, int col, const POINT& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);

	return m_CmdBar.TrackPopupMenu(menu.GetSubMenu(h == m_List ? 0 : 2), 0, pt.x, pt.y);
}

LRESULT CMainFrame::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	auto hr = m_TaskSvc.Init();
	if (FAILED(hr)) {
		AtlMessageBox(m_hWnd, L"Failed to connect to task scheduler service.", IDS_APP_TITLE, MB_ICONERROR);
		return -1;
	}

	CMenuHandle menu = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		auto fileMenu = menu.GetSubMenu(0);
		fileMenu.DeleteMenu(0, MF_BYPOSITION);
		fileMenu.DeleteMenu(0, MF_BYPOSITION);
		CString text;
		GetWindowText(text);
		text += L" (Administrator)";
		SetWindowText(text);
	}

	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, nullptr, ATL_SIMPLE_CMDBAR_PANE_STYLE);

	m_CmdBar.SetAlphaImages(true);
	m_CmdBar.AttachMenu(menu);
	InitCommandBar();

	UIAddMenu(menu);
	SetMenu(nullptr);

	CToolBarCtrl tb;
	tb.Create(m_hWnd, nullptr, nullptr, ATL_SIMPLE_TOOLBAR_PANE_STYLE, 0, ATL_IDW_TOOLBAR);
	InitToolBar(tb, 24);
	UIAddToolBar(tb);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(tb, nullptr, TRUE);

	CReBarCtrl rb(m_hWndToolBar);
	rb.LockBands(true);

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP | SBT_TOOLTIPS);
	m_StatusBar.SubclassWindow(m_hWndStatusBar);

	m_hWndClient = m_MainSplitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	m_Tree.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_DISABLEDRAGDROP | TVS_SHOWSELALWAYS, 0);
	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 4, 4);
		UINT icons[] = {
			IDI_FOLDER, IDI_FOLDER_OPEN, IDR_MAINFRAME, IDI_PLAY
		};
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_Tree.SetImageList(images, TVSIL_NORMAL);
	}
	::SetWindowTheme(m_Tree, L"Explorer", nullptr);

	m_ListSplitter.Create(m_MainSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	m_ListSplitter.SetSplitterPosPct(50);
	m_MainSplitter.SetSplitterPanes(m_Tree, m_ListSplitter);
	m_MainSplitter.SetSplitterPosPct(25);
	m_MainSplitter.UpdateSplitterLayout();

	m_List.Create(m_ListSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, TaskListId);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 4, 4);
		UINT icons[] = {
			IDI_TASK, IDI_TASK_DISABLED, IDI_PLAY
		};
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_List.SetImageList(images, LVSIL_SMALL);
	}

	m_Details.Create(m_ListSplitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 0, DetailsListId);
	m_Details.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	{
		CImageList images;
		images.Create(16, 16, ILC_COLOR32, 8, 4);
		UINT icons[] = {
			IDI_GENERIC, IDI_YES, IDI_CANCEL, IDI_ACTION, IDI_TIMER
		};
		for (auto icon : icons)
			images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
		m_Details.SetImageList(images, LVSIL_SMALL);
	}

	auto cm = GetColumnManager(m_Details);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 150);
	cm->AddColumn(L"Details", LVCFMT_LEFT, 700);

	SwitchColumns();

	m_ListSplitter.SetSplitterPanes(m_List, m_Details);

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UpdateLayout();
	PostMessage(WM_BUILD_TREE);
	UpdateUI();

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnExit(WORD, WORD, HWND, BOOL&) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnAbout(WORD, WORD, HWND, BOOL&) {
	CAboutDlg().DoModal();
	return 0;
}

LRESULT CMainFrame::OnBuildTree(UINT, WPARAM, LPARAM, BOOL&) {
	BuildTaskTree();
	return 0;
}

LRESULT CMainFrame::OnTreeSelChanged(int, LPNMHDR, BOOL&) {
	RefreshList();
	return 0;
}

void CMainFrame::RefreshList() {
	m_Items.clear();
	m_List.SetItemCount(0);
	m_spCurrentFolder = nullptr;
	auto item = m_Tree.GetSelectedItem();
	auto oldNodeType = m_CurrentNodeType;
	m_CurrentNodeType = static_cast<NodeType>(item.GetData());
	UpdateDetails();
	switch (static_cast<NodeType>(m_Tree.GetItemData(item))) {
		case NodeType::AllTasks:
			if (oldNodeType == NodeType::RunningTasks)
				SwitchColumns();
			EnumAllTasks();
			DoSort(GetSortInfo(m_List));
			return;

		case NodeType::RunningTasks:
			SwitchColumns();
			EnumRunningTasks();
			DoSort(GetSortInfo(m_List));
			return;
	}

	if (oldNodeType == NodeType::RunningTasks)
		SwitchColumns();
	auto folderPath = GetFolderPath(item);
	if (!folderPath.IsEmpty()) {
		m_TaskSvc->GetFolder(CComBSTR(folderPath), &m_spCurrentFolder);
		ATLASSERT(m_spCurrentFolder);
		EnumTasks();
		DoSort(GetSortInfo(m_List));
	}
	return;
}

void CMainFrame::UpdateDetails() {
	int selected = m_List.GetSelectedIndex();
	if (selected < 0) {
		m_Details.SetItemCount(0);
	}
	else {
		auto count = UpdateItemDetails(m_Items[selected]);
		m_Details.SetItemCountEx(count, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
		m_Details.RedrawItems(m_Details.GetTopIndex(), m_Details.GetTopIndex() + m_Details.GetCountPerPage());
	}
}

int CMainFrame::UpdateItemDetails(const TaskItem& item) {
	m_ItemDetails.clear();
	m_ItemDetails.reserve(32);

	m_ItemDetails.push_back(DetailsItem{ L"Name", item.Name });
	m_ItemDetails.push_back(DetailsItem{ L"Path", item.Path });
	m_ItemDetails.push_back(DetailsItem{ L"State", TaskHelper::TaskStateToString(GetTaskState(item)) });

	CString text;
	if (item.spRunningTask) {
		DWORD pid;
		item.spRunningTask->get_EnginePID(&pid);
		text.Format(L"PID: %u (0x%X) [%s]", pid, pid, (PCWSTR)TaskHelper::GetProcessName(pid));
		m_ItemDetails.push_back(DetailsItem{ L"Process", text });
		CComBSTR btext;
		item.spRunningTask->get_InstanceGuid(&btext);
		m_ItemDetails.push_back(DetailsItem{ L"Instance", CString(btext) });
		item.spRunningTask->get_CurrentAction(&btext);
		m_ItemDetails.push_back(DetailsItem{ L"Current Action", CString(btext) });
	}
	else {
		// get triggers
		CComPtr<ITaskDefinition> spDef;
		item.spTask->get_Definition(&spDef);
		CComPtr<ITriggerCollection> spColl;
		spDef->get_Triggers(&spColl);
		long count;
		spColl->get_Count(&count);
		if (count > 0) {
			int t = 0;
			ComHelper::DoForeachNoVariant<ITriggerCollection, ITrigger>(spColl, [&](ITrigger* trigger) {
				t++;
				VARIANT_BOOL enabled;
				trigger->get_Enabled(&enabled);
				TASK_TRIGGER_TYPE2 triggerType;
				trigger->get_Type(&triggerType);
				CComBSTR id;
				trigger->get_Id(&id);
				CComPtr<IRepetitionPattern> spRepeat;
				trigger->get_Repetition(&spRepeat);
				auto repeat = TaskHelper::GetRepeatPattern(spRepeat);

				DetailsItem di;
				text = L"Trigger";
				if (count > 1)
					di.Name.Format(L"%s %d", (PCWSTR)text, t);
				else
					di.Name = text;
				text = TaskHelper::TriggerTypeToString(triggerType);
				di.Name += L": " + text;
				text.Empty();
				if (!enabled)
					text += L"(Disabled) ";

				CComBSTR boundary;
				trigger->get_StartBoundary(&boundary);
				if (boundary.Length()) {
					text += L"Start: " + TaskHelper::CreateDateTimeToString(CString(boundary)) + L" ";
				}
				trigger->get_EndBoundary(&boundary);
				if (boundary.Length()) {
					text += L"End: " + TaskHelper::CreateDateTimeToString(CString(boundary)) + L" ";
				}
				trigger->get_ExecutionTimeLimit(&boundary);
				if (boundary.Length()) {
					text += L"Time Limit: " + CString(boundary) + L" ";
				}

				di.Details = text + repeat;
				di.Image = IconIndex::Trigger;

				if(CComQIPtr<ILogonTrigger> spLogon(trigger); spLogon) {
					spLogon->get_Delay(&boundary);
					if (boundary.Length()) {
						text.Format(L" Delay: %s", boundary.m_str);
						di.Details += text;
					}
					spLogon->get_UserId(&boundary);
					if (boundary.Length()) {
						text.Format(L" User: %s", boundary.m_str);
						di.Details += text;
					}
				}
				if(CComQIPtr<ISessionStateChangeTrigger> spSession(trigger); spSession) {
					spSession->get_Delay(&boundary);
					if (boundary.Length()) {
						text.Format(L" Delay: %s", boundary.m_str);
						di.Details += text;
					}
					spSession->get_UserId(&boundary);
					if (boundary.Length()) {
						text.Format(L" User: %s", boundary.m_str);
						di.Details += text;
					}
					TASK_SESSION_STATE_CHANGE_TYPE type;
					spSession->get_StateChange(&type);
					di.Details += CString(L" State Change: ") + TaskHelper::StateChangeToString(type);
				}
				m_ItemDetails.push_back(std::move(di));
			});
		}

		CComPtr<IActionCollection> spActions;
		spDef->get_Actions(&spActions);
		CComBSTR context;
		spActions->get_Context(&context);
		if (context.Length()) {
			m_ItemDetails.push_back(DetailsItem{ L"Actions Context", CString(context) });
		}
		spActions->get_Count(&count);
		if (count > 0) {
			int index = 0;
			ComHelper::DoForeachNoVariant<IActionCollection, IAction>(spActions, [&](IAction* action) {
				index++;
				TASK_ACTION_TYPE type;
				action->get_Type(&type);
				DetailsItem di;
				text = L"Action";
				if (count > 1)
					di.Name.Format(L"%s %d", text, index);
				di.Name += text + L": " + TaskHelper::ActionTypeToString(type);

				CComQIPtr<IExecAction> spExecAction(action);
				if (spExecAction) {
					text.Empty();
					CComBSTR path;
					spExecAction->get_Path(&path);
					text = L"Path: " + CString(path);
					spExecAction->get_Arguments(&path);
					if (path.Length())
						text += L" Arguments: " + CString(path);
					spExecAction->get_WorkingDirectory(&path);
					if (path.Length())
						text += L" Working Directory: " + CString(path);
					di.Details = text;
				}
				CComQIPtr<IShowMessageAction> spShowAction(action);
				if (spShowAction) {
					CComBSTR value;
					spShowAction->get_Title(&value);
					text = L"Title: " + CString(value);
					spShowAction->get_MessageBody(&value);
					text = L"Message: " + CString(value);
					di.Details = text;
				}
				CComQIPtr<IComHandlerAction> spHandler(action);
				if (spHandler) {
					CComBSTR value;
					spHandler->get_ClassId(&value);
					text = L"CLSID: " + CString(value);
					spHandler->get_Data(&value);
					if (value.Length())
						text += L" Data: " + CString(value);
					di.Details = text;
				}
				di.Image = IconIndex::Action;
				m_ItemDetails.push_back(std::move(di));
				});
		}

		CComBSTR value;
		spDef->get_Data(&value);
		if (value.Length()) {
			m_ItemDetails.push_back(DetailsItem{ L"User Data", CString(value) });
		}
		CComPtr<ITaskSettings> spSettings;
		spDef->get_Settings(&spSettings);
		VARIANT_BOOL b;
		spSettings->get_Hidden(&b);
		m_ItemDetails.push_back(DetailsItem{ L"Visible", b ? L"False" : L"True", b ? IconIndex::No : IconIndex::Yes });
		spSettings->get_AllowDemandStart(&b);
		m_ItemDetails.push_back(DetailsItem{ L"Allow Demand Start", b ? L"True" : L"False" , b ? IconIndex::Yes : IconIndex::No });
		spSettings->get_AllowHardTerminate(&b);
		m_ItemDetails.push_back(DetailsItem{ L"Allow Hard Terminate", b ? L"True" : L"False" , b ? IconIndex::Yes : IconIndex::No });
		spSettings->get_DisallowStartIfOnBatteries(&b);
		m_ItemDetails.push_back(DetailsItem{ L"Allow Start on Battery", b ? L"False" : L"True" , b ? IconIndex::No : IconIndex::Yes });
		int priority;
		spSettings->get_Priority(&priority);
		text.Format(L"%d", priority);
		m_ItemDetails.push_back(DetailsItem{ L"Priority", text });
		spSettings->get_ExecutionTimeLimit(&value);
		if (value.Length())
			m_ItemDetails.push_back(DetailsItem{ L"Execution Time Limit", CString(value) });
	}
	DoSort(GetSortInfo(m_Details));
	return static_cast<int>(m_ItemDetails.size());
}

LRESULT CMainFrame::OnRunAsAdmin(WORD, WORD, HWND, BOOL&) {
	if (SecurityHelper::RunElevated())
		PostMessage(WM_CLOSE);

	return 0;
}

LRESULT CMainFrame::OnEnableDisableTask(WORD, WORD, HWND, BOOL&) {
	auto index = m_List.GetSelectedIndex();
	ATLASSERT(index >= 0);

	auto& item = m_Items[index];
	ATLASSERT(item.spTask);
	VARIANT_BOOL enabled;
	item.spTask->get_Enabled(&enabled);
	auto hr = item.spTask->put_Enabled(enabled ? VARIANT_FALSE : VARIANT_TRUE);
	if (FAILED(hr)) {
		AtlMessageBox(m_hWnd, (PCWSTR)TaskHelper::FormatMessage(hr), IDS_APP_TITLE, MB_ICONERROR);
	}
	else {
		m_List.RedrawItems(index, index);
		UpdateUI();
	}
	return 0;
}

LRESULT CMainFrame::OnListItemChanged(int, LPNMHDR, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	if (selected != m_CurrentSelectedItem) {
		m_CurrentSelectedItem = selected;
		UpdateDetails();
		UpdateUI();
	}
	return 0;
}

LRESULT CMainFrame::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	RefreshList();
	m_List.SetFocus();
	if (selected >= 0) {
		m_List.SetItemState(selected, LVIS_SELECTED, LVIS_SELECTED);
	}
	return 0;
}

LRESULT CMainFrame::OnRunTask(WORD, WORD, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	ATLASSERT(selected >= 0);
	auto& item = m_Items[selected];
	ATLASSERT(item.spTask);
	CComPtr<IRunningTask> spRunning;
	auto hr = item.spTask->Run(CComVariant(), &spRunning);
	if (FAILED(hr)) {
		AtlMessageBox(m_hWnd, (PCWSTR)TaskHelper::FormatMessage(hr), IDS_APP_TITLE, MB_ICONERROR);
	}
	else {
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

LRESULT CMainFrame::OnStopTask(WORD, WORD, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	ATLASSERT(selected >= 0);
	auto& item = m_Items[selected];
	HRESULT hr;
	if (item.spTask) {
		TASK_STATE state;
		item.spTask->get_State(&state);
		if (state != TASK_STATE_RUNNING) {
			AtlMessageBox(m_hWnd, L"Task is not running.", IDS_APP_TITLE, MB_ICONWARNING);
			return 0;
		}
		hr = item.spTask->Stop(0);
	}
	else {
		hr = item.spRunningTask->Stop();
	}
	if (FAILED(hr)) {
		AtlMessageBox(m_hWnd, (PCWSTR)TaskHelper::FormatMessage(hr), IDS_APP_TITLE, MB_ICONERROR);
	}
	else {
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

LRESULT CMainFrame::OnDeleteTask(WORD, WORD, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	ATLASSERT(selected >= 0);
	auto& item = m_Items[selected];
	ATLASSERT(item.spTask);
	auto hr = m_spCurrentFolder->DeleteTask(CComBSTR(item.Name), 0);
	if (FAILED(hr)) {
		AtlMessageBox(m_hWnd, (PCWSTR)TaskHelper::FormatMessage(hr), IDS_APP_TITLE, MB_ICONERROR);
	}
	else {
		m_Items.erase(m_Items.begin() + selected);
		m_List.SetItemCountEx(static_cast<int>(m_Items.size()), LVSICF_NOSCROLL);
		m_List.RedrawItems(selected, selected);
	}
	return 0;
}

void CMainFrame::InitCommandBar() {
	struct {
		UINT id, icon;
		HICON hIcon = nullptr;
	} cmds[] = {
		{ ID_FILE_RUNASADMIN, 0, IconHelper::GetShieldIcon() },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_TASK_RUN, IDI_PLAY },
		{ ID_TASK_STOP, IDI_STOP },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_TASK_DELETE, IDI_DELETE },
	};
	for (auto& cmd : cmds) {
		HICON hIcon = cmd.hIcon;
		if (!hIcon) {
			hIcon = AtlLoadIconImage(cmd.icon, 0, 16, 16);
			ATLASSERT(hIcon);
		}
		m_CmdBar.AddIcon(cmd.icon ? hIcon : cmd.hIcon, cmd.id);
	}
}

void CMainFrame::InitToolBar(CToolBarCtrl& tb, int size) {
	CImageList tbImages;
	tbImages.Create(size, size, ILC_COLOR32, 8, 4);
	tb.SetImageList(tbImages);

	const struct {
		UINT id;
		int image;
		BYTE style = BTNS_BUTTON;
		PCWSTR text = nullptr;
	} buttons[] = {
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ 0 },
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_FIND, IDI_FIND },
		{ 0 },
		{ ID_TASK_RUN, IDI_PLAY },
	};
	for (auto& b : buttons) {
		if (b.id == 0)
			tb.AddSeparator(0);
		else {
			auto hIcon = AtlLoadIconImage(b.image, 0, size, size);
			ATLASSERT(hIcon);
			int image = tbImages.AddIcon(hIcon);
			tb.AddButton(b.id, b.style, TBSTATE_ENABLED, image, b.text, 0);
		}
	}
}

HRESULT CMainFrame::BuildTaskTree() {
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();

	auto root = m_Tree.InsertItem(L"Task Scheduler (Local)", 2, 2, TVI_ROOT, TVI_LAST);
	root.SetData(static_cast<DWORD_PTR>(NodeType::TaskSched));
	auto running = m_Tree.InsertItem(L"Running Tasks", 3, 3, root, TVI_LAST);
	running.SetData(static_cast<DWORD_PTR>(NodeType::RunningTasks));
	auto all = m_Tree.InsertItem(L"All Tasks", 2, 2, root, TVI_LAST);
	all.SetData(static_cast<DWORD_PTR>(NodeType::AllTasks));

	CComPtr<ITaskFolder> spFolder;
	auto hr = m_TaskSvc->GetFolder(CComBSTR(L"\\"), &spFolder);
	if (spFolder)
		hr = BuildTaskTree(spFolder, root);
	root.Expand(TVE_EXPAND);
	m_Tree.SetRedraw(TRUE);
	m_Tree.SelectItem(root);

	return hr;
}

HRESULT CMainFrame::BuildTaskTree(ITaskFolder* folder, CTreeItem root) {
	CComPtr<ITaskFolderCollection> spColl;
	auto hr = folder->GetFolders(0, &spColl);
	if (FAILED(hr))
		return hr;

	ComHelper::DoForeach<ITaskFolderCollection, ITaskFolder>(spColl, [&](auto subfolder) {
		CComBSTR name;
		subfolder->get_Name(&name);
		auto item = m_Tree.InsertItem(name, 0, 1, root, TVI_LAST);
		item.SetData(static_cast<DWORD_PTR>(NodeType::Folder));
		BuildTaskTree(subfolder, item);
		});

	return S_OK;
}

CString CMainFrame::GetFolderPath(HTREEITEM hItem) const {
	CString path;
	while (hItem && m_Tree.GetItemData(hItem) == static_cast<DWORD_PTR>(NodeType::Folder)) {
		CString name;
		m_Tree.GetItemText(hItem, name);
		path = name + L"\\" + path;
		hItem = m_Tree.GetParentItem(hItem);
	}
	return L"\\" + path.TrimRight(L"\\");
}

TASK_STATE CMainFrame::GetTaskState(const TaskItem& item) const {
	TASK_STATE state;
	item.spTask ? item.spTask->get_State(&state) : item.spRunningTask->get_State(&state);
	return state;
}

CString CMainFrame::GetTaskAuthor(const TaskItem& item) const {
	auto spInfo = TaskHelper::GetRegistrationInfo(item.spTask);
	CComBSTR author;
	spInfo->get_Author(&author);
	return CString(author);
}

CString CMainFrame::GetTaskCreateDate(const TaskItem& item) const {
	auto spInfo = TaskHelper::GetRegistrationInfo(item.spTask);
	CComBSTR date;
	spInfo->get_Date(&date);
	return date == 0 ? L"" : (PCWSTR)TaskHelper::CreateDateTimeToString(date);
}

void CMainFrame::EnumTasks() {
	if (m_spCurrentFolder == nullptr)
		return;
	auto path = GetFolderPath(m_Tree.GetSelectedItem());
	path.TrimRight(L"\\");
	CComPtr<IRegisteredTaskCollection> spColl;
	m_spCurrentFolder->GetTasks(TASK_ENUM_HIDDEN, &spColl);
	ComHelper::DoForeach<IRegisteredTaskCollection, IRegisteredTask>(spColl, [&](auto task) {
		TaskItem item;
		item.spTask = task;
		CComBSTR name;
		task->get_Name(&name);
		item.Name = name;
		task->get_Path(&name);
		item.Path = name;
		m_Items.push_back(item);
		});
	m_List.SetItemCountEx(static_cast<int>(m_Items.size()), LVSICF_NOSCROLL);
}

void CMainFrame::EnumAllTasks() {
	CWaitCursor wait;
	CComPtr<ITaskFolder> spFolder;
	m_TaskSvc->GetFolder(CComBSTR(L"\\"), &spFolder);
	m_Items.clear();
	AddTasks(spFolder);
	m_List.SetItemCountEx(static_cast<int>(m_Items.size()), LVSICF_NOSCROLL);
}

void CMainFrame::EnumRunningTasks() {
	m_Items.clear();
	CComPtr<IRunningTaskCollection> spColl;
	m_TaskSvc->GetRunningTasks(TASK_ENUM_HIDDEN, &spColl);
	ComHelper::DoForeach<IRunningTaskCollection, IRunningTask>(spColl, [&](auto task) {
		CComBSTR name;
		task->get_Name(&name);
		TaskItem item;
		item.Name = name;
		task->get_Path(&name);
		item.Path = name;
		item.spRunningTask = task;
		m_Items.push_back(item);
		});
	m_List.SetItemCountEx(static_cast<int>(m_Items.size()), LVSICF_NOSCROLL);
}

void CMainFrame::AddTasks(ITaskFolder* folder) {
	CComPtr<IRegisteredTaskCollection> spColl;
	folder->GetTasks(TASK_ENUM_HIDDEN, &spColl);
	ComHelper::DoForeach<IRegisteredTaskCollection, IRegisteredTask>(spColl, [&](auto task) {
		TaskItem item;
		item.spTask = task;
		CComBSTR name;
		task->get_Name(&name);
		item.Name = name;
		task->get_Path(&name);
		item.Path = name;
		m_Items.push_back(item);
		});

	CComPtr<ITaskFolderCollection> spFolderColl;
	folder->GetFolders(0, &spFolderColl);
	ComHelper::DoForeach<ITaskFolderCollection, ITaskFolder>(spFolderColl, [&](auto p) {
		AddTasks(p);
		});
}

void CMainFrame::SwitchColumns() {
	auto cm = GetColumnManager(m_List);
	if (m_CurrentNodeType == NodeType::RunningTasks) {
		m_TasksColumns = SaveState(m_List);
	}
	else {
		m_RunningTasksColumns = SaveState(m_List);
	}
	cm->Clear();
	ClearSort(m_List);
	if (m_CurrentNodeType == NodeType::RunningTasks) {
		cm->AddColumn(L"Name", LVCFMT_LEFT, 250, ColumnType::Name);
		cm->AddColumn(L"Status", LVCFMT_LEFT, 80, ColumnType::Status);
		cm->AddColumn(L"Process", LVCFMT_LEFT, 190, ColumnType::RunningPid);
		cm->AddColumn(L"Instance", LVCFMT_LEFT, 160, ColumnType::InstanceGuid);
		cm->AddColumn(L"Current Action", LVCFMT_LEFT, 240, ColumnType::CurrentAction);
		cm->AddColumn(L"Path", LVCFMT_LEFT, 350, ColumnType::Path);
		cm->UpdateColumns();
		LoadState(m_List, m_RunningTasksColumns);
	}
	else {
		cm->AddColumn(L"Name", LVCFMT_LEFT, 250, ColumnType::Name);
		cm->AddColumn(L"Status", LVCFMT_LEFT, 80, ColumnType::Status);
		cm->AddColumn(L"Triggers", LVCFMT_LEFT, 150, ColumnType::Triggers);
		cm->AddColumn(L"Created", LVCFMT_LEFT, 120, ColumnType::Created);
		cm->AddColumn(L"Next Run", LVCFMT_RIGHT, 150, ColumnType::NextRun);
		cm->AddColumn(L"Last Run", LVCFMT_RIGHT, 150, ColumnType::LastRun);
		cm->AddColumn(L"Last Result", LVCFMT_LEFT, 150, ColumnType::RunResults);
		cm->AddColumn(L"Author", LVCFMT_LEFT, 150, ColumnType::Author);
		cm->AddColumn(L"Path", LVCFMT_LEFT, 350, ColumnType::Path);
		cm->UpdateColumns();
		LoadState(m_List, m_TasksColumns);
	}
}

void CMainFrame::UpdateUI() {
	bool listFocus = ::GetFocus() == m_List;
	auto selected = m_List.GetSelectedIndex();
	auto item = selected < 0 ? nullptr : &m_Items[selected];

	UIEnable(ID_TASK_ENABLE, item && item->spTask);
	TASK_STATE state = TASK_STATE_UNKNOWN;
	if (item && item->spTask) {
		VARIANT_BOOL enabled;
		item->spTask->get_Enabled(&enabled);
		item->spTask->get_State(&state);
		UISetText(ID_TASK_ENABLE, enabled ? L"&Disable" : L"&Enable");
	}
	UIEnable(ID_TASK_RUN, item && state != TASK_STATE_RUNNING && item->spRunningTask == nullptr);
	UIEnable(ID_TASK_EXPORT, item && item->spTask);
	UIEnable(ID_TASK_DELETE, selected >= 0);
	UIEnable(ID_TASK_STOP, item && (item->spRunningTask || state == TASK_STATE_RUNNING));
}
