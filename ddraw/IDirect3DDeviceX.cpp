/**
* Copyright (C) 2024 Elisha Riedlinger
*
* This software is  provided 'as-is', without any express  or implied  warranty. In no event will the
* authors be held liable for any damages arising from the use of this software.
* Permission  is granted  to anyone  to use  this software  for  any  purpose,  including  commercial
* applications, and to alter it and redistribute it freely, subject to the following restrictions:
*
*   1. The origin of this software must not be misrepresented; you must not claim that you  wrote the
*      original  software. If you use this  software  in a product, an  acknowledgment in the product
*      documentation would be appreciated but is not required.
*   2. Altered source versions must  be plainly  marked as such, and  must not be  misrepresented  as
*      being the original software.
*   3. This notice may not be removed or altered from any source distribution.
*/

#include "ddraw.h"
#include "d3d9\d3d9External.h"

// Cached wrapper interface
namespace {
	m_IDirect3DDevice* WrapperInterfaceBackup = nullptr;
	m_IDirect3DDevice2* WrapperInterfaceBackup2 = nullptr;
	m_IDirect3DDevice3* WrapperInterfaceBackup3 = nullptr;
	m_IDirect3DDevice7* WrapperInterfaceBackup7 = nullptr;
}

HRESULT m_IDirect3DDeviceX::QueryInterface(REFIID riid, LPVOID FAR * ppvObj, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ") " << riid;

	if (!ppvObj)
	{
		return E_POINTER;
	}
	*ppvObj = nullptr;

	if (riid == IID_GetRealInterface)
	{
		*ppvObj = ProxyInterface;
		return D3D_OK;
	}
	if (riid == IID_GetInterfaceX)
	{
		*ppvObj = this;
		return D3D_OK;
	}

	if (DirectXVersion != 1 && DirectXVersion != 2 && DirectXVersion != 3 && DirectXVersion != 7)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: wrapper interface version not found: " << DirectXVersion);
		return E_NOINTERFACE;
	}

	DWORD DxVersion = (CheckWrapperType(riid) && (Config.Dd7to9 || Config.ConvertToDirect3D7)) ? GetGUIDVersion(riid) : DirectXVersion;

	if (riid == GetWrapperType(DxVersion) || riid == IID_IUnknown)
	{
		*ppvObj = GetWrapperInterfaceX(DxVersion);

		AddRef(DxVersion);

		return D3D_OK;
	}

	return ProxyQueryInterface(ProxyInterface, riid, ppvObj, GetWrapperType(DxVersion));
}

void *m_IDirect3DDeviceX::GetWrapperInterfaceX(DWORD DirectXVersion)
{
	switch (DirectXVersion)
	{
	case 0:
		if (WrapperInterface7) return WrapperInterface7;
		if (WrapperInterface3) return WrapperInterface3;
		if (WrapperInterface2) return WrapperInterface2;
		if (WrapperInterface) return WrapperInterface;
		break;
	case 1:
		return GetInterfaceAddress(WrapperInterface, WrapperInterfaceBackup, (LPDIRECT3DDEVICE)ProxyInterface, this);
	case 2:
		return GetInterfaceAddress(WrapperInterface2, WrapperInterfaceBackup2, (LPDIRECT3DDEVICE2)ProxyInterface, this);
	case 3:
		return GetInterfaceAddress(WrapperInterface3, WrapperInterfaceBackup3, (LPDIRECT3DDEVICE3)ProxyInterface, this);
	case 7:
		return GetInterfaceAddress(WrapperInterface7, WrapperInterfaceBackup7, (LPDIRECT3DDEVICE7)ProxyInterface, this);
	}
	LOG_LIMIT(100, __FUNCTION__ << " Error: wrapper interface version not found: " << DirectXVersion);
	return nullptr;
}

ULONG m_IDirect3DDeviceX::AddRef(DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ") v" << DirectXVersion;

	if (Config.Dd7to9)
	{
		switch (DirectXVersion)
		{
		case 1:
			return InterlockedIncrement(&RefCount1);
		case 2:
			return InterlockedIncrement(&RefCount2);
		case 3:
			return InterlockedIncrement(&RefCount3);
		case 7:
			return InterlockedIncrement(&RefCount7);
		default:
			LOG_LIMIT(100, __FUNCTION__ << " Error: wrapper interface version not found: " << DirectXVersion);
			return 0;
		}
	}

	return ProxyInterface->AddRef();
}

ULONG m_IDirect3DDeviceX::Release(DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ") v" << DirectXVersion;

	ULONG ref;

	if (Config.Dd7to9)
	{
		switch (DirectXVersion)
		{
		case 1:
			ref = (InterlockedCompareExchange(&RefCount1, 0, 0)) ? InterlockedDecrement(&RefCount1) : 0;
			break;
		case 2:
			ref = (InterlockedCompareExchange(&RefCount2, 0, 0)) ? InterlockedDecrement(&RefCount2) : 0;
			break;
		case 3:
			ref = (InterlockedCompareExchange(&RefCount3, 0, 0)) ? InterlockedDecrement(&RefCount3) : 0;
			break;
		case 7:
			ref = (InterlockedCompareExchange(&RefCount7, 0, 0)) ? InterlockedDecrement(&RefCount7) : 0;
			break;
		default:
			LOG_LIMIT(100, __FUNCTION__ << " Error: wrapper interface version not found: " << DirectXVersion);
			ref = 0;
		}

		if (InterlockedCompareExchange(&RefCount1, 0, 0) + InterlockedCompareExchange(&RefCount2, 0, 0) +
			InterlockedCompareExchange(&RefCount3, 0, 0) + InterlockedCompareExchange(&RefCount7, 0, 0) == 0)
		{
			delete this;
		}
	}
	else
	{
		ref = ProxyInterface->Release();

		if (ref == 0)
		{
			delete this;
		}
	}

	return ref;
}

HRESULT m_IDirect3DDeviceX::Initialize(LPDIRECT3D lpd3d, LPGUID lpGUID, LPD3DDEVICEDESC lpd3ddvdesc)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		// Returns D3D_OK if successful, otherwise it returns an error.
		return D3D_OK;
	}

	if (lpd3d)
	{
		lpd3d->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpd3d);
	}

	return GetProxyInterfaceV1()->Initialize(lpd3d, lpGUID, lpd3ddvdesc);
}

HRESULT m_IDirect3DDeviceX::CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC lpDesc, LPDIRECT3DEXECUTEBUFFER * lplpDirect3DExecuteBuffer, IUnknown * pUnkOuter)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		if (!lplpDirect3DExecuteBuffer || !lpDesc)
		{
			return DDERR_INVALIDPARAMS;
		}

		if (lpDesc->dwSize != sizeof(D3DEXECUTEBUFFERDESC))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: Incorrect dwSize: " << lpDesc->dwSize);
			return DDERR_INVALIDPARAMS;
		}

		// Validate dwFlags
		if (!(lpDesc->dwFlags & D3DDEB_BUFSIZE))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: D3DDEB_BUFSIZE flag not set.");
			return DDERR_INVALIDPARAMS;
		}

		// Validate dwBufferSize
		if (lpDesc->dwBufferSize == 0 || lpDesc->dwBufferSize > MAX_EXECUTE_BUFFER_SIZE)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: Invalid dwBufferSize: " << lpDesc->dwBufferSize);
			return DDERR_INVALIDPARAMS;
		}

		// Validate dwCaps
		if ((lpDesc->dwFlags & D3DDEB_CAPS) && (lpDesc->dwCaps & D3DDEBCAPS_SYSTEMMEMORY) && (lpDesc->dwCaps & D3DDEBCAPS_VIDEOMEMORY))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: Unsupported dwCaps: " << Logging::hex(lpDesc->dwCaps));
			return DDERR_INVALIDPARAMS;
		}

		// Validate lpData
		if ((lpDesc->dwFlags & D3DDEB_LPDATA) && lpDesc->lpData)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: lpData is non-null, using application data.");
		}

		m_IDirect3DExecuteBuffer* pExecuteBuffer = CreateDirect3DExecuteBuffer(*lplpDirect3DExecuteBuffer, this, lpDesc);

		ExecuteBufferList.push_back(pExecuteBuffer);

		*lplpDirect3DExecuteBuffer = pExecuteBuffer;

		return D3D_OK;
	}

	HRESULT hr = GetProxyInterfaceV1()->CreateExecuteBuffer(lpDesc, lplpDirect3DExecuteBuffer, pUnkOuter);

	if (SUCCEEDED(hr) && lplpDirect3DExecuteBuffer)
	{
		*lplpDirect3DExecuteBuffer = CreateDirect3DExecuteBuffer(*lplpDirect3DExecuteBuffer, nullptr, nullptr);
	}

	return hr;
}

void m_IDirect3DDeviceX::ReleaseExecuteBuffer(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer)
{
	// Check if the input buffer is valid
	if (!lpDirect3DExecuteBuffer)
	{
		return;
	}

	// Find and remove the buffer from the list
	auto it = std::find(ExecuteBufferList.begin(), ExecuteBufferList.end(), lpDirect3DExecuteBuffer);
	if (it != ExecuteBufferList.end())
	{
		// Remove it from the list
		ExecuteBufferList.erase(it);
	}
}

HRESULT m_IDirect3DDeviceX::Execute(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer, LPDIRECT3DVIEWPORT lpDirect3DViewport, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	if (lpDirect3DExecuteBuffer)
	{
		lpDirect3DExecuteBuffer->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DExecuteBuffer);
	}
	if (lpDirect3DViewport)
	{
		lpDirect3DViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DViewport);
	}

	return GetProxyInterfaceV1()->Execute(lpDirect3DExecuteBuffer, lpDirect3DViewport, dwFlags);
}

HRESULT m_IDirect3DDeviceX::Pick(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer, LPDIRECT3DVIEWPORT lpDirect3DViewport, DWORD dwFlags, LPD3DRECT lpRect)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	if (lpDirect3DExecuteBuffer)
	{
		lpDirect3DExecuteBuffer->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DExecuteBuffer);
	}
	if (lpDirect3DViewport)
	{
		lpDirect3DViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DViewport);
	}

	return GetProxyInterfaceV1()->Pick(lpDirect3DExecuteBuffer, lpDirect3DViewport, dwFlags, lpRect);
}

HRESULT m_IDirect3DDeviceX::GetPickRecords(LPDWORD lpCount, LPD3DPICKRECORD lpD3DPickRec)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	return GetProxyInterfaceV1()->GetPickRecords(lpCount, lpD3DPickRec);
}

HRESULT m_IDirect3DDeviceX::CreateMatrix(LPD3DMATRIXHANDLE lpD3DMatHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	return GetProxyInterfaceV1()->CreateMatrix(lpD3DMatHandle);
}

HRESULT m_IDirect3DDeviceX::SetMatrix(D3DMATRIXHANDLE d3dMatHandle, const LPD3DMATRIX lpD3DMatrix)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	return GetProxyInterfaceV1()->SetMatrix(d3dMatHandle, lpD3DMatrix);
}

HRESULT m_IDirect3DDeviceX::GetMatrix(D3DMATRIXHANDLE lpD3DMatHandle, LPD3DMATRIX lpD3DMatrix)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	return GetProxyInterfaceV1()->GetMatrix(lpD3DMatHandle, lpD3DMatrix);
}

HRESULT m_IDirect3DDeviceX::DeleteMatrix(D3DMATRIXHANDLE d3dMatHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion != 1)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	return GetProxyInterfaceV1()->DeleteMatrix(d3dMatHandle);
}

HRESULT m_IDirect3DDeviceX::SetTransform(D3DTRANSFORMSTATETYPE dtstTransformStateType, LPD3DMATRIX lpD3DMatrix)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpD3DMatrix)
		{
			return  DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dtstTransformStateType)
		{
		case D3DTRANSFORMSTATE_WORLD:
			dtstTransformStateType = D3DTS_WORLD;
			break;
		case D3DTRANSFORMSTATE_WORLD1:
			dtstTransformStateType = D3DTS_WORLD1;
			break;
		case D3DTRANSFORMSTATE_WORLD2:
			dtstTransformStateType = D3DTS_WORLD2;
			break;
		case D3DTRANSFORMSTATE_WORLD3:
			dtstTransformStateType = D3DTS_WORLD3;
			break;
		}

		HRESULT hr = (*d3d9Device)->SetTransform(dtstTransformStateType, lpD3DMatrix);

		if (SUCCEEDED(hr))
		{
#ifdef ENABLE_DEBUGOVERLAY
			if (Config.EnableImgui)
			{
				DOverlay.SetTransform(dtstTransformStateType, lpD3DMatrix);
			}
#endif
		}

		return hr;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetTransform(dtstTransformStateType, lpD3DMatrix);
	case 3:
		return GetProxyInterfaceV3()->SetTransform(dtstTransformStateType, lpD3DMatrix);
	case 7:
		return GetProxyInterfaceV7()->SetTransform(dtstTransformStateType, lpD3DMatrix);
	}
}

HRESULT m_IDirect3DDeviceX::GetTransform(D3DTRANSFORMSTATETYPE dtstTransformStateType, LPD3DMATRIX lpD3DMatrix)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpD3DMatrix)
		{
			return  DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dtstTransformStateType)
		{
		case D3DTRANSFORMSTATE_WORLD:
			dtstTransformStateType = D3DTS_WORLD;
			break;
		case D3DTRANSFORMSTATE_WORLD1:
			dtstTransformStateType = D3DTS_WORLD1;
			break;
		case D3DTRANSFORMSTATE_WORLD2:
			dtstTransformStateType = D3DTS_WORLD2;
			break;
		case D3DTRANSFORMSTATE_WORLD3:
			dtstTransformStateType = D3DTS_WORLD3;
			break;
		}

		return (*d3d9Device)->GetTransform(dtstTransformStateType, lpD3DMatrix);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->GetTransform(dtstTransformStateType, lpD3DMatrix);
	case 3:
		return GetProxyInterfaceV3()->GetTransform(dtstTransformStateType, lpD3DMatrix);
	case 7:
		return GetProxyInterfaceV7()->GetTransform(dtstTransformStateType, lpD3DMatrix);
	}
}

HRESULT m_IDirect3DDeviceX::PreLoad(LPDIRECTDRAWSURFACE7 lpddsTexture)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// ToDo: Call PreLoad for the texture.
		// Calling this method indicates that the application will need this managed resource shortly. This method has no effect on nonmanaged resources.
		return D3D_OK;
	}

	if (lpddsTexture)
	{
		lpddsTexture->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpddsTexture);
	}

	return GetProxyInterfaceV7()->PreLoad(lpddsTexture);
}

HRESULT m_IDirect3DDeviceX::Load(LPDIRECTDRAWSURFACE7 lpDestTex, LPPOINT lpDestPoint, LPDIRECTDRAWSURFACE7 lpSrcTex, LPRECT lprcSrcRect, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpDestTex || !lpSrcTex)
		{
			return  DDERR_INVALIDPARAMS;
		}

		// ToDo: support the following dwFlags: 
		// DDSCAPS2_CUBEMAP_ALLFACES - All faces should be loaded with the image data within the source texture.
		// DDSCAPS2_CUBEMAP_NEGATIVEX, DDSCAPS2_CUBEMAP_NEGATIVEY, or DDSCAPS2_CUBEMAP_NEGATIVEZ
		//     The negative x, y, or z faces should receive the image data.
		// DDSCAPS2_CUBEMAP_POSITIVEX, DDSCAPS2_CUBEMAP_POSITIVEY, or DDSCAPS2_CUBEMAP_POSITIVEZ
		//     The positive x, y, or z faces should receive the image data.

		if (dwFlags)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: flags not supported. dwFlags: " << Logging::hex(dwFlags));
		}

		// ToDo: check if source and destination surfaces are valid

		if (!lprcSrcRect && (!lpDestPoint || (lpDestPoint && lpDestPoint->x == 0 && lpDestPoint->y == 0)))
		{
			return lpDestTex->Blt(nullptr, lpSrcTex, nullptr, 0, nullptr);
		}
		else
		{
			// Get source rect
			RECT SrcRect = {};
			if (lprcSrcRect)
			{
				SrcRect = *lprcSrcRect;
			}
			else
			{
				DDSURFACEDESC2 Desc2 = {};
				Desc2.dwSize = sizeof(DDSURFACEDESC2);
				lpSrcTex->GetSurfaceDesc(&Desc2);

				if ((Desc2.dwFlags & (DDSD_WIDTH | DDSD_HEIGHT)) != (DDSD_WIDTH | DDSD_HEIGHT))
				{
					LOG_LIMIT(100, __FUNCTION__ << " Error: rect size doesn't match!");
					return DDERR_GENERIC;
				}

				SrcRect = { 0, 0, (LONG)Desc2.dwWidth, (LONG)Desc2.dwHeight };
			}

			// Get destination point
			POINT DestPoint = {};
			if (lpDestPoint)
			{
				DestPoint = *lpDestPoint;
			}

			// Get destination rect
			RECT DestRect = {
				DestPoint.x,									// left
				DestPoint.y,									// top
				DestPoint.x + (SrcRect.right - SrcRect.left),	// right
				DestPoint.y + (SrcRect.bottom - SrcRect.top),	// bottom
			};

			return lpDestTex->Blt(&DestRect, lpSrcTex, &SrcRect, 0, nullptr);
		}
	}

	if (lpDestTex)
	{
		lpDestTex->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDestTex);
	}
	if (lpSrcTex)
	{
		lpSrcTex->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpSrcTex);
	}

	return GetProxyInterfaceV7()->Load(lpDestTex, lpDestPoint, lpSrcTex, lprcSrcRect, dwFlags);
}

