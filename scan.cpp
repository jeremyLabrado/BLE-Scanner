// BLEScanner.cpp : Scans for a Magic Blue BLE bulb, connects to it and sends it commands
//
// Copyright (C) 2016, Uri Shaked. License: MIT.
//
// ***
// See here for info about the bulb protocol: 
// https://medium.com/@urish/reverse-engineering-a-bluetooth-lightbulb-56580fcb7546
// ***
//


/////////////////////////////////////////////////////////////////////////////////////////////
////// INCLUDES /////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <iostream>
#include <Windows.Foundation.h>
#include <Windows.Devices.Bluetooth.h>
#include <Windows.Devices.Bluetooth.Advertisement.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>
#include <collection.h>
#include <ppltasks.h>
#include <string>
#include <sstream> 
#include <iomanip>
#include <experimental/resumable>
#include <pplawait.h>

using namespace Platform;
using namespace Windows::Devices;
using namespace std

/////////////////////////////////////////////////////////////////////////////////////////////
////// PARAMETERS ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// the service UUID we are looking for
auto serviceUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0xffe5);

// the characteristic UUID we are looking for
auto characteristicUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0xffe9);


/////////////////////////////////////////////////////////////////////////////////////////////
////// PROTOTYPES  //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

std::wstring formatBluetoothAddress(unsigned long long BluetoothAddress) ;
concurrency::task<void> setColor(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic, byte red, byte green, byte blue) ;
concurrency::task<void> connectToBulb(unsigned long long bluetoothAddress) ;


/////////////////////////////////////////////////////////////////////////////////////////////
////// MAIN FUNCTION ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// Main function
int main(Array<String^>^ args) {
	cout << "main()" << std::endl ;
	
	// TODO: Not sure, probably to have a multithread app?
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

	// TODO: Not sure, security of the app? 
	CoInitializeSecurity(
		nullptr, // TODO: "O:BAG:BAD:(A;;0x7;;;PS)(A;;0x3;;;SY)(A;;0x7;;;BA)(A;;0x3;;;AC)(A;;0x3;;;LS)(A;;0x3;;;NS)"
		-1,
		nullptr,
		nullptr,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IDENTIFY,
		NULL,
		EOAC_NONE,
		nullptr);

	// Initialize a new BLE advertiser	
	Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher^ bleAdvertisementWatcher = ref new Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher();
	
	// Setup the scanning mode to active
	bleAdvertisementWatcher->ScanningMode = Bluetooth::Advertisement::BluetoothLEScanningMode::Active;
	
	// Init the BLE handler
	bleAdvertisementWatcher->Received += ref new Windows::Foundation::TypedEventHandler<Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^, 
	Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs ^>
	(
		[bleAdvertisementWatcher](
			Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^watcher, 
			Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs^ eventArgs
			) 
			{
				//Each time a device is scanned, we should come here
				cout << "bleAdvertisementWatcher handler " << std::endl; 
				cout << eventArgs << std::endl; 
				
				// Get the UUIDs of the device scanned
				auto serviceUuids = eventArgs->Advertisement->ServiceUuids;
				unsigned int index = -1;
				
				// Print some debugs logs
				cout << "serviceUUID (the filter): " << serviceUUID << std::endl; 
				cout << "serviceUuids (the current ble device detected): " << serviceUuids << std::endl; 
				cout << "if( " << serviceUuids->IndexOf(serviceUUID, &index) << " )" << std::endl; 
				
				// Filter the service found with the one we want
				if (serviceUuids->IndexOf(serviceUUID, &index)) 
				{
					// We found the services we want
					cout << "the condition of the current Uuid is good" << std::endl;
					
					// Get the (MAC???) address from the device scanned
					String^ strAddress = ref new String(formatBluetoothAddress(eventArgs->BluetoothAddress).c_str());
					
					// Print the address for debug
					std::wcout << "Target service found on device: " << strAddress->Data() << std::endl;
					cout << "We found the device at the address: " << strAddress->Data() << std::endl;
					cout << "Stop the scanner" << std::endl;
					
					// Stop the BLE scanner
					bleAdvertisementWatcher->Stop();
					
					// Connect and search for Services and Characteristic in the BLE device
					connectToBulb(eventArgs->BluetoothAddress);
				}	
			}
	);
	
	// Start the advertisement, each time a device is scanned it should be printed from the handler
	bleAdvertisementWatcher->Start();
	
	// Not sure what this is, it is waiting for the user to type something in the console
	// Probably as a 'press a key to exit' 
	int a;
	std::cin >> a;
	return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////
////// APPLICATION FUNCTIONS ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// This function return the address given in a (MAC??) address format
std::wstring formatBluetoothAddress(unsigned long long BluetoothAddress) {
	cout << "formatBluetoothAddress() " << std::endl;
	cout << BluetoothAddress;
	// Separate the address in the good format XX:XX:XX:XX:XX:XX
	std::wostringstream ret;
	ret << std::hex << std::setfill(L'0')
		<< std::setw(2) << ((BluetoothAddress >> (5 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (4 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (3 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (2 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (1 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (0 * 8)) & 0xff);
	return ret.str();
}

// Write the RGB value in the color characteristic
concurrency::task<void> setColor(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic, byte red, byte green, byte blue) {
	cout << "setColor()" << std::endl;
	cout << "R:" << red << " G:" << green << " B:" << blue << std::endl;
	
	// Init writer with RGB value
	auto writer = ref new Windows::Storage::Streams::DataWriter();
	auto data = new byte[7]{ 0x56, red, green, blue, 0x00, 0xf0, 0xaa };
	writer->WriteBytes(ref new Array<byte>(data, 7));
	
	// Send writer to characteristic
	auto status = co_await characteristic->WriteValueAsync(
		writer->DetachBuffer(), 
		Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse
		);
	
	// Debug print
	std::wcout << "Write result: " << status.ToString()->Data() << std::endl;
}

// This function will connect to a bulb, find the service and characteristic and write some data
concurrency::task<void> connectToBulb(unsigned long long bluetoothAddress) {
	cout << "connectToBulb() " << std::endl;
	cout << "address: " << bluetoothAddress << std::endl;
	cout << "service filter: " << serviceUUID << std::endl;
	
	// Get device from (MAC???) Address
	auto leDevice = co_await Bluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
	
	// Get Service from short UUID
	auto servicesResult = co_await leDevice->GetGattServicesForUuidAsync(serviceUUID);
	auto service = servicesResult->Services->GetAt(0);
	
	// Get Characteristic from short UUID
	auto characteristicsResult = co_await service->GetCharacteristicsForUuidAsync(characteristicUUID);
	auto characteristic = characteristicsResult->Characteristics->GetAt(0);

	cout << "leDevice: " << leDevice << std::endl;
	cout << "servicesResult: " << servicesResult << std::endl;
	cout << "service: " << service << std::endl;
	cout << "characteristicsResult: " << characteristicsResult << std::endl;
	cout << "characteristic: " << characteristic << std::endl;
	cout << "TODO: Set up notification" << std::endl;
	
	// The following function are for the LED-BLE to change the colors
	co_await setColor(characteristic, 0, 0xff, 0); // Green
	for (;;) {
		Sleep(1000);
		co_await setColor(characteristic, 0xff, 0xff, 0);	// Yellow

		Sleep(1000);
		co_await setColor(characteristic, 0xff, 0, 0);	// Red
	}
}
