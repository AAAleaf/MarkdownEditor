
// MarkdownEditorView.cpp : CMarkdownEditorView 类的实现
//

#include "stdafx.h"
#include "Util.h"
#include <string>
#include <wrl/client.h>
#include <wrl/event.h>
#include <shellapi.h>
#include "WebView2.h"
// WRL 的 Callback（用于实现 WebView2 的 COM 事件回调）位于 Microsoft::WRL 命名空间，
// 须显式引入，否则 Callback<> 会报 “undeclared identifier”，进而级联出一堆 ICoreWebView2 错误。
using namespace Microsoft::WRL;
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "MarkdownEditor.h"
#endif

#include "MarkdownEditorDoc.h"
#include "MarkdownEditorView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMarkdownEditorView

IMPLEMENT_DYNCREATE(CMarkdownEditorView, CView)

BEGIN_MESSAGE_MAP(CMarkdownEditorView, CView)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

// CMarkdownEditorView 构造/析构

CMarkdownEditorView::CMarkdownEditorView()
{
	// TODO: 在此处添加构造代码
	_bFirstNavigate = true;
	m_bWebViewReady = false;
	initCSS();
}

CMarkdownEditorView::~CMarkdownEditorView()
{
}

void CMarkdownEditorView::OnDraw(CDC* /*pDC*/)
{
	// 预览由 WebView2 负责渲染，这里不需要绘制任何内容
}

BOOL CMarkdownEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式

	return CView::PreCreateWindow(cs);
}

void CMarkdownEditorView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	// 启动 WebView2（异步），就绪前先缓存 HTML
	InitializeWebView();
}

void CMarkdownEditorView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	ResizeWebView();
}

void CMarkdownEditorView::OnDestroy()
{
	if (m_controller)
		m_controller->Close();
	CView::OnDestroy();
}


// CMarkdownEditorView 诊断

#ifdef _DEBUG
void CMarkdownEditorView::AssertValid() const
{
	CView::AssertValid();
}

void CMarkdownEditorView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMarkdownEditorDoc* CMarkdownEditorView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMarkdownEditorDoc)));
	return (CMarkdownEditorDoc*)m_pDocument;
}
#endif //_DEBUG


// UTF-8 字节串 -> Unicode(CStringW)，用于传给 WebView2 的宽字符接口
CStringW CMarkdownEditorView::Utf8ToWide(const string& utf8)
{
	if (utf8.empty())
		return CStringW();
	int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
	CStringW w;
	LPWSTR buf = w.GetBuffer(n);
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), buf, n);
	w.ReleaseBuffer(n);
	return w;
}

