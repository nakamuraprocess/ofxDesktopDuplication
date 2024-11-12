#include "ofxDesktopDuplication.h"


//--------------------------------------------------------------
ofxDesktopDuplication::~ofxDesktopDuplication(){
	waitForThread();
	stopThread();

	device->Release();
	immediateContext->Release();

	if (deskDupl) {
		deskDupl->Release();
	}

	if (acquiredDesktopImage) {
		acquiredDesktopImage->Release();
	}

	gDIImage->Release();
	destImage->Release();

	delete[] colorImageBuffer;
}

//--------------------------------------------------------------
void ofxDesktopDuplication::setup(const int width, const int height, const int channnel, UINT output){

	this->iColorWidth = width;
	this->iColorHeight = height;
	this->iChannnel = channnel;

	do {
		D3D_FEATURE_LEVEL lFeatureLevel;
		HRESULT hr(E_FAIL);

		// Create device
		for (UINT DriverTypeIndex = 0; DriverTypeIndex < gNumDriverTypes; ++DriverTypeIndex) {
			hr = D3D11CreateDevice(nullptr, gDriverTypes[DriverTypeIndex], nullptr, 0, gFeatureLevels, gNumFeatureLevels, D3D11_SDK_VERSION, &device, &lFeatureLevel, &immediateContext);

			if (SUCCEEDED(hr)) {
				// Device creation success, no need to loop anymore
				break;
			}

			device->Release();
			immediateContext->Release();
		}

		if (FAILED(hr)) {
			break;
		}

		Sleep(100);

		if (device == nullptr) {
			break;
		}

		// Get DXGI device
		IDXGIDevice* lDxgiDevice;

		hr = device->QueryInterface(IID_PPV_ARGS(&lDxgiDevice));

		if (FAILED(hr)) {
			break;
		}

		// Get DXGI adapter
		IDXGIAdapter* lDxgiAdapter;
		hr = lDxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(&lDxgiAdapter));

		if (FAILED(hr)) {
			break;
		}

		lDxgiDevice->Release();


		// Get output
		IDXGIOutput* lDxgiOutput;
		hr = lDxgiAdapter->EnumOutputs(output, &lDxgiOutput);

		if (FAILED(hr)) {
			break;
		}

		lDxgiAdapter->Release();

		hr = lDxgiOutput->GetDesc(&outputDesc);

		if (FAILED(hr)) {
			break;
		}

		// QI for Output 1
		IDXGIOutput1* lDxgiOutput1;
		hr = lDxgiOutput->QueryInterface(IID_PPV_ARGS(&lDxgiOutput1));

		if (FAILED(hr)) {
			break;
		}

		lDxgiOutput->Release();

		// Create desktop duplication
		hr = lDxgiOutput1->DuplicateOutput(device, &deskDupl);

		if (FAILED(hr)) {
			break;
		}

		lDxgiOutput1->Release();

		// Create GUI drawing texture
		deskDupl->GetDesc(&outputDuplDesc);
		D3D11_TEXTURE2D_DESC desc;
		desc.Width = outputDuplDesc.ModeDesc.Width;
		desc.Height = outputDuplDesc.ModeDesc.Height;
		desc.Format = outputDuplDesc.ModeDesc.Format;
		desc.ArraySize = 1;
		desc.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags = D3D11_RESOURCE_MISC_GDI_COMPATIBLE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MipLevels = 1;
		desc.CPUAccessFlags = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;

		hr = device->CreateTexture2D(&desc, NULL, &gDIImage);

		if (FAILED(hr)) {
			break;
		}

		if (gDIImage == nullptr) {
			break;
		}

		// Create CPU access texture
		desc.Width = outputDuplDesc.ModeDesc.Width;
		desc.Height = outputDuplDesc.ModeDesc.Height;
		desc.Format = outputDuplDesc.ModeDesc.Format;
		desc.ArraySize = 1;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.MipLevels = 1;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.Usage = D3D11_USAGE_STAGING;

		hr = device->CreateTexture2D(&desc, NULL, &destImage);

		if (FAILED(hr)) {
			break;
		}

		if (destImage == nullptr) {
			break;
		}

	} while (false);

	colorImageBuffer = new BYTE[iColorWidth * iColorHeight * iChannnel];
	colorTexture.allocate(iColorWidth, iColorHeight, GL_RGBA);

	startThread();
}