HRESULT m_IDirect3DDeviceX::SwapTextureHandles(LPDIRECT3DTEXTURE2 lpD3DTex1, LPDIRECT3DTEXTURE2 lpD3DTex2)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 2)
	{

		if (!lpD3DTex1 || !lpD3DTex2)
		{
			return DDERR_INVALIDPARAMS;
		}

		m_IDirect3DTextureX* pTextureX1 = nullptr;
		lpD3DTex1->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pTextureX1);

		m_IDirect3DTextureX* pTextureX2 = nullptr;
		lpD3DTex2->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pTextureX2);

		if (!pTextureX1 || !pTextureX2)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture wrapper!");
			return DDERR_INVALIDPARAMS;
		}

		// Find handle associated with texture1
		auto it1 = std::find_if(TextureHandleMap.begin(), TextureHandleMap.end(), [&](const auto& pair) {
			return pair.second == pTextureX1;
			});

		// Find handle associated with texture2
		auto it2 = std::find_if(TextureHandleMap.begin(), TextureHandleMap.end(), [&](const auto& pair) {
			return pair.second == pTextureX2;
			});

		// Check if handles are found
		if (it1 == TextureHandleMap.end() || it2 == TextureHandleMap.end())
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not find texture handles!");
			return DDERR_INVALIDPARAMS;
		}

		// If handles are found, swap them
		DWORD Handle1 = it1->first, Handle2 = it2->first;
		SetTextureHandle(Handle1, pTextureX2);
		SetTextureHandle(Handle2, pTextureX1);

		// Update handles associated with textures
		pTextureX1->SetHandle(Handle2);
		pTextureX2->SetHandle(Handle1);

		// If texture handle is set then use new texture
		if (rsTextureHandle == Handle1 || rsTextureHandle == Handle2)
		{
			SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE, rsTextureHandle);
		}

		return D3D_OK;
	}

	if (lpD3DTex1)
	{
		lpD3DTex1->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpD3DTex1);
	}
	if (lpD3DTex2)
	{
		lpD3DTex2->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpD3DTex2);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->SwapTextureHandles((LPDIRECT3DTEXTURE)lpD3DTex1, (LPDIRECT3DTEXTURE)lpD3DTex2);
	case 2:
		return GetProxyInterfaceV2()->SwapTextureHandles(lpD3DTex1, lpD3DTex2);
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK lpd3dEnumTextureProc, LPVOID lpArg)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->EnumTextureFormats(lpd3dEnumTextureProc, lpArg);
	case 2:
		return GetProxyInterfaceV2()->EnumTextureFormats(lpd3dEnumTextureProc, lpArg);
	case 3:
	case 7:
	case 9:
	{
		if (!lpd3dEnumTextureProc)
		{
			return DDERR_INVALIDPARAMS;
		}

		struct EnumPixelFormat
		{
			LPVOID lpContext;
			LPD3DENUMTEXTUREFORMATSCALLBACK lpCallback;

			static HRESULT CALLBACK ConvertCallback(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext)
			{
				EnumPixelFormat* self = (EnumPixelFormat*)lpContext;

				// Only RGB formats are supported
				if ((lpDDPixFmt->dwFlags & DDPF_RGB) == NULL)
				{
					return DDENUMRET_OK;
				}

				DDSURFACEDESC Desc = {};
				Desc.dwSize = sizeof(DDSURFACEDESC);
				Desc.dwFlags = DDSD_CAPS | DDSD_PIXELFORMAT;
				Desc.ddpfPixelFormat = *lpDDPixFmt;
				Desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;

				return self->lpCallback(&Desc, self->lpContext);
			}
		} CallbackContext = {};
		CallbackContext.lpContext = lpArg;
		CallbackContext.lpCallback = lpd3dEnumTextureProc;

		return EnumTextureFormats(EnumPixelFormat::ConvertCallback, &CallbackContext);
	}
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK lpd3dEnumPixelProc, LPVOID lpArg)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpd3dEnumPixelProc)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, false)))
		{
			return DDERR_INVALIDOBJECT;
		}

		LPDIRECT3D9 d3d9Object = ddrawParent->GetDirectD9Object();

		if (!d3d9Object)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: failed to get d3d9 object!");
			return DDERR_GENERIC;
		}

		// Get texture list
		std::vector<D3DFORMAT> TextureList = {
			D3DFMT_R5G6B5,
			D3DFMT_X1R5G5B5,
			D3DFMT_A1R5G5B5,
			D3DFMT_A4R4G4B4,
			//D3DFMT_R8G8B8,	// Requires emulation
			D3DFMT_X8R8G8B8,
			D3DFMT_A8R8G8B8,
			D3DFMT_V8U8,
			D3DFMT_X8L8V8U8,
			D3DFMT_L6V5U5,
			D3DFMT_YUY2,
			D3DFMT_UYVY,
			D3DFMT_AYUV,
			D3DFMT_DXT1,
			D3DFMT_DXT2,
			D3DFMT_DXT3,
			D3DFMT_DXT4,
			D3DFMT_DXT5,
			D3DFMT_P8,
			D3DFMT_L8,
			D3DFMT_A8,
			D3DFMT_A4L4,
			D3DFMT_A8L8 };

		// Add FourCCs to texture list
		for (D3DFORMAT format : FourCCTypes)
		{
			TextureList.push_back(format);
		}

		// Check for supported textures
		DDPIXELFORMAT ddpfPixelFormat = {};
		ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);

		bool IsDirectDraw8bit = (ddrawParent->GetDisplayBPP(nullptr) == 8);

		for (D3DFORMAT format : TextureList)
		{
			if (!IsUnsupportedFormat(format) && ((format == D3DFMT_P8 && IsDirectDraw8bit) ||
				SUCCEEDED(d3d9Object->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, format))))
			{
				SetPixelDisplayFormat(format, ddpfPixelFormat);
				if (lpd3dEnumPixelProc(&ddpfPixelFormat, lpArg) == DDENUMRET_CANCEL)
				{
					return D3D_OK;
				}
			}
		}

		return D3D_OK;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->EnumTextureFormats(lpd3dEnumPixelProc, lpArg);
	case 7:
		return GetProxyInterfaceV7()->EnumTextureFormats(lpd3dEnumPixelProc, lpArg);
	}
}

HRESULT m_IDirect3DDeviceX::GetTexture(DWORD dwStage, LPDIRECT3DTEXTURE2* lplpTexture)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		if (!lplpTexture || dwStage >= MaxTextureStages)
		{
			return DDERR_INVALIDPARAMS;
		}
		*lplpTexture = nullptr;

		// Get surface stage
		LPDIRECTDRAWSURFACE7 pSurface = nullptr;
		HRESULT hr = GetTexture(dwStage, &pSurface);

		if (FAILED(hr))
		{
			return hr;
		}

		// First relese ref for surface
		pSurface->Release();

		// Get surface wrapper
		m_IDirectDrawSurfaceX* pSurfaceX = nullptr;
		pSurface->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pSurfaceX);
		if (!pSurfaceX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get surface wrapper!");
			return DDERR_INVALIDPARAMS;
		}

		// Get attached texture from surface
		m_IDirect3DTextureX* pTextureX = pSurfaceX->GetAttachedTexture();
		if (!pTextureX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture!");
			return DDERR_INVALIDPARAMS;
		}

		// Add ref to texture
		pTextureX->AddRef();

		*lplpTexture = (LPDIRECT3DTEXTURE2)pTextureX->GetWrapperInterfaceX(0);

		return D3D_OK;
	}

	HRESULT hr = GetProxyInterfaceV3()->GetTexture(dwStage, lplpTexture);

	if (SUCCEEDED(hr) && lplpTexture)
	{
		*lplpTexture = ProxyAddressLookupTable.FindAddress<m_IDirect3DTexture2>(*lplpTexture, 2);
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::GetTexture(DWORD dwStage, LPDIRECTDRAWSURFACE7* lplpTexture)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lplpTexture || dwStage >= MaxTextureStages)
		{
			return DDERR_INVALIDPARAMS;
		}
		*lplpTexture = nullptr;

		HRESULT hr = DDERR_GENERIC;

		if (AttachedTexture[dwStage])
		{
			AttachedTexture[dwStage]->AddRef();

			*lplpTexture = AttachedTexture[dwStage];

			hr = D3D_OK;
		}

		return hr;
	}

	HRESULT hr = GetProxyInterfaceV7()->GetTexture(dwStage, lplpTexture);

	if (SUCCEEDED(hr) && lplpTexture)
	{
		*lplpTexture = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpTexture, 7);
	}

	return hr;
}

void m_IDirect3DDeviceX::ReleaseTextureHandle(m_IDirect3DTextureX* lpTexture)
{
	// Find handle associated with texture
	auto it = TextureHandleMap.begin();
	while (it != TextureHandleMap.end())
	{
		if (it->second == lpTexture)
		{
			// Remove entry from map
			it = TextureHandleMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

HRESULT m_IDirect3DDeviceX::SetTextureHandle(DWORD tHandle, m_IDirect3DTextureX* lpTexture)
{
	if (!tHandle || !lpTexture)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: NULL pointer found! " << lpTexture << " -> " << tHandle);
		return DDERR_INVALIDPARAMS;
	}

	TextureHandleMap[tHandle] = lpTexture;

	return D3D_OK;
}

HRESULT m_IDirect3DDeviceX::SetTexture(DWORD dwStage, LPDIRECT3DTEXTURE2 lpTexture)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		if (dwStage >= MaxTextureStages)
		{
			return DDERR_INVALIDPARAMS;
		}

		if (!lpTexture)
		{
			return SetTexture(dwStage, (LPDIRECTDRAWSURFACE7)nullptr);
		}

		m_IDirect3DTextureX *pTextureX = nullptr;
		lpTexture->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pTextureX);
		if (!pTextureX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture wrapper!");
			return DDERR_INVALIDPARAMS;
		}

		m_IDirectDrawSurfaceX *pSurfaceX = pTextureX->GetSurface();
		if (!pSurfaceX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get surface!");
			return DDERR_INVALIDPARAMS;
		}

		return SetTexture(dwStage, (LPDIRECTDRAWSURFACE7)pSurfaceX->GetWrapperInterfaceX(0));
	}

	if (lpTexture)
	{
		lpTexture->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpTexture);
	}

	return GetProxyInterfaceV3()->SetTexture(dwStage, lpTexture);
}

HRESULT m_IDirect3DDeviceX::SetTexture(DWORD dwStage, LPDIRECTDRAWSURFACE7 lpSurface)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (dwStage >= MaxTextureStages)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		m_IDirectDrawSurfaceX* lpDDSrcSurfaceX = nullptr;

		HRESULT hr;

		if (!lpSurface)
		{
			hr = (*d3d9Device)->SetTexture(dwStage, nullptr);
		}
		else
		{
			lpSurface->QueryInterface(IID_GetInterfaceX, (LPVOID*)&lpDDSrcSurfaceX);
			if (!lpDDSrcSurfaceX)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Error: could not get surface wrapper!");
				return DDERR_INVALIDPARAMS;
			}

			IDirect3DTexture9* pTexture9 = lpDDSrcSurfaceX->GetD3d9Texture();
			if (!pTexture9)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture!");
				return DDERR_INVALIDPARAMS;
			}

			if (lpCurrentRenderTargetX && lpCurrentRenderTargetX->IsPalette() && !lpDDSrcSurfaceX->IsPalette())
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: setting non-palette texture on a paletted render target!");
			}

			hr = (*d3d9Device)->SetTexture(dwStage, pTexture9);
		}

		if (SUCCEEDED(hr))
		{
			AttachedTexture[dwStage] = lpSurface;
			CurrentTextureSurfaceX[dwStage] = lpDDSrcSurfaceX;
		}

		return hr;
	}

	if (lpSurface)
	{
		lpSurface->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpSurface);
	}

	return GetProxyInterfaceV7()->SetTexture(dwStage, lpSurface);
}

HRESULT m_IDirect3DDeviceX::SetRenderTarget(LPDIRECTDRAWSURFACE7 lpNewRenderTarget, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpNewRenderTarget)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Don't reset existing render target
		if (CurrentRenderTarget == lpNewRenderTarget)
		{
			return D3D_OK;
		}

		// dwFlags: Not currently used; set to 0.

		// ToDo: if DirectXVersion < 7 then invalidate the current material and viewport:
		// Unlike this method's implementation in previous interfaces, IDirect3DDevice7::SetRenderTarget does not invalidate the current material or viewport for the device.

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		m_IDirectDrawSurfaceX* lpDDSrcSurfaceX = nullptr;
		lpNewRenderTarget->QueryInterface(IID_GetInterfaceX, (LPVOID*)&lpDDSrcSurfaceX);

		if (!lpDDSrcSurfaceX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get surface wrapper!");
			return DDERR_INVALIDPARAMS;
		}

		HRESULT hr = ddrawParent->SetRenderTargetSurface(lpDDSrcSurfaceX);

		if (SUCCEEDED(hr))
		{
			lpNewRenderTarget = CurrentRenderTarget;

			lpCurrentRenderTargetX = lpDDSrcSurfaceX;
		}

		return D3D_OK;
	}

	if (lpNewRenderTarget)
	{
		lpNewRenderTarget->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpNewRenderTarget);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetRenderTarget((LPDIRECTDRAWSURFACE)lpNewRenderTarget, dwFlags);
	case 3:
		return GetProxyInterfaceV3()->SetRenderTarget((LPDIRECTDRAWSURFACE4)lpNewRenderTarget, dwFlags);
	case 7:
		return GetProxyInterfaceV7()->SetRenderTarget(lpNewRenderTarget, dwFlags);
	}
}

HRESULT m_IDirect3DDeviceX::GetRenderTarget(LPDIRECTDRAWSURFACE7* lplpRenderTarget, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lplpRenderTarget)
		{
			return DDERR_INVALIDPARAMS;
		}

		if (!CurrentRenderTarget)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: render target not set.");
			return DDERR_GENERIC;
		}

		// ToDo: Validate RenderTarget address
		*lplpRenderTarget = CurrentRenderTarget;

		CurrentRenderTarget->AddRef();

		return D3D_OK;
	}

	HRESULT hr = DDERR_GENERIC;

	switch (ProxyDirectXVersion)
	{
	case 2:
		hr = GetProxyInterfaceV2()->GetRenderTarget((LPDIRECTDRAWSURFACE*)lplpRenderTarget);
		break;
	case 3:
		hr = GetProxyInterfaceV3()->GetRenderTarget((LPDIRECTDRAWSURFACE4*)lplpRenderTarget);
		break;
	case 7:
		hr = GetProxyInterfaceV7()->GetRenderTarget(lplpRenderTarget);
		break;
	}

	if (SUCCEEDED(hr) && lplpRenderTarget)
	{
		*lplpRenderTarget = ProxyAddressLookupTable.FindAddress<m_IDirectDrawSurface7>(*lplpRenderTarget, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE dwState, LPDWORD lpdwValue)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpdwValue)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dwState)
		{
		case D3DTSS_ADDRESS:
		{
			DWORD ValueU = 0, ValueV = 0;
			(*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_ADDRESSU, &ValueU);
			(*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_ADDRESSV, &ValueV);
			if (ValueU == ValueV)
			{
				*lpdwValue = ValueU;
				return D3D_OK;
			}
			else
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: AddressU and AddressV don't match");
				*lpdwValue = 0;
				return D3D_OK;
			}
		}
		case D3DTSS_ADDRESSU:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_ADDRESSU, lpdwValue);
		case D3DTSS_ADDRESSV:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_ADDRESSV, lpdwValue);
		case D3DTSS_ADDRESSW:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_ADDRESSW, lpdwValue);
		case D3DTSS_BORDERCOLOR:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_BORDERCOLOR, lpdwValue);
		case D3DTSS_MAGFILTER:
		{
			HRESULT hr = (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MAGFILTER, lpdwValue);
			if (SUCCEEDED(hr) && *lpdwValue == D3DTEXF_ANISOTROPIC)
			{
				*lpdwValue = D3DTFG_ANISOTROPIC;
			}
			return hr;
		}
		case D3DTSS_MINFILTER:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MINFILTER, lpdwValue);
		case D3DTSS_MIPFILTER:
		{
			HRESULT hr = (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MIPFILTER, lpdwValue);
			if (SUCCEEDED(hr))
			{
				switch (*lpdwValue)
				{
				default:
				case D3DTEXF_NONE:
					*lpdwValue = D3DTFP_NONE;
					break;
				case D3DTEXF_POINT:
					*lpdwValue = D3DTFP_POINT;
					break;
				case D3DTEXF_LINEAR:
					*lpdwValue = D3DTFP_LINEAR;
					break;
				}
			}
			return hr;
		}
		case D3DTSS_MIPMAPLODBIAS:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MIPMAPLODBIAS, lpdwValue);
		case D3DTSS_MAXMIPLEVEL:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MAXMIPLEVEL, lpdwValue);
		case D3DTSS_MAXANISOTROPY:
			return (*d3d9Device)->GetSamplerState(dwStage, D3DSAMP_MAXANISOTROPY, lpdwValue);
		}

		if (!CheckTextureStageStateType(dwState))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Texture Stage state type not implemented: " << dwState);
		}

		return (*d3d9Device)->GetTextureStageState(dwStage, dwState, lpdwValue);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->GetTextureStageState(dwStage, dwState, lpdwValue);
	case 7:
		return GetProxyInterfaceV7()->GetTextureStageState(dwStage, dwState, lpdwValue);
	}
}

