#pragma once

#include "TaskHelper.h"
#include "VirtualListView.h"

class CMainFrame :
	public CFrameWindowImpl<CMainFrame>,
	public CAutoUpdateUI<CMainFrame>,
	public CVirtualListView<CMainFrame>,
	public CMessageFilter,
	public CIdleHandler {
public:
	DECLARE_FRAME_WND_CLASS(nullptr, IDR_MAINFRAME)

	const UINT WM_BUILD_TREE = WM_APP + 11;

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual BOOL OnIdle();

	CString GetColumnText(HWND, int row, int col) const;
	CString GetDetailsColumnText(HWND, int row, int col) const;

	int GetRowImage(HWND, int row) const;
	void DoSort(const SortInfo* si);
	void DoSortDetails(const SortInfo* si);
	BOOL OnRightClickList(HWND, int row, int col, const POINT&);

	enum {
		TaskListId = 129,
		DetailsListId
	};

	enum class IconIndex {
		Generic, Yes, No, Action, Trigger,
	};
	struct DetailsItem {
		CString Name;
		CString Details;
		IconIndex Image{ IconIndex::Generic };
	};

	BEGIN_MSG_MAP(CMainFrame)
		NOTIFY_CODE_HANDLER(TVN_SELCHANGED, OnTreeSelChanged)
		NOTIFY_HANDLER(TaskListId, LVN_ITEMCHANGED, OnListItemChanged)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_BUILD_TREE, OnBuildTree)
		COMMAND_ID_HANDLER(ID_FILE_RUNASADMIN, OnRunAsAdmin)
		COMMAND_ID_HANDLER(ID_TASK_ENABLE, OnEnableDisableTask)
		COMMAND_ID_HANDLER(ID_TASK_RUN, OnRunTask)
		COMMAND_ID_HANDLER(ID_TASK_STOP, OnStopTask)
		COMMAND_ID_HANDLER(ID_TASK_DELETE, OnDeleteTask)
		COMMAND_ID_HANDLER(IDM_EXIT, OnExit)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		COMMAND_ID_HANDLER(IDM_ABOUT, OnAbout)
		CHAIN_MSG_MAP(CAutoUpdateUI<CMainFrame>)
		CHAIN_MSG_MAP(CVirtualListView<CMainFrame>)
		CHAIN_MSG_MAP(CFrameWindowImpl<CMainFrame>)
		REFLECT_NOTIFICATIONS_EX()
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

private:
	enum class ColumnType {
		Name, Status, Triggers, Created, Path,
		Author, NextRun, LastRun, RunResults,
		RunningPid, InstanceGuid, CurrentAction
	};
	enum class NodeType {
		None,
		Folder, TaskSched, RunningTasks, AllTasks
	};
	struct TaskItem {
		CString Name;
		CString Path;
		mutable DATE NextRun{ 0 };
		CComPtr<IRegisteredTask> spTask;
		CComPtr<IRunningTask> spRunningTask;
	};

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBuildTree(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTreeSelChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnRunAsAdmin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnEnableDisableTask(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnListItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewRefreshAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRunTask(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStopTask(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDeleteTask(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void InitCommandBar();
	void InitToolBar(CToolBarCtrl& tb, int size = 24);
	HRESULT BuildTaskTree();
	HRESULT BuildTaskTree(ITaskFolder* folder, CTreeItem root);
	CString GetFolderPath(HTREEITEM hItem) const;
	TASK_STATE GetTaskState(const TaskItem& item) const;
	CString GetTaskAuthor(const TaskItem& item) const;
	CString GetTaskCreateDate(const TaskItem& item) const;
	void RefreshList();
	void UpdateDetails();
	int UpdateItemDetails(const TaskItem& item);

	void EnumTasks();
	void EnumAllTasks();
	void EnumRunningTasks();
	void AddTasks(ITaskFolder* folder);
	void SwitchColumns();
	void UpdateUI();

	CCommandBarCtrl m_CmdBar;
	CSplitterWindow m_MainSplitter;
	CHorSplitterWindow m_ListSplitter;
	CMultiPaneStatusBarCtrl m_StatusBar;
	CListViewCtrl m_List;
	CTreeViewCtrlEx m_Tree;
	CListViewCtrl m_Details;
	TaskSchedSvc m_TaskSvc;
	std::vector<TaskItem> m_Items;
	std::vector<DetailsItem> m_ItemDetails;
	CComPtr<ITaskFolder> m_spCurrentFolder;
	NodeType m_CurrentNodeType{ NodeType::None };
	ColumnsState m_TasksColumns, m_RunningTasksColumns;
	int m_CurrentSelectedItem{ -1 };
};
