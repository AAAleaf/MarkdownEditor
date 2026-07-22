
// MainFrm.cpp : CMainFrame 类的实现
//

#include "stdafx.h"
#include "MarkdownEditor.h"

#include "MainFrm.h"
#include "LeftView.h"
#include "MarkdownEditorView.h"
#include "MarkdownEditorDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 兜底定义：若 resource.h 中未定义 H5/H6（旧版或未随本文件一起提交推送），
// 这里保证 C++ 编译通过。一旦 resource.h 正确定义了它们，#ifndef 会跳过，不冲突。
// 注意：菜单项与快捷键要真正出现，仍需把 resource.h 与 .rc 一起提交。
#ifndef ID_PARA_H5
#define ID_PARA_H5 32793
#endif
#ifndef ID_PARA_H6
#define ID_PARA_H6 32794
#endif

#define IS_VIEWER_KEY  "isViewer"
void saveViewer(bool enable) //把是否为阅读器模式保存到注册表,方便下次打开程序时自动使用之前的状态
{
	int value = enable ? 1 : 0;
	AfxGetApp()->WriteProfileInt("", IS_VIEWER_KEY, value);
}
bool isViewer() //根据
{
	int value = AfxGetApp()->GetProfileInt("", IS_VIEWER_KEY, 0);
	return value == 1;
}
// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

	BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
		ON_WM_CREATE()
		ON_WM_SIZE()
		ON_COMMAND(IDM_SWITCH, &CMainFrame::OnSwitch)
		ON_COMMAND(IDM_ABOUT, &CMainFrame::OnAbout)
		ON_COMMAND_RANGE(ID_PARA_H1, ID_PARA_OL, &CMainFrame::OnParaCommand)
		ON_COMMAND_RANGE(ID_PARA_H5, ID_PARA_H6, &CMainFrame::OnParaCommand)
		ON_COMMAND_RANGE(ID_FORMAT_BOLD, ID_FORMAT_IMAGE, &CMainFrame::OnFormatCommand)
	END_MESSAGE_MAP()

	static UINT indicators[] =
	{
		ID_SEPARATOR,           // 状态行指示器
		ID_INDICATOR_CAPS,
		ID_INDICATOR_NUM,
		ID_INDICATOR_SCRL,
	};

	// CMainFrame 构造/析构

	CMainFrame::CMainFrame()
	{
		_bInited = false;
		_bShowLeft = !isViewer();
		// TODO: 在此添加成员初始化代码
	}

	CMainFrame::~CMainFrame()
	{
	}

	int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
			return -1;

		this->MoveWindow(0,0,800,600);
		this->CenterWindow();

		if (!m_wndStatusBar.Create(this))
		{
			TRACE0("未能创建状态栏\n");
			return -1;      // 未能创建
		}
		m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));
		return 0;
	}

	BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/,
		CCreateContext* pContext)
	{
		// 创建拆分窗口
		if (!m_wndSplitter.CreateStatic(this, 1, 2))
			return FALSE;

		CRect rect;
		this->GetWindowRect(&rect);
		CSize size(rect.Width() /2 , rect.Height() /2);

		if (!m_wndSplitter.CreateView(0, 0, RUNTIME_CLASS(CLeftView), size, pContext) ||
			!m_wndSplitter.CreateView(0, 1, RUNTIME_CLASS(CMarkdownEditorView), size, pContext))
		{
			m_wndSplitter.DestroyWindow();
			return FALSE;
		}

		_bInited = true;
		return TRUE;
	}

	BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
	{
		if( !CFrameWnd::PreCreateWindow(cs) )
			return FALSE;
		// TODO: 在此处通过修改
		//  CREATESTRUCT cs 来修改窗口类或样式

		return TRUE;
	}

	// CMainFrame 诊断

#ifdef _DEBUG
	void CMainFrame::AssertValid() const
	{
		CFrameWnd::AssertValid();
	}

	void CMainFrame::Dump(CDumpContext& dc) const
	{
		CFrameWnd::Dump(dc);
	}