HRESULT m_IDirect3DDeviceX::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE dwState, DWORD dwValue)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (dwStage >= MaxTextureStages)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dwState)
		{
		case D3DTSS_ADDRESS:
			(*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_ADDRESSU, dwValue);
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_ADDRESSV, dwValue);
		case D3DTSS_ADDRESSU:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_ADDRESSU, dwValue);
		case D3DTSS_ADDRESSV:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_ADDRESSV, dwValue);
		case D3DTSS_ADDRESSW:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_ADDRESSW, dwValue);
		case D3DTSS_BORDERCOLOR:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_BORDERCOLOR, dwValue);
		case D3DTSS_MAGFILTER:
			if (dwValue == D3DTFG_ANISOTROPIC)
			{
				dwValue = D3DTEXF_ANISOTROPIC;
			}
			else if (dwValue == D3DTFG_FLATCUBIC || dwValue == D3DTFG_GAUSSIANCUBIC)
			{
				dwValue = D3DTEXF_LINEAR;
			}
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MAGFILTER, dwValue);
		case D3DTSS_MINFILTER:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MINFILTER, dwValue);
		case D3DTSS_MIPFILTER:
			switch (dwValue)
			{
			default:
			case D3DTFP_NONE:
				dwValue = D3DTEXF_NONE;
				break;
			case D3DTFP_POINT:
				dwValue = D3DTEXF_POINT;
				break;
			case D3DTFP_LINEAR:
				dwValue = D3DTEXF_LINEAR;
				break;
			}
			ssMipFilter[dwStage] = dwValue;
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MIPFILTER, dwValue);
		case D3DTSS_MIPMAPLODBIAS:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MIPMAPLODBIAS, dwValue);
		case D3DTSS_MAXMIPLEVEL:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MAXMIPLEVEL, dwValue);
		case D3DTSS_MAXANISOTROPY:
			return (*d3d9Device)->SetSamplerState(dwStage, D3DSAMP_MAXANISOTROPY, dwValue);
		}

		if (!CheckTextureStageStateType(dwState))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Texture Stage state type not implemented: " << dwState);
			return D3D_OK;	// Just return OK for now!
		}

		return (*d3d9Device)->SetTextureStageState(dwStage, dwState, dwValue);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->SetTextureStageState(dwStage, dwState, dwValue);
	case 7:
		return GetProxyInterfaceV7()->SetTextureStageState(dwStage, dwState, dwValue);
	}
}

HRESULT m_IDirect3DDeviceX::GetCaps(LPD3DDEVICEDESC lpD3DHWDevDesc, LPD3DDEVICEDESC lpD3DHELDevDesc)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->GetCaps(lpD3DHWDevDesc, lpD3DHELDevDesc);
	case 2:
		return GetProxyInterfaceV2()->GetCaps(lpD3DHWDevDesc, lpD3DHELDevDesc);
	case 3:
		return GetProxyInterfaceV3()->GetCaps(lpD3DHWDevDesc, lpD3DHELDevDesc);
	case 7:
	case 9:
	{
		if ((!lpD3DHWDevDesc && !lpD3DHELDevDesc) ||
			(lpD3DHWDevDesc && lpD3DHWDevDesc->dwSize != D3DDEVICEDESC1_SIZE && lpD3DHWDevDesc->dwSize != D3DDEVICEDESC5_SIZE && lpD3DHWDevDesc->dwSize != D3DDEVICEDESC6_SIZE) ||
			(lpD3DHELDevDesc && lpD3DHELDevDesc->dwSize != D3DDEVICEDESC1_SIZE && lpD3DHELDevDesc->dwSize != D3DDEVICEDESC5_SIZE && lpD3DHELDevDesc->dwSize != D3DDEVICEDESC6_SIZE))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: Incorrect dwSize: " << ((lpD3DHWDevDesc) ? lpD3DHWDevDesc->dwSize : -1) << " " << ((lpD3DHELDevDesc) ? lpD3DHELDevDesc->dwSize : -1));
			return DDERR_INVALIDPARAMS;
		}

		D3DDEVICEDESC7 D3DDevDesc;
		HRESULT hr = GetCaps(&D3DDevDesc);

		if (SUCCEEDED(hr))
		{
			if (lpD3DHWDevDesc)
			{
				ConvertDeviceDesc(*lpD3DHWDevDesc, D3DDevDesc);
			}

			if (lpD3DHELDevDesc)
			{
				ConvertDeviceDesc(*lpD3DHELDevDesc, D3DDevDesc);
			}
		}

		return hr;
	}
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::GetCaps(LPD3DDEVICEDESC7 lpD3DDevDesc)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpD3DDevDesc)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		D3DCAPS9 Caps9 = {};

		HRESULT hr = (*d3d9Device)->GetDeviceCaps(&Caps9);

		if (SUCCEEDED(hr))
		{
			ConvertDeviceDesc(*lpD3DDevDesc, Caps9);
		}

		return hr;
	}

	return GetProxyInterfaceV7()->GetCaps(lpD3DDevDesc);
}

HRESULT m_IDirect3DDeviceX::GetStats(LPD3DSTATS lpD3DStats, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->GetStats(lpD3DStats);
	case 2:
		return GetProxyInterfaceV2()->GetStats(lpD3DStats);
	case 3:
		return GetProxyInterfaceV3()->GetStats(lpD3DStats);
	default:

		if (DirectXVersion == 3)
		{
			// The method returns E_NOTIMPL / DDERR_UNSUPPORTED.
			return DDERR_UNSUPPORTED;
		}

		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}
}

HRESULT m_IDirect3DDeviceX::AddViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9 || ProxyDirectXVersion == 7)
	{
		// This method will fail, returning DDERR_INVALIDPARAMS, if you attempt to add a viewport that has already been assigned to the device.
		if (!lpDirect3DViewport || IsViewportAttached(lpDirect3DViewport))
		{
			return DDERR_INVALIDPARAMS;
		}

		// ToDo: Validate Viewport address
		AttachedViewports.push_back(lpDirect3DViewport);

		lpDirect3DViewport->AddRef();

		return D3D_OK;
	}

	if (lpDirect3DViewport)
	{
		lpDirect3DViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DViewport);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->AddViewport(lpDirect3DViewport);
	case 2:
		return GetProxyInterfaceV2()->AddViewport(lpDirect3DViewport);
	case 3:
		return GetProxyInterfaceV3()->AddViewport(lpDirect3DViewport);
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::DeleteViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		if (!lpDirect3DViewport)
		{
			return DDERR_INVALIDPARAMS;
		}

		// ToDo: Figure out what to do if an attempting to delete the SetCurrentViewport
		bool ret = DeleteAttachedViewport(lpDirect3DViewport);

		if (!ret)
		{
			return DDERR_INVALIDPARAMS;
		}

		// ToDo: Validate Viewport address
		lpDirect3DViewport->Release();

		return D3D_OK;
	}

	if (lpDirect3DViewport)
	{
		lpDirect3DViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DViewport);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->DeleteViewport(lpDirect3DViewport);
	case 2:
		return GetProxyInterfaceV2()->DeleteViewport(lpDirect3DViewport);
	case 3:
		return GetProxyInterfaceV3()->DeleteViewport(lpDirect3DViewport);
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::NextViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport, LPDIRECT3DVIEWPORT3* lplpDirect3DViewport, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		if (!lplpDirect3DViewport || (dwFlags == D3DNEXT_NEXT && !lpDirect3DViewport))
		{
			return DDERR_INVALIDPARAMS;
		}

		*lplpDirect3DViewport = nullptr;

		if (AttachedViewports.size() == 0)
		{
			return D3DERR_NOVIEWPORTS;
		}

		switch (dwFlags)
		{
		case D3DNEXT_HEAD:
			// Retrieve the item at the beginning of the list.
			*lplpDirect3DViewport = AttachedViewports.front();
			break;
		case D3DNEXT_TAIL:
			// Retrieve the item at the end of the list.
			*lplpDirect3DViewport = AttachedViewports.back();
			break;
		case D3DNEXT_NEXT:
			// Retrieve the next item in the list.
			// If you attempt to retrieve the next viewport in the list when you are at the end of the list, this method returns D3D_OK but lplpAnotherViewport is NULL.
			for (UINT x = 1; x < AttachedViewports.size(); x++)
			{
				if (AttachedViewports[x - 1] == lpDirect3DViewport)
				{
					*lplpDirect3DViewport = AttachedViewports[x];
					break;
				}
			}
			break;
		default:
			return DDERR_INVALIDPARAMS;
			break;
		}

		// ToDo: Validate return Viewport address
		return D3D_OK;
	}

	if (lpDirect3DViewport)
	{
		lpDirect3DViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpDirect3DViewport);
	}

	HRESULT hr = DDERR_GENERIC;

	switch (ProxyDirectXVersion)
	{
	case 1:
		hr = GetProxyInterfaceV1()->NextViewport(lpDirect3DViewport, (LPDIRECT3DVIEWPORT*)lplpDirect3DViewport, dwFlags);
		break;
	case 2:
		hr = GetProxyInterfaceV2()->NextViewport(lpDirect3DViewport, (LPDIRECT3DVIEWPORT2*)lplpDirect3DViewport, dwFlags);
		break;
	case 3:
		hr = GetProxyInterfaceV3()->NextViewport(lpDirect3DViewport, lplpDirect3DViewport, dwFlags);
		break;
	}

	if (SUCCEEDED(hr) && lplpDirect3DViewport)
	{
		*lplpDirect3DViewport = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport3>(*lplpDirect3DViewport, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::SetCurrentViewport(LPDIRECT3DVIEWPORT3 lpd3dViewport)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9 || ProxyDirectXVersion == 7)
	{
		// Before calling this method, applications must have already called the AddViewport method to add the viewport to the device.
		if (!lpd3dViewport || !IsViewportAttached(lpd3dViewport))
		{
			return DDERR_INVALIDPARAMS;
		}

		m_IDirect3DViewportX* lpViewportX = nullptr;
		if (FAILED(lpd3dViewport->QueryInterface(IID_GetInterfaceX, (LPVOID*)&lpViewportX)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get ViewportX interface!");
			return DDERR_GENERIC;
		}

		D3DVIEWPORT Viewport = {};
		Viewport.dwSize = sizeof(D3DVIEWPORT);

		// ToDo: Validate Viewport address
		HRESULT hr = lpd3dViewport->GetViewport(&Viewport);

		if (SUCCEEDED(hr))
		{
			D3DVIEWPORT7 Viewport7;

			ConvertViewport(Viewport7, Viewport);

			hr = SetViewport(&Viewport7);

			if (SUCCEEDED(hr))
			{
				lpCurrentViewport = lpd3dViewport;

				lpCurrentViewport->AddRef();

				lpCurrentViewportX = lpViewportX;

				lpCurrentViewportX->SetCurrentViewportActive(true, true, true);
			}
		}

		return hr;
	}

	if (lpd3dViewport)
	{
		lpd3dViewport->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpd3dViewport);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetCurrentViewport(lpd3dViewport);
	case 3:
		return GetProxyInterfaceV3()->SetCurrentViewport(lpd3dViewport);
	}
}

HRESULT m_IDirect3DDeviceX::GetCurrentViewport(LPDIRECT3DVIEWPORT3* lplpd3dViewport, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9 || ProxyDirectXVersion == 7)
	{
		if (!lplpd3dViewport)
		{
			return DDERR_INVALIDPARAMS;
		}

		if (!lpCurrentViewport)
		{
			return D3DERR_NOCURRENTVIEWPORT;
		}

		// ToDo: Validate Viewport address
		*lplpd3dViewport = lpCurrentViewport;

		lpCurrentViewport->AddRef();

		return D3D_OK;
	}

	HRESULT hr = DDERR_GENERIC;

	switch (ProxyDirectXVersion)
	{
	case 2:
		hr = GetProxyInterfaceV2()->GetCurrentViewport((LPDIRECT3DVIEWPORT2*)lplpd3dViewport);
		break;
	case 3:
		hr = GetProxyInterfaceV3()->GetCurrentViewport(lplpd3dViewport);
		break;
	}

	if (SUCCEEDED(hr) && lplpd3dViewport)
	{
		*lplpd3dViewport = ProxyAddressLookupTable.FindAddress<m_IDirect3DViewport3>(*lplpd3dViewport, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::SetViewport(LPD3DVIEWPORT7 lpViewport)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpViewport)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->SetViewport((D3DVIEWPORT9*)lpViewport);
	}

	D3DVIEWPORT7 Viewport7;
	if (Config.DdrawUseNativeResolution && lpViewport)
	{
		ConvertViewport(Viewport7, *lpViewport);
		Viewport7.dwX = (LONG)(Viewport7.dwX * ScaleDDWidthRatio) + ScaleDDPadX;
		Viewport7.dwY = (LONG)(Viewport7.dwY * ScaleDDHeightRatio) + ScaleDDPadY;
		Viewport7.dwWidth = (LONG)(Viewport7.dwWidth * ScaleDDWidthRatio);
		Viewport7.dwHeight = (LONG)(Viewport7.dwHeight * ScaleDDHeightRatio);
		lpViewport = &Viewport7;
	}

	return GetProxyInterfaceV7()->SetViewport(lpViewport);
}

HRESULT m_IDirect3DDeviceX::GetViewport(LPD3DVIEWPORT7 lpViewport)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpViewport)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->GetViewport((D3DVIEWPORT9*)lpViewport);
	}

	return GetProxyInterfaceV7()->GetViewport(lpViewport);
}

HRESULT m_IDirect3DDeviceX::Begin(D3DPRIMITIVETYPE d3dpt, DWORD d3dvt, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->Begin(d3dpt, (D3DVERTEXTYPE)d3dvt, dwFlags);
	case 3:
		return GetProxyInterfaceV3()->Begin(d3dpt, d3dvt, dwFlags);
	}
}

HRESULT m_IDirect3DDeviceX::BeginIndexed(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dvtVertexType, LPVOID lpvVertices, DWORD dwNumVertices, DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->BeginIndexed(dptPrimitiveType, (D3DVERTEXTYPE)dvtVertexType, lpvVertices, dwNumVertices, dwFlags);
	case 3:
		return GetProxyInterfaceV3()->BeginIndexed(dptPrimitiveType, dvtVertexType, lpvVertices, dwNumVertices, dwFlags);
	}
}

HRESULT m_IDirect3DDeviceX::Vertex(LPVOID lpVertexType)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->Vertex(lpVertexType);
	case 3:
		return GetProxyInterfaceV3()->Vertex(lpVertexType);
	}
}

HRESULT m_IDirect3DDeviceX::Index(WORD wVertexIndex)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->Index(wVertexIndex);
	case 3:
		return GetProxyInterfaceV3()->Index(wVertexIndex);
	}
}

HRESULT m_IDirect3DDeviceX::End(DWORD dwFlags)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->End(dwFlags);
	case 3:
		return GetProxyInterfaceV3()->End(dwFlags);
	}
}

HRESULT m_IDirect3DDeviceX::BeginScene()
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		// Set 3D Enabled
		ddrawParent->Enable3D();

		HRESULT hr = (*d3d9Device)->BeginScene();

		if (SUCCEEDED(hr))
		{
			IsInScene = true;

#ifdef ENABLE_PROFILING
			Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr;
			sceneTime = std::chrono::high_resolution_clock::now();
#endif
		}

		return hr;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->BeginScene();

	case 2:
		return GetProxyInterfaceV2()->BeginScene();

	case 3:
		return GetProxyInterfaceV3()->BeginScene();

	case 7:
	default:
		return GetProxyInterfaceV7()->BeginScene();
	}
}

HRESULT m_IDirect3DDeviceX::EndScene()
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		// The IDirect3DDevice7::EndScene method ends a scene that was begun by calling the IDirect3DDevice7::BeginScene method.
		// When this method succeeds, the scene has been rendered, and the device surface holds the rendered scene.

		HRESULT hr = (*d3d9Device)->EndScene();
		
		if (SUCCEEDED(hr))
		{
			IsInScene = false;

#ifdef ENABLE_PROFILING
			Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr << " Timing = " << Logging::GetTimeLapseInMS(sceneTime);
#endif

			m_IDirectDrawSurfaceX* PrimarySurface = ddrawParent->GetPrimarySurface();
			if (!PrimarySurface || FAILED(PrimarySurface->GetFlipStatus(DDGFS_CANFLIP)) || PrimarySurface == ddrawParent->GetRenderTargetSurface() || !PrimarySurface->IsRenderTarget())
			{
				ddrawParent->PresentScene(nullptr);
			}
		}

		return hr;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
		return GetProxyInterfaceV1()->EndScene();
	case 2:
		return GetProxyInterfaceV2()->EndScene();
	case 3:
		return GetProxyInterfaceV3()->EndScene();
	case 7:
		return GetProxyInterfaceV7()->EndScene();
	default:
		return DDERR_GENERIC;
	}
}

