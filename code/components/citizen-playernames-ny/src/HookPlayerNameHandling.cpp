/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"

#include <FontRenderer.h>

#include <NetLibrary.h>
#include <ResourceManager.h>
#include <ResourceEventComponent.h>

#include <Screen.h>

#include <msgpack.hpp>

#include <CoreNetworking.h>
#include <DrawCommands.h>
#include <CPlayerInfo.h>

static NetLibrary* g_netLibrary;
static std::unordered_map<int, std::string> g_netIdToNames;
static std::unordered_map<int, std::string> g_netIdToEmojiNames;

static int GetPlayerInfoNetId(CPlayerInfo* playerInfo)
{
	int netId = (playerInfo->GetGamerInfo()->peerAddress.localAddr.ip.addr & 0xFFFF) ^ 0xFEED;

	return netId;
}

static const char* __fastcall CPlayerInfo__GetName(CPlayerInfo* playerInfo)
{
	int netId = GetPlayerInfoNetId(playerInfo);

	return g_netIdToNames[netId].c_str();
}

static std::string AddRandomEmoji(const std::string& name);

static const char* CPlayerInfo__GetEmojiName(CPlayerInfo* playerInfo)
{
	int netId = GetPlayerInfoNetId(playerInfo);

	auto emojiNameIt = g_netIdToEmojiNames.find(netId);

	if (emojiNameIt == g_netIdToEmojiNames.end())
	{
		auto pair = g_netIdToEmojiNames.insert(std::make_pair(netId, AddRandomEmoji(g_netIdToNames[netId])));

		emojiNameIt = pair.first;
	}
	
	return emojiNameIt->second.c_str();
}

static CRGBA g_lastColor;
static void* g_lastPlayer;
static uint32_t g_drawnNameBitfield;
static int g_lastAlignment;

void DrawNetworkNameText(float x, float y, const wchar_t* text, int, int)
{
	// find a player info with this ped
	for (int i = 0; i < 32; i++)
	{
		auto player = CPlayerInfo::GetPlayer(i);

		if (player && player->GetPed() == g_lastPlayer)
		{
			if ((g_drawnNameBitfield & (1 << i)) == 0)
			{
				static float fontSize = round((28.0f / 1440.0f) * GetScreenResolutionY());

				g_drawnNameBitfield |= (1 << i);

				wchar_t wideStr[512];
				MultiByteToWideChar(CP_UTF8, 0, CPlayerInfo__GetEmojiName(player), -1, wideStr, _countof(wideStr));

				x *= GetScreenResolutionX();
				y *= GetScreenResolutionY();

				float offsetX = 0;

				if (g_lastAlignment == 0) // center
				{
					CRect outRect;
					TheFonts->GetStringMetrics(wideStr, fontSize, 1.0f, "Segoe UI", outRect);

					offsetX = outRect.Width() / 2;
				}

				CRect rect(round(x - offsetX), y, x + 300, y + 40);

				TheFonts->DrawText(wideStr, rect, g_lastColor, fontSize, 1.0f, "Segoe UI");

				return;
			}
		}
	}
}

void __declspec(naked) DrawNetworkNameTextStub()
{
	__asm
	{
		mov g_lastPlayer, edi

		jmp DrawNetworkNameText
	}
}

void DrawNetworkNameSetColor(CRGBA color)
{
	g_lastColor = color;

	// BGR swap the color
	uint8_t tempVar = g_lastColor.red;
	g_lastColor.red = g_lastColor.blue;
	g_lastColor.blue = tempVar;
}

void DrawNetworkNameSetAlignment(int alignment)
{
	g_lastAlignment = alignment;
}

static HookFunction hookFunction([] ()
{
	// CPlayerInfo non-inlined function
	hook::jump(hook::get_call(hook::get_pattern("33 F6 8B C8 89 74 24 14", 8)), CPlayerInfo__GetName);

	// temp dbg: also show network player name for local player
#if _DEBUG
	//hook::nop(hook::get_pattern("3B 7C 24 54 0F 84", 4), 6);
#endif

	// network name CFont::SetColour
	hook::call(hook::get_pattern("0F 85 ? ? ? ? 50 E8 ? ? ? ? 64 A1", 7), DrawNetworkNameSetColor);

	// network name alignment setting
	hook::call(hook::get_pattern("F3 0F 11 44 24 30 6A 01 E8", 8), DrawNetworkNameSetAlignment);

	// ignore ep2 check in network name drawing
	hook::put<uint16_t>(hook::get_pattern("83 3D ? ? ? ? 02 0F 85 ? ? ? ? 8B 87", 7), 0xE990);

	// network name text draw call
	hook::call(hook::get_pattern("F3 0F 11 4C 24 30 F3 0F 11 0C 24 E8 ? ? ? ? 83 C4", 11), DrawNetworkNameTextStub);
});

static bool CheckForEmoji(const std::string& name)
{
	// convert to codepoints
	std::codecvt_utf8<char32_t> converter;
	std::mbstate_t mb;

	std::vector<char32_t> outArray(name.size());

	const char* inMid;
	char32_t* outMid;
	converter.in(mb, &name[0], &name[name.size()], inMid, &outArray[0], &outArray[outArray.size()], outMid);

	outArray.resize(outMid - &outArray[0]);

	// check codepoint array for emoji
	bool containsEmoji = false;

	for (auto i : outArray)
	{
		if (i >= 0x1F300 && i <= 0x1F6FF) 
		{
			containsEmoji = true;
			break;
		}
	}

	return containsEmoji;
}

