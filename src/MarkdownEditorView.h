
// MarkdownEditorView.h : CMarkdownEditorView 类的接口
//

#pragma once
#include <string>
#include <wrl/client.h>
#include <wrl/event.h>
#include "WebView2.h"
using namespace std;
using Microsoft::WRL::ComPtr;

class CMarkdownEditorView : public CView
{
private:
	bool _bFirstNavigate;
	string _strCSS;
	void initCSS();
	string GetMdHtml(const string& str);

	// WebView2（Edge / Chromium）相关成员，替代原 CHtmlView(IE)
	ComPtr<ICoreWebView2Environment> m_environment;
	ComPtr<ICoreWebView2Controller>  m_controller;
	ComPtr<ICoreWebView2>            m_webView;
	bool m_bWebViewReady;          // WebView2 控制器是否就绪
	CStringW m_pendingHtml;        // 就绪前缓存待渲染的 HTML
	CStringW m_tempHtmlPath;       // 渲染用的临时 .html 文件路径（file:// 导航用）

	void InitializeWebView();
	void ResizeWebView();
	void NavigateToHtml(const CStringW& html);
	CStringW Utf8ToWide(const string& utf8);

public:
	void UpdateMd(const string& strMd);

protected: // 仅从序列化创建
	CMarkdownEditorView();
	DECLARE_DYNCREATE(CMarkdownEditorView)

// 特性
public:
	CMarkdownEditorDoc* GetDocument() const;

// 操作
public:

// 重写
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void OnInitialUpdate();         // 构造后第一次调用
	virtual void OnDraw(CDC* pDC);           // CView 要求的纯虚函数，预览由 WebView2 渲染
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();

// 实现
public:
	virtual ~CMarkdownEditorView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// 生成的消息映射函数
protected:
	DECLARE_MESSAGE_MAP()
	virtual void OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/);
};

#ifndef _DEBUG  // MarkdownEditorView.cpp 中的调试版本
inline CMarkdownEditorDoc* CMarkdownEditorView::GetDocument() const
   { return reinterpret_cast<CMarkdownEditorDoc*>(m_pDocument); }
#endif
