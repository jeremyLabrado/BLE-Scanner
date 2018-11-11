// BLEScanner.cpp : Scans for a BLE peripheral
//
// Copyright (C) 2018, Jeremy Labrado. License: MIT.
//
//

/////////////////////////////////////////////////////////////////////////////////////////////
////// INCLUDES /////////////////////a////////////////////////////////////////////////////////
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
using namespace std;

/////////////////////////////////////////////////////////////////////////////////////////////
////// PARAMETERS ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
// BLEService        bleService("00000000-5F5F-4B4F-5041-524F2E434F4D");        //ID service ("__KOPARO.COM")

// the service UUID we are looking for
GUID serviceUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0x00000000);

// the characteristic UUID we are looking for
GUID characteristicUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0x00000001);


/////////////////////////////////////////////////////////////////////////////////////////////
////// PROTOTYPES  //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

std::wstring formatBluetoothAddress(unsigned long long BluetoothAddress);
concurrency::task<void> write_to_characteristic(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic, byte red, byte green, byte blue);
concurrency::task<void> connectToPlate(unsigned long long bluetoothAddress);
void printf_guid(GUID guid);
concurrency::task<void> readCharacteristic(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic);
concurrency::task<void> subscribeRequest(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic);


/////////////////////////////////////////////////////////////////////////////////////////////
////// MAIN FUNCTION ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// Main function
int main(Array<String^>^ args) {
	cout << "main()" << std::endl;

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

	// BLEService        bleService("00000000-5F5F-4B4F-5041-524F2E434F4D");        //ID service ("__KOPARO.COM")

	serviceUUID.Data1 = 0x00000000;
	serviceUUID.Data2 = 0x5F5F;
	serviceUUID.Data3 = 0x4B4F;
	serviceUUID.Data4[0] = 0x50;
	serviceUUID.Data4[1] = 0x41;
	serviceUUID.Data4[2] = 0x52;
	serviceUUID.Data4[3] = 0x4F,
	serviceUUID.Data4[4] = 0x2E;
	serviceUUID.Data4[5] = 0x43;
	serviceUUID.Data4[6] = 0x4F;
	serviceUUID.Data4[7] = 0x4D;

	characteristicUUID.Data1 = 0x00000001;
	characteristicUUID.Data2 = 0x5F5F;
	characteristicUUID.Data3 = 0x4B4F;
	characteristicUUID.Data4[0] = 0x50;
	characteristicUUID.Data4[1] = 0x41;
	characteristicUUID.Data4[2] = 0x52;
	characteristicUUID.Data4[3] = 0x4F,
	characteristicUUID.Data4[4] = 0x2E;
	characteristicUUID.Data4[5] = 0x43;
	characteristicUUID.Data4[6] = 0x4F;
	characteristicUUID.Data4[7] = 0x4D;

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

		// Get the UUIDs of the device scanned
		auto serviceUuids = eventArgs->Advertisement->ServiceUuids;
		unsigned int index = -1;

		// Print some debugs logs
		cout << "serviceUUID (the filter): ";
		printf_guid(serviceUUID);
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

			// Stop the BLE scanner
			bleAdvertisementWatcher->Stop();

			// Connect and search for Services and Characteristic in the BLE device
			connectToPlate(eventArgs->BluetoothAddress);
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
concurrency::task<void> write_to_characteristic(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic, byte red, byte green, byte blue) {
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




// Read the data in the characteristic
// TODO; The value of the read is only read once and then repeat for an unknown reason
//		 - Maybe due to the ReadValueAsync() that is reading the Windows cache
//		 - I wanted to use ReadValueAsync()(.uncache) but it is not working
//		 	Maybe it is only a C# function?
concurrency::task<void> readCharacteristic(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic) {

	auto result = await characteristic->ReadValueAsync();

	if (result->Status != Bluetooth::GenericAttributeProfile::GattCommunicationStatus::Success) {
		cout << "error ble read attribute" << endl;
		throw ref new FailureException(result->Status.ToString());
	}
	
	auto reader = Windows::Storage::Streams::DataReader::FromBuffer(result->Value);
	for (unsigned int i = 0; i < result->Value->Length; i++) {
		cout << int(reader->ReadByte()) << " ";
	}

	// Debug print
	std::wcout << std::endl;

}



// This function will connect to a plate, find the service and characteristic and write some data
concurrency::task<void> connectToPlate(unsigned long long bluetoothAddress) {
	cout << "connectToBulb() " << std::endl;
	cout << "address: " << bluetoothAddress << std::endl;
//	cout << "service filter: " << serviceUUID << std::endl;

	// Get device from (MAC???) Address
	auto leDevice = co_await Bluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);

	// Get Service from short UUID
	auto servicesResult = co_await leDevice->GetGattServicesForUuidAsync(serviceUUID);
	auto service = servicesResult->Services->GetAt(0);

	// Get Characteristic from short UUID
	auto characteristicsResult = co_await service->GetCharacteristicsForUuidAsync(characteristicUUID);
	auto characteristic = characteristicsResult->Characteristics->GetAt(0);

//	cout << "leDevice: " << leDevice << std::endl;
//	cout << "servicesResult: " << servicesResult << std::endl;
//	cout << "service: " << service << std::endl;
//	cout << "characteristicsResult: " << characteristicsResult << std::endl;
//	cout << "characteristic: " << characteristic << std::endl;

	cout << "Service: ";
	printf_guid(serviceUUID);
	cout << "Characteristic: ";
	printf_guid(characteristicUUID);
	
	// Give some time to the plate to get some values
	Sleep(10000);

	// The following function are for the LED-BLE to change the colors
	//co_await setColor(characteristic, 0, 0xff, 0); // Green
	for (;;) {
		Sleep(1000);
		readCharacteristic(characteristic);
	}
}

//Print the GUID in a UUID format
void printf_guid(GUID guid) {
	printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX } \r\n",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}



