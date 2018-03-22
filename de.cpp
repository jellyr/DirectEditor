// de.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "de.h"
#include <string>

#include <vector>


#include <d2d1.h>
#include <dwrite.h>

#include <functional>

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")



#ifndef UNICODE
typedef    std::string TString;
#else
typedef    std::wstring TString;
#endif


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


void msg(TCHAR * s)
{
	::MessageBox(0, s, 0, 0);
}


struct DrawingContext
{
	DrawingContext(ID2D1RenderTarget * renderTarget,
		ID2D1Brush * defaultBrush)
	{
		this->renderTarget = renderTarget;
		this->defaultBrush = defaultBrush;
	}

	ID2D1RenderTarget * renderTarget;
	ID2D1Brush * defaultBrush;
};


enum class BackgroundMode
{
	TextHeight,
	TextHeightWithLineGap,
	LineHeight
};

enum class UnderlineType
{
	None = 0,
	Single = 1,
	Double = 2,
	Triple = 3,
	Squiggly = 4


};


class CharacterFormatSpecifier : IUnknown
{
public:

	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		m_refCount++;
		return m_refCount;
	}


	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		m_refCount--;
		LONG newCount = m_refCount;

		if (m_refCount == 0)
			delete this;

		return newCount;
	}


	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
	{
		*ppvObject = nullptr;
		HRESULT hr = S_OK;

		if (riid == __uuidof(IUnknown))
		{
			*ppvObject = this;
			AddRef();
		}
		else
		{
			hr = E_NOINTERFACE;
		}
		return hr;
	}

	void GetBackgroundBrush(BackgroundMode * pMode, ID2D1Brush **pBrush)
	{
		*pMode = m_backgroundMode;
		*pBrush = m_backgroundBrush;
	}

	static HRESULT SetFormatting(
		IDWriteTextLayout * textLayout,
		DWRITE_TEXT_RANGE textRange,
		std::function<void(CharacterFormatSpecifier*)> setField)
	{

		const UINT32 endPosition = textRange.startPosition + textRange.length;
		UINT32 currentPosition = textRange.startPosition;

		while (currentPosition < endPosition)
		{
			CharacterFormatSpecifier * specifier = nullptr;
			DWRITE_TEXT_RANGE queryTextRange;
			HRESULT hr;

			hr = textLayout->GetDrawingEffect(currentPosition,
				(IUnknown **)& specifier,
				&queryTextRange);
			if (S_OK != hr)
			{
				return hr;
			}

			if (specifier == nullptr)
			{
				specifier = new CharacterFormatSpecifier();
			}
			else
			{
				specifier = specifier->Clone();
			}

			setField(specifier);

			UINT32 queryEndPos = queryTextRange.startPosition + queryTextRange.length;

			UINT32 setLength = min(endPosition, queryEndPos) - currentPosition;

			DWRITE_TEXT_RANGE setTextRange;
			setTextRange.startPosition = currentPosition;
			setTextRange.length = setLength;

			hr = textLayout->SetDrawingEffect((IUnknown *)specifier, setTextRange);

			if (hr != S_OK)
			{
				msg(_T("fail to set textLayout->SetDrawingEffect((IUnknown *)specifier, setTextRange);"));
			}

			currentPosition = currentPosition + setLength;


		}


		return S_OK;
	}
	CharacterFormatSpecifier* Clone()
	{
		CharacterFormatSpecifier * specifier = new CharacterFormatSpecifier();

		specifier->m_backgroundBrush = this->m_backgroundBrush;
		specifier->m_backgroundMode = this->m_backgroundMode;

		specifier->m_underlineBrush = this->m_underlineBrush;
		specifier->m_underlineType = this->m_underlineType;
		return specifier;
	}

	static HRESULT SetBackgroundBrush(IDWriteTextLayout * textLayout,
		BackgroundMode backgroundMode,
		ID2D1Brush* brush,
		DWRITE_TEXT_RANGE textRange)
	{
		return SetFormatting(textLayout,
			textRange, 
			[backgroundMode, brush](CharacterFormatSpecifier * specifier)
		{
			specifier->m_backgroundBrush = brush;
			specifier->m_backgroundMode = backgroundMode;
		});
	}
	
private:
	LONG m_refCount;
	
	BackgroundMode  m_backgroundMode;
	ID2D1Brush *    m_backgroundBrush;

	UnderlineType  m_underlineType;
	ID2D1Brush *    m_underlineBrush;



};


