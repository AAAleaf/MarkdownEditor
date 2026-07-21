
// MarkdownEditorView.cpp : CMarkdownEditorView 类的实现
//

#include "stdafx.h"
#include "Util.h"
#include <string>
// SHARED_HANDLERS 可以在实现预览、缩略图和搜索筛选器句柄的
// ATL 项目中进行定义，并允许与该项目共享文档代码。
#ifndef SHARED_HANDLERS
#include "MarkdownEditor.h"
#endif

#include "MarkdownEditorDoc.h"
#include "MarkdownEditorView.h"
#include "MyClickEvents.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMarkdownEditorView

IMPLEMENT_DYNCREATE(CMarkdownEditorView, CHtmlView)

BEGIN_MESSAGE_MAP(CMarkdownEditorView, CHtmlView)
END_MESSAGE_MAP()

// CMarkdownEditorView 构造/析构

CMarkdownEditorView::CMarkdownEditorView()
{
	// TODO: 在此处添加构造代码
	_bFirstNavigate = true;
	initCSS();
}

CMarkdownEditorView::~CMarkdownEditorView()
{
}

BOOL CMarkdownEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: 在此处通过修改
	//  CREATESTRUCT cs 来修改窗口类或样式

	return CHtmlView::PreCreateWindow(cs);
}

void CMarkdownEditorView::OnInitialUpdate()
{
	CHtmlView::OnInitialUpdate();

}


// CMarkdownEditorView 诊断

#ifdef _DEBUG
void CMarkdownEditorView::AssertValid() const
{
	CHtmlView::AssertValid();
}

void CMarkdownEditorView::Dump(CDumpContext& dc) const
{
	CHtmlView::Dump(dc);
}

CMarkdownEditorDoc* CMarkdownEditorView::GetDocument() const // 非调试版本是内联的
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMarkdownEditorDoc)));
	return (CMarkdownEditorDoc*)m_pDocument;
}
#endif //_DEBUG

void setClickEvents(IHTMLDocument2* htmlDocument2, const char* dir) {

	static CMyClickEvents clickEvents;
	clickEvents.SetContext(htmlDocument2, dir);
	_variant_t clickDispatch;
	clickDispatch.vt = VT_DISPATCH;
	clickDispatch.pdispVal = &clickEvents;

	htmlDocument2->put_onclick(clickDispatch);
}

// CMarkdownEditorView 消息处理程序

void CMarkdownEditorView::NavigateHTML(const string& strHtml)
{
	IDispatch* pDoc = GetHtmlDocument();
	if(NULL == pDoc)
		return;
	// 取得文档中的IPersistStreamInit对象
    CComPtr<IHTMLDocument2> pHtmlDoc;
	HRESULT hr = pDoc ->QueryInterface(IID_IHTMLDocument2, (void**)&pHtmlDoc);
    if (FAILED(hr))
        return;

	// 输入是 UTF-8 编码的 HTML，显式按 CP_UTF8 转 BSTR，避免依赖系统默认代码页导致中文乱码
	int nLenW = MultiByteToWideChar(CP_UTF8, 0, strHtml.c_str(), -1, NULL, 0);
	wchar_t* pwsz = new wchar_t[nLenW];
	MultiByteToWideChar(CP_UTF8, 0, strHtml.c_str(), -1, pwsz, nLenW);
	BSTR bstr = SysAllocString(pwsz);
	delete[] pwsz;

	// Creates a new one-dimensional array
	SAFEARRAY *psaStrings = SafeArrayCreateVector(VT_VARIANT, 0, 1);
	if (psaStrings == NULL) {
		pHtmlDoc->close();
		return;
	}
	VARIANT *param;
	hr = SafeArrayAccessData(psaStrings, (LPVOID*)&param);
	param->vt = VT_BSTR;
	param->bstrVal = bstr;
	hr = SafeArrayUnaccessData(psaStrings);
	hr = pHtmlDoc->write(psaStrings);

	setClickEvents(pHtmlDoc, GetDocument()->getFilePath().c_str());
	// SafeArrayDestroy calls SysFreeString for each BSTR
	if (psaStrings != NULL) {
		SafeArrayDestroy(psaStrings);
		pHtmlDoc->close();
	}
}

