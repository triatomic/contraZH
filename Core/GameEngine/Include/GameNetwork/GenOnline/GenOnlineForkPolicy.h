#pragma once

// The fork's policy switches for the GenOnline client.

// 1 lets the client download and launch the GenOnline patcher, then quit, when the version check asks for an update
#define GENONLINE_FORK_SELF_UPDATE 0

// 1 sends the machine GUID, MAC address and volume serial at sign-in and lists the loaded modules for a module scan
#define GENONLINE_FORK_HW_FINGERPRINT 0

// 1 ignores the Options.ini texture filter, anisotropy and anti-aliasing as GenOnline does
#define GENONLINE_FORK_NO_TEXTURE_FILTER_OPTIONS 0