#endif //_DEBUG


	// CMainFrame 消息处理程序


	void CMainFrame::OnSize(UINT nType, int cx, int cy)
	{
		CFrameWnd::OnSize(nType, cx, cy);
		if(!_bInited)
			return;
		if(cx == 0 || cy == 0)
			return;
		int cxCur, cxMin;
		m_wndSplitter.GetColumnInfo(0, cxCur, cxMin); 
		if(cxCur <= 0)
			return;
		m_wndSplitter.SetColumnInfo(0,cx/2,10);
		m_wndSplitter.RecalcLayout();

		//打开时保留上次是否显示左侧编辑框的状态
		static bool sFirst = true;
		if (sFirst) {
			sFirst = false;
			bool show = isViewer();
			if (show)
				switchViewer(show);
		}
	}


	void CMainFrame::OnSwitch(){
		_bShowLeft = !_bShowLeft;
		switchViewer(!_bShowLeft);
		saveViewer(!_bShowLeft);
	}
	void CMainFrame::switchViewer(bool viewer) {
		m_wndSplitter.ShowLeft(!viewer);
	}


	const string STR_ABOUT = "# MarkdownEditor 1.2\nProject: <https://github.com/jijinggang/MarkdownEditor>\n## Author\njijinggang@gmail.com\n## Copyright\nFree For All";
	//注意，此相应函数必须放在MainFrame中，如果放在MarkdownEditorView中，如果MarkdownEditorView失去焦点，则菜单不能点
	void CMainFrame::OnAbout()
	{
		static bool s_bShowAbout = false;
		CMarkdownEditorView* pView = dynamic_cast<CMarkdownEditorView*>(m_wndSplitter.GetPane(0,1));
		if(pView == NULL)
			return;
		if(!s_bShowAbout)
			pView->UpdateMd(STR_ABOUT);
		else{
			CLeftView* pLeft = dynamic_cast<CLeftView*>(m_wndSplitter.GetPane(0,0));
			pView->GetDocument()->UpdateAllViews(pLeft, LPARAM_Update);
		}
		s_bShowAbout = !s_bShowAbout;
	}




//=========== 新增：段落 / 格式 菜单功能 ===========
CLeftView* CMainFrame::GetLeftView()
{
	return dynamic_cast<CLeftView*>(m_wndSplitter.GetPane(0,0));
}

// 在当前光标处插入文本；selStartRel/selEndRel 为“距插入文本末尾的字符数”，
// 用于把光标/选区定位到插入文本内部（例如超链接里选中 URL，让你直接改）。
void CMainFrame::InsertAtCaret(const CString& text, int selStartRel, int selEndRel)
{
	CLeftView* pLeft = GetLeftView();
	if(!pLeft) return;
	CRichEditCtrl& edit = pLeft->GetRichEditCtrl();
	edit.ReplaceSel(text, TRUE);
	long ns, ne; edit.GetSel(ns, ne); // 插入后选区收拢到文本末尾
	long a = ns - selStartRel; if(a < 0) a = 0;
	long b = ns - selEndRel;   if(b < 0) b = 0;
	edit.SetSel(a, b);
	edit.SetFocus();
}

// 用 prefix/suffix 包裹当前选区；无选区则只在光标处插入标记并把光标放在中间
void CMainFrame::WrapSelection(const CString& prefix, const CString& suffix)
{
	CLeftView* pLeft = GetLeftView();
	if(!pLeft) return;
	CRichEditCtrl& edit = pLeft->GetRichEditCtrl();
	long s0, e0; edit.GetSel(s0, e0);
	CString sel; edit.GetTextRange((int)s0, (int)e0, sel);
	CString ins = prefix + sel + suffix;
	edit.ReplaceSel(ins, TRUE);
	long ns, ne; edit.GetSel(ns, ne); // ns == ne == 插入文本末尾
	long innerStart = ns - suffix.GetLength() - sel.GetLength();
	long innerEnd   = ns - suffix.GetLength();
	if(innerStart < 0) innerStart = 0;
	edit.SetSel(innerStart, innerEnd);
	edit.SetFocus();
}