class TCustomRender : public IDWriteTextRenderer
{
	void FillRectangle(void * clientDrawingContext,
		IUnknown * clientDrawingEffect,
		float x, float y,
		float width, float thickness,
		DWRITE_READING_DIRECTION readingDirection,
		DWRITE_FLOW_DIRECTION flowDirection)
	{
		DrawingContext * drawingContext =
			static_cast<DrawingContext *>(clientDrawingContext);

		// Get brush
		ID2D1Brush * brush = drawingContext->defaultBrush;



		if (clientDrawingEffect != nullptr)
		{
			void * pInterface;

			if (S_OK == clientDrawingEffect->QueryInterface(__uuidof(ID2D1Brush), &pInterface))
			{
				brush = static_cast<ID2D1Brush *>(pInterface);
			}
		}


		//if (clientDrawingEffect != nullptr)
		//{
		//	brush = static_cast<ID2D1Brush *>(clientDrawingEffect);
		//}

		D2D1_RECT_F rect = D2D1::RectF(x, y, x + width, y + thickness);
		drawingContext->renderTarget->FillRectangle(&rect, brush);
	}

public:

	enum class RenderPass
	{
		Initial,
		Main,
		Final
	};




	RenderPass m_renderPass;
public:
	LONG m_refCount;

	std::vector < DWRITE_LINE_METRICS> m_lineMetrics;

	int m_lineIndex;

	int m_charIndex;



	ID2D1RenderTarget * m_renderTarget{ nullptr };

	ID2D1Brush        * m_defaultBrush{ nullptr };



public:
	TCustomRender() : m_refCount(0)
	{

	}


	HRESULT customDraw(ID2D1RenderTarget * renderTarget,
		IDWriteTextLayout *textLayout,
		D2D1_POINT_2F origin,
		ID2D1Brush * defaultBrush)
	{
		HRESULT hr;
		UINT32 actualLineCount;
		hr = textLayout->GetLineMetrics(nullptr, 0, &actualLineCount);


		if (E_NOT_SUFFICIENT_BUFFER != hr)
		{
			return hr;
		}

		m_lineMetrics = std::vector<DWRITE_LINE_METRICS>(actualLineCount);

		hr = textLayout->GetLineMetrics(m_lineMetrics.data(),
										m_lineMetrics.size(),
										&actualLineCount);

		if (S_OK != hr)
		{
			return hr;
		}

		m_renderTarget = renderTarget;
		m_defaultBrush = defaultBrush;


		for (m_renderPass = RenderPass::Initial;
			m_renderPass <= RenderPass::Final;
			m_renderPass = (RenderPass)((int)m_renderPass + 1))
		{
			m_lineIndex = m_charIndex = 0;

			hr = textLayout->Draw(nullptr, this, origin.x, origin.y);
		}


		return hr;
	}