HRESULT m_IDirect3DDeviceX::Clear(DWORD dwCount, LPD3DRECT lpRects, DWORD dwFlags, D3DCOLOR dwColor, D3DVALUE dvZ, DWORD dwStencil)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		if ((dwFlags & D3DCLEAR_TARGET) && lpCurrentRenderTargetX)
		{
			lpCurrentRenderTargetX->PrepareRenderTarget();
		}

		// Clear primary surface
		/*if (!Config.DdrawEnableRenderTarget && lpCurrentRenderTargetX && (dwFlags & D3DCLEAR_TARGET))
		{
			// Clear each specified rectangle
			if (dwCount && lpRects)
			{
				for (UINT x = 0; x < dwCount; x++)
				{
					lpCurrentRenderTargetX->ColorFill((RECT*)&lpRects[x], (!Config.DdrawEnableRenderTarget && Config.DdrawFlipFillColor) ? Config.DdrawFlipFillColor : dwColor);
				}
			}
			// Clear the entire surface if no rectangles are specified
			else
			{
				lpCurrentRenderTargetX->ColorFill(nullptr, (!Config.DdrawEnableRenderTarget && Config.DdrawFlipFillColor) ? Config.DdrawFlipFillColor : dwColor);
			}
			lpCurrentRenderTargetX->ClearDirtyFlags();
		}*/

		return (*d3d9Device)->Clear(dwCount, lpRects, dwFlags, dwColor, dvZ, dwStencil);
	}

	return GetProxyInterfaceV7()->Clear(dwCount, lpRects, dwFlags, dwColor, dvZ, dwStencil);
}

HRESULT m_IDirect3DDeviceX::GetDirect3D(LPDIRECT3D7* lplpD3D, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lplpD3D)
		{
			return DDERR_INVALIDPARAMS;
		}
		*lplpD3D = nullptr;

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, false)))
		{
			return DDERR_INVALIDOBJECT;
		}

		m_IDirect3DX** lplpD3DX = ddrawParent->GetCurrentD3D();

		if (!(*lplpD3DX))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: missing Direct3D wrapper!");
			return DDERR_GENERIC;
		}

		*lplpD3D = (LPDIRECT3D7)(*lplpD3DX)->GetWrapperInterfaceX(DirectXVersion);

		if (!(*lplpD3D))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get Direct3D interface!");
			return DDERR_GENERIC;
		}

		(*lplpD3D)->AddRef();

		return D3D_OK;
	}

	HRESULT hr = DDERR_GENERIC;

	switch (ProxyDirectXVersion)
	{
	case 1:
		hr = GetProxyInterfaceV1()->GetDirect3D((LPDIRECT3D*)lplpD3D);
		break;
	case 2:
		hr = GetProxyInterfaceV2()->GetDirect3D((LPDIRECT3D2*)lplpD3D);
		break;
	case 3:
		hr = GetProxyInterfaceV3()->GetDirect3D((LPDIRECT3D3*)lplpD3D);
		break;
	case 7:
		hr = GetProxyInterfaceV7()->GetDirect3D(lplpD3D);
		break;
	}

	if (SUCCEEDED(hr) && lplpD3D)
	{
		*lplpD3D = ProxyAddressLookupTable.FindAddress<m_IDirect3D7>(*lplpD3D, DirectXVersion);
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::GetLightState(D3DLIGHTSTATETYPE dwLightStateType, LPDWORD lpdwLightState)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		if (!lpdwLightState)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Light state called with nullptr: " << dwLightStateType);
			return DDERR_INVALIDPARAMS;
		}

		DWORD RenderState = 0;
		switch (dwLightStateType)
		{
		case D3DLIGHTSTATE_MATERIAL:
			*lpdwLightState = lsMaterialHandle;
			return D3D_OK;
		case D3DLIGHTSTATE_AMBIENT:
			RenderState = D3DRENDERSTATE_AMBIENT;
			break;
		case D3DLIGHTSTATE_COLORMODEL:
			*lpdwLightState = D3DCOLOR_RGB;
			return D3D_OK;
		case D3DLIGHTSTATE_FOGMODE:
			RenderState = D3DRENDERSTATE_FOGVERTEXMODE;
			break;
		case D3DLIGHTSTATE_FOGSTART:
			RenderState = D3DRENDERSTATE_FOGSTART;
			break;
		case D3DLIGHTSTATE_FOGEND:
			RenderState = D3DRENDERSTATE_FOGEND;
			break;
		case D3DLIGHTSTATE_FOGDENSITY:
			RenderState = D3DRENDERSTATE_FOGDENSITY;
			break;
		case D3DLIGHTSTATE_COLORVERTEX:
			RenderState = D3DRENDERSTATE_COLORVERTEX;
			break;
		default:
			break;
		}

		if (!RenderState)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: unknown LightStateType: " << dwLightStateType);
			return DDERR_INVALIDPARAMS;
		}

		return GetRenderState((D3DRENDERSTATETYPE)RenderState, lpdwLightState);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->GetLightState(dwLightStateType, lpdwLightState);
	case 3:
		return GetProxyInterfaceV3()->GetLightState(dwLightStateType, lpdwLightState);
	}
}

HRESULT m_IDirect3DDeviceX::SetLightState(D3DLIGHTSTATETYPE dwLightStateType, DWORD dwLightState)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (ProxyDirectXVersion > 3)
	{
		DWORD RenderState = 0;
		switch (dwLightStateType)
		{
		case D3DLIGHTSTATE_MATERIAL:
		{
			D3DMATERIAL Material;

			if (dwLightState == NULL)
			{
				Material = {};
			}
			else if (MaterialHandleMap.find(dwLightState) != MaterialHandleMap.end())
			{
				m_IDirect3DMaterialX* pMaterialX = MaterialHandleMap[dwLightState];
				if (!pMaterialX)
				{
					LOG_LIMIT(100, __FUNCTION__ << " Error: could not get material wrapper!");
					return DDERR_INVALIDPARAMS;
				}

				Material.dwSize = sizeof(D3DMATERIAL);
				if (FAILED(pMaterialX->GetMaterial(&Material)))
				{
					return DDERR_INVALIDPARAMS;
				}
			}
			else
			{
				LOG_LIMIT(100, __FUNCTION__ << " Error: could not get material handle!");
				return D3D_OK;
			}

			D3DMATERIAL7 Material7;

			ConvertMaterial(Material7, Material);

			SetMaterial(&Material7);

			if (Material.hTexture)
			{
				SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE, Material.hTexture);
			}

			lsMaterialHandle = dwLightState;

			return D3D_OK;
		}
		case D3DLIGHTSTATE_AMBIENT:
			RenderState = D3DRENDERSTATE_AMBIENT;
			break;
		case D3DLIGHTSTATE_COLORMODEL:
			if (dwLightState != D3DCOLOR_RGB)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DLIGHTSTATE_COLORMODEL' not implemented! " << dwLightState);
			}
			return D3D_OK;
		case D3DLIGHTSTATE_FOGMODE:
			RenderState = D3DRENDERSTATE_FOGVERTEXMODE;
			break;
		case D3DLIGHTSTATE_FOGSTART:
			RenderState = D3DRENDERSTATE_FOGSTART;
			break;
		case D3DLIGHTSTATE_FOGEND:
			RenderState = D3DRENDERSTATE_FOGEND;
			break;
		case D3DLIGHTSTATE_FOGDENSITY:
			RenderState = D3DRENDERSTATE_FOGDENSITY;
			break;
		case D3DLIGHTSTATE_COLORVERTEX:
			RenderState = D3DRENDERSTATE_COLORVERTEX;
			break;
		default:
			break;
		}

		if (!RenderState)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: unknown LightStateType: " << dwLightStateType);
			return DDERR_INVALIDPARAMS;
		}

		return SetRenderState((D3DRENDERSTATETYPE)RenderState, dwLightState);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetLightState(dwLightStateType, dwLightState);
	case 3:
		return GetProxyInterfaceV3()->SetLightState(dwLightStateType, dwLightState);
	}
}

void m_IDirect3DDeviceX::ReleaseLightInterface(m_IDirect3DLight* lpLight)
{
	// Find handle associated with Light
	auto it = LightIndexMap.begin();
	while (it != LightIndexMap.end())
	{
		if (it->second == lpLight)
		{
			// Disable light before removing
			LightEnable(it->first, FALSE);

			// Remove entry from map
			it = LightIndexMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

HRESULT m_IDirect3DDeviceX::SetLight(m_IDirect3DLight* lpLightInterface, LPD3DLIGHT lpLight)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (!lpLightInterface || !lpLight || (lpLight->dwSize != sizeof(D3DLIGHT) && lpLight->dwSize != sizeof(D3DLIGHT2)))
	{
		return DDERR_INVALIDPARAMS;
	}

	D3DLIGHT7 Light7;

	// ToDo: the dvAttenuation members are interpreted differently in D3DLIGHT2 than they were for D3DLIGHT.
	ConvertLight(Light7, *lpLight);

	DWORD dwLightIndex = 0;

	// Check if Light exists in the map
	for (auto& entry : LightIndexMap)
	{
		if (entry.second == lpLightInterface)
		{
			dwLightIndex = entry.first;
			break;
		}
	}

	// Create index and add light to the map
	if (dwLightIndex == 0)
	{
		BYTE Start = (BYTE)((DWORD)lpLightInterface & 0xff);
		for (BYTE x = Start; x != Start - 1; x++)
		{
			bool Flag = true;
			for (auto& entry : LightIndexMap)
			{
				if (entry.first == x)
				{
					Flag = false;
					break;
				}
			}
			if (x != 0 && Flag)
			{
				dwLightIndex = x;
				break;
			}
		}
	}

	if (dwLightIndex == 0)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Failed to find an available Light Index");
		return DDERR_INVALIDPARAMS;
	}

	// Add light to index map
	LightIndexMap[dwLightIndex] = lpLightInterface;

	HRESULT hr = SetLight(dwLightIndex, &Light7);

	if (SUCCEEDED(hr))
	{
		if (((LPD3DLIGHT2)lpLight)->dwSize == sizeof(D3DLIGHT2) && (((LPD3DLIGHT2)lpLight)->dwFlags & D3DLIGHT_ACTIVE) == NULL)
		{
			LightEnable(dwLightIndex, FALSE);
		}
		else
		{
			LightEnable(dwLightIndex, TRUE);
		}
	}

	return hr;
}

HRESULT m_IDirect3DDeviceX::SetLight(DWORD dwLightIndex, LPD3DLIGHT7 lpLight)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpLight)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: called with nullptr: " << lpLight);
			return DDERR_INVALIDPARAMS;
		}

		if (lpLight->dltType == D3DLIGHT_PARALLELPOINT || lpLight->dltType == D3DLIGHT_GLSPOT)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Light Type: " << lpLight->dltType << " Not Implemented");
			return D3D_OK;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		D3DLIGHT9 Light = *(D3DLIGHT9*)lpLight;

		// Make spot light work more like it did in Direct3D7
		if (Light.Type == D3DLIGHTTYPE::D3DLIGHT_SPOT)
		{
			// Theta must be in the range from 0 through the value specified by Phi
			if (Light.Theta <= Light.Phi)
			{
				Light.Theta /= 1.75f;
			}
		}

		HRESULT hr = (*d3d9Device)->SetLight(dwLightIndex, &Light);

		if (SUCCEEDED(hr))
		{
#ifdef ENABLE_DEBUGOVERLAY
			if (Config.EnableImgui)
			{
				DOverlay.SetLight(dwLightIndex, lpLight);
			}
#endif
		}

		return hr;
	}

	return GetProxyInterfaceV7()->SetLight(dwLightIndex, lpLight);
}

HRESULT m_IDirect3DDeviceX::GetLight(DWORD dwLightIndex, LPD3DLIGHT7 lpLight)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpLight)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->GetLight(dwLightIndex, (D3DLIGHT9*)lpLight);
	}

	return GetProxyInterfaceV7()->GetLight(dwLightIndex, lpLight);
}

HRESULT m_IDirect3DDeviceX::LightEnable(DWORD dwLightIndex, BOOL bEnable)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		HRESULT hr = (*d3d9Device)->LightEnable(dwLightIndex, bEnable);

		if (SUCCEEDED(hr))
		{
#ifdef ENABLE_DEBUGOVERLAY
			if (Config.EnableImgui)
			{
				DOverlay.LightEnable(dwLightIndex, bEnable);
			}
#endif
		}

		return hr;
	}

	return GetProxyInterfaceV7()->LightEnable(dwLightIndex, bEnable);
}

HRESULT m_IDirect3DDeviceX::GetLightEnable(m_IDirect3DLight* lpLightInterface, BOOL* pbEnable)
{
	if (!lpLightInterface || !pbEnable)
	{
		return DDERR_INVALIDPARAMS;
	}

	DWORD dwLightIndex = 0;

	// Check if Light exists in the map
	for (auto& entry : LightIndexMap)
	{
		if (entry.second == lpLightInterface)
		{
			dwLightIndex = entry.first;
			break;
		}
	}

	if (dwLightIndex == 0)
	{
		return DDERR_INVALIDPARAMS;
	}

	return GetLightEnable(dwLightIndex, pbEnable);
}

HRESULT m_IDirect3DDeviceX::GetLightEnable(DWORD dwLightIndex, BOOL* pbEnable)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!pbEnable)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->GetLightEnable(dwLightIndex, pbEnable);
	}

	return GetProxyInterfaceV7()->GetLightEnable(dwLightIndex, pbEnable);
}

HRESULT m_IDirect3DDeviceX::MultiplyTransform(D3DTRANSFORMSTATETYPE dtstTransformStateType, LPD3DMATRIX lpD3DMatrix)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dtstTransformStateType)
		{
		case D3DTRANSFORMSTATE_WORLD:
			dtstTransformStateType = D3DTS_WORLD;
			break;
		case D3DTRANSFORMSTATE_WORLD1:
			dtstTransformStateType = D3DTS_WORLD1;
			break;
		case D3DTRANSFORMSTATE_WORLD2:
			dtstTransformStateType = D3DTS_WORLD2;
			break;
		case D3DTRANSFORMSTATE_WORLD3:
			dtstTransformStateType = D3DTS_WORLD3;
			break;
		}

		return (*d3d9Device)->MultiplyTransform(dtstTransformStateType, lpD3DMatrix);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->MultiplyTransform(dtstTransformStateType, lpD3DMatrix);
	case 3:
		return GetProxyInterfaceV3()->MultiplyTransform(dtstTransformStateType, lpD3DMatrix);
	case 7:
		return GetProxyInterfaceV7()->MultiplyTransform(dtstTransformStateType, lpD3DMatrix);
	}
}

void m_IDirect3DDeviceX::ReleaseMaterialHandle(m_IDirect3DMaterialX* lpMaterial)
{
	// Find handle associated with Material
	auto it = MaterialHandleMap.begin();
	while (it != MaterialHandleMap.end())
	{
		if (it->second == lpMaterial)
		{
			// Remove entry from map
			it = MaterialHandleMap.erase(it);
		}
		else
		{
			++it;
		}
	}
}

HRESULT m_IDirect3DDeviceX::SetMaterialHandle(D3DMATERIALHANDLE mHandle, m_IDirect3DMaterialX* lpMaterial)
{
	if (!mHandle || !lpMaterial)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: NULL pointer found! " << lpMaterial << " -> " << mHandle);
		return DDERR_GENERIC;
	}

	MaterialHandleMap[mHandle] = lpMaterial;

	return D3D_OK;
}

HRESULT m_IDirect3DDeviceX::SetMaterial(LPD3DMATERIAL lpMaterial)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (!lpMaterial)
	{
		return DDERR_INVALIDPARAMS;
	}

	D3DMATERIAL7 Material7;

	ConvertMaterial(Material7, *lpMaterial);

	HRESULT hr = SetMaterial(&Material7);

	if (FAILED(hr))
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Failed to set material: " << (D3DERR)hr);
		return hr;
	}

	if (lpMaterial->dwRampSize)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Warning: RampSize Not Implemented: " << lpMaterial->dwRampSize);
	}

	if (lpMaterial->hTexture)
	{
		SetRenderState(D3DRENDERSTATE_TEXTUREHANDLE, lpMaterial->hTexture);
	}

	return D3D_OK;
}

HRESULT m_IDirect3DDeviceX::SetMaterial(LPD3DMATERIAL7 lpMaterial)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpMaterial)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->SetMaterial((D3DMATERIAL9*)lpMaterial);
	}

	return GetProxyInterfaceV7()->SetMaterial(lpMaterial);
}

HRESULT m_IDirect3DDeviceX::GetMaterial(LPD3DMATERIAL7 lpMaterial)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpMaterial)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->GetMaterial((D3DMATERIAL9*)lpMaterial);
	}

	return GetProxyInterfaceV7()->GetMaterial(lpMaterial);
}

