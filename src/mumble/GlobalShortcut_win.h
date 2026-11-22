// Copyright 2007-2023 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_GLOBALSHORTCUT_WIN_H_
#define MUMBLE_MUMBLE_GLOBALSHORTCUT_WIN_H_

#include "GlobalShortcut.h"

#ifdef USE_XBOXINPUT
#	include "XboxInput.h"
#endif

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <gsl/span>

#include <rigtorp/SPSCQueue.h>

#ifdef USE_GKEY
class GKeyLibrary;
#endif

class GlobalShortcutWin : public GlobalShortcutEngine {
	Q_OBJECT
	Q_DISABLE_COPY(GlobalShortcutWin)

public:
	static constexpr size_t QUEUE_CAPACITY = 64;

	static void registerMetaTypes();

	/// @param oldShortcuts List of shortcuts to migrate.
	/// @returns List of shortcuts in the new format.
	static QList< Shortcut > migrateSettings(const QList< Shortcut > &oldShortcuts);

	/// Inject a native Windows raw input message into GlobalShortcutWin's
	/// thread. This method is meant to be called from the main thread
	/// to pass native Windows keyboard messages to GlobalShortcutWin.
	///
	/// @param msg The raw input handle to inject into GlobalShortcutWin.
	/// @param foreground Whether the input occurred while one of our windows had focus (RIM_INPUT).
	void injectRawInputMessage(HRAWINPUT handle, bool foreground);

	/// Windows doesn't call our low-level hooks for input that goes to our own foreground window,
	/// so suppressible keys arrive there as regular window messages.
	/// This method is meant to be called from the main thread for every native message;
	/// it handles such keys the same way the keyboard hook would. Their raw input is ignored,
	/// so that each key event is handled exactly once.
	///
	/// Limitations while one of our windows has focus: mouse buttons aren't suppressed (they go to
	/// the window under the cursor, which may belong to another application), and neither are keys
	/// that Windows consumes before they reach a window (e.g. Print Screen) or input to non-Qt
	/// windows such as native file dialogs.
	///
	/// @returns Whether the message must be dropped (the key is suppressed).
	bool filterOwnInputMessage(const MSG &msg);

	ButtonInfo buttonInfo(const QVariant &button) override;
	void run() override;
	void needRemap() override;
	bool canSuppress() override;

	GlobalShortcutWin();
	~GlobalShortcutWin() override;
public slots:
	void deviceRemoved(const HANDLE deviceHandle);

protected:
	class MsgRaw {
	public:
		enum Type : uint8_t { Hid, Keyboard, Mouse };

		constexpr Type type() { return m_type; };

		virtual ~MsgRaw() = default;

	protected:
		MsgRaw(const Type type) : m_type(type) {}

	private:
		Type m_type;
	};

	class MsgHid : public MsgRaw {
	public:
		using RawReports = gsl::span< const BYTE >;

		MsgHid(HANDLE deviceHandle, const RawReports rawReports, const DWORD reportSize)
			: MsgRaw(Hid), deviceHandle(deviceHandle), reports(rawReports.size()), reportSize(reportSize) {
			memcpy(reports.data(), rawReports.data(), reports.size());
		}

		HANDLE deviceHandle;
		std::vector< char > reports;
		DWORD reportSize;
	};

	class MsgKeyboard : public MsgRaw {
	public:
		MsgKeyboard(const USHORT flags, const USHORT scanCode, const USHORT virtualKey)
			: MsgRaw(Keyboard), flags(flags), scanCode(scanCode), virtualKey(virtualKey) {}

		USHORT flags;
		USHORT scanCode;
		USHORT virtualKey;
	};

	class MsgMouse : public MsgRaw {
	public:
		MsgMouse(const USHORT buttonFlags) : MsgRaw(Mouse), buttonFlags(buttonFlags) {}

		USHORT buttonFlags;
	};

	struct Device {
		std::string name;
		std::string prefix;
		std::vector< uint8_t > data;
		std::vector< bool > buttons;
		std::pair< uint16_t, uint16_t > usageRange;
#ifdef USE_XBOXINPUT
		bool xinput;
		Device() : xinput(false){};
#endif
	};

	void processMsgHid(MsgHid &msg);
	void processMsgKeyboard(MsgKeyboard &msg);
	void processMsgMouse(const MsgMouse &msg);
	void processOther();

	std::condition_variable m_condVar;
	/// Locked briefly before notifying m_condVar about remap and shutdown requests, so that they can't get lost.
	std::mutex m_waitMutex;

	rigtorp::SPSCQueue< std::unique_ptr< MsgRaw > > m_msgQueue;

	typedef std::unordered_map< HANDLE, Device > DeviceMap;
	DeviceMap m_devices;
#ifdef USE_XBOXINPUT
	std::unique_ptr< XboxInput > m_xinput;
	/// Holds the number of XInput devices available on the system.
	/// It is filled out by addDevice().
	uint8_t m_xinputDevices;
	/// Holds the last packet number that was processed.
	/// Any new data queried for a device is only valid
	/// if the packet number is different than last time we queried it.
	uint32_t m_xinputLastPacket[XBOXINPUT_MAX_DEVICES];