	// IUnknown methods
	virtual ULONG STDMETHODCALLTYPE AddRef() override
	{
		m_refCount++;
		return m_refCount;
	}
	virtual ULONG STDMETHODCALLTYPE Release() override
	{
		m_refCount--;
		LONG newCount = m_refCount;  // 應該加上互斥啊！ 

		if (m_refCount == 0)
			delete this;

		return newCount;
	}
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppOutput) override
	{
		*ppOutput = nullptr;
		HRESULT hr = S_OK;

		if (riid == __uuidof(IDWriteTextRenderer))
		{
			*ppOutput = static_cast<IDWriteTextRenderer*>(this);
			AddRef();
		}
		else if (riid == __uuidof(IDWritePixelSnapping))
		{
			*ppOutput = static_cast<IDWritePixelSnapping*>(this);
			AddRef();
		}
		else if (riid == __uuidof(IUnknown))
		{
			*ppOutput = this;
			AddRef();
		}
		else
		{
			hr = E_NOINTERFACE;
		}
		return hr;
	}

	// IDWritePixelSnapping methods
	virtual HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void * clientDrawingContext,
		_Out_ BOOL * isDisabled) override
	{
		*isDisabled = false;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void * clientDrawingContext,
		_Out_ FLOAT * pixelsPerDip) override
	{
		if (clientDrawingContext == nullptr)
		{
			float dpiX, dpiY;
			m_renderTarget->GetDpi(&dpiX, &dpiY);
			*pixelsPerDip = dpiX / 96;
			return S_OK;

		}
		else
		{
			DrawingContext * drawingContext =
				static_cast<DrawingContext *>(clientDrawingContext);

			float dpiX, dpiY;
			drawingContext->renderTarget->GetDpi(&dpiX, &dpiY);
			*pixelsPerDip = dpiX / 96;
			return S_OK;
		}


	}

	virtual HRESULT STDMETHODCALLTYPE GetCurrentTransform(void * clientDrawingContext,
		_Out_ DWRITE_MATRIX * transform) override
	{
		if (clientDrawingContext == nullptr)
		{
			if (m_renderTarget)
			{
				m_renderTarget->GetTransform((D2D1_MATRIX_3X2_F *)transform);
				return S_OK;
			}
			return E_NOTIMPL;
		}
			
		DrawingContext * drawingContext =
			static_cast<DrawingContext *>(clientDrawingContext);

		// Matrix structures are defined identically
		drawingContext->renderTarget->GetTransform((D2D1_MATRIX_3X2_F *)transform);
		return S_OK;
	}

	// IDWriteTextRenderer methods
	virtual HRESULT STDMETHODCALLTYPE DrawGlyphRun(void * clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		DWRITE_MEASURING_MODE measuringMode,
		_In_ const DWRITE_GLYPH_RUN * glyphRun,
		_In_ const DWRITE_GLYPH_RUN_DESCRIPTION * glyphRunDescription,
		IUnknown * clientDrawingEffect) override
	{
		if (clientDrawingContext == nullptr)
		{

			if (m_renderTarget)
			{
				ID2D1Brush * foregroundBrush = m_defaultBrush;
				switch (m_renderPass)
				{
				case TCustomRender::RenderPass::Initial:
					break;
				case TCustomRender::RenderPass::Main:
					m_renderTarget->DrawGlyphRun(D2D1::Point2F(baselineOriginX,
						baselineOriginY), glyphRun, foregroundBrush, measuringMode);
					break;
				case TCustomRender::RenderPass::Final:
					break;
				default:
					break;
				}

				

			}

			if (clientDrawingEffect)
			{
				CharacterFormatSpecifier * specifier = nullptr;

				void * pInterface;
				if (S_OK == clientDrawingEffect->QueryInterface(__uuidof(IUnknown), &pInterface))
				{
					specifier = (CharacterFormatSpecifier *)pInterface;
					if (specifier)
					{
						ID2D1Brush * backgroundBrush = nullptr;
						

						ID2D1Brush * highlightBrush = nullptr;

						BackgroundMode backgroundMode = BackgroundMode::TextHeight;

						//specifier->GetBackgroundBrush(&backgroundMode, &backgroundBrush);

						//ID2D1Brush * brush = specifier->GetForegroundBrush();

						//if (brush != nullptr)
						//{
						//	foregroundBrush = brush;
						//}

						//highlightBrush = specifier->GetHighlight();

					}
				}
			}
			return S_OK;
		}
		else
		{
			DrawingContext * drawingContext =
				static_cast<DrawingContext *>(clientDrawingContext);

			ID2D1RenderTarget * renderTarget = drawingContext->renderTarget;

			// Get brush
			ID2D1Brush * brush = drawingContext->defaultBrush;

			if (clientDrawingEffect != nullptr)
			{
				void * pInterface;

				if (S_OK == clientDrawingEffect->QueryInterface(__uuidof(ID2D1Brush), &pInterface))
				{
					brush = static_cast<ID2D1Brush *>(pInterface);
				}
			}



					








			renderTarget->DrawGlyphRun(D2D1::Point2F(baselineOriginX, baselineOriginY),
				glyphRun, brush, measuringMode);
			return S_OK;
		}



	}

	virtual HRESULT STDMETHODCALLTYPE DrawUnderline(void * clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		_In_ const DWRITE_UNDERLINE * underline,
		IUnknown * clientDrawingEffect) override
	{


		if (clientDrawingContext == nullptr)
			return E_NOTIMPL;

		FillRectangle(clientDrawingContext,
			clientDrawingEffect,
			baselineOriginX,
			baselineOriginY + underline->offset,
			underline->width,
			underline->thickness,
			underline->readingDirection,
			underline->flowDirection);


		FillRectangle(clientDrawingContext,
			clientDrawingEffect,
			baselineOriginX,
			baselineOriginY + underline->offset + 4,
			underline->width,
			underline->thickness,
			underline->readingDirection,
			underline->flowDirection);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DrawStrikethrough(void * clientDrawingContext,
		FLOAT baselineOriginX,
		FLOAT baselineOriginY,
		_In_ const DWRITE_STRIKETHROUGH * strikethrough,
		IUnknown * clientDrawingEffect) override
	{
		if (clientDrawingContext == nullptr)
			return E_NOTIMPL;


		FillRectangle(clientDrawingContext,
			clientDrawingEffect,
			baselineOriginX,
			baselineOriginY + strikethrough->offset,
			strikethrough->width,
			strikethrough->thickness,
			strikethrough->readingDirection,
			strikethrough->flowDirection);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE DrawInlineObject(void * clientDrawingContext,
		FLOAT originX,
		FLOAT originY,
		IDWriteInlineObject * inlineObject,
		BOOL isSideways,
		BOOL isRightToLeft,
		IUnknown * clientDrawingEffect) override
	{

		if (clientDrawingContext == nullptr)
			return E_NOTIMPL;

		DrawingContext * drawingContext =
			static_cast<DrawingContext *>(clientDrawingContext);

		return inlineObject->Draw(clientDrawingContext,
			this,
			originX,
			originY,
			isSideways,
			isRightToLeft,
			clientDrawingEffect);
	}


};




template <class T> inline void SafeRelease(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}


ID2D1Factory* pD2dFactory = nullptr;

IDWriteFactory* pDwrtieFactory = nullptr;

IDWriteTextFormat* pDwriteTextFormat = nullptr;

IDWriteTextLayout *g_pTextLayout = nullptr;

ID2D1HwndRenderTarget * pRenderTarget = nullptr;

ID2D1SolidColorBrush * pTextBrush = nullptr;

ID2D1SolidColorBrush * m_blackBrush = nullptr;

ID2D1SolidColorBrush * m_whiteBrush = nullptr;


ID2D1SolidColorBrush * redBrush = nullptr;

ID2D1SolidColorBrush * greenBrush = nullptr;

ID2D1SolidColorBrush * blueBrush = nullptr;

ID2D1SolidColorBrush * m_overlayBrush = nullptr;

ID2D1SolidColorBrush * magentaBrush = nullptr;


void CreateDeviceDependentResources()
{
	try
	{
		if (!pRenderTarget)
		{
			throw _T("pRenderTarget == nullptr");
		}

		if (pTextBrush)
		{
			SafeRelease(&pTextBrush);
		}
		if (redBrush)
		{
			SafeRelease(&redBrush);
		}
		if (greenBrush)
		{
			SafeRelease(&greenBrush);
		}
		if (blueBrush)
		{
			SafeRelease(&blueBrush);
		}
		if (m_overlayBrush)
		{
			SafeRelease(&m_overlayBrush);
		}
		if (magentaBrush)
		{
			SafeRelease(&magentaBrush);
		}
		if (m_whiteBrush)
		{
			SafeRelease(&magentaBrush);
		}
		if (m_blackBrush)
		{
			SafeRelease(&magentaBrush);
		}
		

		HRESULT hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(RGB(255, 255, 255)), &pTextBrush);

		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush pTextBrush error!");
		}

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Magenta), &magentaBrush);
		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush magentaBrush error!");
		}

		hr =  pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_blackBrush);
		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush m_blackBrush error!");
		}

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_whiteBrush);
		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush m_whiteBrush error!");
		}
		

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0, 0, 0.5f), &m_overlayBrush);
		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush m_overlayBrush error!");
		}
		

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(RGB(255, 0, 0)), &blueBrush);

		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush blueBrush error!");
		}

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(RGB(0, 255, 0)), &greenBrush);

		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush greenBrush error!");
		}

		hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(RGB(0, 0, 255)), &redBrush );

		if (FAILED(hr))
		{
			throw _T("CreateSolidColorBrush redBrush error!");
		}

	}
	catch (TCHAR * errorMsg)
	{
		msg(errorMsg);
		
	}

}