HRESULT m_IDirect3DDeviceX::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ") " << dwRenderStateType << " " << dwRenderState;

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dwRenderStateType)
		{
		case D3DRENDERSTATE_TEXTUREHANDLE:		// 1
			if (dwRenderState == NULL)
			{
				rsTextureHandle = dwRenderState;
				return SetTexture(0, (LPDIRECT3DTEXTURE2)nullptr);
			}
			else if (TextureHandleMap.find(dwRenderState) != TextureHandleMap.end())
			{
				m_IDirect3DTextureX* pTextureX = TextureHandleMap[dwRenderState];
				if (!pTextureX)
				{
					LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture wrapper!");
					return DDERR_INVALIDPARAMS;
				}

				IDirect3DTexture2* lpTexture = (IDirect3DTexture2*)pTextureX->GetWrapperInterfaceX(0);
				if (!lpTexture)
				{
					LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture address!");
					return DDERR_INVALIDPARAMS;
				}

				rsTextureHandle = dwRenderState;
				return SetTexture(0, lpTexture);
			}
			else
			{
				LOG_LIMIT(100, __FUNCTION__ << " Error: could not get texture handle!");
			}
			return D3D_OK;
		case D3DRENDERSTATE_ANTIALIAS:			// 2
			rsAntiAliasChanged = true;
			rsAntiAlias = dwRenderStateType;
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREADDRESS:		// 3
			return SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESS, dwRenderState);
		case D3DRENDERSTATE_TEXTUREPERSPECTIVE:	// 4
			// For the IDirect3DDevice3 interface, the default value is TRUE. For earlier interfaces, the default is FALSE.
			return D3D_OK;
		case D3DRENDERSTATE_WRAPU:				// 5
			rsTextureWrappingChanged = true;
			rsTextureWrappingU = dwRenderState;
			return D3D_OK;
		case D3DRENDERSTATE_WRAPV:				// 6
			rsTextureWrappingChanged = true;
			rsTextureWrappingV = dwRenderState;
			return D3D_OK;
		case D3DRENDERSTATE_LINEPATTERN:		// 10
			if (dwRenderState != 0)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_LINEPATTERN' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_MONOENABLE:			// 11
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_MONOENABLE' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_ROP2:				// 12
			if (dwRenderState != R2_COPYPEN)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_ROP2' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_PLANEMASK:			// 13
			if (dwRenderState != (DWORD)-1)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_PLANEMASK' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREMAG:			// 17
			// Only the first two (D3DFILTER_NEAREST and D3DFILTER_LINEAR) are valid with D3DRENDERSTATE_TEXTUREMAG.
			switch (dwRenderState)
			{
			case D3DFILTER_NEAREST:
			case D3DFILTER_LINEAR:
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MAGFILTER, dwRenderState);
			default:
				LOG_LIMIT(100, __FUNCTION__ << " Warning: unsupported 'D3DRENDERSTATE_TEXTUREMAG' state: " << dwRenderState);
				return DDERR_INVALIDPARAMS;
			}
		case D3DRENDERSTATE_TEXTUREMIN:			// 18
			switch (dwRenderState)
			{
			case D3DFILTER_NEAREST:
			case D3DFILTER_LINEAR:
				rsTextureMin = dwRenderState;
				ssMipFilter[0] = D3DTEXF_NONE;
				(*d3d9Device)->SetSamplerState(0, D3DSAMP_MINFILTER, dwRenderState);
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			case D3DFILTER_MIPNEAREST:
				rsTextureMin = dwRenderState;
				ssMipFilter[0] = D3DTEXF_POINT;
				(*d3d9Device)->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
			case D3DFILTER_MIPLINEAR:
				rsTextureMin = dwRenderState;
				ssMipFilter[0] = D3DTEXF_POINT;
				(*d3d9Device)->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
			case D3DFILTER_LINEARMIPNEAREST:
				rsTextureMin = dwRenderState;
				ssMipFilter[0] = D3DTEXF_LINEAR;
				(*d3d9Device)->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
			case D3DFILTER_LINEARMIPLINEAR:
				rsTextureMin = dwRenderState;
				ssMipFilter[0] = D3DTEXF_LINEAR;
				(*d3d9Device)->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
				return (*d3d9Device)->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
			default:
				LOG_LIMIT(100, __FUNCTION__ << " Warning: unsupported 'D3DRENDERSTATE_TEXTUREMIN' state: " << dwRenderState);
				return DDERR_INVALIDPARAMS;
			}
		case D3DRENDERSTATE_SRCBLEND:			// 19
			rsSrcBlend = dwRenderState;
			break;
		case D3DRENDERSTATE_DESTBLEND:			// 20
			rsDestBlend = dwRenderState;
			break;
		case D3DRENDERSTATE_TEXTUREMAPBLEND:	// 21
			switch (dwRenderState)
			{
			case D3DTBLEND_COPY:
			case D3DTBLEND_DECAL:
				// Reset states
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

				// Decal texture-blending mode is supported. In this mode, the RGB and alpha values of the texture replace the colors that would have been used with no texturing.
				(*d3d9Device)->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				(*d3d9Device)->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

				// Save state
				rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_DECALALPHA:
				// Reset states
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				// Decal-alpha texture-blending mode is supported. In this mode, the RGB and alpha values of the texture are 
				// blended with the colors that would have been used with no texturing.
				(*d3d9Device)->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				(*d3d9Device)->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

				// Save state
				rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_DECALMASK:
				// This blending mode is not supported. When the least-significant bit of the texture's alpha component is zero, 
				// the effect is as if texturing were disabled.
				LOG_LIMIT(100, __FUNCTION__ << " Warning: unsupported 'D3DTBLEND_DECALMASK' state: " << dwRenderState);

				// Save state
				//rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_MODULATE:
				// Reset states
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				// Modulate texture-blending mode is supported. In this mode, the RGB values of the texture are multiplied 
				// with the RGB values that would have been used with no texturing. Any alpha values in the texture replace 
				// the alpha values in the colors that would have been used with no texturing; if the texture does not contain 
				// an alpha component, alpha values at the vertices in the source are interpolated between vertices.
				(*d3d9Device)->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				(*d3d9Device)->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

				// Save state
				rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_MODULATEALPHA:
				// Reset states
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				// Modulate-alpha texture-blending mode is supported. In this mode, the RGB values of the texture are multiplied 
				// with the RGB values that would have been used with no texturing, and the alpha values of the texture are multiplied 
				// with the alpha values that would have been used with no texturing.
				(*d3d9Device)->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				(*d3d9Device)->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

				// Save state
				rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_MODULATEMASK:
				// This blending mode is not supported. When the least-significant bit of the texture's alpha component is zero, 
				// the effect is as if texturing were disabled.
				LOG_LIMIT(100, __FUNCTION__ << " Warning: unsupported 'D3DTBLEND_MODULATEMASK' state: " << dwRenderState);

				// Save state
				//rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			case D3DTBLEND_ADD:
				// Reset states
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

				// Add the Gouraud interpolants to the texture lookup with saturation semantics
				// (that is, if the color value overflows it is set to the maximum possible value).
				(*d3d9Device)->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				(*d3d9Device)->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
				(*d3d9Device)->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

				// Save state
				rsTextureMapBlend = dwRenderState;
				return D3D_OK;
			default:
				LOG_LIMIT(100, __FUNCTION__ << " Warning: unsupported 'D3DRENDERSTATE_TEXTUREMAPBLEND' state: " << dwRenderState);
				return D3D_OK;
			}
		case D3DRENDERSTATE_ALPHAREF:			// 24
			dwRenderState &= 0xFF;
			break;
		case D3DRENDERSTATE_ALPHABLENDENABLE:	// 27
			rsAlphaBlendEnabled = dwRenderState;
			break;
		case D3DRENDERSTATE_ZVISIBLE:			// 30
			// This render state is not supported.
			return D3D_OK;
		case D3DRENDERSTATE_SUBPIXEL:			// 31
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_SUBPIXEL' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_SUBPIXELX:			// 32
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_SUBPIXELX' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEDALPHA:		// 33
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_STIPPLEDALPHA' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEENABLE:		// 39
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_STIPPLEENABLE' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_EDGEANTIALIAS:		// 40
			rsAntiAliasChanged = true;
			rsEdgeAntiAlias = dwRenderStateType;
			return D3D_OK;
		case D3DRENDERSTATE_COLORKEYENABLE:		// 41
			rsColorKeyEnabled = dwRenderState;
			return D3D_OK;
		case D3DRENDERSTATE_OLDALPHABLENDENABLE:// 42
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_OLDALPHABLENDENABLE' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_BORDERCOLOR:		// 43
			if (dwRenderState != 0x00000000)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_BORDERCOLOR' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREADDRESSU:	// 44
			return SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, dwRenderState);
		case D3DRENDERSTATE_TEXTUREADDRESSV:	// 45
			return SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, dwRenderState);
		case D3DRENDERSTATE_MIPMAPLODBIAS:		// 46
			return SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_MIPMAPLODBIAS, dwRenderState);
		case D3DRENDERSTATE_ZBIAS:				// 47
		{
			FLOAT Biased = static_cast<FLOAT>(dwRenderState) * -0.000005f;
			dwRenderState = *reinterpret_cast<const DWORD*>(&Biased);
			dwRenderStateType = D3DRS_DEPTHBIAS;
			break;
		}
		case D3DRENDERSTATE_FLUSHBATCH:			// 50
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_FLUSHBATCH' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:	// 51
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEPATTERN00:	// 64
		case D3DRENDERSTATE_STIPPLEPATTERN01:	// 65
		case D3DRENDERSTATE_STIPPLEPATTERN02:	// 66
		case D3DRENDERSTATE_STIPPLEPATTERN03:	// 67
		case D3DRENDERSTATE_STIPPLEPATTERN04:	// 68
		case D3DRENDERSTATE_STIPPLEPATTERN05:	// 69
		case D3DRENDERSTATE_STIPPLEPATTERN06:	// 70
		case D3DRENDERSTATE_STIPPLEPATTERN07:	// 71
		case D3DRENDERSTATE_STIPPLEPATTERN08:	// 72
		case D3DRENDERSTATE_STIPPLEPATTERN09:	// 73
		case D3DRENDERSTATE_STIPPLEPATTERN10:	// 74
		case D3DRENDERSTATE_STIPPLEPATTERN11:	// 75
		case D3DRENDERSTATE_STIPPLEPATTERN12:	// 76
		case D3DRENDERSTATE_STIPPLEPATTERN13:	// 77
		case D3DRENDERSTATE_STIPPLEPATTERN14:	// 78
		case D3DRENDERSTATE_STIPPLEPATTERN15:	// 79
		case D3DRENDERSTATE_STIPPLEPATTERN16:	// 80
		case D3DRENDERSTATE_STIPPLEPATTERN17:	// 81
		case D3DRENDERSTATE_STIPPLEPATTERN18:	// 82
		case D3DRENDERSTATE_STIPPLEPATTERN19:	// 83
		case D3DRENDERSTATE_STIPPLEPATTERN20:	// 84
		case D3DRENDERSTATE_STIPPLEPATTERN21:	// 85
		case D3DRENDERSTATE_STIPPLEPATTERN22:	// 86
		case D3DRENDERSTATE_STIPPLEPATTERN23:	// 87
		case D3DRENDERSTATE_STIPPLEPATTERN24:	// 88
		case D3DRENDERSTATE_STIPPLEPATTERN25:	// 89
		case D3DRENDERSTATE_STIPPLEPATTERN26:	// 90
		case D3DRENDERSTATE_STIPPLEPATTERN27:	// 91
		case D3DRENDERSTATE_STIPPLEPATTERN28:	// 92
		case D3DRENDERSTATE_STIPPLEPATTERN29:	// 93
		case D3DRENDERSTATE_STIPPLEPATTERN30:	// 94
		case D3DRENDERSTATE_STIPPLEPATTERN31:	// 95
			if (dwRenderState != 0)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_STIPPLEPATTERN00' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_EXTENTS:			// 138
			// ToDo: use this to enable/disable clip plane extents set by SetClipStatus()
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_EXTENTS' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		case D3DRENDERSTATE_COLORKEYBLENDENABLE:// 144
			if (dwRenderState != FALSE)
			{
				LOG_LIMIT(100, __FUNCTION__ << " Warning: 'D3DRENDERSTATE_COLORKEYBLENDENABLE' not implemented! " << dwRenderState);
			}
			return D3D_OK;
		}

		if (!CheckRenderStateType(dwRenderStateType))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Render state type not implemented: " << dwRenderStateType << " " << dwRenderState);
			return D3D_OK;	// Just return OK for now!
		}

		return (*d3d9Device)->SetRenderState(dwRenderStateType, dwRenderState);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetRenderState(dwRenderStateType, dwRenderState);
	case 3:
		return GetProxyInterfaceV3()->SetRenderState(dwRenderStateType, dwRenderState);
	case 7:
		return GetProxyInterfaceV7()->SetRenderState(dwRenderStateType, dwRenderState);
	}
}

HRESULT m_IDirect3DDeviceX::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ") " << dwRenderStateType;

	if (Config.Dd7to9)
	{
		if (!lpdwRenderState)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Render state called with nullptr: " << dwRenderStateType);
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		switch ((DWORD)dwRenderStateType)
		{
		case D3DRENDERSTATE_TEXTUREHANDLE:		// 1
			*lpdwRenderState = rsTextureHandle;
			return D3D_OK;
		case D3DRENDERSTATE_ANTIALIAS:			// 2
			*lpdwRenderState = rsAntiAlias;
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREADDRESS:		// 3
			return GetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESS, lpdwRenderState);
		case D3DRENDERSTATE_TEXTUREPERSPECTIVE:	// 4
			// For the IDirect3DDevice3 interface, the default value is TRUE. For earlier interfaces, the default is FALSE.
			*lpdwRenderState = TRUE;
			return D3D_OK;
		case D3DRENDERSTATE_WRAPU:				// 5
			*lpdwRenderState = rsTextureWrappingU;
			return D3D_OK;
		case D3DRENDERSTATE_WRAPV:				// 6
			*lpdwRenderState = rsTextureWrappingV;
			return D3D_OK;
		case D3DRENDERSTATE_LINEPATTERN:		// 10
			*lpdwRenderState = 0;
			return D3D_OK;
		case D3DRENDERSTATE_MONOENABLE:			// 11
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_ROP2:				// 12
			*lpdwRenderState = R2_COPYPEN;
			return D3D_OK;
		case D3DRENDERSTATE_PLANEMASK:			// 13
			*lpdwRenderState = (DWORD)-1;
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREMAG:			// 17
			return (*d3d9Device)->GetSamplerState(0, D3DSAMP_MAGFILTER, lpdwRenderState);
		case D3DRENDERSTATE_TEXTUREMIN:			// 18
			*lpdwRenderState = rsTextureMin;
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREMAPBLEND:	// 21
			*lpdwRenderState = rsTextureMapBlend;
			return D3D_OK;
		case D3DRENDERSTATE_ZVISIBLE:			// 30
			// This render state is not supported.
			*lpdwRenderState = 0;
			return D3D_OK;
		case D3DRENDERSTATE_SUBPIXEL:			// 31
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_SUBPIXELX:			// 32
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEDALPHA:		// 33
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEENABLE:		// 39
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_EDGEANTIALIAS:		// 40
			*lpdwRenderState = rsEdgeAntiAlias;
			return D3D_OK;
		case D3DRENDERSTATE_COLORKEYENABLE:		// 41
			*lpdwRenderState = rsColorKeyEnabled;
			return D3D_OK;
		case D3DRENDERSTATE_OLDALPHABLENDENABLE:// 42
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_BORDERCOLOR:		// 43
			*lpdwRenderState = 0x00000000;
			return D3D_OK;
		case D3DRENDERSTATE_TEXTUREADDRESSU:	// 44
			return GetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSU, lpdwRenderState);
		case D3DRENDERSTATE_TEXTUREADDRESSV:	// 45
			return GetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_ADDRESSV, lpdwRenderState);
		case D3DRENDERSTATE_MIPMAPLODBIAS:		// 46
			return GetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)D3DTSS_MIPMAPLODBIAS, lpdwRenderState);
		case D3DRENDERSTATE_ZBIAS:				// 47
			(*d3d9Device)->GetRenderState(D3DRS_DEPTHBIAS, lpdwRenderState);
			*lpdwRenderState = static_cast<DWORD>(*reinterpret_cast<const FLOAT*>(lpdwRenderState) * -200000.0f);
			return D3D_OK;
		case D3DRENDERSTATE_FLUSHBATCH:			// 50
			*lpdwRenderState = 0;
			return D3D_OK;
		case D3DRENDERSTATE_TRANSLUCENTSORTINDEPENDENT:	// 51
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_STIPPLEPATTERN00:	// 64
		case D3DRENDERSTATE_STIPPLEPATTERN01:	// 65
		case D3DRENDERSTATE_STIPPLEPATTERN02:	// 66
		case D3DRENDERSTATE_STIPPLEPATTERN03:	// 67
		case D3DRENDERSTATE_STIPPLEPATTERN04:	// 68
		case D3DRENDERSTATE_STIPPLEPATTERN05:	// 69
		case D3DRENDERSTATE_STIPPLEPATTERN06:	// 70
		case D3DRENDERSTATE_STIPPLEPATTERN07:	// 71
		case D3DRENDERSTATE_STIPPLEPATTERN08:	// 72
		case D3DRENDERSTATE_STIPPLEPATTERN09:	// 73
		case D3DRENDERSTATE_STIPPLEPATTERN10:	// 74
		case D3DRENDERSTATE_STIPPLEPATTERN11:	// 75
		case D3DRENDERSTATE_STIPPLEPATTERN12:	// 76
		case D3DRENDERSTATE_STIPPLEPATTERN13:	// 77
		case D3DRENDERSTATE_STIPPLEPATTERN14:	// 78
		case D3DRENDERSTATE_STIPPLEPATTERN15:	// 79
		case D3DRENDERSTATE_STIPPLEPATTERN16:	// 80
		case D3DRENDERSTATE_STIPPLEPATTERN17:	// 81
		case D3DRENDERSTATE_STIPPLEPATTERN18:	// 82
		case D3DRENDERSTATE_STIPPLEPATTERN19:	// 83
		case D3DRENDERSTATE_STIPPLEPATTERN20:	// 84
		case D3DRENDERSTATE_STIPPLEPATTERN21:	// 85
		case D3DRENDERSTATE_STIPPLEPATTERN22:	// 86
		case D3DRENDERSTATE_STIPPLEPATTERN23:	// 87
		case D3DRENDERSTATE_STIPPLEPATTERN24:	// 88
		case D3DRENDERSTATE_STIPPLEPATTERN25:	// 89
		case D3DRENDERSTATE_STIPPLEPATTERN26:	// 90
		case D3DRENDERSTATE_STIPPLEPATTERN27:	// 91
		case D3DRENDERSTATE_STIPPLEPATTERN28:	// 92
		case D3DRENDERSTATE_STIPPLEPATTERN29:	// 93
		case D3DRENDERSTATE_STIPPLEPATTERN30:	// 94
		case D3DRENDERSTATE_STIPPLEPATTERN31:	// 95
			*lpdwRenderState = 0;
			return D3D_OK;
		case D3DRENDERSTATE_EXTENTS:			// 138
			// ToDo: use this to report on clip plane extents set by SetClipStatus()
			*lpdwRenderState = FALSE;
			return D3D_OK;
		case D3DRENDERSTATE_COLORKEYBLENDENABLE:// 144
			*lpdwRenderState = FALSE;
			return D3D_OK;
		}

		if (!CheckRenderStateType(dwRenderStateType))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Render state type not implemented: " << dwRenderStateType);
			*lpdwRenderState = 0;
			return D3D_OK;	// Just return OK for now!
		}

		return (*d3d9Device)->GetRenderState(dwRenderStateType, lpdwRenderState);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->GetRenderState(dwRenderStateType, lpdwRenderState);
	case 3:
		return GetProxyInterfaceV3()->GetRenderState(dwRenderStateType, lpdwRenderState);
	case 7:
		return GetProxyInterfaceV7()->GetRenderState(dwRenderStateType, lpdwRenderState);
	}
}

