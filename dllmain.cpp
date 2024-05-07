#include "stdafx.h"
#include <cstdint>
#include "tahoma.ttf.h"
#include "imgui/Kiero/kiero.h"
#include "imgui_draw.h"
#include "game.h"
#include "sdk.h"
#include "Menu.h"
#include<d3d12.h>
#include<winnt.h>
#include"memory.h"
#include"imgui/imgui.h"
#include "xorstr.hpp"
#pragma comment(lib, "user32.lib")
#include "globals.h"
#include"customstyle.h"
#include "mem.h"

#define INRANGE(x, a, b) (x >= a && x <= b)
#define GET_BITS( x ) (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define GET_BYTE( x ) (GET_BITS(x[0]) << 4 | GET_BITS(x[1]))

typedef long(__fastcall* Present)(IDXGISwapChain*, UINT, UINT);
static Present ori_present = NULL;
void WndProc_hk();

typedef LRESULT(CALLBACK* tWndProc)(HWND hWnd, UINT Msg, WPARAM wp, LPARAM lp);
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void WriteLogFile(const char* szString)
{
	

		FILE* pFile = fopen("log.txt", "a");
	fprintf(pFile, "%s\n", szString);
	fclose(pFile);

	

}
namespace d3d12test
{
	ID3D12CommandQueue* d3d12CommandQueue2 = nullptr;
	ID3D12CommandQueue* d3d12CommandQueueoriginal = nullptr;
}
namespace d3d12
{
	
	IDXGISwapChain3* pSwapChain;
	ID3D12Device* pDevice;
	ID3D12CommandQueue* pCommandQueue;
	ID3D12Fence* pFence;
	ID3D12DescriptorHeap* d3d12DescriptorHeapBackBuffers = nullptr;
	ID3D12DescriptorHeap* d3d12DescriptorHeapImGuiRender = nullptr;
	ID3D12DescriptorHeap* pSrvDescHeap = nullptr;;
	ID3D12DescriptorHeap* pRtvDescHeap = nullptr;;
	ID3D12GraphicsCommandList* pCommandList;

	
	FrameContext* FrameContextArray;
	ID3D12Resource** pID3D12ResourceArray;
	D3D12_CPU_DESCRIPTOR_HANDLE* RenderTargetDescriptorArray;

	
	HANDLE hSwapChainWaitableObject;
	HANDLE hFenceEvent;

	
	UINT NUM_FRAMES_IN_FLIGHT;
	UINT NUM_BACK_BUFFERS;

	
	UINT   frame_index = 0;
	UINT64 fenceLastSignaledValue = 0;
}

void unhookPresent();
void hookpresent();


namespace imgui
{
	bool is_ready;
	bool is_need_reset_imgui;

	bool IsReady()
	{
		return is_ready;
	}

	void reset_imgui_request()
	{
		is_need_reset_imgui = true;
	}

	__forceinline bool get_is_need_reset_imgui()
	{
		return is_need_reset_imgui;
	}

	void init_d3d12(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue)
	{

		d3d12::pSwapChain = pSwapChain;
		d3d12::pCommandQueue = pCommandQueue;

		if (!SUCCEEDED(d3d12::pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12::pDevice)))
			Exit();