HRESULT CreateTextLayouts();
bool InitD2DResource(HWND hwnd)
{
	msg(_T("init d2d"));
	try
	{
		// init d2d factory 
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2dFactory);
		if (FAILED(hr))
		{
			throw _T("D2D1CreateFactory error!");
		}


		// init d2d font
		hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDwrtieFactory));
		if (FAILED(hr))
		{
			throw _T("DWriteCreateFactory error!");
		}

		hr = pDwrtieFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 30, L"", &pDwriteTextFormat);
		if (FAILED(hr))
		{
			throw _T("CreateTextFormat error!");
		}


		//init device
		RECT rc;
		GetClientRect(hwnd, &rc);
		D2D1_SIZE_U sz = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();

		hr = pD2dFactory->CreateHwndRenderTarget(rtProps,
			D2D1::HwndRenderTargetProperties(hwnd, sz),
			&pRenderTarget
			);

		if (FAILED(hr))
		{
			throw _T("CreateHwndRenderTarget error!");
		}

		CreateDeviceDependentResources();
		CreateTextLayouts();
	}
	catch (TCHAR * errorMsg)
	{
		msg(errorMsg);
		return false;
	}
	return true;
}

//IDWriteTextLayout *g_pTextLayout = nullptr;
//
//ID2D1Factory* pD2dFactory = nullptr;
//
//IDWriteFactory* pDwrtieFactory = nullptr;
//
//IDWriteTextFormat* pDwriteTextFormat = nullptr;
//
//ID2D1HwndRenderTarget * pRenderTarget = nullptr;
//
//ID2D1SolidColorBrush * pTextBrush = nullptr;



