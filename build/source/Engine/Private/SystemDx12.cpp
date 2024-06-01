#include <pch.h>
#include <Engine/SystemDx12.h>
#include <Engine/SystemWin64.h>
#include <Engine/DebugLog.h>

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------- */
RHI::SDx12State _d3d;
RHI::SDx12State *gd3d = &_d3d;

bool RHI::Dx12::Initialize()
{
	if (FAILED(::D3D12CreateDevice(gSystem->pDxgiAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gd3d->pDevice))))
		return false;

	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	gd3d->pDevice->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&gd3d->pCmdQueue));

	gd3d->pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gd3d->pQueueFence));
	gd3d->FenceEvent = ::CreateEvent(nullptr, false, false, nullptr);
	gd3d->FenceValue = 0;

	gd3d->pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gd3d->pCmdAllocator));
	gd3d->pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gd3d->pCmdAllocator, nullptr, IID_PPV_ARGS(&gd3d->pCmd));

	gd3d->RenderTargetSize = gd3d->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	gd3d->GenericDescriptorSize = gd3d->pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	return true;
}

void RHI::Dx12::Shutdown()
{
	WaitForDrawing();

	gd3d->pCmd->Release();
	gd3d->pCmdAllocator->Release();
	::CloseHandle(gd3d->FenceEvent);
	gd3d->pQueueFence->Release();
	gd3d->pDevice->Release();

	ZeroThat(gd3d);
}

bool RHI::Dx12::WaitForDrawing()
{
	if (gd3d->pQueueFence->GetCompletedValue() != gd3d->FenceValue) {
		gd3d->pQueueFence->SetEventOnCompletion(gd3d->FenceValue, gd3d->FenceEvent);
		::WaitForSingleObject(gd3d->FenceEvent, INFINITE);
		return true;
	}
	return false;
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------- */
/* ---------------------------------------------------------------------------------------------------------------------------------------------------------------- */
bool _bWndClass = false;

LRESULT CALLBACK WndProcImpl(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
{
	auto pWnd = (RHI::CWindow *)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (pWnd) {
		switch (umsg) {

		case WM_CLOSE: { pWnd->Release(); } break;
		case WM_SIZE: {	pWnd->OnResize(); } break;
		case WM_ACTIVATEAPP: { pWnd->OnActivate(wparam); } break;
		}
	}
	return ::DefWindowProc(hwnd, umsg, wparam, lparam);
}

RHI::CWindow::CWindow(int Width, int Height, const TCHAR *Title)
{
	if (!_bWndClass) {
		WNDCLASSEX Wc = {};
		Wc.cbSize = sizeof(WNDCLASSEX);
		Wc.cbWndExtra = sizeof(intptr_t);
		Wc.hInstance = System::gSystem->hInstance;
		Wc.hCursor = ::LoadCursor(Wc.hInstance, IDC_ARROW);
		Wc.lpszClassName = L"System::CWindow";
		Wc.lpfnWndProc = WndProcImpl;
		Wc.style = CS_VREDRAW | CS_HREDRAW;
		::RegisterClassEx(&Wc);
		_bWndClass = true;
	}

	MONITORINFO Monitor = { sizeof(Monitor) };
	::GetMonitorInfo(::MonitorFromPoint({}, 1), &Monitor);

	RECT Rect = { 0, 0, Width, Height };
	::AdjustWindowRect(&Rect, WS_TILEDWINDOW, false);

	auto Px = ((Monitor.rcWork.right / 2) - (Rect.right / 2));
	auto Py = ((Monitor.rcWork.bottom / 2) - (Rect.bottom / 2));

	m_hWnd = ::CreateWindowEx(0, L"System::CWindow", Title, WS_TILEDWINDOW, Px, Py, Rect.right, Rect.bottom, nullptr, nullptr, gSystem->hInstance, nullptr);
	VERIFY(m_hWnd);

	::SetWindowLongPtrW(m_hWnd, GWLP_USERDATA, (uintptr_t)this);

	::GetClientRect(m_hWnd, &Rect);
	m_uSize[0] = (Rect.right - Rect.left);
	m_uSize[1] = (Rect.bottom - Rect.top);

	m_bInFocus = true;
	m_bShouldClose = false;

	// -- SwapChain;
	IDXGISwapChain1 *pSwap1;
	
	DXGI_SWAP_CHAIN_DESC1 Scd = {};
	Scd.BufferCount = 2;
	Scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Scd.SampleDesc.Count = 1;
	Scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Scd.Width = m_uSize[0];
	Scd.Height = m_uSize[1];
	
	gSystem->pDxgiFactory->CreateSwapChainForHwnd(gd3d->pCmdQueue, m_hWnd, &Scd, nullptr, nullptr, &pSwap1);
	pSwap1->QueryInterface(&m_pSwapChain);
	pSwap1->Release();

	m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pSwapChainImages[0]));
	m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pSwapChainImages[1]));

	D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
	HeapDesc.NumDescriptors = 2;
	HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	gd3d->pDevice->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&m_pRenderTargetHeap));

	auto Rtv = m_pRenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
	gd3d->pDevice->CreateRenderTargetView(m_pSwapChainImages[0], nullptr, Rtv);
	Rtv.ptr += gd3d->RenderTargetSize;
	gd3d->pDevice->CreateRenderTargetView(m_pSwapChainImages[1], nullptr, Rtv);
}

void System::CWindow::Release()
{
	Dx12::WaitForDrawing();

	m_pRenderTargetHeap->Release();
	m_pSwapChainImages[1]->Release();
	m_pSwapChainImages[0]->Release();
	m_pSwapChain->Release();

	m_bShouldClose = true;
}

void RHI::CWindow::OnResize()
{
	RECT Rect;
	::GetClientRect(hwnd, &Rect);
	m_uSize[0] = (Rect.right - Rect.left);
	m_uSize[1] = (Rect.bottom - Rect.top);

	m_pSwapChainImages[1]->Release();
	m_pSwapChainImages[0]->Release();
	m_pSwapChain->ResizeBuffers(2, m_uSize[0], m_uSize[1], DXGI_FORMAT_R8G8B8A8_UNORM, 0);
	m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pSwapChainImages[0]));
	m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pSwapChainImages[1]));

	auto Rtv = m_pRenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
	gd3d->pDevice->CreateRenderTargetView(m_pSwapChainImages[0], nullptr, Rtv);
	Rtv.ptr += gd3d->RenderTargetSize;
	gd3d->pDevice->CreateRenderTargetView(m_pSwapChainImages[1], nullptr, Rtv);
}

void System::CWindow::Show() const { ::ShowWindow(m_hWnd, SW_SHOW); }
void System::CWindow::Hide() const { ::ShowWindow(m_hWnd, SW_HIDE); }

ID3D12Resource *System::CWindow::GetCurrentRenderTargetResource() const { 
	return m_pSwapChainImages[m_pSwapChain->GetCurrentBackBufferIndex()]; 
}

D3D12_CPU_DESCRIPTOR_HANDLE System::CWindow::GetCurrentRenderTarget() const { 
	auto Rtv = m_pRenderTargetHeap->GetCPUDescriptorHandleForHeapStart(); 
	Rtv.ptr += (m_pSwapChain->GetCurrentBackBufferIndex() * gd3d->RenderTargetSize); 
	return Rtv; 
}

void System::CWindow::PumpEvents()
{
	MSG Msg;
	while (::PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE)) {
		::TranslateMessage(&Msg);
		::DispatchMessage(&Msg);
	}
}