CComPtr<IHTMLTextContainer> getContainer(IDispatch* pDisp){
	if(NULL == pDisp)
		return NULL;
		CComPtr<IHTMLDocument2> pDocument2 = NULL; 
            if (S_OK == pDisp->QueryInterface(IID_IHTMLDocument2, (LPVOID*)&pDocument2)) 
            { 
                CComPtr<IHTMLElement> pElement = NULL; 
                if (S_OK == pDocument2->get_body(&pElement)) 
                { 
                    CComPtr<IHTMLTextContainer> pTextContainer = NULL; 
                    if (S_OK == pElement->QueryInterface(IID_IHTMLTextContainer, (LPVOID*)&pTextContainer)) 
                    { 
						return pTextContainer;
                    } 
                }                 
           } 
		return NULL;
}
float getScrollTop(IDispatch* pDisp)
{
    long scrollTop;
	CComPtr<IHTMLTextContainer> pTextContainer = getContainer(pDisp);
    if (pTextContainer &&  S_OK == pTextContainer->get_scrollTop(&scrollTop) ) 
    {
		long height;
		pTextContainer->get_scrollHeight(&height);
		return ((float)scrollTop)/height ;
    } 
	return 0.0;
}
void setScrollTop(IDispatch* pDisp, float scrollPercent)
{
	CComPtr<IHTMLTextContainer> pTextContainer = getContainer(pDisp);
    if (pTextContainer)
    {
		long top,height;
		pTextContainer->get_scrollTop(&top);
		pTextContainer->get_scrollHeight(&height);
		pTextContainer->put_scrollTop((long)(scrollPercent * height));
    } 
}
void CMarkdownEditorView::OnUpdate(CView* pSender, LPARAM /*lHint*/lParam, CObject* /*pHint*/)
{
	if(_bFirstNavigate){
		_bFirstNavigate = false;
		Navigate2(_T("about:blank"),NULL,NULL);
		//return;
	}
	if(!(lParam & LPARAM_Update))
		return;
	float scrollTop = 0;
	IDispatch* pDisp =GetHtmlDocument();
	
	if(pSender != NULL){
		scrollTop = getScrollTop(pDisp);
	}
	const string& str = GetDocument()->getText();	

	UpdateMd(str);
	if(lParam & LPARAM_MoveEnd){
		scrollTop = 1.0;
	}
	if(pSender != NULL){
		setScrollTop(pDisp,scrollTop);
	}



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

string&  replaceImgSrc(string& str, string path)
{
	// 拼“文档所在目录”，而不是把 .md 文件全路径直接挂在图片名前
	//（旧逻辑会得到 C:\x\doc.mdimage.png 这种畸形路径，IE 加载非法 file:// 时容易访问异常 → 程序崩溃）
	string dir = DirOfPath(path);
	if (dir.size() == 0)
		return str;
	string old_value = "<img src=\"";
	string new_value = "<img src=\"" + dir;
	for (string::size_type pos(0); pos != string::npos; pos += old_value.length())   {
		if ((pos = str.find(old_value, pos)) != string::npos){
			const char* start = str.c_str() + pos + old_value.length();
			// 跳过 http(s):// 网络图 与 data: 内联占位图，这两种不需要拼本地目录
			if (strnicmp(start, "http://", 7) != 0 && strnicmp(start, "https://", 8) != 0 && strnicmp(start, "data:", 5) != 0)
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
	// IE/CHtmlView 用 Unicode BSTR 写入，这里先把整体 HTML 转成 UTF-8，
	// 再用 CP_UTF8 转成 BSTR，避免中文乱码。
	return Util::ANSIToUTF8(strHtml.c_str());
}

void CMarkdownEditorView::UpdateMd(const string& strMd)
{
	string strHtml = GetMdHtml(strMd);
	NavigateHTML(strHtml);
}