static std::string AddRandomEmoji(const std::string& name) 
{
#pragma region emojiArray
	const char* emojiBits[] = {
		"\xF0\x9F\x98\x81",
		"\xF0\x9F\x98\x82",
		"\xF0\x9F\x98\x83",
		"\xF0\x9F\x98\x84",
		"\xF0\x9F\x98\x85",
		"\xF0\x9F\x98\x86",
		"\xF0\x9F\x98\x89",
		"\xF0\x9F\x98\x8A",
		"\xF0\x9F\x98\x8B",
		"\xF0\x9F\x98\x8C",
		"\xF0\x9F\x98\x8D",
		"\xF0\x9F\x98\x8F",
		"\xF0\x9F\x98\x92",
		"\xF0\x9F\x98\x93",
		"\xF0\x9F\x98\x94",
		"\xF0\x9F\x98\x96",
		"\xF0\x9F\x98\x98",
		"\xF0\x9F\x98\x9A",
		"\xF0\x9F\x98\x9C",
		"\xF0\x9F\x98\x9D",
		"\xF0\x9F\x98\x9E",
		"\xF0\x9F\x98\xA0",
		"\xF0\x9F\x98\xA1",
		"\xF0\x9F\x98\xA2",
		"\xF0\x9F\x98\xA3",
		"\xF0\x9F\x98\xA4",
		"\xF0\x9F\x98\xA5",
		"\xF0\x9F\x98\xA8",
		"\xF0\x9F\x98\xA9",
		"\xF0\x9F\x98\xAA",
		"\xF0\x9F\x98\xAB",
		"\xF0\x9F\x98\xAD",
		"\xF0\x9F\x98\xB0",
		"\xF0\x9F\x98\xB1",
		"\xF0\x9F\x98\xB2",
		"\xF0\x9F\x98\xB3",
		"\xF0\x9F\x98\xB5",
		"\xF0\x9F\x98\xB7",
		"\xF0\x9F\x98\xB8",
		"\xF0\x9F\x98\xB9",
		"\xF0\x9F\x98\xBA",
		"\xF0\x9F\x98\xBB",
		"\xF0\x9F\x98\xBC",
		"\xF0\x9F\x98\xBD",
		"\xF0\x9F\x98\xBE",
		"\xF0\x9F\x98\xBF",
		"\xF0\x9F\x99\x80",
		"\xF0\x9F\x99\x85",
		"\xF0\x9F\x99\x86",
		"\xF0\x9F\x99\x87",
		"\xF0\x9F\x99\x88",
		"\xF0\x9F\x99\x89",
		"\xF0\x9F\x99\x8A",
		"\xF0\x9F\x99\x8B",
		"\xF0\x9F\x99\x8C",
		"\xF0\x9F\x99\x8D",
		"\xF0\x9F\x99\x8E",
		"\xF0\x9F\x99\x8F",
		"\xE2\x9C\x82",
		"\xE2\x9C\x85",
		"\xE2\x9C\x88",
		"\xE2\x9C\x89",
		"\xE2\x9C\x8A",
		"\xE2\x9C\x8B",
		"\xE2\x9C\x8C",
		"\xE2\x9C\x8F",
		"\xE2\x9C\x92",
		"\xE2\x9C\x94",
		"\xE2\x9C\x96",
		"\xE2\x9C\xA8",
		"\xE2\x9C\xB3",
		"\xE2\x9C\xB4",
		"\xE2\x9D\x84",
		"\xE2\x9D\x87",
		"\xE2\x9D\x8C",
		"\xE2\x9D\x8E",
		"\xE2\x9D\x93",
		"\xE2\x9D\x94",
		"\xE2\x9D\x95",
		"\xE2\x9D\x97",
		"\xE2\x9D\xA4",
		"\xE2\x9E\x95",
		"\xE2\x9E\x96",
		"\xE2\x9E\x97",
		"\xE2\x9E\xA1",
		"\xE2\x9E\xB0",
		"\xF0\x9F\x9A\x80",
		"\xF0\x9F\x9A\x83",
		"\xF0\x9F\x9A\x84",
		"\xF0\x9F\x9A\x85",
		"\xF0\x9F\x9A\x87",
		"\xF0\x9F\x9A\x89",
		"\xF0\x9F\x9A\x8C",
		"\xF0\x9F\x9A\x8F",
		"\xF0\x9F\x9A\x91",
		"\xF0\x9F\x9A\x92",
		"\xF0\x9F\x9A\x93",
		"\xF0\x9F\x9A\x95",
		"\xF0\x9F\x9A\x97",
		"\xF0\x9F\x9A\x99",
		"\xF0\x9F\x9A\x9A",
		"\xF0\x9F\x9A\xA2",
		"\xF0\x9F\x9A\xA4",
		"\xF0\x9F\x9A\xA5",
		"\xF0\x9F\x9A\xA7",
		"\xF0\x9F\x9A\xA8",
		"\xF0\x9F\x9A\xA9",
		"\xF0\x9F\x9A\xAA",
		"\xF0\x9F\x9A\xAB",
		"\xF0\x9F\x9A\xAC",
		"\xF0\x9F\x9A\xAD",
		"\xF0\x9F\x9A\xB2",
		"\xF0\x9F\x9A\xB6",
		"\xF0\x9F\x9A\xB9",
		"\xF0\x9F\x9A\xBA",
		"\xF0\x9F\x9A\xBB",
		"\xF0\x9F\x9A\xBC",
		"\xF0\x9F\x9A\xBD",
		"\xF0\x9F\x9A\xBE",
		"\xF0\x9F\x9B\x80",
		"\xE2\x93\x82",
		"\xF0\x9F\x85\xB0",
		"\xF0\x9F\x85\xB1",
		"\xF0\x9F\x85\xBE",
		"\xF0\x9F\x85\xBF",
		"\xF0\x9F\x86\x8E",
		"\xF0\x9F\x86\x91",
		"\xF0\x9F\x86\x92",
		"\xF0\x9F\x86\x93",
		"\xF0\x9F\x86\x94",
		"\xF0\x9F\x86\x95",
		"\xF0\x9F\x86\x96",
		"\xF0\x9F\x86\x97",
		"\xF0\x9F\x86\x98",
		"\xF0\x9F\x86\x99",
		"\xF0\x9F\x86\x9A",
		"\xF0\x9F\x88\x81",
		"\xF0\x9F\x88\x82",
		"\xF0\x9F\x88\x9A",
		"\xF0\x9F\x88\xAF",
		"\xF0\x9F\x88\xB2",
		"\xF0\x9F\x88\xB3",
		"\xF0\x9F\x88\xB4",
		"\xF0\x9F\x88\xB5",
		"\xF0\x9F\x88\xB6",
		"\xF0\x9F\x88\xB7",
		"\xF0\x9F\x88\xB8",
		"\xF0\x9F\x88\xB9",
		"\xF0\x9F\x88\xBA",
		"\xF0\x9F\x89\x90",
		"\xF0\x9F\x89\x91",
		"\xC2\xA9",
		"\xC2\xAE",
		"\xE2\x80\xBC",
		"\xE2\x81\x89",
		"\x38\xE2\x83\xA3",
		"\x39\xE2\x83\xA3",
		"\x37\xE2\x83\xA3",
		"\x36\xE2\x83\xA3",
		"\x31\xE2\x83\xA3",
		"\x30\xE2\x83\xA3",
		"\x32\xE2\x83\xA3",
		"\x33\xE2\x83\xA3",
		"\x35\xE2\x83\xA3",
		"\x34\xE2\x83\xA3",
		"\x23\xE2\x83\xA3",
		"\xE2\x84\xA2",
		"\xE2\x84\xB9",
		"\xE2\x86\x94",
		"\xE2\x86\x95",
		"\xE2\x86\x96",
		"\xE2\x86\x97",
		"\xE2\x86\x98",
		"\xE2\x86\x99",
		"\xE2\x86\xA9",
		"\xE2\x86\xAA",
		"\xE2\x8C\x9A",
		"\xE2\x8C\x9B",
		"\xE2\x8F\xA9",
		"\xE2\x8F\xAA",
		"\xE2\x8F\xAB",
		"\xE2\x8F\xAC",
		"\xE2\x8F\xB0",
		"\xE2\x8F\xB3",
		"\xE2\x96\xAA",
		"\xE2\x96\xAB",
		"\xE2\x96\xB6",
		"\xE2\x97\x80",
		"\xE2\x97\xBB",
		"\xE2\x97\xBC",
		"\xE2\x97\xBD",
		"\xE2\x97\xBE",
		"\xE2\x98\x80",
		"\xE2\x98\x81",
		"\xE2\x98\x8E",
		"\xE2\x98\x91",
		"\xE2\x98\x94",
		"\xE2\x98\x95",
		"\xE2\x98\x9D",
		"\xE2\x98\xBA",
		"\xE2\x99\x88",
		"\xE2\x99\x89",
		"\xE2\x99\x8A",
		"\xE2\x99\x8B",
		"\xE2\x99\x8C",
		"\xE2\x99\x8D",
		"\xE2\x99\x8E",
		"\xE2\x99\x8F",
		"\xE2\x99\x90",
		"\xE2\x99\x91",
		"\xE2\x99\x92",
		"\xE2\x99\x93",
		"\xE2\x99\xA0",
		"\xE2\x99\xA3",
		"\xE2\x99\xA5",
		"\xE2\x99\xA6",
		"\xE2\x99\xA8",
		"\xE2\x99\xBB",
		"\xE2\x99\xBF",
		"\xE2\x9A\x93",
		"\xE2\x9A\xA0",
		"\xE2\x9A\xA1",
		"\xE2\x9A\xAA",
		"\xE2\x9A\xAB",
		"\xE2\x9A\xBD",
		"\xE2\x9A\xBE",
		"\xE2\x9B\x84",
		"\xE2\x9B\x85",
		"\xE2\x9B\x8E",
		"\xE2\x9B\x94",
		"\xE2\x9B\xAA",
		"\xE2\x9B\xB2",
		"\xE2\x9B\xB3",
		"\xE2\x9B\xB5",
		"\xE2\x9B\xBA",
		"\xE2\x9B\xBD",
		"\xE2\xA4\xB4",
		"\xE2\xA4\xB5",
		"\xE2\xAC\x85",
		"\xE2\xAC\x86",
		"\xE2\xAC\x87",
		"\xE2\xAC\x9B",
		"\xE2\xAC\x9C",
		"\xE2\xAD\x90",
		"\xE2\xAD\x95",
		"\xE3\x80\xB0",
		"\xE3\x80\xBD",
		"\xE3\x8A\x97",
		"\xE3\x8A\x99",
		"\xF0\x9F\x80\x84",
		"\xF0\x9F\x83\x8F",
		"\xF0\x9F\x8C\x80",
		"\xF0\x9F\x8C\x81",
		"\xF0\x9F\x8C\x82",
		"\xF0\x9F\x8C\x83",
		"\xF0\x9F\x8C\x84",
		"\xF0\x9F\x8C\x85",
		"\xF0\x9F\x8C\x86",
		"\xF0\x9F\x8C\x87",
		"\xF0\x9F\x8C\x88",
		"\xF0\x9F\x8C\x89",
		"\xF0\x9F\x8C\x8A",
		"\xF0\x9F\x8C\x8B",
		"\xF0\x9F\x8C\x8C",
		"\xF0\x9F\x8C\x8F",
		"\xF0\x9F\x8C\x91",
		"\xF0\x9F\x8C\x93",
		"\xF0\x9F\x8C\x94",
		"\xF0\x9F\x8C\x95",
		"\xF0\x9F\x8C\x99",
		"\xF0\x9F\x8C\x9B",
		"\xF0\x9F\x8C\x9F",
		"\xF0\x9F\x8C\xA0",
		"\xF0\x9F\x8C\xB0",
		"\xF0\x9F\x8C\xB1",
		"\xF0\x9F\x8C\xB4",
		"\xF0\x9F\x8C\xB5",
		"\xF0\x9F\x8C\xB7",
		"\xF0\x9F\x8C\xB8",
		"\xF0\x9F\x8C\xB9",
		"\xF0\x9F\x8C\xBA",
		"\xF0\x9F\x8C\xBB",
		"\xF0\x9F\x8C\xBC",
		"\xF0\x9F\x8C\xBD",
		"\xF0\x9F\x8C\xBE",
		"\xF0\x9F\x8C\xBF",
		"\xF0\x9F\x8D\x80",
		"\xF0\x9F\x8D\x81",
		"\xF0\x9F\x8D\x82",
		"\xF0\x9F\x8D\x83",
		"\xF0\x9F\x8D\x84",
		"\xF0\x9F\x8D\x85",
		"\xF0\x9F\x8D\x86",
		"\xF0\x9F\x8D\x87",
		"\xF0\x9F\x8D\x88",
		"\xF0\x9F\x8D\x89",
		"\xF0\x9F\x8D\x8A",
		"\xF0\x9F\x8D\x8C",
		"\xF0\x9F\x8D\x8D",
		"\xF0\x9F\x8D\x8E",
		"\xF0\x9F\x8D\x8F",
		"\xF0\x9F\x8D\x91",
		"\xF0\x9F\x8D\x92",
		"\xF0\x9F\x8D\x93",
		"\xF0\x9F\x8D\x94",
		"\xF0\x9F\x8D\x95",
		"\xF0\x9F\x8D\x96",
		"\xF0\x9F\x8D\x97",
		"\xF0\x9F\x8D\x98",
		"\xF0\x9F\x8D\x99",
		"\xF0\x9F\x8D\x9A",
		"\xF0\x9F\x8D\x9B",
		"\xF0\x9F\x8D\x9C",
		"\xF0\x9F\x8D\x9D",
		"\xF0\x9F\x8D\x9E",
		"\xF0\x9F\x8D\x9F",
		"\xF0\x9F\x8D\xA0",
		"\xF0\x9F\x8D\xA1",
		"\xF0\x9F\x8D\xA2",
		"\xF0\x9F\x8D\xA3",
		"\xF0\x9F\x8D\xA4",
		"\xF0\x9F\x8D\xA5",
		"\xF0\x9F\x8D\xA6",
		"\xF0\x9F\x8D\xA7",
		"\xF0\x9F\x8D\xA8",
		"\xF0\x9F\x8D\xA9",
		"\xF0\x9F\x8D\xAA",
		"\xF0\x9F\x8D\xAB",
		"\xF0\x9F\x8D\xAC",
		"\xF0\x9F\x8D\xAD",
		"\xF0\x9F\x8D\xAE",
		"\xF0\x9F\x8D\xAF",
		"\xF0\x9F\x8D\xB0",
		"\xF0\x9F\x8D\xB1",
		"\xF0\x9F\x8D\xB2",
		"\xF0\x9F\x8D\xB3",
		"\xF0\x9F\x8D\xB4",
		"\xF0\x9F\x8D\xB5",
		"\xF0\x9F\x8D\xB6",
		"\xF0\x9F\x8D\xB7",
		"\xF0\x9F\x8D\xB8",
		"\xF0\x9F\x8D\xB9",
		"\xF0\x9F\x8D\xBA",
		"\xF0\x9F\x8D\xBB",
		"\xF0\x9F\x8E\x80",
		"\xF0\x9F\x8E\x81",
		"\xF0\x9F\x8E\x82",
		"\xF0\x9F\x8E\x83",
		"\xF0\x9F\x8E\x84",
		"\xF0\x9F\x8E\x85",
		"\xF0\x9F\x8E\x86",
		"\xF0\x9F\x8E\x87",
		"\xF0\x9F\x8E\x88",
		"\xF0\x9F\x8E\x89",
		"\xF0\x9F\x8E\x8A",
		"\xF0\x9F\x8E\x8B",
		"\xF0\x9F\x8E\x8C",
		"\xF0\x9F\x8E\x8D",
		"\xF0\x9F\x8E\x8E",
		"\xF0\x9F\x8E\x8F",
		"\xF0\x9F\x8E\x90",
		"\xF0\x9F\x8E\x91",
		"\xF0\x9F\x8E\x92",
		"\xF0\x9F\x8E\x93",
		"\xF0\x9F\x8E\xA0",
		"\xF0\x9F\x8E\xA1",
		"\xF0\x9F\x8E\xA2",
		"\xF0\x9F\x8E\xA3",
		"\xF0\x9F\x8E\xA4",
		"\xF0\x9F\x8E\xA5",
		"\xF0\x9F\x8E\xA6",
		"\xF0\x9F\x8E\xA7",
		"\xF0\x9F\x8E\xA8",
		"\xF0\x9F\x8E\xA9",
		"\xF0\x9F\x8E\xAA",
		"\xF0\x9F\x8E\xAB",
		"\xF0\x9F\x8E\xAC",
		"\xF0\x9F\x8E\xAD",
		"\xF0\x9F\x8E\xAE",
		"\xF0\x9F\x8E\xAF",
		"\xF0\x9F\x8E\xB0",
		"\xF0\x9F\x8E\xB1",
		"\xF0\x9F\x8E\xB2",
		"\xF0\x9F\x8E\xB3",
		"\xF0\x9F\x8E\xB4",
		"\xF0\x9F\x8E\xB5",
		"\xF0\x9F\x8E\xB6",
		"\xF0\x9F\x8E\xB7",
		"\xF0\x9F\x8E\xB8",
		"\xF0\x9F\x8E\xB9",
		"\xF0\x9F\x8E\xBA",
		"\xF0\x9F\x8E\xBB",
		"\xF0\x9F\x8E\xBC",
		"\xF0\x9F\x8E\xBD",
		"\xF0\x9F\x8E\xBE",
		"\xF0\x9F\x8E\xBF",
		"\xF0\x9F\x8F\x80",
		"\xF0\x9F\x8F\x81",
		"\xF0\x9F\x8F\x82",
		"\xF0\x9F\x8F\x83",
		"\xF0\x9F\x8F\x84",
		"\xF0\x9F\x8F\x86",
		"\xF0\x9F\x8F\x88",
		"\xF0\x9F\x8F\x8A",
		"\xF0\x9F\x8F\xA0",
		"\xF0\x9F\x8F\xA1",
		"\xF0\x9F\x8F\xA2",
		"\xF0\x9F\x8F\xA3",
		"\xF0\x9F\x8F\xA5",
		"\xF0\x9F\x8F\xA6",
		"\xF0\x9F\x8F\xA7",
		"\xF0\x9F\x8F\xA8",
		"\xF0\x9F\x8F\xA9",
		"\xF0\x9F\x8F\xAA",
		"\xF0\x9F\x8F\xAB",
		"\xF0\x9F\x8F\xAC",
		"\xF0\x9F\x8F\xAD",
		"\xF0\x9F\x8F\xAE",
		"\xF0\x9F\x8F\xAF",
		"\xF0\x9F\x8F\xB0",
		"\xF0\x9F\x90\x8C",
		"\xF0\x9F\x90\x8D",
		"\xF0\x9F\x90\x8E",
		"\xF0\x9F\x90\x91",
		"\xF0\x9F\x90\x92",
		"\xF0\x9F\x90\x94",
		"\xF0\x9F\x90\x97",
		"\xF0\x9F\x90\x98",
		"\xF0\x9F\x90\x99",
		"\xF0\x9F\x90\x9A",
		"\xF0\x9F\x90\x9B",
		"\xF0\x9F\x90\x9C",
		"\xF0\x9F\x90\x9D",
		"\xF0\x9F\x90\x9E",
		"\xF0\x9F\x90\x9F",
		"\xF0\x9F\x90\xA0",
		"\xF0\x9F\x90\xA1",
		"\xF0\x9F\x90\xA2",
		"\xF0\x9F\x90\xA3",
		"\xF0\x9F\x90\xA4",
		"\xF0\x9F\x90\xA5",
		"\xF0\x9F\x90\xA6",
		"\xF0\x9F\x90\xA7",
		"\xF0\x9F\x90\xA8",
		"\xF0\x9F\x90\xA9",
		"\xF0\x9F\x90\xAB",
		"\xF0\x9F\x90\xAC",
		"\xF0\x9F\x90\xAD",
		"\xF0\x9F\x90\xAE",
		"\xF0\x9F\x90\xAF",
		"\xF0\x9F\x90\xB0",
		"\xF0\x9F\x90\xB1",
		"\xF0\x9F\x90\xB2",
		"\xF0\x9F\x90\xB3",
		"\xF0\x9F\x90\xB4",
		"\xF0\x9F\x90\xB5",
		"\xF0\x9F\x90\xB6",
		"\xF0\x9F\x90\xB7",
		"\xF0\x9F\x90\xB8",
		"\xF0\x9F\x90\xB9",
		"\xF0\x9F\x90\xBA",
		"\xF0\x9F\x90\xBB",
		"\xF0\x9F\x90\xBC",
		"\xF0\x9F\x90\xBD",
		"\xF0\x9F\x90\xBE",
		"\xF0\x9F\x91\x80",
		"\xF0\x9F\x91\x82",
		"\xF0\x9F\x91\x83",
		"\xF0\x9F\x91\x84",
		"\xF0\x9F\x91\x85",
		"\xF0\x9F\x91\x86",
		"\xF0\x9F\x91\x87",
		"\xF0\x9F\x91\x88",
		"\xF0\x9F\x91\x89",
		"\xF0\x9F\x91\x8A",
		"\xF0\x9F\x91\x8B",
		"\xF0\x9F\x91\x8C",
		"\xF0\x9F\x91\x8D",
		"\xF0\x9F\x91\x8E",
		"\xF0\x9F\x91\x8F",
		"\xF0\x9F\x91\x90",
		"\xF0\x9F\x91\x91",
		"\xF0\x9F\x91\x92",
		"\xF0\x9F\x91\x93",
		"\xF0\x9F\x91\x94",
		"\xF0\x9F\x91\x95",
		"\xF0\x9F\x91\x96",
		"\xF0\x9F\x91\x97",
		"\xF0\x9F\x91\x98",
		"\xF0\x9F\x91\x99",
		"\xF0\x9F\x91\x9A",
		"\xF0\x9F\x91\x9B",
		"\xF0\x9F\x91\x9C",
		"\xF0\x9F\x91\x9D",
		"\xF0\x9F\x91\x9E",
		"\xF0\x9F\x91\x9F",
		"\xF0\x9F\x91\xA0",
		"\xF0\x9F\x91\xA1",
		"\xF0\x9F\x91\xA2",
		"\xF0\x9F\x91\xA3",
		"\xF0\x9F\x91\xA4",
		"\xF0\x9F\x91\xA6",
		"\xF0\x9F\x91\xA7",
		"\xF0\x9F\x91\xA8",
		"\xF0\x9F\x91\xA9",
		"\xF0\x9F\x91\xAA",
		"\xF0\x9F\x91\xAB",
		"\xF0\x9F\x91\xAE",
		"\xF0\x9F\x91\xAF",
		"\xF0\x9F\x91\xB0",
		"\xF0\x9F\x91\xB1",
		"\xF0\x9F\x91\xB2",
		"\xF0\x9F\x91\xB3",
		"\xF0\x9F\x91\xB4",
		"\xF0\x9F\x91\xB5",
		"\xF0\x9F\x91\xB6",
		"\xF0\x9F\x91\xB7",
		"\xF0\x9F\x91\xB8",
		"\xF0\x9F\x91\xB9",
		"\xF0\x9F\x91\xBA",
		"\xF0\x9F\x91\xBB",
		"\xF0\x9F\x91\xBC",
		"\xF0\x9F\x91\xBD",
		"\xF0\x9F\x91\xBE",
		"\xF0\x9F\x91\xBF",
		"\xF0\x9F\x92\x80",
		"\xF0\x9F\x92\x81",
		"\xF0\x9F\x92\x82",
		"\xF0\x9F\x92\x83",
		"\xF0\x9F\x92\x84",
		"\xF0\x9F\x92\x85",
		"\xF0\x9F\x92\x86",
		"\xF0\x9F\x92\x87",
		"\xF0\x9F\x92\x88",
		"\xF0\x9F\x92\x89",
		"\xF0\x9F\x92\x8A",
		"\xF0\x9F\x92\x8B",
		"\xF0\x9F\x92\x8C",
		"\xF0\x9F\x92\x8D",
		"\xF0\x9F\x92\x8E",
		"\xF0\x9F\x92\x8F",
		"\xF0\x9F\x92\x90",
		"\xF0\x9F\x92\x91",
		"\xF0\x9F\x92\x92",
		"\xF0\x9F\x92\x93",
		"\xF0\x9F\x92\x94",
		"\xF0\x9F\x92\x95",
		"\xF0\x9F\x92\x96",
		"\xF0\x9F\x92\x97",
		"\xF0\x9F\x92\x98",
		"\xF0\x9F\x92\x99",
		"\xF0\x9F\x92\x9A",
		"\xF0\x9F\x92\x9B",
		"\xF0\x9F\x92\x9C",
		"\xF0\x9F\x92\x9D",
		"\xF0\x9F\x92\x9E",
		"\xF0\x9F\x92\x9F",
		"\xF0\x9F\x92\xA0",
		"\xF0\x9F\x92\xA1",
		"\xF0\x9F\x92\xA2",
		"\xF0\x9F\x92\xA3",
		"\xF0\x9F\x92\xA4",
		"\xF0\x9F\x92\xA5",
		"\xF0\x9F\x92\xA6",
		"\xF0\x9F\x92\xA7",
		"\xF0\x9F\x92\xA8",
		"\xF0\x9F\x92\xA9",
		"\xF0\x9F\x92\xAA",
		"\xF0\x9F\x92\xAB",
		"\xF0\x9F\x92\xAC",
		"\xF0\x9F\x92\xAE",
		"\xF0\x9F\x92\xAF",
		"\xF0\x9F\x92\xB0",
		"\xF0\x9F\x92\xB1",
		"\xF0\x9F\x92\xB2",
		"\xF0\x9F\x92\xB3",
		"\xF0\x9F\x92\xB4",
		"\xF0\x9F\x92\xB5",
		"\xF0\x9F\x92\xB8",
		"\xF0\x9F\x92\xB9",
		"\xF0\x9F\x92\xBA",
		"\xF0\x9F\x92\xBB",
		"\xF0\x9F\x92\xBC",
		"\xF0\x9F\x92\xBD",
		"\xF0\x9F\x92\xBE",
		"\xF0\x9F\x92\xBF",
		"\xF0\x9F\x93\x80",
		"\xF0\x9F\x93\x81",
		"\xF0\x9F\x93\x82",
		"\xF0\x9F\x93\x83",
		"\xF0\x9F\x93\x84",
		"\xF0\x9F\x93\x85",
		"\xF0\x9F\x93\x86",
		"\xF0\x9F\x93\x87",
		"\xF0\x9F\x93\x88",
		"\xF0\x9F\x93\x89",
		"\xF0\x9F\x93\x8A",
		"\xF0\x9F\x93\x8B",
		"\xF0\x9F\x93\x8C",
		"\xF0\x9F\x93\x8D",
		"\xF0\x9F\x93\x8E",
		"\xF0\x9F\x93\x8F",
		"\xF0\x9F\x93\x90",
		"\xF0\x9F\x93\x91",
		"\xF0\x9F\x93\x92",
		"\xF0\x9F\x93\x93",
		"\xF0\x9F\x93\x94",
		"\xF0\x9F\x93\x95",
		"\xF0\x9F\x93\x96",
		"\xF0\x9F\x93\x97",
		"\xF0\x9F\x93\x98",
		"\xF0\x9F\x93\x99",
		"\xF0\x9F\x93\x9A",
		"\xF0\x9F\x93\x9B",
		"\xF0\x9F\x93\x9C",
		"\xF0\x9F\x93\x9D",
		"\xF0\x9F\x93\x9E",
		"\xF0\x9F\x93\x9F",
		"\xF0\x9F\x93\xA0",
		"\xF0\x9F\x93\xA1",
		"\xF0\x9F\x93\xA2",
		"\xF0\x9F\x93\xA3",
		"\xF0\x9F\x93\xA4",
		"\xF0\x9F\x93\xA5",
		"\xF0\x9F\x93\xA6",
		"\xF0\x9F\x93\xA7",
		"\xF0\x9F\x93\xA8",
		"\xF0\x9F\x93\xA9",
		"\xF0\x9F\x93\xAA",
		"\xF0\x9F\x93\xAB",
		"\xF0\x9F\x93\xAE",
		"\xF0\x9F\x93\xB0",
		"\xF0\x9F\x93\xB1",
		"\xF0\x9F\x93\xB2",
		"\xF0\x9F\x93\xB3",
		"\xF0\x9F\x93\xB4",
		"\xF0\x9F\x93\xB6",
		"\xF0\x9F\x93\xB7",
		"\xF0\x9F\x93\xB9",
		"\xF0\x9F\x93\xBA",
		"\xF0\x9F\x93\xBB",
		"\xF0\x9F\x93\xBC",
		"\xF0\x9F\x94\x83",
		"\xF0\x9F\x94\x8A",
		"\xF0\x9F\x94\x8B",
		"\xF0\x9F\x94\x8C",
		"\xF0\x9F\x94\x8D",
		"\xF0\x9F\x94\x8E",
		"\xF0\x9F\x94\x8F",
		"\xF0\x9F\x94\x90",
		"\xF0\x9F\x94\x91",
		"\xF0\x9F\x94\x92",
		"\xF0\x9F\x94\x93",
		"\xF0\x9F\x94\x94",
		"\xF0\x9F\x94\x96",
		"\xF0\x9F\x94\x97",
		"\xF0\x9F\x94\x98",
		"\xF0\x9F\x94\x99",
		"\xF0\x9F\x94\x9A",
		"\xF0\x9F\x94\x9B",
		"\xF0\x9F\x94\x9C",
		"\xF0\x9F\x94\x9D",
		"\xF0\x9F\x94\x9E",
		"\xF0\x9F\x94\x9F",
		"\xF0\x9F\x94\xA0",
		"\xF0\x9F\x94\xA1",
		"\xF0\x9F\x94\xA2",
		"\xF0\x9F\x94\xA3",
		"\xF0\x9F\x94\xA4",
		"\xF0\x9F\x94\xA5",
		"\xF0\x9F\x94\xA6",
		"\xF0\x9F\x94\xA7",
		"\xF0\x9F\x94\xA8",
		"\xF0\x9F\x94\xA9",
		"\xF0\x9F\x94\xAA",
		"\xF0\x9F\x94\xAB",
		"\xF0\x9F\x94\xAE",
		"\xF0\x9F\x94\xAF",
		"\xF0\x9F\x94\xB0",
		"\xF0\x9F\x94\xB1",
		"\xF0\x9F\x94\xB2",
		"\xF0\x9F\x94\xB3",
		"\xF0\x9F\x94\xB4",
		"\xF0\x9F\x94\xB5",
		"\xF0\x9F\x94\xB6",
		"\xF0\x9F\x94\xB7",
		"\xF0\x9F\x94\xB8",
		"\xF0\x9F\x94\xB9",
		"\xF0\x9F\x94\xBA",
		"\xF0\x9F\x94\xBB",
		"\xF0\x9F\x94\xBC",
		"\xF0\x9F\x94\xBD",
		"\xF0\x9F\x95\x90",
		"\xF0\x9F\x95\x91",
		"\xF0\x9F\x95\x92",
		"\xF0\x9F\x95\x93",
		"\xF0\x9F\x95\x94",
		"\xF0\x9F\x95\x95",
		"\xF0\x9F\x95\x96",
		"\xF0\x9F\x95\x97",
		"\xF0\x9F\x95\x98",
		"\xF0\x9F\x95\x99",
		"\xF0\x9F\x95\x9A",
		"\xF0\x9F\x95\x9B",
		"\xF0\x9F\x97\xBB",
		"\xF0\x9F\x97\xBC",
		"\xF0\x9F\x97\xBD",
		"\xF0\x9F\x97\xBE",
		"\xF0\x9F\x97\xBF",
		"\xF0\x9F\x98\x80",
		"\xF0\x9F\x98\x87",
		"\xF0\x9F\x98\x88",
		"\xF0\x9F\x98\x8E",
		"\xF0\x9F\x98\x90",
		"\xF0\x9F\x98\x91",
		"\xF0\x9F\x98\x95",
		"\xF0\x9F\x98\x97",
		"\xF0\x9F\x98\x99",
		"\xF0\x9F\x98\x9B",
		"\xF0\x9F\x98\x9F",
		"\xF0\x9F\x98\xA6",
		"\xF0\x9F\x98\xA7",
		"\xF0\x9F\x98\xAC",
		"\xF0\x9F\x98\xAE",
		"\xF0\x9F\x98\xAF",
		"\xF0\x9F\x98\xB4",
		"\xF0\x9F\x98\xB6",
		"\xF0\x9F\x9A\x81",
		"\xF0\x9F\x9A\x82",
		"\xF0\x9F\x9A\x86",
		"\xF0\x9F\x9A\x88",
		"\xF0\x9F\x9A\x8A",
		"\xF0\x9F\x9A\x8D",
		"\xF0\x9F\x9A\x8E",
		"\xF0\x9F\x9A\x90",
		"\xF0\x9F\x9A\x94",
		"\xF0\x9F\x9A\x96",
		"\xF0\x9F\x9A\x98",
		"\xF0\x9F\x9A\x9B",
		"\xF0\x9F\x9A\x9C",
		"\xF0\x9F\x9A\x9D",
		"\xF0\x9F\x9A\x9E",
		"\xF0\x9F\x9A\x9F",
		"\xF0\x9F\x9A\xA0",
		"\xF0\x9F\x9A\xA1",
		"\xF0\x9F\x9A\xA3",
		"\xF0\x9F\x9A\xA6",
		"\xF0\x9F\x9A\xAE",
		"\xF0\x9F\x9A\xAF",
		"\xF0\x9F\x9A\xB0",
		"\xF0\x9F\x9A\xB1",
		"\xF0\x9F\x9A\xB3",
		"\xF0\x9F\x9A\xB4",
		"\xF0\x9F\x9A\xB5",
		"\xF0\x9F\x9A\xB7",
		"\xF0\x9F\x9A\xB8",
		"\xF0\x9F\x9A\xBF",
		"\xF0\x9F\x9B\x81",
		"\xF0\x9F\x9B\x82",
		"\xF0\x9F\x9B\x83",
		"\xF0\x9F\x9B\x84",
		"\xF0\x9F\x9B\x85",
		"\xF0\x9F\x8C\x8D",
		"\xF0\x9F\x8C\x8E",
		"\xF0\x9F\x8C\x90",
		"\xF0\x9F\x8C\x92",
		"\xF0\x9F\x8C\x96",
		"\xF0\x9F\x8C\x97",
		"\xF0\x9F\x8C\x98",
		"\xF0\x9F\x8C\x9A",
		"\xF0\x9F\x8C\x9C",
		"\xF0\x9F\x8C\x9D",
		"\xF0\x9F\x8C\x9E",
		"\xF0\x9F\x8C\xB2",
		"\xF0\x9F\x8C\xB3",
		"\xF0\x9F\x8D\x8B",
		"\xF0\x9F\x8D\x90",
		"\xF0\x9F\x8D\xBC",
		"\xF0\x9F\x8F\x87",
		"\xF0\x9F\x8F\x89",
		"\xF0\x9F\x8F\xA4",
		"\xF0\x9F\x90\x80",
		"\xF0\x9F\x90\x81",
		"\xF0\x9F\x90\x82",
		"\xF0\x9F\x90\x83",
		"\xF0\x9F\x90\x84",
		"\xF0\x9F\x90\x85",
		"\xF0\x9F\x90\x86",
		"\xF0\x9F\x90\x87",
		"\xF0\x9F\x90\x88",
		"\xF0\x9F\x90\x89",
		"\xF0\x9F\x90\x8A",
		"\xF0\x9F\x90\x8B",
		"\xF0\x9F\x90\x8F",
		"\xF0\x9F\x90\x90",
		"\xF0\x9F\x90\x93",
		"\xF0\x9F\x90\x95",
		"\xF0\x9F\x90\x96",
		"\xF0\x9F\x90\xAA",
		"\xF0\x9F\x91\xA5",
		"\xF0\x9F\x91\xAC",
		"\xF0\x9F\x91\xAD",
		"\xF0\x9F\x92\xAD",
		"\xF0\x9F\x92\xB6",
		"\xF0\x9F\x92\xB7",
		"\xF0\x9F\x93\xAC",
		"\xF0\x9F\x93\xAD",
		"\xF0\x9F\x93\xAF",
		"\xF0\x9F\x93\xB5",
		"\xF0\x9F\x94\x80",
		"\xF0\x9F\x94\x81",
		"\xF0\x9F\x94\x82",
		"\xF0\x9F\x94\x84",
		"\xF0\x9F\x94\x85",
		"\xF0\x9F\x94\x86",
		"\xF0\x9F\x94\x87",
		"\xF0\x9F\x94\x89",
		"\xF0\x9F\x94\x95",
		"\xF0\x9F\x94\xAC",
		"\xF0\x9F\x94\xAD",
		"\xF0\x9F\x95\x9C",
		"\xF0\x9F\x95\x9D",
		"\xF0\x9F\x95\x9E",
		"\xF0\x9F\x95\x9F",
		"\xF0\x9F\x95\xA0",
		"\xF0\x9F\x95\xA1",
		"\xF0\x9F\x95\xA2",
		"\xF0\x9F\x95\xA3",
		"\xF0\x9F\x95\xA4",
		"\xF0\x9F\x95\xA5",
		"\xF0\x9F\x95\xA6",
		"\xF0\x9F\x95\xA7",
	};
#pragma endregion

	// add emoji to name (note: in UTF-8 encoding!)
	std::string nameReturn = name;

	if (!CheckForEmoji(name))
	{
		srand(GetTickCount());
		nameReturn = name + emojiBits[rand() % _countof(emojiBits)];
	}

	// return name, or altered name instead
	return nameReturn;
}