		{
			DXGI_SWAP_CHAIN_DESC1 desc;

			if (!SUCCEEDED(d3d12::pSwapChain->GetDesc1(&desc)))
				Exit();

			d3d12::NUM_BACK_BUFFERS = desc.BufferCount;
			d3d12::NUM_FRAMES_IN_FLIGHT = desc.BufferCount;
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = d3d12::NUM_BACK_BUFFERS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;

			if (!SUCCEEDED(d3d12::pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12::pRtvDescHeap))))
				Exit();
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0;

			if (!SUCCEEDED(d3d12::pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12::pSrvDescHeap))))
				Exit();
		}

		if (!SUCCEEDED(d3d12::pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12::pFence))))
			Exit();

		d3d12::FrameContextArray = new FrameContext[d3d12::NUM_FRAMES_IN_FLIGHT];
		d3d12::pID3D12ResourceArray = new ID3D12Resource * [d3d12::NUM_BACK_BUFFERS];
		d3d12::RenderTargetDescriptorArray = new D3D12_CPU_DESCRIPTOR_HANDLE[d3d12::NUM_BACK_BUFFERS];

		for (UINT i = 0; i < d3d12::NUM_FRAMES_IN_FLIGHT; ++i)
		{
			if (!SUCCEEDED(d3d12::pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12::FrameContextArray[i].CommandAllocator))))
				Exit();
		}

		SIZE_T nDescriptorSize = d3d12::pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12::pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

		for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
		{
			d3d12::RenderTargetDescriptorArray[i] = rtvHandle;
			rtvHandle.ptr += nDescriptorSize;
		}


		if (!SUCCEEDED(d3d12::pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12::FrameContextArray[0].CommandAllocator, NULL, IID_PPV_ARGS(&d3d12::pCommandList))) ||
			!SUCCEEDED(d3d12::pCommandList->Close()))
		{
			Exit();
		}


		d3d12::hSwapChainWaitableObject = d3d12::pSwapChain->GetFrameLatencyWaitableObject();

		d3d12::hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (d3d12::hFenceEvent == NULL)
			Exit();


		ID3D12Resource* pBackBuffer;
		for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
		{
			if (!SUCCEEDED(d3d12::pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer))))
				Exit();

			d3d12::pDevice->CreateRenderTargetView(pBackBuffer, NULL, d3d12::RenderTargetDescriptorArray[i]);
			d3d12::pID3D12ResourceArray[i] = pBackBuffer;
		}
	}

	void _clear()
	{
		d3d12::pSwapChain = nullptr;
		d3d12::pDevice = nullptr;
		d3d12::pCommandQueue = nullptr;

		if (d3d12::pFence)
		{
			d3d12::pFence->Release();
			d3d12::pFence = nullptr;
		}

		if (d3d12::pSrvDescHeap)
		{
			d3d12::pSrvDescHeap->Release();
			d3d12::pSrvDescHeap = nullptr;
		}

		if (d3d12::pRtvDescHeap)
		{
			d3d12::pRtvDescHeap->Release();
			d3d12::pRtvDescHeap = nullptr;
		}

		if (d3d12::pCommandList)
		{
			d3d12::pCommandList->Release();
			d3d12::pCommandList = nullptr;
		}

		if (d3d12::FrameContextArray)
		{
			for (UINT i = 0; i < d3d12::NUM_FRAMES_IN_FLIGHT; ++i)
			{
				if (d3d12::FrameContextArray[i].CommandAllocator)
				{
					d3d12::FrameContextArray[i].CommandAllocator->Release();
					d3d12::FrameContextArray[i].CommandAllocator = nullptr;
				}
			}

			delete[] d3d12::FrameContextArray;
			d3d12::FrameContextArray = NULL;
		}

		if (d3d12::pID3D12ResourceArray)
		{
			for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
			{
				if (d3d12::pID3D12ResourceArray[i])
				{
					d3d12::pID3D12ResourceArray[i]->Release();
					d3d12::pID3D12ResourceArray[i] = nullptr;
				}
			}

			delete[] d3d12::pID3D12ResourceArray;
			d3d12::pID3D12ResourceArray = NULL;
		}

		if (d3d12::RenderTargetDescriptorArray)
		{
			delete[] d3d12::RenderTargetDescriptorArray;
			d3d12::RenderTargetDescriptorArray = NULL;
		}


		if (d3d12::hSwapChainWaitableObject)
		{
			d3d12::hSwapChainWaitableObject = nullptr;
		}

		if (d3d12::hFenceEvent)
		{
			CloseHandle(d3d12::hFenceEvent);
			d3d12::hFenceEvent = nullptr;
		}


		d3d12::NUM_FRAMES_IN_FLIGHT = 0;
		d3d12::NUM_BACK_BUFFERS = 0;


		d3d12::frame_index = 0;
	}

	void clear()
	{
		if (d3d12::FrameContextArray)
		{
			FrameContext* frameCtxt = &d3d12::FrameContextArray[d3d12::frame_index % d3d12::NUM_FRAMES_IN_FLIGHT];

			UINT64 fenceValue = frameCtxt->FenceValue;

			if (fenceValue == 0)
				return; // No fence was signaled

			frameCtxt->FenceValue = 0;

			bool bNotWait = d3d12::pFence->GetCompletedValue() >= fenceValue;

			if (!bNotWait)
			{
				d3d12::pFence->SetEventOnCompletion(fenceValue, d3d12::hFenceEvent);

				WaitForSingleObject(d3d12::hFenceEvent, INFINITE);
			}

			_clear();
		}
	}

	FrameContext* WaitForNextFrameResources()
	{
		UINT nextFrameIndex = d3d12::frame_index + 1;
		d3d12::frame_index = nextFrameIndex;

		HANDLE waitableObjects[] = { d3d12::hSwapChainWaitableObject, NULL };
		constexpr DWORD numWaitableObjects = 1;

		FrameContext* frameCtxt = &d3d12::FrameContextArray[nextFrameIndex % d3d12::NUM_FRAMES_IN_FLIGHT];

		WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

		return frameCtxt;
	}

	void reinit(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue)
	{
		init_d3d12(pSwapChain, pCommandQueue);
		ImGui_ImplDX12_CreateDeviceObjects();
	}

	ImFont* start(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue, type::tImguiStyle SetStyleFunction)
	{
		static ImFont* s_main_font;

		if (is_ready && get_is_need_reset_imgui())
		{

			reinit(pSwapChain, pCommandQueue);


			is_need_reset_imgui = false;
		}

		if (is_ready)
			return s_main_font;

		init_d3d12(pSwapChain, pCommandQueue);

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		if (SetStyleFunction == nullptr)
			ImGui::StyleColorsDark();
		else
			SetStyleFunction();


		ImGui_ImplWin32_Init(g_data::hWind);
		ImGui_ImplDX12_Init(
			d3d12::pDevice,
			d3d12::NUM_FRAMES_IN_FLIGHT,
			DXGI_FORMAT_R8G8B8A8_UNORM, d3d12::pSrvDescHeap,
			d3d12::pSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			d3d12::pSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

		ImFont* main_font = io.Fonts->AddFontFromMemoryTTF(tahoma_ttf, sizeof(tahoma_ttf), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

		if (main_font == nullptr)
			Exit();

		s_main_font = main_font;

		WndProc_hk();
		
		is_ready = true;

		return s_main_font;
	}

	ImFont* add_font(const char* font_path, float font_size)
	{
		if (!is_ready)
			return nullptr;

		ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(tahoma_ttf, sizeof(tahoma_ttf), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

		if (font == nullptr)
			return 0;

		return font;
	}

	void imgui_frame_header()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void imgui_no_border(type::tESP esp_function, ImFont* font)
	{
		normal(esp_function, font); // standard

		/*switch (globals::theme)
		{
		case 0: DarkMode(esp_function, font); break;
		case 1: White(esp_function, font); break;
		default:
			break;
		}*/

		///////////////////////////////////////////////////////////////////////////////////////////

	
		
		
		
	}

	void imgui_frame_end()
	{
		FrameContext* frameCtxt = WaitForNextFrameResources();
		UINT backBufferIdx = d3d12::pSwapChain->GetCurrentBackBufferIndex();

		{
			frameCtxt->CommandAllocator->Reset();
			static D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.pResource = d3d12::pID3D12ResourceArray[backBufferIdx];
			d3d12::pCommandList->Reset(frameCtxt->CommandAllocator, NULL);
			d3d12::pCommandList->ResourceBarrier(1, &barrier);
			d3d12::pCommandList->OMSetRenderTargets(1, &d3d12::RenderTargetDescriptorArray[backBufferIdx], FALSE, NULL);
			d3d12::pCommandList->SetDescriptorHeaps(1, &d3d12::pSrvDescHeap);
		}

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12::pCommandList);

		static D3D12_RESOURCE_BARRIER barrier = { };
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.pResource = d3d12::pID3D12ResourceArray[backBufferIdx];

		d3d12::pCommandList->ResourceBarrier(1, &barrier);
		d3d12::pCommandList->Close();

		d3d12::pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&d3d12::pCommandList);

		//HRESULT results = ori_present(pSwapChain, SyncInterval, Flags);

		UINT64 fenceValue = d3d12::fenceLastSignaledValue + 1;
		d3d12::pCommandQueue->Signal(d3d12::pFence, fenceValue);
		d3d12::fenceLastSignaledValue = fenceValue;
		frameCtxt->FenceValue = fenceValue;

	}
}