	static bool xinputIsPressed(const uint8_t bit, const XboxInputState &state);
#endif
#ifdef USE_GKEY
	std::unique_ptr< GKeyLibrary > m_gkey;
#endif
	DeviceMap::iterator addDevice(const HANDLE deviceHandle);

	/// Raw input can only observe input, it cannot stop it from reaching other applications.
	/// To implement the "suppress" feature we install low-level keyboard/mouse hooks, but only
	/// while at least one shortcut actually requests suppression, because hooks slow down input
	/// processing for the whole system (see commit 3dc7b00d0).
	/// Note: a hook in a non-elevated process doesn't see input directed to elevated windows,
	/// so suppression doesn't work while one of them has focus (the shortcuts still trigger).
	/// The hooks aren't called for input directed to our own foreground window either,
	/// see filterOwnInputMessage().
	static LRESULT CALLBACK keyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK mouseHookProc(int code, WPARAM wParam, LPARAM lParam);
	static GlobalShortcutWin *s_instance;

	/// Message-only window of the hook thread. filterOwnInputMessage() sends it the input it receives,
	/// so that all suppression decisions are made on the hook thread, as with the hooks themselves.
	static LRESULT CALLBACK hookWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	std::atomic< HWND > m_hookWindow{ nullptr };
	/// Parameters of a WM_OWN_INPUT message sent to m_hookWindow.
	struct OwnInput {
		std::size_t slot;
		QVariant button;
		bool down;
		/// The key's first "down" message, not an auto-repeated one.
		bool firstDown;
	};
	static constexpr UINT WM_OWN_INPUT = WM_APP + 1;
	/// Posted to m_hookWindow to undo the toggle of the lock key given in wParam.
	static constexpr UINT WM_RETOGGLE = WM_APP + 2;
	/// Marks the key events that we inject ourselves (dwExtraInfo), so that we can ignore them.
	static constexpr ULONG_PTR INJECTED_MARKER = 0x4D554D42; // "MUMB"
	bool handleOwnInput(const OwnInput &input);
	/// Called from the hook thread when a key that was pressed in our own window is released.
	void ownWindowKeyReleased(std::size_t slot);

	/// Runs a message loop on a dedicated thread, which (un)installs the hooks and receives their callbacks.
	/// The thread is only created once suppression is needed for the first time.
	void hookThreadMain();
	/// Recomputes m_suppressible and asks the hook thread to (un)install hooks as needed.
	/// Must be called with m_stateMutex held, after remap().
	void updateSuppressionHooks();
	/// Called from the hook thread (hooks and m_hookWindow), only for buttons whose slot is set in m_suppressible.
	/// @returns Whether the input event should be blocked.
	bool handleHookedButton(std::size_t slot, const QVariant &button, bool down);

	/// Protects the button state of GlobalShortcutEngine, which is accessed from both
	/// this engine's thread and the hook thread.
	std::mutex m_stateMutex;
	/// Slots 0 to 0x3FF are keyboard keys (scan code | E0 << 9), the following ones mouse buttons.
	static constexpr std::size_t SUPPRESS_KEYBOARD_SLOTS = 0x400;
	static constexpr std::size_t SUPPRESS_SLOTS          = SUPPRESS_KEYBOARD_SLOTS + 8;
	static constexpr std::size_t SUPPRESS_INVALID_SLOT   = SUPPRESS_SLOTS;
	static std::size_t keyboardSlot(uint16_t code, bool e0);
	static std::size_t mouseSlot(int button);
	/// Whether the button is part of at least one shortcut with suppression enabled.
	/// Atomic so that the hooks can let all other input through without taking m_stateMutex.
	std::array< std::atomic< bool >, SUPPRESS_SLOTS > m_suppressible{};
	/// Suppressed keys whose press went to our own window (hook thread only). Windows saw these presses,
	/// so it considers the keys held and has already toggled Caps/Num/Scroll Lock, which we toggle back
	/// right away. If focus moves elsewhere before the release, the hook lets the key-up through.
	/// Cleared when the shortcuts change.
	std::array< bool, SUPPRESS_KEYBOARD_SLOTS > m_pressedInOwnWindow{};
	bool m_wantKeyboardHook = false;
	bool m_wantMouseHook    = false;
	/// Set if installing a hook failed, in which case we stop offering suppression.
	std::atomic< bool > m_hookFailed{ false };

	std::thread m_hookThread;
	std::atomic< DWORD > m_hookThreadId{ 0 };

private:
	std::map< HANDLE, unsigned int > m_throttleThreshold;
	std::map< HANDLE, unsigned int > m_throttleCounter;

	bool isRateLimited(HANDLE device);
	void tryPlaceEvent(HANDLE device, bool rawhid, std::unique_ptr< MsgRaw > &&event);
};

#endif