HRESULT m_IDirect3DDeviceX::BeginStateBlock()
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		if (IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		HRESULT hr = (*d3d9Device)->BeginStateBlock();

		if (SUCCEEDED(hr))
		{
			IsRecordingState = true;
		}

		return hr;
	}

	return GetProxyInterfaceV7()->BeginStateBlock();
}

HRESULT m_IDirect3DDeviceX::EndStateBlock(LPDWORD lpdwBlockHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpdwBlockHandle)
		{
			return DDERR_INVALIDPARAMS;
		}
		*lpdwBlockHandle = NULL;

		if (!IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		HRESULT hr = (*d3d9Device)->EndStateBlock(reinterpret_cast<IDirect3DStateBlock9**>(lpdwBlockHandle));

		if (SUCCEEDED(hr))
		{
			IsRecordingState = false;
			StateBlockTokens.insert(*lpdwBlockHandle);
		}

		return hr;
	}

	return GetProxyInterfaceV7()->EndStateBlock(lpdwBlockHandle);
}

HRESULT m_IDirect3DDeviceX::DrawPrimitive(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpVertices, DWORD dwVertexCount, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")" <<
		" VertexType = " << Logging::hex(dptPrimitiveType) <<
		" VertexDesc = " << Logging::hex(dwVertexTypeDesc) <<
		" Vertices = " << lpVertices <<
		" VertexCount = " << dwVertexCount <<
		" Flags = " << Logging::hex(dwFlags) <<
		" Version = " << DirectXVersion;

	if (DirectXVersion == 2 && ProxyDirectXVersion > 2)
	{
		if (dwVertexTypeDesc != D3DVT_VERTEX && dwVertexTypeDesc != D3DVT_LVERTEX && dwVertexTypeDesc != D3DVT_TLVERTEX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid Vertex type: " << dwVertexTypeDesc);
			return D3DERR_INVALIDVERTEXTYPE;
		}
		dwVertexTypeDesc = ConvertVertexTypeToFVF((D3DVERTEXTYPE)dwVertexTypeDesc);
	}

	if (Config.Dd7to9)
	{
		if (!lpVertices)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

#ifdef ENABLE_PROFILING
		auto startTime = std::chrono::high_resolution_clock::now();
#endif

		dwFlags = (dwFlags & D3DDP_FORCE_DWORD);

		// Update vertices for Direct3D9 (needs to be first)
		UpdateVertices(dwVertexTypeDesc, lpVertices, dwVertexCount);

		// Set fixed function vertex type
		if (FAILED((*d3d9Device)->SetFVF(dwVertexTypeDesc)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid FVF type: " << Logging::hex(dwVertexTypeDesc));
			return DDERR_INVALIDPARAMS;
		}

		// Handle dwFlags
		SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

		// Draw primitive UP
		HRESULT hr = (*d3d9Device)->DrawPrimitiveUP(dptPrimitiveType, GetNumberOfPrimitives(dptPrimitiveType, dwVertexCount), lpVertices, GetVertexStride(dwVertexTypeDesc));

		// Handle dwFlags
		RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

		if (FAILED(hr))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: 'DrawPrimitiveUP' call failed: " << (D3DERR)hr);
		}

#ifdef ENABLE_PROFILING
		Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr << " Timing = " << Logging::GetTimeLapseInMS(startTime);
#endif

		return hr;
	}

	if (Config.DdrawUseNativeResolution)
	{
		ScaleVertices(dwVertexTypeDesc, lpVertices, dwVertexCount);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->DrawPrimitive(dptPrimitiveType, (D3DVERTEXTYPE)dwVertexTypeDesc, lpVertices, dwVertexCount, dwFlags);
	case 3:
		return GetProxyInterfaceV3()->DrawPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			// Handle dwFlags
			SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, Flags);

			// Handle dwFlags
			RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::DrawPrimitiveStrided(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->DrawPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			// Handle dwFlags
			SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, Flags);

			// Handle dwFlags
			RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::DrawPrimitiveVB(D3DPRIMITIVETYPE dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")" <<
		" VertexType = " << Logging::hex(dptPrimitiveType) <<
		" VertexBuffer = " << lpd3dVertexBuffer <<
		" StartVertex = " << dwStartVertex <<
		" NumVertices = " << dwNumVertices <<
		" Flags = " << Logging::hex(dwFlags) <<
		" Version = " << DirectXVersion;

	if (Config.Dd7to9)
	{
		if (!lpd3dVertexBuffer)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

#ifdef ENABLE_PROFILING
		auto startTime = std::chrono::high_resolution_clock::now();
#endif

		dwFlags = (dwFlags & D3DDP_FORCE_DWORD);

		// ToDo: Validate vertex buffer
		m_IDirect3DVertexBufferX* pVertexBufferX = nullptr;
		lpd3dVertexBuffer->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pVertexBufferX);
		if (!pVertexBufferX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get vertex buffer wrapper!");
			return DDERR_GENERIC;
		}

		LPDIRECT3DVERTEXBUFFER9 d3d9VertexBuffer = pVertexBufferX->GetCurrentD9VertexBuffer();
		if (!d3d9VertexBuffer)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get d3d9 vertex buffer!");
			return DDERR_GENERIC;
		}

		DWORD FVF = pVertexBufferX->GetFVF9();

		// Set fixed function vertex type
		if (FAILED((*d3d9Device)->SetFVF(FVF)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid FVF type: " << Logging::hex(FVF));
			return DDERR_INVALIDPARAMS;
		}

		// Set stream source
		(*d3d9Device)->SetStreamSource(0, d3d9VertexBuffer, 0, GetVertexStride(FVF));

		// Handle dwFlags
		SetDrawStates(FVF, dwFlags, DirectXVersion);

		// Draw primitive
		HRESULT hr = (*d3d9Device)->DrawPrimitive(dptPrimitiveType, dwStartVertex, GetNumberOfPrimitives(dptPrimitiveType, dwNumVertices));

		// Handle dwFlags
		RestoreDrawStates(FVF, dwFlags, DirectXVersion);

		if (FAILED(hr))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: 'DrawPrimitive' call failed: " << (D3DERR)hr);
		}

#ifdef ENABLE_PROFILING
		Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr << " Timing = " << Logging::GetTimeLapseInMS(startTime);
#endif

		return hr;
	}

	if (lpd3dVertexBuffer)
	{
		lpd3dVertexBuffer->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpd3dVertexBuffer);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->DrawPrimitiveVB(dptPrimitiveType, (LPDIRECT3DVERTEXBUFFER)lpd3dVertexBuffer, dwStartVertex, dwNumVertices, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			D3DVERTEXBUFFERDESC BufferDesc = {};
			if (lpd3dVertexBuffer)
			{
				lpd3dVertexBuffer->GetVertexBufferDesc(&BufferDesc);
			}

			// Handle dwFlags
			SetDrawStates(BufferDesc.dwFVF, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawPrimitiveVB(dptPrimitiveType, lpd3dVertexBuffer, dwStartVertex, dwNumVertices, Flags);

			// Handle dwFlags
			RestoreDrawStates(BufferDesc.dwFVF, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawPrimitiveVB(dptPrimitiveType, lpd3dVertexBuffer, dwStartVertex, dwNumVertices, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::DrawIndexedPrimitive(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPVOID lpVertices, DWORD dwVertexCount, LPWORD lpIndices, DWORD dwIndexCount, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")" <<
		" VertexType = " << Logging::hex(dptPrimitiveType) <<
		" VertexDesc = " << Logging::hex(dwVertexTypeDesc) <<
		" Vertices = " << lpVertices <<
		" VertexCount = " << dwVertexCount <<
		" Indices = " << lpIndices <<
		" IndexCount = " << dwIndexCount <<
		" Flags = " << Logging::hex(dwFlags) <<
		" Version = " << DirectXVersion;

	if (DirectXVersion == 2 && ProxyDirectXVersion > 2)
	{
		if (dwVertexTypeDesc != D3DVT_VERTEX && dwVertexTypeDesc != D3DVT_LVERTEX && dwVertexTypeDesc != D3DVT_TLVERTEX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid Vertex type: " << dwVertexTypeDesc);
			return D3DERR_INVALIDVERTEXTYPE;
		}
		dwVertexTypeDesc = ConvertVertexTypeToFVF((D3DVERTEXTYPE)dwVertexTypeDesc);
	}

	if (Config.Dd7to9)
	{
		if (!lpVertices || !lpIndices)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

#ifdef ENABLE_PROFILING
		auto startTime = std::chrono::high_resolution_clock::now();
#endif

		dwFlags = (dwFlags & D3DDP_FORCE_DWORD);

		// Update vertices for Direct3D9 (needs to be first)
		UpdateVertices(dwVertexTypeDesc, lpVertices, dwVertexCount);

		// Set fixed function vertex type
		if (FAILED((*d3d9Device)->SetFVF(dwVertexTypeDesc)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid FVF type: " << Logging::hex(dwVertexTypeDesc));
			return DDERR_INVALIDPARAMS;
		}

		// Handle dwFlags
		SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

		// Draw indexed primitive UP
		HRESULT hr = (*d3d9Device)->DrawIndexedPrimitiveUP(dptPrimitiveType, 0, dwVertexCount, GetNumberOfPrimitives(dptPrimitiveType, dwIndexCount), lpIndices, D3DFMT_INDEX16, lpVertices, GetVertexStride(dwVertexTypeDesc));

		// Handle dwFlags
		RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

		if (FAILED(hr))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: 'DrawIndexedPrimitiveUP' call failed: " << (D3DERR)hr);
		}

#ifdef ENABLE_PROFILING
		Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr << " Timing = " << Logging::GetTimeLapseInMS(startTime);
#endif

		return hr;
	}

	if (Config.DdrawUseNativeResolution)
	{
		ScaleVertices(dwVertexTypeDesc, lpVertices, dwVertexCount);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->DrawIndexedPrimitive(dptPrimitiveType, (D3DVERTEXTYPE)dwVertexTypeDesc, lpVertices, dwVertexCount, lpIndices, dwIndexCount, dwFlags);
	case 3:
		return GetProxyInterfaceV3()->DrawIndexedPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, lpIndices, dwIndexCount, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			// Handle dwFlags
			SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawIndexedPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, lpIndices, dwIndexCount, Flags);

			// Handle dwFlags
			RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawIndexedPrimitive(dptPrimitiveType, dwVertexTypeDesc, lpVertices, dwVertexCount, lpIndices, dwIndexCount, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::DrawIndexedPrimitiveStrided(D3DPRIMITIVETYPE dptPrimitiveType, DWORD dwVertexTypeDesc, LPD3DDRAWPRIMITIVESTRIDEDDATA lpVertexArray, DWORD dwVertexCount, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		LOG_LIMIT(100, __FUNCTION__ << " Error: Not Implemented");
		return DDERR_UNSUPPORTED;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->DrawIndexedPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, lpwIndices, dwIndexCount, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			// Handle dwFlags
			SetDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawIndexedPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, lpwIndices, dwIndexCount, Flags);

			// Handle dwFlags
			RestoreDrawStates(dwVertexTypeDesc, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawIndexedPrimitiveStrided(dptPrimitiveType, dwVertexTypeDesc, lpVertexArray, dwVertexCount, lpwIndices, dwIndexCount, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::DrawIndexedPrimitiveVB(D3DPRIMITIVETYPE dptPrimitiveType, LPDIRECT3DVERTEXBUFFER7 lpd3dVertexBuffer, DWORD dwStartVertex, DWORD dwNumVertices, LPWORD lpwIndices, DWORD dwIndexCount, DWORD dwFlags, DWORD DirectXVersion)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")" <<
		" VertexType = " << Logging::hex(dptPrimitiveType) <<
		" VertexBuffer = " << lpd3dVertexBuffer <<
		" StartVertex = " << dwStartVertex <<
		" NumVertices = " << dwNumVertices <<
		" Indices = " << lpwIndices <<
		" IndexCount = " << dwIndexCount <<
		" Flags = " << Logging::hex(dwFlags) <<
		" Version = " << DirectXVersion;

	if (Config.Dd7to9)
	{
		if (!lpd3dVertexBuffer || !lpwIndices)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

#ifdef ENABLE_PROFILING
		auto startTime = std::chrono::high_resolution_clock::now();
#endif

		dwFlags = (dwFlags & D3DDP_FORCE_DWORD);

		// ToDo: Validate vertex buffer
		m_IDirect3DVertexBufferX* pVertexBufferX = nullptr;
		lpd3dVertexBuffer->QueryInterface(IID_GetInterfaceX, (LPVOID*)&pVertexBufferX);
		if (!pVertexBufferX)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get vertex buffer wrapper!");
			return DDERR_INVALIDPARAMS;
		}

		LPDIRECT3DVERTEXBUFFER9 d3d9VertexBuffer = pVertexBufferX->GetCurrentD9VertexBuffer();
		if (!d3d9VertexBuffer)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get d3d9 vertex buffer!");
			return DDERR_GENERIC;
		}

		DWORD FVF = pVertexBufferX->GetFVF9();

		// Set fixed function vertex type
		if (FAILED((*d3d9Device)->SetFVF(FVF)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: invalid FVF type: " << Logging::hex(FVF));
			return DDERR_INVALIDPARAMS;
		}

		// No operation to performed
		if (dwIndexCount == 0)
		{
			return D3D_OK;
		}

		LPDIRECT3DINDEXBUFFER9 d3d9IndexBuffer = ddrawParent->GetIndexBuffer(lpwIndices, dwIndexCount);
		if (!d3d9IndexBuffer)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: could not get d3d9 index buffer!");
			return DDERR_GENERIC;
		}

		// Set stream source
		(*d3d9Device)->SetStreamSource(0, d3d9VertexBuffer, 0, GetVertexStride(FVF));

		// Set Index data
		(*d3d9Device)->SetIndices(d3d9IndexBuffer);

		// Handle dwFlags
		SetDrawStates(FVF, dwFlags, DirectXVersion);

		// Draw primitive
		HRESULT hr = (*d3d9Device)->DrawIndexedPrimitive(dptPrimitiveType, dwStartVertex, 0, dwNumVertices, 0, GetNumberOfPrimitives(dptPrimitiveType, dwIndexCount));

		// Handle dwFlags
		RestoreDrawStates(FVF, dwFlags, DirectXVersion);

		if (FAILED(hr))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: 'DrawIndexedPrimitive' call failed: " << (D3DERR)hr);
		}

#ifdef ENABLE_PROFILING
		Logging::Log() << __FUNCTION__ << " (" << this << ") hr = " << (D3DERR)hr << " Timing = " << Logging::GetTimeLapseInMS(startTime);
#endif

		return hr;
	}

	if (lpd3dVertexBuffer)
	{
		lpd3dVertexBuffer->QueryInterface(IID_GetRealInterface, (LPVOID*)&lpd3dVertexBuffer);
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->DrawIndexedPrimitiveVB(dptPrimitiveType, (LPDIRECT3DVERTEXBUFFER)lpd3dVertexBuffer, lpwIndices, dwIndexCount, dwFlags);
	case 7:
		if (DirectXVersion != 7)
		{
			D3DVERTEXBUFFERDESC BufferDesc = {};
			if (lpd3dVertexBuffer)
			{
				lpd3dVertexBuffer->GetVertexBufferDesc(&BufferDesc);
			}

			// Handle dwFlags
			SetDrawStates(BufferDesc.dwFVF, dwFlags, DirectXVersion);

			DWORD Flags = dwFlags & ~(D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS);
			HRESULT hr = GetProxyInterfaceV7()->DrawIndexedPrimitiveVB(dptPrimitiveType, lpd3dVertexBuffer, dwStartVertex, dwNumVertices, lpwIndices, dwIndexCount, Flags);

			// Handle dwFlags
			RestoreDrawStates(BufferDesc.dwFVF, dwFlags, DirectXVersion);

			return hr;
		}
		else
		{
			return GetProxyInterfaceV7()->DrawIndexedPrimitiveVB(dptPrimitiveType, lpd3dVertexBuffer, dwStartVertex, dwNumVertices, lpwIndices, dwIndexCount, dwFlags);
		}
	}
}

HRESULT m_IDirect3DDeviceX::ComputeSphereVisibility(LPD3DVECTOR lpCenters, LPD3DVALUE lpRadii, DWORD dwNumSpheres, DWORD dwFlags, LPDWORD lpdwReturnValues)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpCenters || !lpRadii || !dwNumSpheres || !lpdwReturnValues)
		{
			return DDERR_INVALIDPARAMS;
		}

		LOG_LIMIT(100, __FUNCTION__ << " Warning: function not fully implemented");

		// Sphere visibility is computed by back-transforming the viewing frustum to the model space, using the inverse of the combined world, view, or projection matrices.
		// If the combined matrix can't be inverted (if the determinant is 0), the method will fail, returning D3DERR_INVALIDMATRIX.
		for (UINT x = 0; x < dwNumSpheres; x++)
		{
			// If a sphere is completely visible, the corresponding entry in lpdwReturnValues is 0.
			lpdwReturnValues[x] = 0;	// Just return all is visible for now
		}

		return D3D_OK;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->ComputeSphereVisibility(lpCenters, lpRadii, dwNumSpheres, dwFlags, lpdwReturnValues);
	case 7:
		return GetProxyInterfaceV7()->ComputeSphereVisibility(lpCenters, lpRadii, dwNumSpheres, dwFlags, lpdwReturnValues);
	}
}

HRESULT m_IDirect3DDeviceX::ValidateDevice(LPDWORD lpdwPasses)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpdwPasses)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		DWORD FVF, Size;
		IDirect3DVertexBuffer9* vertexBuffer = ddrawParent->GetValidateDeviceVertexBuffer(FVF, Size);

		if (!vertexBuffer)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: Failed to get vertex buffer!");
			return DDERR_GENERIC;
		}

		// Bind the vertex buffer to the device
		(*d3d9Device)->SetStreamSource(0, vertexBuffer, 0, Size);

		// Set a simple FVF (Flexible Vertex Format)
		(*d3d9Device)->SetFVF(FVF);

		// Call ValidateDevice
		HRESULT hr = (*d3d9Device)->ValidateDevice(lpdwPasses);

		if (FAILED(hr))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: ValidateDevice() function failed: " << (DDERR)hr);
		}

		return hr;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	case 2:
	default:
		return DDERR_GENERIC;
	case 3:
		return GetProxyInterfaceV3()->ValidateDevice(lpdwPasses);
	case 7:
		return GetProxyInterfaceV7()->ValidateDevice(lpdwPasses);
	}
}