//--------------------------------------------------------------
void ofxDesktopDuplication::update() {
	colorTexture.loadData(colorImageBuffer, iColorWidth, iColorHeight, GL_BGRA);
}

//--------------------------------------------------------------
void ofxDesktopDuplication::draw() {	
	ofDisableAlphaBlending();
	colorTexture.draw(0, 0, iColorWidth, iColorHeight);
	ofEnableAlphaBlending();
}

//--------------------------------------------------------------
void ofxDesktopDuplication::updateData(){
	HRESULT hr = S_OK;

	IDXGIResource* DesktopResource;
	DXGI_OUTDUPL_FRAME_INFO FrameInfo;

	int lTryCount = 4;

	do {
		// Get new frame
		hr = deskDupl->AcquireNextFrame(250, &FrameInfo, &DesktopResource);

		if (SUCCEEDED(hr)) {
			break;
		}

		if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
			continue;
		}
		else if (FAILED(hr)) {
			break;
		}

	} while (--lTryCount > 0);

	if (FAILED(hr)) {
		return;
	}

	// QI for ID3D11Texture2D
	hr = DesktopResource->QueryInterface(IID_PPV_ARGS(&acquiredDesktopImage));
	DesktopResource->Release();
	DesktopResource = nullptr;

	if (FAILED(hr)) {
		printf("Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER");
		return;
	}

	if (acquiredDesktopImage == nullptr) {
		printf("acquiredDesktopImage is null");
		return;
	}

	// Copy image into GDI drawing texture
	immediateContext->CopyResource(gDIImage, acquiredDesktopImage);

	// Draw cursor image into GDI drawing texture
	IDXGISurface1* IDXGISurface1;
	hr = gDIImage->QueryInterface(IID_PPV_ARGS(&IDXGISurface1));

	if (FAILED(hr)) {
		printf("QueryInterface was faild");
		return;
	}

	CURSORINFO lCursorInfo = { 0 };
	lCursorInfo.cbSize = sizeof(lCursorInfo);
	auto lBoolres = GetCursorInfo(&lCursorInfo);
	if (lBoolres == TRUE) {
		if (lCursorInfo.flags == CURSOR_SHOWING) {
			auto lCursorPosition = lCursorInfo.ptScreenPos;
			auto lCursorSize = lCursorInfo.cbSize;
			HDC  lHDC;
			IDXGISurface1->GetDC(FALSE, &lHDC);
			DrawIconEx(lHDC, lCursorPosition.x, lCursorPosition.y, lCursorInfo.hCursor, 0, 0, 0, 0, DI_NORMAL | DI_DEFAULTSIZE);
			IDXGISurface1->ReleaseDC(nullptr);
		}
	}

	// Copy image into CPU access texture
	immediateContext->CopyResource(destImage, gDIImage);

	// Copy from CPU access texture to bitmap buffer
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	immediateContext->Map(destImage, subresource, D3D11_MAP_READ_WRITE, 0, &resource);

	// Copy from CPU access bitmap buffer to ofTexture
	BYTE* sptr = reinterpret_cast<BYTE*>(resource.pData);
	BYTE* dptr = colorImageBuffer;

	if (outputDuplDesc.ModeDesc.Width == iColorWidth) {
		memcpy(colorImageBuffer, reinterpret_cast<BYTE*>(resource.pData), iColorWidth * iColorHeight * iChannnel);
	}
	else if (outputDuplDesc.ModeDesc.Width > iColorWidth) {
		for (size_t h = 0; h < iColorHeight; ++h) {
			memcpy_s(dptr, iColorWidth * iChannnel, sptr, iColorWidth * iChannnel);
			sptr += resource.RowPitch;
			dptr += iColorWidth * iChannnel;
		}
	}

	if (deskDupl) {
		deskDupl->ReleaseFrame();
	}

	if (acquiredDesktopImage){
		acquiredDesktopImage->Release();
		acquiredDesktopImage = nullptr;
	}
}

//--------------------------------------------------------------
void ofxDesktopDuplication::threadedFunction() {
	while (isThreadRunning()) {
		lock();
		updateData();
		unlock();
	}
}