static hook::cdecl_stub<void()> _drawPlayerNames([]()
{
	auto loc = hook::get_pattern("83 C4 08 E8 ? ? ? ? E8 ? ? ? ? 83 3D ? ? ? ? 00 74", 8);
	auto temp = hook::get_call(loc);

	// don't do player names at the *right* time as we're too lazy to put our calls in the right place
	// *this is here* as order
	hook::nop(loc, 5);

	return temp;
});

static InitFunction initFunction([] ()
{
	srand(GetTickCount());

	NetLibrary::OnNetLibraryCreate.Connect([] (NetLibrary* netLibrary)
	{
		g_netLibrary = netLibrary;
	});

	fx::ResourceManager::OnInitializeInstance.Connect([](fx::ResourceManager* manager)
	{
		fwRefContainer<fx::ResourceEventManagerComponent> eventComponent = manager->GetComponent<fx::ResourceEventManagerComponent>();

		if (eventComponent.GetRef())
		{
			eventComponent->OnTriggerEvent.Connect([](const std::string& eventName, const std::string& eventPayload, const std::string& eventSource, bool* eventCanceled)
			{
				if (eventName == "onPlayerJoining" || eventName == "onPlayerDropped")
				{
					try
					{
						// deserialize the arguments
						msgpack::unpacked msg;
						msgpack::unpack(msg, eventPayload.c_str(), eventPayload.size());

						msgpack::object obj = msg.get();

						// get the netid/name pair

						// convert to an array
						std::vector<msgpack::object> arguments;
						obj.convert(arguments);

						// get the fields from the dictionary, if existent
							if (arguments.size() >= 2)
						{
							// convert to the concrete types
							int netId = arguments[0].as<int>();
							std::string name = arguments[1].as<std::string>();

							if (eventName == "onPlayerJoining")
							{
								// and add to the list
								g_netIdToNames[netId] = name;
							}

							if (arguments.size() >= 3)
							{
								uint32_t slotId = arguments[2].as<uint32_t>();

								if (slotId != -1)
								{
									// trickle the event down the stack
									NetLibraryClientInfo clientInfo;
									clientInfo.netId = netId;
									clientInfo.name = name;
									clientInfo.slotId = slotId;

									if (eventName == "onPlayerJoining")
									{
										g_netLibrary->OnClientInfoReceived(clientInfo);
									}
									else if (eventName == "onPlayerDropped")
									{
										g_netLibrary->OnClientInfoDropped(clientInfo);
									}
								}
							}
						}
					}
					catch (std::runtime_error & e)
					{
						trace("Failed to unpack onPlayerJoining event: %s\n", e.what());
					}
				}
			});
		}
	}, 500);

	OnPostFrontendRender.Connect([] ()
	{
		g_drawnNameBitfield = 0;

		_drawPlayerNames();
	}, -50);
});
