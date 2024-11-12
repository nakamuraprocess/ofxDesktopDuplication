#pragma once

#include <dxgi1_2.h>
#include <d3d11.h>
#include <shlobj.h>
#include "ofMain.h"

class ofxDesktopDuplication : public ofThread{
	private:

		// Driver types supported
		D3D_DRIVER_TYPE gDriverTypes[1] = { D3D_DRIVER_TYPE_HARDWARE };
		UINT gNumDriverTypes = ARRAYSIZE(gDriverTypes);

		// Feature levels supported
		D3D_FEATURE_LEVEL gFeatureLevels[4] = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_1
		};
		UINT gNumFeatureLevels = ARRAYSIZE(gFeatureLevels);


		ID3D11Device*			device;
		DXGI_OUTPUT_DESC		outputDesc;
		DXGI_OUTDUPL_DESC		outputDuplDesc;
		ID3D11DeviceContext*	immediateContext;
		IDXGIOutputDuplication* deskDupl;

		ID3D11Texture2D*		acquiredDesktopImage;
		ID3D11Texture2D*		gDIImage;
		ID3D11Texture2D*		destImage;

		void threadedFunction();
		void updateData();

	public:
		~ofxDesktopDuplication();
		void setup(const int width, const int height, const int channel, UINT output);
		void update();
		void draw();

		int iColorWidth;
		int iColorHeight;
		int	iChannnel;

		BYTE*		colorImageBuffer;
		ofTexture	colorTexture;
};