void(*oExecuteCommandListsD3D12)(ID3D12CommandQueue*, UINT, ID3D12CommandList*);

void hookExecuteCommandListsD3D12(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists)
{
	if (!d3d12test::d3d12CommandQueue2)
		d3d12test::d3d12CommandQueue2 = queue;

	oExecuteCommandListsD3D12(queue, NumCommandLists, ppCommandLists);
}
__declspec(dllexport)HRESULT present_hk(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!pSwapChain)
		return ori_present(pSwapChain, SyncInterval, Flags);
	

	d3d12test::d3d12CommandQueue2 = *reinterpret_cast<decltype(d3d12test::d3d12CommandQueue2)*>((std::uintptr_t)pSwapChain + 0x118);
	
	ImFont* main_font = imgui::start(static_cast<IDXGISwapChain3*>(pSwapChain), d3d12test::d3d12CommandQueue2, nullptr);

	if(!screenshot::visuals)
		return ori_present(pSwapChain, SyncInterval, Flags);

	imgui::imgui_frame_header();
	
	g_menu::menu();

	imgui::imgui_no_border(g_game::init, main_font);

	imgui::imgui_frame_end();

	return  ori_present(pSwapChain, SyncInterval, Flags);
	
}







using tbitblt = bool(WINAPI*)(HDC hdcdst, int x, int y, int cx, int cy, HDC hdcsrc, int x1, int y1, DWORD rop);
tbitblt obitblt = nullptr;
tbitblt bitblttramp = nullptr;
bool WINAPI hkbitblt1(HDC hdcdst, int x, int y, int cx, int cy, HDC hdcsrc, int x1, int y1, DWORD rop)
{
	
		
		// let the game take a screenshot
	screenshot::visuals = false;
	Sleep(500);
	auto bbitbltresult = bitblttramp(hdcdst, x, y, cx, cy, hdcsrc, x1, y1, rop);
	// re-enable  drawing
	screenshot::visuals = true;
	screenshot::screenshot_counter++;
	return bbitbltresult;
}
 