HRESULT m_IDirect3DDeviceX::ApplyStateBlock(DWORD dwBlockHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!dwBlockHandle || StateBlockTokens.find(dwBlockHandle) == StateBlockTokens.end())
		{
			return DDERR_INVALIDPARAMS;
		}

		if (IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		return reinterpret_cast<IDirect3DStateBlock9*>(dwBlockHandle)->Apply();
	}

	return GetProxyInterfaceV7()->ApplyStateBlock(dwBlockHandle);
}

HRESULT m_IDirect3DDeviceX::CaptureStateBlock(DWORD dwBlockHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!dwBlockHandle || StateBlockTokens.find(dwBlockHandle) == StateBlockTokens.end())
		{
			return DDERR_INVALIDPARAMS;
		}

		if (IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		return reinterpret_cast<IDirect3DStateBlock9*>(dwBlockHandle)->Capture();
	}

	return GetProxyInterfaceV7()->CaptureStateBlock(dwBlockHandle);
}

HRESULT m_IDirect3DDeviceX::DeleteStateBlock(DWORD dwBlockHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!dwBlockHandle || StateBlockTokens.find(dwBlockHandle) == StateBlockTokens.end())
		{
			return DDERR_INVALIDPARAMS;
		}

		if (IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		reinterpret_cast<IDirect3DStateBlock9*>(dwBlockHandle)->Release();

		StateBlockTokens.erase(dwBlockHandle);

		return D3D_OK;
	}

	return GetProxyInterfaceV7()->DeleteStateBlock(dwBlockHandle);
}

HRESULT m_IDirect3DDeviceX::CreateStateBlock(D3DSTATEBLOCKTYPE d3dsbtype, LPDWORD lpdwBlockHandle)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpdwBlockHandle)
		{
			return DDERR_INVALIDPARAMS;
		}
		*lpdwBlockHandle = NULL;

		if (IsRecordingState)
		{
			return DDERR_GENERIC;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		HRESULT hr = (*d3d9Device)->CreateStateBlock(d3dsbtype, reinterpret_cast<IDirect3DStateBlock9**>(lpdwBlockHandle));

		if (SUCCEEDED(hr))
		{
			StateBlockTokens.insert(*lpdwBlockHandle);
		}

		return hr;
	}

	return GetProxyInterfaceV7()->CreateStateBlock(d3dsbtype, lpdwBlockHandle);
}

HRESULT m_IDirect3DDeviceX::SetClipStatus(LPD3DCLIPSTATUS lpD3DClipStatus)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// D3DCLIPSTATUS_EXTENTS2 flag cannot be combined with D3DCLIPSTATUS_EXTENTS3.
		if (!lpD3DClipStatus || (lpD3DClipStatus->dwFlags & (D3DCLIPSTATUS_EXTENTS2 | D3DCLIPSTATUS_EXTENTS3)) == (D3DCLIPSTATUS_EXTENTS2 | D3DCLIPSTATUS_EXTENTS3))
		{
			return DDERR_INVALIDPARAMS;
		}

		// D3DCLIPSTATUS_EXTENTS3 is Not currently implemented in DirectDraw.
		if (lpD3DClipStatus->dwFlags & D3DCLIPSTATUS_EXTENTS3)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: only clip status flag is supported. Using unsupported dwFlags combination: " << Logging::hex(lpD3DClipStatus->dwFlags));
			return DDERR_INVALIDPARAMS;
		}
		else if (lpD3DClipStatus->dwFlags & D3DCLIPSTATUS_EXTENTS2)
		{
			LOG_LIMIT(100, __FUNCTION__ << " Warning: Extents 2D flag Not Implemented: " << *lpD3DClipStatus);
			D3DClipStatus = *lpD3DClipStatus;
		}

		// For now just save clip status
		if (lpD3DClipStatus->dwFlags & D3DCLIPSTATUS_STATUS)
		{
			D3DClipStatus = *lpD3DClipStatus;
			D3DClipStatus.dwFlags = D3DCLIPSTATUS_STATUS;
			D3DClipStatus.dwStatus = 0;
			return D3D_OK;
		}

		// ToDo: set clip status from dwStatus

		// ToDo: check the D3DRENDERSTATE_EXTENTS flag and use that to enable or disable extents clip planes
		// The default setting for this state, FALSE, disables extent updates. Applications that intend to use the
		// IDirect3DDevice7::GetClipStatus and IDirect3DDevice7::SetClipStatus methods to manipulate the viewport extents
		// can enable extents updates by setting D3DRENDERSTATE_EXTENTS to TRUE.

		// Check for device interface
		/*if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		// Calculate the center of the bounding box
		float centerX = (lpD3DClipStatus->minx + lpD3DClipStatus->maxx) * 0.5f;
		float centerY = (lpD3DClipStatus->miny + lpD3DClipStatus->maxy) * 0.5f;
		float centerZ = (lpD3DClipStatus->minz + lpD3DClipStatus->maxz) * 0.5f;

		// Calculate the extents (half-width, half-height, and half-depth) of the bounding box
		float halfWidth = (lpD3DClipStatus->maxx - lpD3DClipStatus->minx) * 0.5f;
		float halfHeight = (lpD3DClipStatus->maxy - lpD3DClipStatus->miny) * 0.5f;
		float halfDepth = (lpD3DClipStatus->maxz - lpD3DClipStatus->minz) * 0.5f;

		// Calculate the front clipping plane coefficients
		float frontNormalX = -1.0f; // Clipping towards the negative X direction
		float frontNormalY = 0.0f;
		float frontNormalZ = 0.0f;
		float frontDistance = centerX - halfWidth;

		float frontClipPlane[4] = { frontNormalX, frontNormalY, frontNormalZ, frontDistance };

		// Calculate the back clipping plane coefficients
		float backNormalX = 1.0f; // Clipping towards the positive X direction
		float backNormalY = 0.0f;
		float backNormalZ = 0.0f;
		float backDistance = -(centerX + halfWidth);

		float backClipPlane[4] = { backNormalX, backNormalY, backNormalZ, backDistance };

		// Calculate the top clipping plane coefficients
		float topNormalX = 0.0f;
		float topNormalY = 1.0f; // Clipping towards the positive Y direction
		float topNormalZ = 0.0f;
		float topDistance = -(centerY + halfHeight);

		float topClipPlane[4] = { topNormalX, topNormalY, topNormalZ, topDistance };

		// Calculate the bottom clipping plane coefficients
		float bottomNormalX = 0.0f;
		float bottomNormalY = -1.0f; // Clipping towards the negative Y direction
		float bottomNormalZ = 0.0f;
		float bottomDistance = centerY - halfHeight;

		float bottomClipPlane[4] = { bottomNormalX, bottomNormalY, bottomNormalZ, bottomDistance };

		// Calculate the near clipping plane coefficients
		float nearNormalX = 0.0f;
		float nearNormalY = 0.0f;
		float nearNormalZ = -1.0f; // Clipping towards the negative Z direction
		float nearDistance = centerZ - halfDepth;

		float nearClipPlane[4] = { nearNormalX, nearNormalY, nearNormalZ, nearDistance };

		// Calculate the far clipping plane coefficients
		float farNormalX = 0.0f;
		float farNormalY = 0.0f;
		float farNormalZ = 1.0f; // Clipping towards the positive Z direction
		float farDistance = -(centerZ + halfDepth);

		float farClipPlane[4] = { farNormalX, farNormalY, farNormalZ, farDistance };

		// Set the clip planes
		int x = 0;
		for (auto clipPlane : { frontClipPlane, backClipPlane, topClipPlane, bottomClipPlane, nearClipPlane, farClipPlane })
		{
			HRESULT hr = (*d3d9Device)->SetClipPlane(x, clipPlane);
			if (FAILED(hr))
			{
				LOG_LIMIT(100, __FUNCTION__ << " Error: 'SetClipPlane' call failed: " << (D3DERR)hr <<
					" call: { " << clipPlane[0] << ", " << clipPlane[1] << ", " << clipPlane[2] << ", " << clipPlane[3] << "}");
				return hr;
			}
			x++;
		}

		// To enable a clipping plane, set the corresponding bit in the DWORD value applied to the D3DRS_CLIPPLANEENABLE render state.
		(*d3d9Device)->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0 | D3DCLIPPLANE1 | D3DCLIPPLANE2 | D3DCLIPPLANE3 | D3DCLIPPLANE4 | D3DCLIPPLANE5);*/

		return D3D_OK;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->SetClipStatus(lpD3DClipStatus);
	case 3:
		return GetProxyInterfaceV3()->SetClipStatus(lpD3DClipStatus);
	case 7:
		return GetProxyInterfaceV7()->SetClipStatus(lpD3DClipStatus);
	}
}

HRESULT m_IDirect3DDeviceX::GetClipStatus(LPD3DCLIPSTATUS lpD3DClipStatus)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!lpD3DClipStatus)
		{
			return DDERR_INVALIDPARAMS;
		}

		// ToDo: get clip status from Direct3D9
		*lpD3DClipStatus = D3DClipStatus;

		return D3D_OK;
	}

	switch (ProxyDirectXVersion)
	{
	case 1:
	default:
		return DDERR_GENERIC;
	case 2:
		return GetProxyInterfaceV2()->GetClipStatus(lpD3DClipStatus);
	case 3:
		return GetProxyInterfaceV3()->GetClipStatus(lpD3DClipStatus);
	case 7:
		return GetProxyInterfaceV7()->GetClipStatus(lpD3DClipStatus);
	}
}

HRESULT m_IDirect3DDeviceX::SetClipPlane(DWORD dwIndex, D3DVALUE* pPlaneEquation)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->SetClipPlane(dwIndex, pPlaneEquation);
	}

	return GetProxyInterfaceV7()->SetClipPlane(dwIndex, pPlaneEquation);
}

HRESULT m_IDirect3DDeviceX::GetClipPlane(DWORD dwIndex, D3DVALUE* pPlaneEquation)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!pPlaneEquation)
		{
			return DDERR_INVALIDPARAMS;
		}

		// Check for device interface
		if (FAILED(CheckInterface(__FUNCTION__, true)))
		{
			return DDERR_INVALIDOBJECT;
		}

		return (*d3d9Device)->SetClipPlane(dwIndex, pPlaneEquation);
	}

	return GetProxyInterfaceV7()->GetClipPlane(dwIndex, pPlaneEquation);
}

HRESULT m_IDirect3DDeviceX::GetInfo(DWORD dwDevInfoID, LPVOID pDevInfoStruct, DWORD dwSize)
{
	Logging::LogDebug() << __FUNCTION__ << " (" << this << ")";

	if (Config.Dd7to9)
	{
		if (!pDevInfoStruct || dwSize == 0)
		{
			return DDERR_GENERIC;
		}

#ifdef _DEBUG
		// Fill device info structures
		switch (dwDevInfoID)
		{
		case D3DDEVINFOID_TEXTUREMANAGER:
		case D3DDEVINFOID_D3DTEXTUREMANAGER:
			if (dwSize == sizeof(D3DDEVINFO_TEXTUREMANAGER))
			{
				// Simulate a default texture manager structure for a good video card
				D3DDEVINFO_TEXTUREMANAGER* pTexManagerInfo = (D3DDEVINFO_TEXTUREMANAGER*)pDevInfoStruct;
				pTexManagerInfo->bThrashing = FALSE;
				pTexManagerInfo->dwNumEvicts = 0;
				pTexManagerInfo->dwNumVidCreates = 0;
				pTexManagerInfo->dwNumTexturesUsed = 0;
				pTexManagerInfo->dwNumUsedTexInVid = 0;
				pTexManagerInfo->dwWorkingSet = 0;
				pTexManagerInfo->dwWorkingSetBytes = 0;
				pTexManagerInfo->dwTotalManaged = 0;
				pTexManagerInfo->dwTotalBytes = 0;
				pTexManagerInfo->dwLastPri = 0;
				break;
			}
			return DDERR_GENERIC;

		case D3DDEVINFOID_TEXTURING:
			if (dwSize == sizeof(D3DDEVINFO_TEXTURING))
			{
				// Simulate a default texturing activity structure for a good video card
				D3DDEVINFO_TEXTURING* pTexturingInfo = (D3DDEVINFO_TEXTURING*)pDevInfoStruct;
				pTexturingInfo->dwNumLoads = 0;
				pTexturingInfo->dwApproxBytesLoaded = 0;
				pTexturingInfo->dwNumPreLoads = 0;
				pTexturingInfo->dwNumSet = 0;
				pTexturingInfo->dwNumCreates = 0;
				pTexturingInfo->dwNumDestroys = 0;
				pTexturingInfo->dwNumSetPriorities = 0;
				pTexturingInfo->dwNumSetLODs = 0;
				pTexturingInfo->dwNumLocks = 0;
				pTexturingInfo->dwNumGetDCs = 0;
				break;
			}
			return DDERR_GENERIC;

		default:
			Logging::LogDebug() << __FUNCTION__ << " Error: Unknown DevInfoID: " << dwDevInfoID;
			return DDERR_GENERIC;
		}
#endif

		// This method is intended to be used for performance tracking and debugging during product development (on the debug version of DirectX). 
		// The method can succeed, returning S_FALSE, without retrieving device data.
		// This occurs when the retail version of the DirectX runtime is installed on the host system.
		return S_FALSE;
	}

	return GetProxyInterfaceV7()->GetInfo(dwDevInfoID, pDevInfoStruct, dwSize);
}

/************************/
/*** Helper functions ***/
/************************/