HRESULT CreateTextLayouts()
{
	HRESULT hr = S_OK;

	// The text that will be rendered.
	//WCHAR* pStr = L"Test of formatting: normal, bold, underlined, italic, huge, bold&italic&underlined.\n"
	//	L"Test of coloring: orange, red, blue.\n"
	//	L"Test of hit-testing: move your mouse over the third word of this sentence.\n"
	//	L"Test of word wrapping: this text has some longer lines to test the proper working of the word wrapping feature of DirectWrite. "
	//	L"To test the correct word wrapping, try to resize this window and verify that this whole piece of text is readable without missing words.\n\n"
	//	L"The Arabic example below shows that rendering right-to-left languages is not a problem at all. NOTE: I have no idea what that text is saying, "
	//	L"so do not blame me if it says something wrong.\n\n"
	//	L"The \"Fancy Typography Rendering\" example below uses some special typographic features of the Grabriola font. "
	//	L"It also shows how to mix different text alignment for different paragraphs and how to use gradient brushes.\n\n";


	std::wstring m_text = L"This paragraph of text rendered with "
		L"DirectWrite is based on "
		L"IDWriteTextFormat and IDWriteTextLayout "
		L"objects and is capable of different "
		L"RBG RGB foreground colors, such as red, "
		L"green, and blue by passing brushes to "
		L"the SetDrawingEffect method."
		L"and using different font sizes and "
		L"redtext greentext bluetext Underline strikethrough";

	D2D1_SIZE_F sizeRT = pRenderTarget->GetSize();

	try
	{
		// Create a text layout object.
		if (g_pTextLayout)
		{
			SafeRelease(&g_pTextLayout);
		}

		hr = pDwrtieFactory->CreateTextLayout(m_text.c_str(),
												m_text.length(),
												pDwriteTextFormat, sizeRT.width, sizeRT.height, &g_pTextLayout);

		if (FAILED(hr))
		{
			throw _T("CreateTextLayout error!");
		}


		DWRITE_TEXT_RANGE textRange;

		std::wstring strFind = L"IDWriteTextFormat";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
		hr = g_pTextLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, textRange);
		if (FAILED(hr))
		{
			throw _T("SetFontStyle IDWriteTextFormat error!");
		}

		strFind = L"sizes";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
		hr = g_pTextLayout->SetFontSize(48.0f, textRange);
		if (FAILED(hr))
		{
			throw _T("SetFontSize sizes error!");
		}


		



		strFind = L"Underline";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
		hr = g_pTextLayout->SetUnderline(TRUE, textRange);
		if (FAILED(hr))
		{
			throw _T("SetUnderline  error!");
		}






		strFind = L"strikethrough";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
		hr = g_pTextLayout->SetStrikethrough(TRUE, textRange);
		if (FAILED(hr))
		{
			throw _T("SetStrikethrough  error!");
		}


		//  magentaBrush
		strFind = L"IDWriteTextFormat and IDWriteTextLayout";

		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();

			CharacterFormatSpecifier::SetBackgroundBrush(g_pTextLayout,
		BackgroundMode::LineHeight,
			magentaBrush,
			textRange);





		strFind = L"redtext";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
	
		//hr = g_pTextLayout->SetStrikethrough(true, textRange);
		textRange.length = 7;
	
		hr = g_pTextLayout->SetDrawingEffect(redBrush, textRange);
		if (FAILED(hr))
		{
			throw _T("SetDrawingEffect redBrush error!");
		}


		strFind = L"greentext";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();

		//hr = g_pTextLayout->SetStrikethrough(true, textRange);
	

		hr = g_pTextLayout->SetDrawingEffect(greenBrush, textRange);
		if (FAILED(hr))
		{
			throw _T("SetDrawingEffect redBrush error!");
		}


		strFind = L"bluetext";
		textRange.startPosition = m_text.find(strFind.data());
		textRange.length = strFind.length();
		hr = g_pTextLayout->SetDrawingEffect(blueBrush, textRange);
		if (FAILED(hr))
		{
			throw _T("SetDrawingEffect redBrush error!");
		}

	}
	catch (TCHAR * errorMsg)
	{
		msg(errorMsg);
	}


	return hr;

}