typedef bool(APIENTRY* NtGdiStretchBltHook_t)(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop, DWORD dwBackColor);

NtGdiStretchBltHook_t NtGdiStretchBlt_original;

 bool  APIENTRY  NtGdiStretchBltHook1(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop, DWORD dwBackColor) {

	 screenshot::screenshot_counter++;
	/* *screenshot::visuals = false;*/
	 screenshot::visuals = false;
	 Sleep(500);
	 bool ok = NtGdiStretchBlt_original(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop, dwBackColor);
	/* *screenshot::visuals = true;*/
	 screenshot::visuals = true;
	return ok;
}



 uintptr_t find_pattern(const char* module_name, const char* pattern) {
	 const auto get_module_size = [=](uintptr_t module_base)
	 {
		 return reinterpret_cast<PIMAGE_NT_HEADERS>(module_base + reinterpret_cast<PIMAGE_DOS_HEADER>(module_base)->e_lfanew)->OptionalHeader.SizeOfImage;
	 };
	 const auto module_start = (uintptr_t)GetModuleHandle(module_name);
	 if (module_start != 0ULL)
	 {
		 const auto module_end = module_start + get_module_size(module_start);

		 const char* pattern_current = pattern;
		 uintptr_t current_match = NULL;

		 MEMORY_BASIC_INFORMATION64 page_information = {};
		 for (auto current_page = reinterpret_cast<unsigned char*>(module_start); current_page < reinterpret_cast<unsigned char*>(module_end); current_page = reinterpret_cast<unsigned char*>(page_information.BaseAddress + page_information.RegionSize))
		 {
			 VirtualQuery(reinterpret_cast<LPCVOID>(current_page), reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&page_information), sizeof(MEMORY_BASIC_INFORMATION));
			 if (page_information.Protect == PAGE_NOACCESS)
				 continue;

			 if (page_information.State != MEM_COMMIT)
				 continue;

			 if (page_information.Protect & PAGE_GUARD)
				 continue;

			 for (auto current_address = reinterpret_cast<unsigned char*>(page_information.BaseAddress); current_address < reinterpret_cast<unsigned char*>(page_information.BaseAddress + page_information.RegionSize - 0x8); current_address++)
			 {
				 if (*current_address != GET_BYTE(pattern_current) && *pattern_current != '\?') {
					 current_match = 0ULL;
					 pattern_current = pattern;
					 continue;
				 }

				 if (!current_match)
					 current_match = reinterpret_cast<uintptr_t>(current_address);

				 pattern_current += 3;
				 if (pattern_current[-1] == NULL)
					 return current_match;
			 }
		 }
	 }

	 return 0ULL;
 }

 
