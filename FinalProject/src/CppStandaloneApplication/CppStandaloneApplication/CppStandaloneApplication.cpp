// CppStandaloneApplication.cpp : Defines the entry point for the console application.
//



#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <ctime>
#include <functional>
#include <assert.h>
#include <vector>

#import "C:\\Program Files\\Zemax OpticStudio\\ZOS-API\\Libraries\\ZOSAPII.tlb"
#import "C:\\Program Files\\Zemax OpticStudio\\ZOS-API\\Libraries\\ZOSAPI.tlb"
// Note - .tlh files will be generated from the .tlb files (above) once the project is compiled.
// Visual Studio will incorrectly continue to report IntelliSense error messages however until it is restarted.



using namespace std;
using namespace ZOSAPI;
using namespace ZOSAPI_Interfaces;

double nth_radius(int n, double focal_len, double wavlen);
int zone_ind(double radius, double focal_len, double wavlen);
void handleError(std::string msg);
void logInfo(std::string msg);
void finishStandaloneApplication(IZOSAPI_ApplicationPtr TheApplication);

int _tmain(int argc, _TCHAR* argv[])
{
	CoInitialize(NULL);

	// Create the initial connection class
	IZOSAPI_ConnectionPtr TheConnection(__uuidof(ZOSAPI_Connection));


	// Attempt to create a Standalone connection
	IZOSAPI_ApplicationPtr TheApplication = TheConnection->CreateNewApplication();
	if (TheApplication == nullptr)
	{
		handleError("An unknown error occurred!");
		return -1;
	}

	// Check the connection status
	if (!TheApplication->IsValidLicenseForAPI)
	{
		handleError("License check failed!");
		return -1;
	}
	if (TheApplication->Mode != ZOSAPI_Mode::ZOSAPI_Mode_Server)
	{
		handleError("Standlone application was started in the incorrect mode!");
		return -1;
	}

	// Start the Zemax application
	IOpticalSystemPtr TheSystem = TheApplication->PrimarySystem;

	// Define parameters of the optical system
	double focal_len = 1373;	// Focal length of the zone plate in mm.
	double aper_rad = 88.0;		// Maximum size of the aperture in mm.
	double w0 = 1.9351e-5;
	double w1 = 1.9512e-5;
	double wavlen = w0;	// Specified wavelength of the zone plate in mm (1e-5 mm = 1 Angstrom)
	//double wavlen = 5.5e-4;
	int tot_zones = 100;		// Set the target number of zones for computational tractibility
	int max_n = zone_ind(aper_rad, focal_len, wavlen);	// Calculate the maximum n, given the radius
	//int dn = floor(floor(max_n / tot_zones) / 2) * 2 + 1;		// Calculate the jump in n required to keep the total number of zones constant
	int dn = 1;


	// Open a new Zemax file
	TheSystem->New(false);

	// Open the lens data editor
	ILensDataEditorPtr lde = TheSystem->LDE;

	// Create circular obscurations at increasing radii 
	// until the aperture limit is reached
	int i = 0;
	int n = 0;		// looping variable
	double inner_rad;
	double outer_rad;
	while (true) {

		if (i > tot_zones) {
			break;
		}

		// Calculate the inner and outer radius of the circular obscuration
		inner_rad = nth_radius(n, focal_len, wavlen);
		n += dn;
		outer_rad = nth_radius(n, focal_len, wavlen);
		n++;

		cout << i << " " << n << " " << inner_rad << " " << outer_rad << endl;

		// If the outer radius exceeds the specified aperture, break out of the loop
		if (outer_rad > aper_rad) {
			break;
		}

		// Create a new surface in the lens data editor
		ILDERowPtr surf = lde->InsertNewSurfaceAt(i + 2);

		// Create new circular obscuration
		ISurfaceApertureTypePtr surf_apType = surf->ApertureData->CreateApertureTypeSettings(SurfaceApertureTypes_CircularObscuration);	

		// Set the min and max radius of the obscuration
		ISurfaceApertureCircularPtr circ = surf_apType->Get_S_CircularObscuration();
		circ->PutMinimumRadius(inner_rad);
		circ->PutMaximumRadius(outer_rad);

		// Update the aperture data
		surf->ApertureData->ChangeApertureTypeSettings(surf_apType);

		surf->PhysicalOpticsData->UseRaysToPropagateToNextSurface = true;
		//surf->PhysicalOpticsData->AutoResample = true;
		//surf->PhysicalOpticsData->UseAngularSpectrumPropagator = true;

		//Sleep(1000);

		i++;
	}

	// Insert surface to put image in correct position
	int num_s = 5;
	int s_i = i;
	double ds = focal_len / (double) num_s;
	for (int s = 0; s < num_s; s++) {
		ILDERowPtr surf = lde->InsertNewSurfaceAt(i + 2);
		surf->Thickness = ds;
		surf->DrawData->DoNotDrawThisSurface = true;
		surf->DrawData->SkipRaysToThisSurface = true;
		i++;
	}

	//ILDERowPtr surf = lde->InsertNewSurfaceAt(i + 2);
	//surf->Thickness = focal_len;

	// Set the aperture size to the width of the outermost zone
	TheSystem->SystemData->Aperture->ApertureValue = 2 * outer_rad;

	//TheSystem->SystemData->Wavelengths->AddWavelength(w0 * 1e3,1);
	TheSystem->SystemData->Wavelengths->AddWavelength(w1 * 1e3,1);
	TheSystem->SystemData->Wavelengths->GetWavelength(1)->Wavelength = wavlen * 1e3;
	
	ILDERowPtr img = lde->GetSurfaceAt(i + 2);
	img->DrawData->DoNotDrawThisSurface = true;
	img->DrawData->SkipRaysToThisSurface = true;

	/*IA_Ptr psf = TheSystem->Analyses->New_HuygensPsf();
	psf->WaitForCompletion();
	IAS_HuygensPsfPtr psf_settings = psf->GetSettings();
	psf_settings->PupilSampleSize = ZOSAPI_Interfaces::SampleSizes_S_4096x4096;
	psf_settings->ImageSampleSize = ZOSAPI_Interfaces::SampleSizes_S_128x128;
	psf->ApplyAndWaitForCompletion();

	IAR_Ptr psf_results = psf->GetResults();
	IAR_DataGridPtr psf_data = psf_results->GetDataGrid(0);
	SAFEARRAY * psf_values = psf_data->Values;
	void * pv;
	SafeArrayAccessData(psf_values, &pv);
	double * psf_double = reinterpret_cast<double *>(pv);

	FILE * psf_file = fopen("E:\\Users\\byrdie\\School\\Classes\\EELE582_OpticalDesign\\FinalProject\\psf.bin", "wb");
	fwrite(psf_double, sizeof(double), psf_data->Nx * psf_data->Ny, psf_file);
	fclose(psf_file);

	FILE * psf_meta = fopen("E:\\Users\\byrdie\\School\\Classes\\EELE582_OpticalDesign\\FinalProject\\meta.bin", "wb");
	int nx = psf_data->Nx;
	int ny = psf_data->Ny;
	double min_x = psf_data->MinX;
	double min_y = psf_data->MinY;
	double dx = psf_data->Dx;
	double dy = psf_data->Dy;
	fwrite(&focal_len, sizeof(double), 1, psf_meta);
	fwrite(&nx, sizeof(int), 1, psf_meta);
	fwrite(&ny, sizeof(int), 1, psf_meta);
	fwrite(&min_x, sizeof(double), 1, psf_meta);
	fwrite(&min_y, sizeof(double), 1, psf_meta);
	fwrite(&dx, sizeof(double), 1, psf_meta);
	fwrite(&dy, sizeof(double), 1, psf_meta);
	fclose(psf_meta);*/


	TheSystem->SaveAs(_bstr_t::_bstr_t("E:\\Users\\byrdie\\School\\Classes\\EELE582_OpticalDesign\\FinalProject\\zemax\\test.zmx"));

	TheSystem->Close(false);
			
	// Clean up
	finishStandaloneApplication(TheApplication);
	

	return 0;
}

double nth_radius(int n, double focal_len, double wavlen) {

	return sqrt((n * wavlen * focal_len) + (0.25 * n * n * wavlen * wavlen));

}

int zone_ind(double radius, double focal_len, double wavlen) {
	return floor(2 * (sqrt(focal_len * focal_len + radius * radius) - focal_len) / wavlen);
}

void handleError(std::string msg)
{
	throw new exception(msg.c_str());
}

void logInfo(std::string msg)
{
	printf("%s", msg.c_str());
}

void finishStandaloneApplication(IZOSAPI_ApplicationPtr TheApplication)
{
    // Note - TheApplication will close automatically when this application exits, so this isn't strictly necessary in most cases
	if (TheApplication != nullptr)
	{
		TheApplication->CloseApplication();
	}
}