TCustomRender customRender;
int drawcount = 0;

void onPaint()
{
	if (!pRenderTarget || !pTextBrush || !pDwriteTextFormat || !g_pTextLayout)
	{
	
		msg(_T("on paint error!"));
	}
	pRenderTarget->BeginDraw();

	// Clear Background  
	pRenderTarget->Clear(D2D1::ColorF(0.0, 0.0, 0.00));

	// Draw Text   
	D2D1_SIZE_F size = pRenderTarget->GetSize();

	WCHAR * text = L"这个函数的作用是，使得Client的一个矩形区域变得无效，rect结构体可以自己编辑，也可以使用GetClientRcet（）来填充（这里的矩形大小Client的大小），最主要的是第三个参数，第三个参数决定了是否发送WM_ERASEBKGND消息，从而决定了是否擦除Client原有的图形。当然InvalidateRect发送WM_PAINT的形式是一种POST形式（即发送到程序消息队列），而不是像SendMessage一样直接让操作系统带着消息，调用WndProc。一些中文 can show chinese ! جميعها";

	int modc = 4;
	if (drawcount%modc == 0)
	{
		pRenderTarget->DrawText(text, wcslen(text),
			pDwriteTextFormat,
			D2D1::RectF(100, 170, size.width, size.height),
			pTextBrush);
	}
	else if (drawcount % modc == 1)
	{
		D2D1_POINT_2F origin;
		origin.x = origin.y = 0;
		pRenderTarget->DrawTextLayout(origin,
			g_pTextLayout,
			pTextBrush);

	}
	else if (drawcount % modc == 2)
	{
		DrawingContext drawingContext(pRenderTarget, m_overlayBrush);

		D2D1_POINT_2F origin = D2D1::Point2F(0.f, 0.f);

		g_pTextLayout->Draw(&drawingContext,
			&customRender,
			origin.x, origin.y);
		
	}
	else if (drawcount % modc == 3)
	{
		D2D1_POINT_2F origin = D2D1::Point2F(0.f, 0.f);
		customRender.customDraw(pRenderTarget, g_pTextLayout, origin, m_whiteBrush);


	}


	//DrawingContext drawingContext(pRenderTarget, m_overlayBrush);

	//D2D1_POINT_2F origin = D2D1::Point2F(0.f, 0.f);

	//g_pTextLayout->Draw(&drawingContext,
	//	&customRender,
	//	origin.x, origin.y);
	pRenderTarget->EndDraw();
}








int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

	
    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DE));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

	case WM_SIZE:
	{
		if (pRenderTarget)
			pRenderTarget->Resize(D2D1::SizeU(LOWORD(lParam), HIWORD(lParam)));
		else
			msg(_T("pRenderTarget is nullptr"));

		CreateTextLayouts();
		
	}
	break;

	case WM_LBUTTONDOWN:
		++drawcount;
		RECT r;
		GetClientRect(hWnd, &r);
		InvalidateRect(hWnd, &r, false);
		break;
	case WM_CREATE:
	{
		InitD2DResource(hWnd);
	}
	break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
			onPaint();
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