VOID initialize()
{
	g_data::init();
	//WriteLogFile("Init");
	
	// *g_Addrs.navigateblueprintddl = find_pattern(BaseModule, xorstr_("E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? 48 8D 4C 24 ?? E8 ?? ?? ?? ?? 83 F8 04 74 76"));
	//g_Addrs.jmp_rbx = find_pattern(BaseModule, xorstr_("FF 23"));

	/*auto Targetbitblt = GetProcAddress(GetModuleHandleA("Gdi32.dll"), "BitBlt");
	auto TargetStretchbitblt = GetProcAddress(GetModuleHandleA("win32u.dll"), "NtGdiStretchBlt");*/
	if (kiero::init(kiero::RenderType::D3D12) == kiero::Status::Success)
	{
		
	
		
	
			



		
	
		kiero::bind(140, (void**)&ori_present, present_hk);
		
		

		


		
	}


	

	


	

}

void hookpresent()
{

	kiero::bind(140, (void**)&ori_present, present_hk);
}
void unhookPresent()
{
	kiero::unbind(140);
}
//#define DEBASE(a) ((size_t)a - (size_t)(unsigned long long)GetModuleHandleA(NULL))
//
//uintptr_t dwProcessBase;
//uint64_t backup = 0, Online_Loot__GetItemQuantity = 0, stackFix = 0;
//NTSTATUS(*NtContinue)(PCONTEXT threadContext, BOOLEAN raiseAlert) = nullptr;
//
//DWORD64 resolveRelativeAddress(DWORD64 instr, DWORD offset, DWORD instrSize) {
//	return instr == 0ui64 ? 0ui64 : (instr + instrSize + *(int*)(instr + offset));
//}
//
//bool compareByte(const char* pData, const char* bMask, const char* szMask) {
//	for (; *szMask; ++szMask, ++pData, ++bMask)
//		if (*szMask == 'x' && *pData != *bMask)
//			return false;
//	return (*szMask) == NULL;
//}
//
//DWORD64 findPattern(DWORD64 dwAddress, DWORD64 dwLen, const char* bMask, const char* szMask) {
//	DWORD length = (DWORD)strlen(szMask);
//	for (DWORD i = 0; i < dwLen - length; i++)
//		if (compareByte((const char*)(dwAddress + i), bMask, szMask))
//			return (DWORD64)(dwAddress + i);
//	return 0ui64;
//}
//
//LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
//{
//	if (pExceptionInfo && pExceptionInfo->ExceptionRecord->ExceptionCode == STATUS_ACCESS_VIOLATION)
//	{
//		if (pExceptionInfo->ContextRecord->R11 == 0xDEEDBEEF89898989)
//		{
//			pExceptionInfo->ContextRecord->R11 = backup;
//
//			if (pExceptionInfo->ContextRecord->Rip > Online_Loot__GetItemQuantity && pExceptionInfo->ContextRecord->Rip < (Online_Loot__GetItemQuantity + 0x1000))
//			{
//				pExceptionInfo->ContextRecord->Rip = stackFix;
//				pExceptionInfo->ContextRecord->Rax = 1;
//			}
//			NtContinue(pExceptionInfo->ContextRecord, 0);
//		}
//	}
//
//	return EXCEPTION_CONTINUE_SEARCH;
//}
//
//void SetupExceptionHook()
//{
//	HMODULE ntdll = GetModuleHandleA(xorstr_("ntdll"));
//	NtContinue = (decltype(NtContinue))GetProcAddress(ntdll, xorstr_("NtContinue"));
//
//	void(*RtlAddVectoredExceptionHandler)(LONG First, PVECTORED_EXCEPTION_HANDLER Handler) = (decltype(RtlAddVectoredExceptionHandler))GetProcAddress(ntdll, xorstr_("RtlAddVectoredExceptionHandler"));
//	RtlAddVectoredExceptionHandler(0, TopLevelExceptionHandler);
//
//	uint64_t FindOnline_Loot__GetItemQuantity = findPattern(dwProcessBase + 0x1000000, 0xF000000, xorstr_("\x48\x8D\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x8B\xC8\xC5\xF0\x57\xC9\xC4\xE1\xF3\x2A\xC9"), xorstr_("xxx????x????xxxxxxxxxxx"));
//
//	if (FindOnline_Loot__GetItemQuantity)
//	{
//		Online_Loot__GetItemQuantity = resolveRelativeAddress(FindOnline_Loot__GetItemQuantity + 7, 1, 5);
//
//		uint64_t FindDvar = findPattern(Online_Loot__GetItemQuantity, 0x1000, ("\x4C\x8B\x1D"), xorstr_("xxx"));
//		uint64_t FindStackFix = findPattern(Online_Loot__GetItemQuantity, 0x2000, xorstr_("\xE8\x00\x00\x00\x00\x00\x8B\x00\x00\x00\x00\x8B"), xorstr_("x?????x????x"));
//
//		if (FindStackFix)
//		{
//			stackFix = (FindStackFix + 5);
//
//			backup = *(uint64_t*)resolveRelativeAddress(FindDvar, 3, 7);
//			*(uint64_t*)resolveRelativeAddress(FindDvar, 3, 7) = 0xDEEDBEEF89898989;
//		}
//	}
//}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) 
{
	if (reason == DLL_PROCESS_ATTACH) 
	{
		
		
		
		Beep(500, 500); // Cheat injected
		I_beginthreadex(0, 0, (_beginthreadex_proc_type)initialize, 0, 0, 0);
		
	}

	return TRUE;
}





namespace ogr_function
{
	tWndProc WndProc;
}

LRESULT hkWndProc(HWND hWnd, UINT Msg, WPARAM wp, LPARAM lp)
{
	switch (Msg)
	{
		case 0x403:
		case WM_SIZE:
		{
			if (Msg == WM_SIZE && wp == SIZE_MINIMIZED)
				break;

			if (imgui::IsReady())
			{

				imgui::clear();

				ImGui_ImplDX12_InvalidateDeviceObjects();


				imgui::reset_imgui_request();
			}
			break;
		}
	};

	ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wp, lp);

	return ogr_function::WndProc(hWnd, Msg, wp, lp);
}

void WndProc_hk()
{
	ogr_function::WndProc = (WNDPROC)SetWindowLongPtrW(g_data::hWind, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
}