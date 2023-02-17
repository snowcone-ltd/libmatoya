// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdio.h>

#include <windows.h>
#include <initguid.h>
#include <devpkey.h>
#include <cfgmgr32.h>

static const char *ps5_haptics_get_device_id(const char *name)
{
	const char *haptics = NULL;
	WCHAR *name_w = MTY_MultiToWideD(name); // TODO better cleanup handling, needs free

	PBYTE buf = NULL;
	ULONG buf_size = 0;
	DEVPROPTYPE property_type;
	// Get Instance Path
	CM_Get_Device_Interface_Property(name_w, &DEVPKEY_Device_InstanceId, &property_type, NULL, &buf_size, 0);
	if (!buf_size)
		return NULL;

	buf = MTY_Alloc(buf_size, 1);
	HRESULT e = CM_Get_Device_Interface_Property(name_w, &DEVPKEY_Device_InstanceId, &property_type, buf, &buf_size, 0);
	if (e != S_OK) {
		MTY_Free(buf);
		return NULL;
	}

	DEVINST instance_id;
	// Get Instance ID from Instance Path
	e = CM_Locate_DevNode(&instance_id, (DEVINSTID_W) buf, CM_LOCATE_DEVNODE_NORMAL);
	MTY_Free(buf);
	if (e != S_OK) return NULL;

	// Get the instance path of the parent device which is actually.. itself
	buf_size = 0;
	CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Parent, &property_type, NULL, &buf_size, 0);
	if (!buf_size) return NULL;
	buf = MTY_Alloc(buf_size, 1);
	e = CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Parent, &property_type, buf, &buf_size, 0);
	if (e != S_OK) {
		MTY_Free(buf);
		return NULL;
	}
	// Get the instance ID of the actual parent device
	e = CM_Locate_DevNode(&instance_id,(DEVINSTID_W) buf, CM_LOCATE_DEVNODE_NORMAL);
	MTY_Free(buf);
	if (e != S_OK) return NULL;

	// Get the instance path of the parent device
	buf_size = 0;
	CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Parent, &property_type, NULL, &buf_size, 0);
	if (!buf_size) return NULL;
	buf = MTY_Alloc(buf_size, 1);
	e = CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Parent, &property_type, buf, &buf_size, 0);
	if (e != S_OK) {
		MTY_Free(buf);
		return NULL;
	}
	// Get the instance ID of the parent device
	e = CM_Locate_DevNode(&instance_id,(DEVINSTID_W) buf, CM_LOCATE_DEVNODE_NORMAL);
	MTY_Free(buf);
	if (e != S_OK) return NULL;

	// Get a list of the children of the parent device
	buf_size = 0;
	CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Children, &property_type, NULL, &buf_size, 0);
	if (!buf_size) return NULL;
	buf = MTY_Alloc(buf_size, 1);
	e = CM_Get_DevNode_Property(instance_id, &DEVPKEY_Device_Children, &property_type, buf, &buf_size, 0);
	if (e != S_OK) {
		MTY_Free(buf);
		return NULL;
	}

	WCHAR *children = (WCHAR *) buf;
	size_t offset = 0;
	size_t string_length = 0;
	if (e == S_OK && buf_size > 0) {
		bool found = false;
		// Iterate through the children of the parent device to discover the media parent
		while (wcslen(&children[offset])) {
			string_length = wcslen(&children[offset]);
			DEVINST devinst;
			e = CM_Locate_DevNode(&devinst, &children[offset], CM_LOCATE_DEVNODE_NORMAL);
			ULONG node_size = 0;
			PBYTE node_buf = NULL;
			e = CM_Get_DevNode_Property(devinst, &DEVPKEY_Device_Class, &property_type, NULL, &node_size, 0);
			node_buf = MTY_Alloc(node_size, 1);
			e = CM_Get_DevNode_Property(devinst, &DEVPKEY_Device_Class, &property_type, node_buf, &node_size, 0);
			WCHAR *class = (WCHAR *) node_buf;
			// If we find the media parent, iterate through the children of this device to find the speakers
			if (!wcscmp(class, L"MEDIA")) {
				MTY_Free(node_buf);
				found = true;
				// Get a list of the children of the parent device
				node_size = 0;
				CM_Get_DevNode_Property(devinst, &DEVPKEY_Device_Children, &property_type, NULL, &node_size, 0);
				if (!node_size) break;

				node_buf = MTY_Alloc(node_size, 1);
				e = CM_Get_DevNode_Property(devinst, &DEVPKEY_Device_Children, &property_type, node_buf, &node_size, 0);
				if (e != S_OK) {
					MTY_Free(node_buf);
					break;
				}

				WCHAR *media_children = (WCHAR *) node_buf;
				size_t node_offset = 0;
				size_t node_string_length = 0;
				if (e == S_OK && buf_size > 0) {
					while (wcslen(&media_children[node_offset])) {
						node_string_length = wcslen(&media_children[node_offset]);
						DEVINST media_inst;
						e = CM_Locate_DevNode(&media_inst, &media_children[node_offset], CM_LOCATE_DEVNODE_NORMAL);
						if (e != S_OK) break;

						ULONG media_size = 0;
						PBYTE media_buf = NULL;
						CM_Get_DevNode_Property(media_inst, &DEVPKEY_Device_FriendlyName, &property_type, NULL, &media_size, 0);
						if (!media_size) break;
						media_buf = MTY_Alloc(media_size, 1);
						e = CM_Get_DevNode_Property(media_inst, &DEVPKEY_Device_FriendlyName, &property_type, media_buf, &media_size, 0);
						if (e != S_OK) {
							MTY_Free(media_buf);
							break;
						}
						WCHAR *media_class = (WCHAR *) media_buf;
						if (wcsstr(media_class, L"Speakers")) {
							printf ("I found the device: %S\n", &media_children[node_offset]);
							WCHAR *string_iterator = NULL;
							// Truncate everything prior to the last instance of "\"
							string_iterator = wcsstr(&media_children[node_offset], L"\\");
							WCHAR *guid_w = NULL;
							while (string_iterator) {
								string_iterator += 1;
								guid_w = string_iterator;
								string_iterator = wcsstr(string_iterator, L"\\");
							}
							if (guid_w) {
								printf ("GUID is %S\n", guid_w);
								haptics = MTY_WideToMultiDL(guid_w);
								MTY_Free(media_buf);
								break;
							}

						}
						MTY_Free(media_buf);
						node_offset += node_string_length + 1;
					}
				MTY_Free(node_buf);
				break;
				}
			}
			MTY_Free(node_buf);

			offset += wcslen(&children[offset]) + 1;
		}
	}

	MTY_Free(buf);

	return haptics;
}