void m_IDirect3DDeviceX::InitInterface(DWORD DirectXVersion)
{
	if (!Config.Dd7to9)
	{
		return;
	}

	if (ddrawParent)
	{
		d3d9Device = ddrawParent->GetDirectD9Device();
		ddrawParent->SetD3DDevice(this);

		if (CurrentRenderTarget)
		{
			m_IDirectDrawSurfaceX* lpDDSrcSurfaceX = nullptr;

			CurrentRenderTarget->QueryInterface(IID_GetInterfaceX, (LPVOID*)&lpDDSrcSurfaceX);
			if (lpDDSrcSurfaceX)
			{
				lpCurrentRenderTargetX = lpDDSrcSurfaceX;
				ddrawParent->SetRenderTargetSurface(lpCurrentRenderTargetX);
			}
		}
	}

	AddRef(DirectXVersion);
}

void m_IDirect3DDeviceX::ReleaseInterface()
{
	// Don't delete wrapper interface
	SaveInterfaceAddress(WrapperInterface, WrapperInterfaceBackup);
	SaveInterfaceAddress(WrapperInterface2, WrapperInterfaceBackup2);
	SaveInterfaceAddress(WrapperInterface3, WrapperInterfaceBackup3);
	SaveInterfaceAddress(WrapperInterface7, WrapperInterfaceBackup7);

	// Release ExecuteBuffers
	std::vector<m_IDirect3DExecuteBuffer*> NewExecuteBufferList;
	NewExecuteBufferList = std::move(ExecuteBufferList);
	for (auto& entry : NewExecuteBufferList)
	{
		if (entry->Release())
		{
			entry->DeleteMe();
		}
	}

	if (ddrawParent && !Config.Exiting)
	{
		ReleaseAllStateBlocks();
		ddrawParent->ClearD3DDevice();
	}
}

HRESULT m_IDirect3DDeviceX::CheckInterface(char *FunctionName, bool CheckD3DDevice)
{
	// Check ddrawParent device
	if (!ddrawParent)
	{
		LOG_LIMIT(100, FunctionName << " Error: no ddraw parent!");
		return DDERR_INVALIDOBJECT;
	}

	// Check d3d9 device
	if (CheckD3DDevice)
	{
		if (!ddrawParent->CheckD9Device(FunctionName) || !d3d9Device || !*d3d9Device)
		{
			LOG_LIMIT(100, FunctionName << " Error: d3d9 device not setup!");
			return DDERR_INVALIDOBJECT;
		}
		if (bSetDefaults)
		{
			SetDefaults();
		}
	}

	return D3D_OK;
}

HRESULT m_IDirect3DDeviceX::BackupStates()
{
	if (!d3d9Device || !*d3d9Device)
	{
		Logging::Log() << __FUNCTION__ " Error: Failed to get the device state!";
		return DDERR_GENERIC;
	}

	// Backup render states
	for (UINT x = 0; x < 255; x++)
	{
		(*d3d9Device)->GetRenderState((D3DRENDERSTATETYPE)x, &backup.RenderState[x]);
	}

	// Backup texture states
	for (UINT y = 0; y < MaxTextureStages; y++)
	{
		for (UINT x = 0; x < 255; x++)
		{
			(*d3d9Device)->GetTextureStageState(y, (D3DTEXTURESTAGESTATETYPE)x, &backup.TextureState[y][x]);
		}
	}

	// Backup sampler states
	for (UINT y = 0; y < MaxTextureStages; y++)
	{
		for (UINT x = 0; x < 14; x++)
		{
			(*d3d9Device)->GetSamplerState(y, (D3DSAMPLERSTATETYPE)x, &backup.SamplerState[y][x]);
		}
	}

	// Backup lights
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*d3d9Device)->GetLight(i, &backup.lights[i]);
		(*d3d9Device)->GetLightEnable(i, &backup.lightEnabled[i]);
	}

	// Backup material
	(*d3d9Device)->GetMaterial(&backup.material);

	// Backup transform
	(*d3d9Device)->GetTransform(D3DTS_WORLD, &backup.worldMatrix);
	(*d3d9Device)->GetTransform(D3DTS_VIEW, &backup.viewMatrix);
	(*d3d9Device)->GetTransform(D3DTS_PROJECTION, &backup.projectionMatrix);

	// Backup viewport
	(*d3d9Device)->GetViewport(&backup.viewport);

	backup.IsBackedUp = true;

	return D3D_OK;
}

HRESULT m_IDirect3DDeviceX::RestoreStates()
{
	if (!d3d9Device || !*d3d9Device)
	{
		Logging::Log() << __FUNCTION__ " Error: Failed to restore the device state!";
		return DDERR_GENERIC;
	}

	if (!backup.IsBackedUp)
	{
		return D3D_OK;
	}

	// Restore render states
	for (UINT x = 0; x < 255; x++)
	{
		(*d3d9Device)->SetRenderState((D3DRENDERSTATETYPE)x, backup.RenderState[x]);
	}

	// Restore texture states
	for (UINT y = 0; y < MaxTextureStages; y++)
	{
		for (UINT x = 0; x < 255; x++)
		{
			(*d3d9Device)->SetTextureStageState(y, (D3DTEXTURESTAGESTATETYPE)x, backup.TextureState[y][x]);
		}
	}

	// Restore lights
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		(*d3d9Device)->SetLight(i, &backup.lights[i]);
		(*d3d9Device)->LightEnable(i, backup.lightEnabled[i]);
	}

	// Restore material
	(*d3d9Device)->SetMaterial(&backup.material);

	// Restore transform
	(*d3d9Device)->SetTransform(D3DTS_WORLD, &backup.worldMatrix);
	(*d3d9Device)->SetTransform(D3DTS_VIEW, &backup.viewMatrix);
	(*d3d9Device)->SetTransform(D3DTS_PROJECTION, &backup.projectionMatrix);

	// Restore sampler states
	for (UINT y = 0; y < MaxTextureStages; y++)
	{
		for (UINT x = 0; x < 14; x++)
		{
			(*d3d9Device)->SetSamplerState(y, (D3DSAMPLERSTATETYPE)x, backup.SamplerState[y][x]);
		}
	}

	// Restore viewport
	D3DVIEWPORT9 viewport = {};
	(*d3d9Device)->GetViewport(&viewport);
	backup.viewport.Width = viewport.Width;
	backup.viewport.Height = viewport.Height;
	(*d3d9Device)->SetViewport(&backup.viewport);

	backup.IsBackedUp = false;

	return D3D_OK;
}

void m_IDirect3DDeviceX::BeforeResetDevice()
{
	BackupStates();
	if (IsRecordingState)
	{
		DWORD dwBlockHandle = NULL;
		if (SUCCEEDED(EndStateBlock(&dwBlockHandle)))
		{
			DeleteStateBlock(dwBlockHandle);
		}
	}
}

void m_IDirect3DDeviceX::AfterResetDevice()
{
	RestoreStates();
}

void m_IDirect3DDeviceX::ClearDdraw()
{
	ReleaseAllStateBlocks();
	ddrawParent = nullptr;
	colorkeyPixelShader = nullptr;
	d3d9Device = nullptr;
}

void m_IDirect3DDeviceX::ReleaseAllStateBlocks()
{
	while (!StateBlockTokens.empty())
	{
		DWORD Token = *StateBlockTokens.begin();
		if (FAILED(DeleteStateBlock(Token)))
		{
			LOG_LIMIT(100, __FUNCTION__ << " Error: failed to delete all StateBlocks");
			break;
		}
	}
}

void m_IDirect3DDeviceX::SetDefaults()
{
	// Reset defaults flag
	bSetDefaults = false;

	// Reset in scene flag
	IsInScene = false;

	// Reset state block
	IsRecordingState = false;

	// Clip status
	D3DClipStatus = {};

	// Light states
	lsMaterialHandle = NULL;

	// Render states
	rsAntiAliasChanged = true;
	rsAntiAlias = D3DANTIALIAS_NONE;
	rsEdgeAntiAlias = FALSE;
	rsTextureWrappingChanged = false;
	rsTextureWrappingU = FALSE;
	rsTextureWrappingV = FALSE;
	rsTextureMin = D3DFILTER_NEAREST;
	rsTextureMapBlend = D3DTBLEND_MODULATE;
	rsAlphaBlendEnabled = FALSE;
	rsSrcBlend = 0;
	rsDestBlend = 0;
	rsColorKeyEnabled = FALSE;

	// Set DirectDraw defaults
	SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(3, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(4, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(5, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(6, D3DTSS_TEXCOORDINDEX, 0);

	// Get default structures
	(*d3d9Device)->GetViewport(&DefaultViewport);
}

inline void m_IDirect3DDeviceX::SetDrawStates(DWORD dwVertexTypeDesc, DWORD& dwFlags, DWORD DirectXVersion)
{
	if (DirectXVersion < 7)
	{
		// dwFlags (D3DDP_WAIT) can be ignored safely

		// Handle texture wrapping
		if (rsTextureWrappingChanged)
		{
			DWORD RenderState = (rsTextureWrappingU ? D3DWRAP_U : 0) | (rsTextureWrappingV ? D3DWRAP_V : 0);
			SetRenderState(D3DRENDERSTATE_WRAP0, RenderState);
		}

		// Handle dwFlags
		if (dwFlags & D3DDP_DONOTCLIP)
		{
			GetRenderState(D3DRENDERSTATE_CLIPPING, &DrawStates.rsClipping);
			SetRenderState(D3DRENDERSTATE_CLIPPING, FALSE);
		}
		if ((dwFlags & D3DDP_DONOTLIGHT) || !(dwVertexTypeDesc & D3DFVF_NORMAL))
		{
			GetRenderState(D3DRENDERSTATE_LIGHTING, &DrawStates.rsLighting);
			SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
		}
		if (dwFlags & D3DDP_DONOTUPDATEEXTENTS)
		{
			GetRenderState(D3DRENDERSTATE_EXTENTS, &DrawStates.rsExtents);
			SetRenderState(D3DRENDERSTATE_EXTENTS, FALSE);
		}
	}
	// Handle antialiasing
	if (rsAntiAliasChanged)
	{
		BOOL AntiAliasEnabled = (bool)((D3DANTIALIASMODE)rsAntiAlias == D3DANTIALIAS_SORTDEPENDENT || (D3DANTIALIASMODE)rsAntiAlias == D3DANTIALIAS_SORTINDEPENDENT);
		SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, AntiAliasEnabled);
		rsAntiAliasChanged = false;
	}
	if (Config.Dd7to9)
	{
		if (lpCurrentRenderTargetX)
		{
			lpCurrentRenderTargetX->PrepareRenderTarget();
		}
		if (Config.DdrawFixByteAlignment > 1)
		{
			for (UINT x = 0; x < MaxTextureStages; x++)
			{
				if (CurrentTextureSurfaceX[x] && CurrentTextureSurfaceX[x]->GetWasBitAlignLocked())
				{
					(*d3d9Device)->GetSamplerState(x, D3DSAMP_MINFILTER, &DrawStates.ssMinFilter[x]);
					(*d3d9Device)->GetSamplerState(x, D3DSAMP_MAGFILTER, &DrawStates.ssMagFilter[x]);

					(*d3d9Device)->SetSamplerState(x, D3DSAMP_MINFILTER, Config.DdrawFixByteAlignment == 2 ? D3DTEXF_POINT : D3DTEXF_LINEAR);
					(*d3d9Device)->SetSamplerState(x, D3DSAMP_MAGFILTER, Config.DdrawFixByteAlignment == 2 ? D3DTEXF_POINT : D3DTEXF_LINEAR);
				}
			}
		}
		for (UINT x = 0; x < MaxTextureStages; x++)
		{
			if (ssMipFilter[x] != D3DTEXF_NONE && CurrentTextureSurfaceX[x] && !CurrentTextureSurfaceX[x]->IsMipMapGenerated())
			{
				CurrentTextureSurfaceX[x]->GenerateMipMapLevels();
			}
		}
		if (rsColorKeyEnabled)
		{
			// Check for color key alpha texture
			for (UINT x = 0; x < MaxTextureStages; x++)
			{
				if (CurrentTextureSurfaceX[x] && CurrentTextureSurfaceX[x]->IsColorKeyTexture() && CurrentTextureSurfaceX[x]->GetD3d9DrawTexture())
				{
					dwFlags |= D3DDP_DXW_ALPHACOLORKEY;
					(*d3d9Device)->SetTexture(x, CurrentTextureSurfaceX[x]->GetD3d9DrawTexture());
				}
			}
			if (dwFlags & D3DDP_DXW_ALPHACOLORKEY)
			{
				(*d3d9Device)->GetRenderState(D3DRS_ALPHATESTENABLE, &DrawStates.rsAlphaTestEnable);
				(*d3d9Device)->GetRenderState(D3DRS_ALPHAFUNC, &DrawStates.rsAlphaFunc);
				(*d3d9Device)->GetRenderState(D3DRS_ALPHAREF, &DrawStates.rsAlphaRef);

				(*d3d9Device)->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
				(*d3d9Device)->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
				(*d3d9Device)->SetRenderState(D3DRS_ALPHAREF, (DWORD)0x01);
			}
		}
		if (dwFlags & D3DDP_DXW_COLORKEYENABLE)
		{
			if (!colorkeyPixelShader || !*colorkeyPixelShader)
			{
				colorkeyPixelShader = ddrawParent->GetColorKeyShader();
			}
			if (colorkeyPixelShader && *colorkeyPixelShader)
			{
				(*d3d9Device)->SetPixelShader(*colorkeyPixelShader);
				(*d3d9Device)->SetPixelShaderConstantF(0, DrawStates.lowColorKey, 1);
				(*d3d9Device)->SetPixelShaderConstantF(1, DrawStates.highColorKey, 1);
			}
		}
	}
}

inline void m_IDirect3DDeviceX::RestoreDrawStates(DWORD dwVertexTypeDesc, DWORD dwFlags, DWORD DirectXVersion)
{
	if (DirectXVersion < 7)
	{
		// Handle dwFlags
		if (dwFlags & D3DDP_DONOTCLIP)
		{
			SetRenderState(D3DRENDERSTATE_CLIPPING, DrawStates.rsClipping);
		}
		if ((dwFlags & D3DDP_DONOTLIGHT) || !(dwVertexTypeDesc & D3DFVF_NORMAL))
		{
			SetRenderState(D3DRENDERSTATE_LIGHTING, DrawStates.rsLighting);
		}
		if (dwFlags & D3DDP_DONOTUPDATEEXTENTS)
		{
			SetRenderState(D3DRENDERSTATE_EXTENTS, DrawStates.rsExtents);
		}
	}
	if (Config.Dd7to9)
	{
		if (Config.DdrawFixByteAlignment > 1)
		{
			for (UINT x = 0; x < MaxTextureStages; x++)
			{
				if (CurrentTextureSurfaceX[x] && CurrentTextureSurfaceX[x]->GetWasBitAlignLocked())
				{
					(*d3d9Device)->SetSamplerState(x, D3DSAMP_MINFILTER, DrawStates.ssMinFilter[x]);
					(*d3d9Device)->SetSamplerState(x, D3DSAMP_MAGFILTER, DrawStates.ssMagFilter[x]);
				}
			}
		}
		if (dwFlags & D3DDP_DXW_ALPHACOLORKEY)
		{
			(*d3d9Device)->SetRenderState(D3DRS_ALPHATESTENABLE, DrawStates.rsAlphaTestEnable);
			(*d3d9Device)->SetRenderState(D3DRS_ALPHAFUNC, DrawStates.rsAlphaFunc);
			(*d3d9Device)->SetRenderState(D3DRS_ALPHAREF, DrawStates.rsAlphaRef);
		}
		if (dwFlags & D3DDP_DXW_COLORKEYENABLE)
		{
			(*d3d9Device)->SetPixelShader(nullptr);
		}
	}
}

inline void m_IDirect3DDeviceX::ScaleVertices(DWORD dwVertexTypeDesc, LPVOID& lpVertices, DWORD dwVertexCount)
{
	if (dwVertexTypeDesc == 3)
	{
		VertexCache.resize(dwVertexCount * sizeof(D3DTLVERTEX));
		memcpy(VertexCache.data(), lpVertices, dwVertexCount * sizeof(D3DTLVERTEX));
		D3DTLVERTEX* pVert = (D3DTLVERTEX*)VertexCache.data();

		for (DWORD x = 0; x < dwVertexCount; x++)
		{
			pVert[x].sx = (D3DVALUE)(pVert[x].sx * ScaleDDWidthRatio) + ScaleDDPadX;
			pVert[x].sy = (D3DVALUE)(pVert[x].sy * ScaleDDHeightRatio) + ScaleDDPadY;
		}

		lpVertices = pVert;
	}
}

inline void m_IDirect3DDeviceX::UpdateVertices(DWORD& dwVertexTypeDesc, LPVOID& lpVertices, DWORD dwVertexCount)
{
	if (dwVertexTypeDesc == D3DFVF_LVERTEX)
	{
		VertexCache.resize(dwVertexCount * sizeof(D3DLVERTEX9));
		ConvertVertices((D3DLVERTEX9*)VertexCache.data(), (D3DLVERTEX*)lpVertices, dwVertexCount);

		dwVertexTypeDesc = D3DFVF_LVERTEX9;
		lpVertices = VertexCache.data();
	}
}