// 初始化 WebView2（Edge / Chromium）。使用系统已安装的 WebView2 运行时（Evergreen）。
void CMarkdownEditorView::InitializeWebView()
{
	HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
		nullptr, nullptr, nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
				if (FAILED(result) || env == nullptr) {
					// WebView2 初始化失败（多半是本机没装 WebView2 运行时，或 exe 旁边缺 WebView2Loader.dll）。
					// 之前是静默返回，导致预览一片空白且无任何提示；这里给出明确弹窗。
					static bool warned = false;
					if (!warned) {
						warned = true;
						AfxMessageBox(
							_T("预览面板无法初始化 Microsoft Edge WebView2（Chromium 内核）。\n")
							_T("常见原因：\n")
							_T("  1. 本机未安装 WebView2 运行时；\n")
							_T("  2. MarkdownEditor.exe 旁边缺少 WebView2Loader.dll。\n\n")
							_T("请安装 WebView2 运行时（或确保 WebView2Loader.dll 与 exe 同目录）后重新打开本程序：\n")
							_T("https://developer.microsoft.com/zh-cn/microsoft-edge/webview2/"),
							MB_ICONINFORMATION);
					}
					return result;
				}
				m_environment = env;
				env->CreateCoreWebView2Controller(
					GetSafeHwnd(),
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
							if (FAILED(result) || controller == nullptr)
								return result;
							m_controller = controller;
							controller->get_CoreWebView2(&m_webView);

							// 点击链接时用默认程序打开（替代原 CHtmlView 的 onclick 处理）。
							// 用 NavigationStarting 拦截：拦截到 http/https/file 链接就外部打开，并取消 WebView2 自身导航，
							// 这样预览页面不会被跳转走。
							// 注意：get_Uri / put_Cancel 是 WebView2 最稳定的接口，跨 SDK 版本通用，
							// 不依赖 get_WebMessageAsString（新版 SDK 已拆分/移除该成员）。
							EventRegistrationToken navToken = { 0 };
							m_webView->add_NavigationStarting(
								Callback<ICoreWebView2NavigationStartingEventHandler>(
									[this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
										LPWSTR pwsz = nullptr;
										if (SUCCEEDED(args->get_Uri(&pwsz)) && pwsz) {
											CStringW uri(pwsz);
											CoTaskMemFree(pwsz);
											// 允许初始内容（NavigateToString 的 data: URI）与空 URI，其余一律外部打开并取消
											if (uri.Left(5) == L"data:" || uri.IsEmpty())
												return S_OK;
											CStringW open = uri;
											if (uri.Left(8) == L"file:///")
												open = uri.Mid(8);
											ShellExecuteW(nullptr, L"open", open.GetString(), nullptr, nullptr, SW_SHOWNORMAL);
											args->put_Cancel(TRUE);
										}
										return S_OK;
									}).Get(),
								&navToken);

							m_bWebViewReady = true;

							RECT rc;
							GetClientRect(&rc);
							controller->put_Bounds(rc);

							// 渲染：优先用就绪前缓存的 HTML；
							// 若为空（例如 OnUpdate 在 WebView2 就绪前没触发过），则直接用“当前文档内容”渲染，
							// 避免“打开文件后预览一直是空白”的情况。
							CStringW html = m_pendingHtml;
							m_pendingHtml.Empty();
							if (html.IsEmpty() && GetDocument())
								html = Utf8ToWide(GetMdHtml(GetDocument()->getText()));
							if (!html.IsEmpty())
								m_webView->NavigateToString(html);
							return S_OK;
						}).Get());
				return S_OK;
			}).Get());
}

void CMarkdownEditorView::ResizeWebView()
{
	if (m_controller) {
		RECT rc;
		GetClientRect(&rc);
		m_controller->put_Bounds(rc);
	}
}

void CMarkdownEditorView::NavigateToHtml(const CStringW& html)
{
	if (m_bWebViewReady && m_webView) {
		m_webView->NavigateToString(html);
	}
	else {
		m_pendingHtml = html;   // WebView2 尚未就绪，先缓存，就绪后再渲染
	}
}

void CMarkdownEditorView::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	if(!(lHint & LPARAM_Update))
		return;
	const string& str = GetDocument()->getText();	
	UpdateMd(str);
	// TODO: 在此添加专用代码和/或调用基类
}


void CMarkdownEditorView::initCSS(){
	string strUserCss = Util::GetExePath() + "user.css";
	if(PathFileExists(strUserCss.c_str())){
		_strCSS = Util::ReadStringFile(strUserCss.c_str());
	}else{
		Util::LoadStringRes(IDR_CSS,"CSS",_strCSS); 
	}
}

// 取文件所在目录（含末尾分隔符），用于把相对图片路径解析到文档目录下
static string DirOfPath(const string& filePath)
{
	if (filePath.empty()) return "";
	size_t pos = filePath.find_last_of("/\\");
	if (pos == string::npos) return "";
	return filePath.substr(0, pos + 1);
}