// 在当前行行首插入前缀（标题、列表用）
void CMainFrame::PrefixCurrentLine(const CString& prefix, bool bAddBlankLineBefore)
{
	CLeftView* pLeft = GetLeftView();
	if(!pLeft) return;
	CRichEditCtrl& edit = pLeft->GetRichEditCtrl();
	long s, e; edit.GetSel(s, e);
	if(s > e){ long t = s; s = e; e = t; }
	long nLine = edit.LineFromChar(s);
	long lineStart = edit.LineIndex(nLine);

	// 列表类命令：如果上一行非空，先插一个空行，避免 Markdown 把新列表合并进上一段/列表
	if (bAddBlankLineBefore && nLine > 0) {
		long prevStart = edit.LineIndex(nLine - 1);
		long prevEnd = lineStart; // 当前行行首即上一行结尾
		if (prevEnd > prevStart) {
			CString prevText;
			edit.GetTextRange((int)prevStart, (int)prevEnd, prevText);
			prevText.Remove(_T('\r'));
			prevText.Remove(_T('\n'));
			if (!prevText.IsEmpty()) {
				edit.SetSel(lineStart, lineStart);
				edit.ReplaceSel(_T("\r\n"), TRUE);
				lineStart += 2;
			}
		}
	}

	edit.SetSel(lineStart, lineStart);
	edit.ReplaceSel(prefix, TRUE);
	edit.SetFocus();
}

void CMainFrame::OnParaCommand(UINT nID)
{
	switch(nID){
		case ID_PARA_H1: PrefixCurrentLine(_T("# "));             break;
		case ID_PARA_H2: PrefixCurrentLine(_T("## "));            break;
		case ID_PARA_H3: PrefixCurrentLine(_T("### "));           break;
		case ID_PARA_H4: PrefixCurrentLine(_T("#### "));          break;
		case ID_PARA_H5: PrefixCurrentLine(_T("##### "));         break;
		case ID_PARA_H6: PrefixCurrentLine(_T("###### "));        break;
		case ID_PARA_UL: PrefixCurrentLine(_T("- "),  true);      break;
		case ID_PARA_OL: PrefixCurrentLine(_T("1. "), true);      break;
	}
}

void CMainFrame::OnFormatCommand(UINT nID)
{
	switch(nID){
		case ID_FORMAT_BOLD:      WrapSelection(_T("**"), _T("**"));     break;
		case ID_FORMAT_ITALIC:    WrapSelection(_T("*"),  _T("*"));      break;
		case ID_FORMAT_UNDERLINE: WrapSelection(_T("<u>"), _T("</u>"));  break;
		case ID_FORMAT_CODE:      WrapSelection(_T("`"),  _T("`"));      break;
		case ID_FORMAT_STRIKE:    WrapSelection(_T("~~"), _T("~~"));     break;
		case ID_FORMAT_LINK:      InsertAtCaret(_T("[链接文字](https://)"), 9, 1); break;
		// 用 data: URI 内联占位图：不触发 IE 任何文件/网络加载，彻底避免“插入图片即崩溃”
		// 用户把整段 data:... 替换成自己的相对/绝对图片路径即可；replaceImgSrc 会把它解析到文档所在目录
		case ID_FORMAT_IMAGE:     InsertAtCaret(_T("![图片描述](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEAAAAAYCAIAAADF1mwTAAAAPUlEQVR42u3YQQ0AIAwAselEE+omAhF7YYKEbGlyBvq9WDtbF0MAeapdAAAAAAAAAAAAAAAAAAAvAK7Ety4snOSzESSKyAAAAABJRU5ErkJggg==)"), 0, 0); break;
	}
}