// 把本地路径规范成 file:/// URL（正斜杠）。浏览器从本地文件加载“裸的 C:\xxx 路径（无协议头）”
// 会按非法 URL 处理并访问异常 → 崩溃；改成 file:///C:/xxx 这种形式即可稳定加载。
static string ToFileUrl(const string& localPath)
{
	// 已经是 URL / 网络图 / 内联图 / 已经是 file:// 则原样返回
	if (localPath.compare(0, 7, "http://") == 0 ||
	    localPath.compare(0, 8, "https://") == 0 ||
	    localPath.compare(0, 5, "data:") == 0 ||
	    localPath.compare(0, 7, "file://") == 0)
		return localPath;

	string p = localPath;
	for (size_t i = 0; i < p.size(); ++i)
		if (p[i] == '\\') p[i] = '/';   // 反斜杠统一成正斜杠

	if (!p.empty() && p[0] == '/')      // 类 unix 绝对路径：file:///xxx
		return string("file://") + p;
	return string("file:///") + p;       // Windows 路径：file:///C:/xxx
}

string&  replaceImgSrc(string& str, string path)
{
	string dir = DirOfPath(path);
	string old_value = "<img src=\"";
	for (string::size_type pos(0); pos != string::npos; pos += old_value.length())   {
		if ((pos = str.find(old_value, pos)) != string::npos){
			const char* start = str.c_str() + pos + old_value.length();
			// 网络图 / 内联图 / 已经是 file:// 的，原样保留
			if (strnicmp(start, "http://", 7) == 0 ||
			    strnicmp(start, "https://", 8) == 0 ||
			    strnicmp(start, "data:", 5) == 0 ||
			    strnicmp(start, "file://", 7) == 0)
				continue;

			// 取出当前图片路径（到下一个引号为止）
			string local = start;
			size_t q = local.find('\"');
			if (q != string::npos) local = local.substr(0, q);

			// 相对路径才拼文档目录；带盘符的绝对路径（含 ':'）直接用，避免拼出畸形路径
			if (dir.size() && !local.empty() && local[0] != '/' && local.find(':') == string::npos)
				local = dir + local;

			string newUrl = ToFileUrl(local);
			string new_value = "<img src=\"" + newUrl;
			str.replace(pos, old_value.length(), new_value);
		}
		else
			break;
	}
	return   str;
}

// 把源码里的单行换行转换成 Markdown 硬换行（行尾加两个空格），
// 这样用户在编辑器里按一次回车，预览里也会换行。
static string MakeHardLineBreaks(const string& s)
{
	string r;
	size_t i = 0;
	while (i < s.size()) {
		size_t nl = s.find('\n', i);
		if (nl == string::npos) {
			r.append(s, i, string::npos);
			break;
		}
		// 行的实际结尾（排除 \r）
		size_t lineEnd = nl;
		if (lineEnd > i && s[lineEnd - 1] == '\r')
			lineEnd--;
		r.append(s, i, lineEnd - i);

		// 判断是否是段落空行（\r\n\r\n 或 \n\n）
		size_t after = nl + 1;
		if (after < s.size() && s[after] == '\r')
			after++;
		bool paraBreak = (after < s.size() && s[after] == '\n');

		if (!paraBreak)
			r.append("  "); // Markdown 硬换行

		r.append(s, nl, after - nl); // 复制换行符本身
		i = after;
	}
	return r;
}

const string HTML_TMPL = "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" /><style type=\"text/css\">{{0}}</style></head><body>{{1}}</body></html>";

string CMarkdownEditorView::GetMdHtml(const string& str){
	string strHtml = HTML_TMPL;
	Util::ReplaceAllStr(strHtml,"{{0}}", _strCSS);
	// 让编辑器里的单次回车也在预览里换行
	string md = Util::Text2Md(MakeHardLineBreaks(str));
	md = replaceImgSrc(md, GetDocument()->getFilePath());
	Util::ReplaceAllStr(strHtml, "{{1}}", md);
	// 工程是多字节字符集，内部文本按系统默认编码（中文系统为 GBK）。
	// 这里先把整体 HTML 转成 UTF-8；WebView2 接收宽字符，渲染前再转成 Unicode，避免中文乱码。
	return Util::ANSIToUTF8(strHtml.c_str());
}

void CMarkdownEditorView::UpdateMd(const string& strMd)
{
	string strHtml = GetMdHtml(strMd);
	NavigateToHtml(Utf8ToWide(strHtml));
}
