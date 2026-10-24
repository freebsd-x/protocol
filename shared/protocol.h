#pragma once

#include <cstdint>
#include <cstddef>   // offsetof — wire-layout freeze static_asserts

namespace fbx::shared {

enum class CommandType : uint32_t {
    DrawRect = 0,
    DrawText = 1,
    DrawPath = 2,
    SetTransform = 3,
    SetColor = 4,
    SetScissor = 5,
    DrawImage = 6,
    ResetScissor = 7  // No payload — resets scissor to the full window rect.
};

struct DrawCommand {
    CommandType type_;
    uint32_t data_offset_; // Offset into the shared data buffer
    uint32_t data_size_;
};

// Represents a rectangle (now upgraded to a Squircle with shadow) to be drawn by xrender
struct RectData {
    float x_, y_;
    float width_, height_;
    float r_, g_, b_, a_; // Color
    float border_radius_; // New: iOS-style continuous curve radius
    float shadow_blur_;   // New: Gaussian shadow spread
    float shadow_alpha_;  // New: Shadow opacity
};

struct ScissorData {
    float x_, y_;
    float width_, height_;
};

// Represents an image to be drawn
struct ImageData {
    float x_, y_;
    float width_, height_;
    uint32_t texture_id_; // Represents a server-side uploaded texture
    const uint8_t* pixel_ptr_; // New: Process-local pixel data pointer
    uint32_t pixel_width_;
    uint32_t pixel_height_;
    float uv_x0_ = 0.0f, uv_y0_ = 0.0f; // texture UV top-left  (default: full image)
    float uv_x1_ = 1.0f, uv_y1_ = 1.0f; // texture UV bottom-right
};

// Represents text to be rendered via SDF
struct TextData {
    float x_, y_;
    float font_size_;
    float r_, g_, b_, a_;
    uint32_t string_length_;
    // Font selection — all zero/empty means "default UI font, Regular, Normal"
    uint16_t font_weight_;     // CSS weight: 100=Thin … 400=Regular … 700=Bold … 900=Black; 0 = default (400)
    uint8_t  font_style_;      // 0=Normal, 1=Italic, 2=Oblique
    uint8_t  _font_pad;
    char     font_family_[32]; // null-terminated family name; all-zero = default UI font
    // The actual UTF-8 character string immediately follows this struct in the buffer.
};

enum class PathOpType : uint32_t {
    MoveTo = 0,
    LineTo = 1,
    QuadTo = 2,
    CubicTo = 3,
    Close = 4
};

struct PathOp {
    PathOpType type_;
    float points_[6]; // x1, y1, x2, y2, x, y (up to 3 points)
};

// Represents a complex vector path to be drawn by the GPU
struct PathData {
    float x_, y_;
    float r_, g_, b_, a_;
    uint32_t num_ops_;
    // Followed by num_ops_ * PathOp
};

// IPC Input Routing Protocol
enum class InputEventType : uint32_t {
    MouseMove = 0,
    MouseButton = 1,
    KeyPress = 2,
    MouseWheel = 3,
    ThemeChanged = 4,
    WorkspaceSwitch = 5,
    SearchToggle = 6,
    DashboardToggle = 7,
    GestureSwipeBegin = 8,
    GestureSwipeUpdate = 9,
    GestureSwipeEnd = 10,
    MoveWindowToOutput = 11, // value_b_: -1 = move left, +1 = move right
    PointerLockChanged = 12, // value_a_: 1 = locked (hide cursor), 0 = unlocked (show cursor)
    CycleFocus         = 13, // Alt+Tab: value_b_: 1=forward, -1=backward
    SwitcherDismiss    = 14, // Alt key released — hide the switcher overlay
    SnapWindow         = 15, // Super+Up/Down: value_a_: 0=maximize, 2=restore

    // ── Touch (direct-touch screens) ─────────────────────────────────────────
    // value_a_ = x (absolute pixels), value_b_ = y, modifiers_ = seat_slot index
    TouchDown  = 16,
    TouchMove  = 17,
    TouchUp    = 18,  // value_a_/b_ = 0; modifiers_ = seat_slot

    // ── Stylus / tablet tool ─────────────────────────────────────────────────
    // value_a_ = x (pixels), value_b_ = y
    // modifiers_ = pressure [0..65535]  (0 = no pressure, 65535 = full)
    // char_value_ = tilt packed: high-16 = tilt_x*10, low-16 = tilt_y*10 (signed, degrees)
    StylusDown      = 19,
    StylusMove      = 20,
    StylusUp        = 21,
    StylusProximity = 22, // value_a_ = 1 (near) / 0 (left); modifiers_ = tool (0=pen,1=eraser,2=brush)

    // ── Pinch gesture (compositor-synthesised from multi-touch) ─────────────
    // GesturePinchBegin:  value_a_ = finger count
    // GesturePinchUpdate: value_a_ = dx*100, value_b_ = dy*100, modifiers_ = scale*1000
    // GesturePinchEnd:    value_a_ = 1 if cancelled, 0 if completed
    GesturePinchBegin  = 23,
    GesturePinchUpdate = 24,
    GesturePinchEnd    = 25,

    // ── IME / Composition (compositor → focused app) ─────────────────────────
    // UTF-8 payload encoding shared by ImePreedit and ImeCommit:
    //   value_a_ = bytes 0-3 of UTF-8 string (raw byte view of int32_t)
    //   value_b_ = bytes 4-7 of UTF-8 string
    //   modifiers_ = byte length (0 = empty / clear for ImePreedit)
    //   char_value_ = 0 (unused)
    ImePreedit = 26, // update inline composition display; modifiers_=0 clears preedit
    ImeCommit  = 27, // insert committed UTF-8 text into the focused text field

    // ── Hardware media keys (consumed by compositor; not forwarded to apps) ──
    // value_a_ = step size in percent (positive = up, negative = down)
    BrightnessUp   = 28, // KEY_BRIGHTNESSUP (225)
    BrightnessDown = 29, // KEY_BRIGHTNESSDOWN (224)

    // ── Audio media keys (consumed by compositor; not forwarded to apps) ──────
    // AudioVolumeUp/Down: value_a_ = step size in percent (default 10)
    // AudioMute:          value_a_ = 0 (toggle)
    AudioVolumeUp   = 32, // KEY_VOLUMEUP (115)
    AudioVolumeDown = 33, // KEY_VOLUMEDOWN (114)
    AudioMute       = 34, // KEY_MUTE (113)

    // ── Screenshot shortcuts (consumed by compositor) ─────────────────────────
    // ScreenshotCapture:       Print Screen — full-screen PNG capture
    // ScreenshotCaptureRegion: Shift+Print Screen — shows region selector overlay
    ScreenshotCapture       = 35,
    ScreenshotCaptureRegion = 36,

    // ── Clipboard history overlay (consumed by xshell) ────────────────────────
    // Triggered by compositor on Super+V.
    ClipboardHistoryToggle = 37,

    // ── Color picker overlay (consumed by xshell) ─────────────────────────────
    // Triggered by compositor on Super+P.
    ColorPickerToggle = 38,

    // ── Playback media keys (broadcast to all clients via MediaPlayerService) ─
    // XF86AudioPlay(164)/Stop(166)/Next(163)/Prev(165) intercepted by input_manager.
    MediaPlay = 39, // XF86AudioPlay  — play / pause toggle
    MediaStop = 40, // XF86AudioStop  — stop playback
    MediaNext = 41, // XF86AudioNext  — next track
    MediaPrev = 42, // XF86AudioPrev  — previous track

    // ── Compositor debug overlay (consumed by compositor) ─────────────────────
    // Triggered on Super+F3. Toggles the GPU-timing HUD drawn over output 0.
    // No payload fields used.
    DebugOverlayToggle = 43,

    // ── Keyboard shortcut help overlay (consumed by xshell) ──────────────────
    // Triggered on Super+/ (slash). Toggles the shortcut reference overlay.
    ShortcutHelpToggle = 44,

    // ── Quick Note scratchpad (consumed by xshell) ────────────────────────────
    // Triggered on Super+N. Toggles the floating note scratchpad overlay.
    QuickNoteToggle = 45,

    // ── Gamepad / controller ──────────────────────────────────────────────────
    // Forwarded to the focused app and also to any app subscribed via
    // XApplication::onGamepadEvent().
    // GamepadButton: value_a_ = BTN_* evdev code, value_b_ = 1 (pressed) / 0 (released)
    //                modifiers_ = controller index (0-based)
    // GamepadAxis:   value_a_ = axis ID (0=LX,1=LY,2=RX,3=RY,4=LT,5=RT,6=DpadX,7=DpadY)
    //                value_b_ = normalised value [-1000,+1000] (triggers: [0,+1000])
    //                modifiers_ = controller index (0-based)
    GamepadButton = 30,
    GamepadAxis   = 31,
};

struct InputMessage {
    InputEventType type_;
    int32_t value_a_;    // e.g., X coordinate or raw Linux keycode
    int32_t value_b_;    // e.g., Y coordinate or key state (1=pressed, 0=released)
    uint32_t modifiers_; // Bitmask: Shift(1), Ctrl(2), Alt(4), Super(8)
    int32_t char_value_; // For KeyPress: US-QWERTY translated Unicode codepoint; 0 = non-printable
};

// --- Compositor Multi-Surface Protocol ---

enum class RenderMessageStatus : uint32_t {
    SurfaceCreated = 0,
    FrameReady = 1,
    SurfaceClosed = 2,
    ShellSurfaceCreated = 3,
    SurfaceReady = 4,
    // Subsurface: stride_ = parent_id, damage_x_ = sub_x, damage_y_ = sub_y
    SubsurfaceCreated = 5,
    // Solid-color 1×1 buffer: stride_ = ARGB packed (0xAARRGGBB), no fd sent
    SolidColor = 6,
    // Layer-shell surface: stride_ = layer (0-3), damage_x_/y_ = anchor-resolved x/y,
    // width_/height_ = configured size. No fd on creation; FrameReady carries the buffer.
    LayerSurfaceCreated = 7,
    // xdg_dialog_v1: surface_id_ = dialog toplevel surface. No other fields used.
    DialogModalSet   = 8,
    DialogModalUnset = 9,
    // Server→bridge: send xdg configure with new size. width_/height_ = new dimensions.
    ConfigureRequest = 10,
    // Bridge→server: xdg_toplevel parent set. stride_ = parent surface_id (0 = unset).
    ToplevelParentSet = 11,
    // Bridge→server: client-initiated resize (CSD). stride_ = XDG resize edge bitmask.
    ResizeBegin = 12
};

// Reserved shell surface ids. The desktop shell uses kShellSurfaceId; the lock
// screen uses a DISTINCT id so it never collides with the still-running
// desktop's per-surface state (window list, double-buffer cache, client map) —
// it is drawn fullscreen above everything while locked, then torn down on
// unlock, leaving the desktop (999) untouched.
constexpr uint32_t kShellSurfaceId = 999;
constexpr uint32_t kLockSurfaceId  = 998;

struct RenderMessage {
    uint32_t surface_id_;
    RenderMessageStatus status_;
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_; // Added: Physical hardware alignment (pitch)

    // Damage Region (Dirty rectangle)
    float damage_x_;
    float damage_y_;
    float damage_width_;
    float damage_height_;

    // xdg_surface.set_window_geometry — content rect within the buffer.
    // geom_w_ == 0 means not set; buffer covers the full surface area.
    int32_t geom_x_{0};
    int32_t geom_y_{0};
    int32_t geom_w_{0};
    int32_t geom_h_{0};

    // Double-buffering: which of the client's alternating render buffers this
    // FrameReady refers to (0 or 1). The client draws into the buffer the
    // compositor is NOT currently sampling, so the composited scanout never
    // reads a buffer mid-rewrite (eliminates tearing). The compositor imports
    // each buffer once and composites the index named here. Default 0 keeps
    // single-buffer senders (e.g. SolidColor paths) working unchanged.
    uint32_t buffer_index_{0};
};

// --- System Services Bus (XBus) Protocol ---

constexpr uint32_t XBUS_MAGIC = 0x58425553; // "XBUS"

struct XBusMessage {
    uint32_t magic_{XBUS_MAGIC};
    uint32_t transaction_id_;
    char service_name_[32];
    uint32_t opcode_;
    uint8_t payload_[256];
};

// ── Wire-layout freeze (A3) ───────────────────────────────────────────────────
// These three are the core wire records that ride the SEQPACKET transport in
// every session. Their size/offset are frozen at compile time so any accidental
// reorder, field resize, or padding change is a BUILD failure in every TU that
// includes this header — not a silent field break on the wire between an app and
// a future compositor. See docs/ABI.md; extend this block when a message type
// becomes part of the stable contract. (Absolute sizes verified on amd64.)
static_assert(sizeof(XBusMessage) == 300, "XBusMessage wire size changed");
static_assert(offsetof(XBusMessage, magic_)   == 0,  "XBusMessage.magic_ must lead the record");
static_assert(offsetof(XBusMessage, opcode_)  == 40, "XBusMessage.opcode_ moved");
static_assert(offsetof(XBusMessage, payload_) == 44, "XBusMessage.payload_ moved");
static_assert(sizeof(InputMessage) == 20, "InputMessage wire size changed");
static_assert(offsetof(InputMessage, type_) == 0, "InputMessage.type_ must lead the record");
// 52 → 56: added buffer_index_ for double-buffered presentation (anti-tearing).
// Stays size-distinct from BridgeTitleMessage (64B); main.cc dispatch
// discriminates a title update from a RenderMessage by exact size.
static_assert(sizeof(RenderMessage) == 56, "RenderMessage wire size changed");
static_assert(offsetof(RenderMessage, surface_id_) == 0, "RenderMessage.surface_id_ must lead the record");

// ─── Audio Service (com.freebsdx.Audio) ──────────────────────────────────────
// OSS /dev/dsp streaming + /dev/mixer volume control + per-stream volume scaling.
//
// Flow:
//   1. App calls OpenStream (sends SHM fd via SCM_RIGHTS) → gets stream_id.
//   2. App mixes PCM into the SHM ring buffer; compositor reads and blends.
//   3. Settings calls GetVolume / SetVolume / ListSinks / ListStreams.
//   4. Media keys route AudioVolumeUp/Down/Mute → adjustVolume / toggleMute.

enum class AudioOpcode : uint32_t {
    OpenStream     = 0x01,  // AudioStreamRequest + SCM_RIGHTS fd → stream_id
    CloseStream    = 0x02,  // uint32_t stream_id in payload
    ListSinks      = 0x03,  // uint32_t page → AudioDevicePage (output devices)
    ListSources    = 0x04,  // uint32_t page → AudioDevicePage (capture devices)
    SetDefaultSink = 0x05,  // AudioDeviceRequest: set default output path
    GetVolume      = 0x06,  // → AudioVolumeState (master OSS mixer level)
    SetVolume      = 0x07,  // AudioVolumeRequest: set master + optional mute
    SetAppVolume   = 0x08,  // AudioAppVolumeRequest: per-stream volume 0-100
    ListStreams     = 0x09,  // uint32_t page → AudioStreamPage (active streams)
    Mute           = 0x0A,  // AudioVolumeRequest.muted_=1 or toggle
    VolumeChanged  = 0x0B,  // broadcast: master volume or device changed
};

// Describes one OSS audio device (output or capture).
struct AudioDevice {            // 64 bytes
    char     name_[40];         // human-readable, e.g. "Built-in Output"
    char     path_[16];         // e.g. "/dev/dsp0"
    uint32_t is_default_;       // 1 = currently selected default
    uint32_t channels_;         // 2 = stereo
};
static_assert(sizeof(AudioDevice) == 64);

// Paged device response — one device per page.
struct AudioDevicePage {        // 80 bytes ≤ 256
    uint32_t   page_;
    uint32_t   total_;
    uint32_t   has_more_;
    uint32_t   valid_;
    AudioDevice device_;
};
static_assert(sizeof(AudioDevicePage) <= 256);

struct AudioDeviceRequest {     // 256 bytes
    char    path_[252];         // /dev/dsp path to make default
    uint8_t _pad[4];
};
static_assert(sizeof(AudioDeviceRequest) == 256);

struct AudioVolumeState {       // ≤ 256
    uint32_t volume_;           // 0-100 master level
    uint32_t muted_;            // 1 = currently muted
    char     sink_path_[16];    // active default sink path
    uint8_t  pad_[228];
};
static_assert(sizeof(AudioVolumeState) <= 256);

struct AudioVolumeRequest {     // 256 bytes
    uint32_t volume_;           // 0-100; 0xFF = leave unchanged (Mute-only call)
    uint32_t muted_;            // 1=mute, 0=unmute, 0xFF=toggle
    uint8_t  pad_[248];
};
static_assert(sizeof(AudioVolumeRequest) == 256);

struct AudioAppVolumeRequest {  // 256 bytes
    uint32_t stream_id_;        // from OpenStream response
    uint32_t volume_;           // 0-100 per-stream scalar
    uint8_t  pad_[248];
};
static_assert(sizeof(AudioAppVolumeRequest) == 256);

// Describes one open audio stream.
struct AudioStreamInfo {        // 64 bytes
    uint32_t stream_id_;
    uint32_t volume_;           // current per-stream volume 0-100
    char     app_name_[56];     // app identifier or "Unknown"
};
static_assert(sizeof(AudioStreamInfo) == 64);

// Paged stream response — one stream per page.
struct AudioStreamPage {        // 80 bytes ≤ 256
    uint32_t       page_;
    uint32_t       total_;
    uint32_t       has_more_;
    uint32_t       valid_;
    AudioStreamInfo stream_;
};
static_assert(sizeof(AudioStreamPage) <= 256);

struct AudioStreamRequest {
    uint32_t sample_rate_; // e.g. 44100
    uint32_t channels_;    // e.g. 2
    uint32_t bit_depth_;   // e.g. 16
    uint32_t buffer_size_; // Size of the SHM ring buffer
    // The client also passes a file descriptor via SCM_RIGHTS during this request
};

struct SystemInfo {
    uint32_t cpu_usage_; // 0-100
    uint32_t mem_used_mb_;
    uint32_t mem_total_mb_;
    uint32_t battery_percentage_;
    uint32_t is_charging_;
    uint32_t frame_count_; // New: Total frames rendered since boot
    uint32_t gpu_temp_;    // New: GPU temperature in degrees Celsius
    char uptime_[16];    // e.g. "01:23:45"
};

enum class NetworkOpcode : uint32_t {
    GetStatus      = 0x01,  // → NetworkStatus (any app)
    Scan           = 0x02,  // WifiScanRequest → WifiScanPage (any app)
    Connect        = 0x03,  // WifiConnectRequest (non-sandboxed)
    Disconnect     = 0x04,  // fire-and-forget (non-sandboxed)
    NetworkChanged = 0x05,  // broadcast: connection state changed
};

struct WifiNetwork {
    char     ssid_[32];
    char     bssid_[20];       // "XX:XX:XX:XX:XX:XX\0"
    int32_t  signal_strength_; // dBm (typically -100 to 0)
    uint32_t is_secure_;       // 1 = requires passphrase
    uint32_t security_type_;   // 0=open, 1=WPA2, 2=WPA3
    uint8_t  pad_[168];        // pad to 228 bytes — allows 1 per payload page
};
static_assert(sizeof(WifiNetwork) <= 256);

// Paged scan result: 1 network per page (WifiNetwork fills most of payload).
struct WifiScanPage {
    uint32_t    page_;     // 0-based
    uint32_t    total_;    // total networks found
    uint32_t    has_more_; // 1 if more pages exist
    uint32_t    valid_;    // 1 if network_ is populated
    WifiNetwork network_;
};
static_assert(sizeof(WifiScanPage) <= 256);

struct WifiScanRequest {
    uint32_t page_;    // which page to fetch
    uint8_t  pad_[252];
};
static_assert(sizeof(WifiScanRequest) <= 256);

struct WifiConnectRequest {
    char    ssid_[64];        // target SSID
    char    passphrase_[128]; // WPA passphrase (empty string for open networks)
    uint8_t pad_[64];
};
static_assert(sizeof(WifiConnectRequest) <= 256);

struct NetworkStatus {
    uint32_t is_connected_;
    char     current_ssid_[32];
    char     ip_address_[16];
    int32_t  signal_strength_;  // dBm of current connection; 0 if disconnected
    uint64_t rx_bytes_;         // cumulative bytes received on the active interface
    uint64_t tx_bytes_;         // cumulative bytes sent
    uint8_t  pad_[184];
};
static_assert(sizeof(NetworkStatus) <= 256);

struct ConfigValue {
    char key_[32];
    char value_[128];
};

enum class SystemOpcode : uint32_t {
    GetStats = 0x01,
    Shutdown = 0x02 // New: Administrative shutdown
};

enum class PortalOpcode : uint32_t {
    OpenFile = 0x01,   // Request a File Picker overlay
    SaveFile = 0x02,   // Request a Save dialog overlay
    RequestCamera = 0x03,
    Handshake = 0x04,
    ListDir = 0x05,    // List a directory: DirListRequest → DirListing
    ZfsRollback = 0x06, // Roll back a ZFS dataset to a snapshot: ZfsActionRequest → ZfsActionResponse
    ZfsSnapshot = 0x07, // Create a new ZFS snapshot of the current directory: ZfsActionRequest → ZfsActionResponse
};

// ── File System Portal ──────────────────────────────────────────────────────

struct DirEntry {
    char     name_[44];   // null-terminated filename (no path component)
    uint32_t is_dir_;     // 1 = directory, 0 = regular file
}; // 48 bytes

struct DirListing {
    uint32_t entry_count_; // entries in this page (0..4)
    uint32_t total_count_; // total entries in the directory
    uint32_t page_;        // 0-based page number of this response
    uint32_t has_more_;    // 1 if more pages exist after this one
    DirEntry entries_[4];  // 4 * 48 = 192 bytes
}; // 208 bytes
static_assert(sizeof(DirListing) <= 256);

struct DirListRequest {
    char     path_[252];  // null-terminated absolute path
    uint32_t page_;       // 0-based page to fetch (4 entries per page)
}; // 256 bytes
static_assert(sizeof(DirListRequest) <= 256);

// Request for ZfsRollback and ZfsSnapshot.
// For ZfsRollback: path_ is the full snapshot path, e.g. /home/user/.zfs/snapshot/snap-2026-06-29
// For ZfsSnapshot: path_ is the dataset directory, e.g. /home/user
struct ZfsActionRequest {
    char path_[252]; // null-terminated absolute path
    uint32_t _pad;   // reserved
}; // 256 bytes
static_assert(sizeof(ZfsActionRequest) == 256);

struct ZfsActionResponse {
    uint32_t success_;   // 1 = OK, 0 = error
    char     message_[252]; // human-readable result or error description
}; // 256 bytes
static_assert(sizeof(ZfsActionResponse) == 256);

// ─── Print Portal ─────────────────────────────────────────────────────────────
// Service: com.freebsdx.Print
// Backend: CUPS via lpstat / lp / cancel CLI tools.
//
// Flow:
//   1. App calls ListPrinters → receives one PrinterListPage per printer.
//   2. App calls PrintFile with a PrintRequest → receives PrintResponse with job_id.
//   3. App polls GetJobStatus with job_id → PrintJobStatus.
//   4. App calls CancelJob to abort.
//   ShowDialog: portal picks default printer and submits immediately (sandbox-safe).

enum class PrintSpoolerOpcode : uint32_t {
    ListPrinters = 0x01, // → one PrinterListPage per printer (paged)
    PrintFile    = 0x02, // PrintRequest → PrintResponse
    GetJobStatus = 0x03, // uint32_t job_id in payload → PrintJobStatus
    CancelJob    = 0x04, // uint32_t job_id in payload → PrintResponse
    ShowDialog   = 0x05, // PrintDialogRequest → PrintResponse (uses default printer)
};

struct PrinterInfo {          // 200 bytes
    char     name_[48];       // CUPS printer name (null-terminated)
    char     description_[80];// human-readable description
    char     location_[64];   // e.g. "Office, 2nd floor"
    uint32_t is_default_;     // 1 = system default printer
    uint32_t state_;          // 0=idle, 1=processing, 2=stopped/disabled
};
static_assert(sizeof(PrinterInfo) == 200);

struct PrinterListPage {      // 216 bytes ≤ 256
    uint32_t total_;          // total number of printers available
    uint32_t page_;           // 0-based index of this message (one printer per page)
    uint32_t count_;          // always 1
    uint32_t _pad;
    PrinterInfo printer_;
};
static_assert(sizeof(PrinterListPage) <= 256);

struct PrintRequest {         // 256 bytes exactly
    char     printer_name_[64]; // empty string = use system default
    char     file_path_[128];   // absolute path to file (portal validates access)
    char     title_[48];        // print job title shown in queue
    uint32_t copies_;           // 1+
    uint32_t color_mode_;       // 0=auto, 1=color, 2=grayscale
    uint32_t duplex_;           // 0=one-sided, 1=two-sided-long, 2=two-sided-short
    uint32_t paper_size_;       // 0=letter, 1=A4, 2=legal, 3=A3
};
static_assert(sizeof(PrintRequest) == 256);

struct PrintResponse {        // 256 bytes exactly
    uint32_t success_;        // 1=OK, 0=error
    uint32_t job_id_;         // CUPS job ID on success; 0 on error
    char     message_[248];   // human-readable status or error
};
static_assert(sizeof(PrintResponse) == 256);

struct PrintJobStatus {       // ≤ 256 bytes
    uint32_t job_id_;
    uint32_t state_;          // 0=pending, 1=processing, 2=completed, 3=cancelled, 4=error
    uint32_t pages_completed_;
    uint32_t pages_total_;
    char     printer_name_[48];
    char     title_[48];
    char     message_[100];   // status string
};
static_assert(sizeof(PrintJobStatus) <= 256);

struct PrintDialogRequest {   // 256 bytes exactly
    char file_path_[128];     // absolute path to file
    char title_[128];         // job title (shown in queue + dialog header)
};
static_assert(sizeof(PrintDialogRequest) == 256);

enum class NotificationOpcode : uint32_t {
    Post             = 0x01, // client → server: post a new notification
    Dismiss          = 0x02, // bidirectional: dismiss by id
    MuteApp          = 0x03, // NotifAppMuteRequest → (no payload response)
    UnmuteApp        = 0x04, // NotifAppMuteRequest → (no payload response)
    GetMutedApps     = 0x05, // (no payload)        → NotifMutedAppList
    MutedAppsChanged = 0x06, // broadcast after mute/unmute
};

// Fits exactly in XBusMessage.payload_ (256 bytes)
struct NotificationPayload {
    uint32_t id_;           //   4 — assigned by server on Post; 0 in client's request
    uint32_t timeout_ms_;   //   4 — auto-dismiss after N ms; 0 = server default (5000)
    char title_[48];        //  48
    char message_[156];     // 156
    char app_name_[44];     //  44 — app identifier for per-app muting; empty = unclassified
};
static_assert(sizeof(NotificationPayload) == 256);

struct NotifAppMuteRequest {    // 256 bytes
    char    app_name_[56];      //  56 — app identifier to mute or unmute
    uint8_t pad_[200];          // 200
};
static_assert(sizeof(NotifAppMuteRequest) == 256);

struct NotifMutedAppList {      // 256 bytes
    uint32_t count_;            //   4 — number of valid entries
    char     names_[8][31];     // 248 — up to 8 muted app names
    uint8_t  pad_[4];           //   4
};
static_assert(sizeof(NotifMutedAppList) == 256);

enum class HardwareOpcode : uint32_t {
    GetBattery = 0x01,
    GetBacklight = 0x02,
    SetBacklight = 0x03,
    GetVolume = 0x04,
    SetVolume = 0x05,
    ToggleDashboard = 0x06
};

struct HardwareStatus {
    uint32_t battery_life_;
    uint32_t battery_state_;
    uint32_t backlight_level_;
    uint32_t volume_level_;
};

// ── Window Management ────────────────────────────────────────────────────────

enum class WindowOpcode : uint32_t {
    SetTitle     = 0x01, // client → server: set the window's title bar text
    Raise        = 0x02, // client → server: raise and focus a surface (payload: uint32_t surface_id)
    Maximize     = 0x03, // shell → server: maximize a surface (payload: uint32_t surface_id)
    SetWorkspace = 0x04, // shell → server: move surface to workspace (WinWorkspaceRequest)
    SetAlwaysOnTop = 0x05, // shell → server: toggle always-on-top (WinAlwaysOnTopRequest)
};

struct WindowTitleRequest {
    uint32_t surface_id_;   // the client's own surface id
    char     title_[252];   // null-terminated UTF-8 title
};
static_assert(sizeof(WindowTitleRequest) <= 256);

struct WinWorkspaceRequest {
    uint32_t surface_id_;
    uint32_t workspace_idx_;
};
static_assert(sizeof(WinWorkspaceRequest) <= 256);

struct WinAlwaysOnTopRequest {
    uint32_t surface_id_;
    uint32_t enabled_; // 1 = on, 0 = off
};
static_assert(sizeof(WinAlwaysOnTopRequest) <= 256);

// ── Shell Service ───────────────────────────────────────────────────────────

enum class ShellOpcode : uint32_t {
    WorkspaceChanged = 0x01, // server→client broadcast: active workspace index changed
    SwitcherUpdate   = 0x02, // server→client broadcast: show/refresh Alt+Tab overlay
    SwitcherDismiss  = 0x03, // server→client broadcast: hide Alt+Tab overlay
    WindowListUpdate = 0x04, // server→client broadcast: taskbar window list changed
    LaunchWithFile   = 0x05, // client→server→shell: open a file with a specific app
    RegisterHotkey   = 0x06, // shell→compositor: bind key combo to a shell InputEventType action
};

// Payload for ShellOpcode::RegisterHotkey
struct HotkeyRequest {
    uint32_t action_id_;   // InputEventType value to trigger on this combo
    uint32_t keycode_;     // Linux evdev KEY_* code; 0 = unregister the action
    uint32_t modifiers_;   // Bitmask matching InputMessage.modifiers_ (Super=8, etc.)
    uint8_t  pad_[244];
};
static_assert(sizeof(HotkeyRequest) == 256);

// Payload for ShellOpcode::LaunchWithFile
struct ShellLaunchRequest {
    char app_id_[64];    // reverse-domain app identifier, e.g. "com.freebsdx.texteditor"
    char file_path_[188]; // absolute path to the file to open
}; // 252 bytes
static_assert(sizeof(ShellLaunchRequest) <= 256);

struct WorkspaceChangedPayload {
    uint32_t index_; // 0-based workspace index
};

// Alt+Tab window switcher — fits inside XBusMessage.payload_ (256 bytes)
struct SwitcherEntry {
    uint32_t surface_id_;
    char     title_[28]; // truncated display title
}; // 32 bytes
static_assert(sizeof(SwitcherEntry) == 32);

struct SwitcherPayload {
    uint32_t focused_id_; // surface_id of the currently focused window
    uint32_t count_;      // number of valid entries (0..7)
    SwitcherEntry entries_[7];
}; // 8 + 7*32 = 232 bytes ≤ 256
static_assert(sizeof(SwitcherPayload) <= 256);

// Taskbar window list — fits inside XBusMessage.payload_ (256 bytes)
struct WindowListPayload {
    uint32_t count_;           // number of valid entries (0..7)
    SwitcherEntry entries_[7]; // reuses SwitcherEntry {surface_id_, title_[28]}
}; // 4 + 7*32 = 228 bytes ≤ 256
static_assert(sizeof(WindowListPayload) <= 256);

// ── Display / Multi-Monitor Service ─────────────────────────────────────────

enum class DisplayOpcode : uint32_t {
    QueryOutputs       = 0x01, // request→response: list all physical outputs
    GetWindowOutput    = 0x02, // request→response: query which output a surface is on
    MoveWindowToOutput = 0x03, // request (no response): move surface to another output
    OutputChanged      = 0x04, // server→client broadcast: topology has changed
    SetOutputScale     = 0x05, // request (no response): set HiDPI scale for an output
    SetVrrEnabled      = 0x06, // request (no response): enable/disable VRR on an output
    QueryVrrCapable    = 0x07, // request→response: query VRR capability for an output
    SetHdrEnabled      = 0x08, // request (no response): enable/disable HDR on an output
    QueryHdrCapable    = 0x09, // request→response: query HDR capability for an output
};

// Describes one physical output (fits 4 of these inside XBusMessage.payload_).
struct DisplayOutputInfo {
    uint32_t index_;        // 0-based
    uint32_t width_;        // logical pixels (physical / scale)
    uint32_t height_;       // logical pixels (physical / scale)
    uint32_t refresh_hz_;   // e.g. 60
    float    x_offset_;     // global canvas x start position (logical)
    float    scale_;        // HiDPI scale factor: 1.0, 1.5, 2.0
    uint32_t vrr_capable_;  // 1 if display supports VRR (FreeSync/G-Sync/AdaptiveSync)
    uint32_t vrr_enabled_;  // 1 if VRR is currently active on this output
    uint32_t hdr_capable_;  // 1 if display supports HDR (HDR_OUTPUT_METADATA connector property)
    uint32_t hdr_enabled_;  // 1 if HDR is currently active on this output
    char     name_[8];      // e.g. "Output-0"
};

struct DisplayScaleRequest {
    uint32_t output_idx_; // 0-based output index
    float    scale_;      // new scale factor (1.0, 1.5, or 2.0)
};

struct DisplayVrrRequest {
    uint32_t output_idx_; // 0-based output index
    uint32_t enabled_;    // 1 = enable VRR, 0 = disable
};

struct DisplayVrrCapability {
    uint32_t output_idx_; // 0-based output index
    uint32_t capable_;    // 1 = hardware supports VRR
    uint32_t enabled_;    // 1 = VRR currently active
};
static_assert(sizeof(DisplayVrrCapability) <= 256);

struct DisplayHdrRequest {
    uint32_t output_idx_;    // 0-based output index
    uint32_t enabled_;       // 1 = enable HDR, 0 = disable
    uint32_t max_luminance_; // peak luminance in nits (e.g. 1000); 0 = use default (1000)
    uint32_t padding_[61];   // pad to 256 bytes
};
static_assert(sizeof(DisplayHdrRequest) <= 256);

struct DisplayHdrCapability {
    uint32_t output_idx_;    // 0-based output index
    uint32_t capable_;       // 1 = hardware supports HDR_OUTPUT_METADATA
    uint32_t enabled_;       // 1 = HDR currently active
    uint32_t max_luminance_; // current max_luminance setting (nits)
};
static_assert(sizeof(DisplayHdrCapability) <= 256);

struct DisplayOutputList {
    uint32_t count_;
    DisplayOutputInfo outputs_[4]; // max 4 outputs; fits ≤ 256
};
static_assert(sizeof(DisplayOutputList) <= 256);

struct DisplayMoveRequest {
    uint32_t surface_id_;
    uint32_t output_idx_;
};

struct DisplayWindowInfo {
    uint32_t output_idx_;
    uint32_t output_width_;
    uint32_t output_height_;
    float    x_offset_;
};

// ─── Night Light (com.freebsdx.NightLight) ───────────────────────────────────
// Reduces blue-light emission by shifting display colour temperature.
// Temperature range: 2700 K (very warm) – 6500 K (neutral/cool).
//
// Flow:
//   1. xshell calls GetState on connect to populate toggle + slider.
//   2. SetEnabled / SetTemperature / SetSchedule take effect immediately.
//   3. StateChanged is broadcast whenever the compositor changes mode
//      (e.g. schedule triggers at sunset/sunrise).

enum class NightLightOpcode : uint32_t {
    GetState        = 0x01, // → NightLightState
    SetEnabled      = 0x02, // payload: NightLightEnableRequest (no response)
    SetTemperature  = 0x03, // payload: NightLightTempRequest   (no response)
    SetSchedule     = 0x04, // payload: NightLightScheduleRequest (no response)
    StateChanged    = 0x05, // broadcast: NightLightState
};

struct NightLightState {
    uint32_t enabled_;          // 1 = filter active
    uint32_t temperature_k_;    // current colour temperature in Kelvin (2700–6500)
    uint32_t schedule_enabled_; // 1 = auto on/off by schedule
    char     from_time_[6];     // "HH:MM\0"
    char     to_time_[6];       // "HH:MM\0"
    uint8_t  pad_[232];
};
static_assert(sizeof(NightLightState) == 256);

struct NightLightEnableRequest {
    uint32_t enabled_; // 1 = on, 0 = off
    uint8_t  pad_[252];
};
static_assert(sizeof(NightLightEnableRequest) <= 256);

struct NightLightTempRequest {
    uint32_t temperature_k_; // 2700–6500
    uint8_t  pad_[252];
};
static_assert(sizeof(NightLightTempRequest) <= 256);

struct NightLightScheduleRequest {
    uint32_t enabled_;    // 1 = schedule active
    char     from_time_[6]; // "HH:MM\0"
    char     to_time_[6];   // "HH:MM\0"
    uint8_t  pad_[238];
};
static_assert(sizeof(NightLightScheduleRequest) <= 256);

// ─── Focus Mode (com.freebsdx.FocusMode) ─────────────────────────────────────
// Session-based focus mode: dims non-allowed app windows at the compositor
// level and suppresses notification popups for the session duration.
// Distinct from DND (notification-only) and the Pomodoro timer (timer-only).
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. Start begins a session; Stop ends it early.
//   3. StateChanged is broadcast every 60 s (remaining time update) and on
//      Start/Stop.

enum class FocusModeCategory : uint32_t {
    Work     = 0,
    Personal = 1,
    All      = 2,
};

enum class FocusModeOpcode : uint32_t {
    GetState     = 0x01, // → FocusModeState
    Start        = 0x02, // payload: FocusStartRequest (no response)
    Stop         = 0x03, // no payload (no response)
    StateChanged = 0x04, // broadcast: FocusModeState
};

struct FocusModeState {
    uint32_t active_;           // 1 = session running
    uint32_t remaining_secs_;   // 0 = unlimited (∞ mode)
    uint32_t category_;         // FocusModeCategory
    uint8_t  pad_[244];
};
static_assert(sizeof(FocusModeState) == 256);

struct FocusStartRequest {
    uint32_t duration_secs_; // 0 = unlimited
    uint32_t category_;      // FocusModeCategory
    uint8_t  pad_[248];
};
static_assert(sizeof(FocusStartRequest) <= 256);

// ── Screen Capture Service (com.freebsdx.Screencopy) ────────────────────────

// Request payload for opcode 0x01 (CaptureOutput)
struct ScreencopyCaptureRequest {
    uint32_t output_idx_;
};

// Response payload: returned in the same XBusMessage along with a shm fd via SCM_RIGHTS.
// format_ = WL_SHM_FORMAT_XRGB8888 (2).
struct ScreencopyFrameInfo {
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_; // bytes per row (width * 4)
    uint32_t format_; // always 2 (WL_SHM_FORMAT_XRGB8888)
};
static_assert(sizeof(ScreencopyFrameInfo) <= 256);

// Sent from Wayland bridge → xserver when xdg_toplevel_set_title fires.
// 64 bytes — distinct from RenderMessage (36) so xserver can dispatch by size.
struct BridgeTitleMessage {
    uint32_t surface_id_;
    char     title_[60]; // null-terminated UTF-8 title
}; // 64 bytes
static_assert(sizeof(BridgeTitleMessage) == 64);

// Sent from xserver → Wayland bridge after each DRM page flip.
// Size is 28 bytes — distinct from InputMessage (20) and RenderMessage (36)
// so handleIpcMessage can dispatch on size alone.
struct PresentFeedback {
    uint32_t surface_id_;  // 0 = resolve all pending feedbacks
    uint32_t tv_sec_hi_;   // high 32 bits of seconds (0 for decades)
    uint32_t tv_sec_lo_;   // low 32 bits of seconds from the DRM flip event
    uint32_t tv_nsec_;     // nanoseconds (tv_usec * 1000)
    uint32_t refresh_ns_;  // display refresh period in ns (e.g. 16666667 for 60 Hz)
    uint32_t seq_hi_;      // flip sequence number, high word
    uint32_t seq_lo_;      // flip sequence number, low word
};
static_assert(sizeof(PresentFeedback) == 28);

// ── App Package Manager (com.freebsdx.AppManager) ───────────────────────────

enum class AppManagerOpcode : uint32_t {
    ListApps        = 0x01, // request→paginated response: enumerate installed apps
    Install         = 0x02, // request: install from local .xpkg or .xapp path
    Uninstall       = 0x03, // request: remove app by identifier
    GetAppInfo      = 0x04, // request→response: details for one app
    InstallProgress = 0x05, // server→all broadcast: async install/uninstall status
};

// Describes one installed application (fits in XBusMessage.payload_ with page header).
struct AppInfo {
    char     identifier_[64]; // reverse-domain id, e.g. "com.freebsdx.texteditor"
    char     name_[48];       // display name, e.g. "Text Editor"
    char     version_[16];    // semver string, e.g. "1.0.0"
    char     icon_path_[80];  // absolute path to icon (PNG or SVG)
    uint32_t installed_;      // 1 = currently installed
}; // 212 bytes
static_assert(sizeof(AppInfo) == 212);

// Paginated list response (one app per message to stay ≤ 256 bytes).
struct AppList {
    uint32_t count_;    // 1 if this message carries an entry, 0 if empty result
    uint32_t total_;    // total installed apps
    uint32_t page_;     // 0-based index of this entry
    uint32_t has_more_; // 1 if more entries follow
    AppInfo  app_;
}; // 16 + 212 = 228 bytes
static_assert(sizeof(AppList) <= 256);

// Request payload for Install (path to a .xpkg archive or .xapp directory).
struct AppInstallRequest {
    char path_[252]; // absolute local path
}; // 252 bytes
static_assert(sizeof(AppInstallRequest) <= 256);

// Request payload for Uninstall / GetAppInfo (app identifier string).
struct AppIdentifierRequest {
    char identifier_[252]; // e.g. "com.freebsdx.texteditor"
}; // 252 bytes
static_assert(sizeof(AppIdentifierRequest) <= 256);

// Broadcast payload for InstallProgress (sent repeatedly during install/uninstall).
struct AppInstallProgress {
    char     identifier_[64]; // which app this concerns
    uint32_t percent_;        // 0-100
    uint32_t status_;         // 0 = in_progress, 1 = success, 2 = error
    char     message_[180];   // human-readable status line
}; // 252 bytes
static_assert(sizeof(AppInstallProgress) <= 256);

// Remote package catalog opcodes (extend AppManagerOpcode)
enum class AppManagerCatalogOpcode : uint32_t {
    UpdateCatalog  = 0x10, // (no payload) → AppManagerCatalogState; downloads catalog in bg
    CatalogUpdated = 0x11, // broadcast: catalog refresh done; AppManagerCatalogState payload
    ListRemote     = 0x12, // AppManagerPageRequest → paged AppManagerRemotePage
    SearchRemote   = 0x13, // AppManagerSearchRequest → paged AppManagerRemotePage
    InstallRemote  = 0x14, // AppManagerRemoteInstallRequest → InstallProgress broadcasts
};

// Page request (reuse for ListRemote)
struct AppManagerPageRequest {   // 256 bytes
    uint32_t page_;
    uint8_t  pad_[252];
};
static_assert(sizeof(AppManagerPageRequest) == 256);

// One remote package page (header + inline entry)
struct AppManagerRemotePage {   // 256 bytes
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;             // 0 = catalog empty / page out of range
    // ── inline entry (240 bytes) ──
    char     identifier_[40];
    char     name_[40];
    char     version_[12];
    char     category_[20];
    char     description_[80];
    uint32_t size_kb_;
    uint32_t installed_;         // 1 if same identifier is locally installed
    uint8_t  entry_pad_[40];
};
static_assert(sizeof(AppManagerRemotePage) == 256);

// Search request
struct AppManagerSearchRequest {  // 256 bytes
    char     query_[128];
    uint32_t page_;
    uint8_t  pad_[124];
};
static_assert(sizeof(AppManagerSearchRequest) == 256);

// Catalog state (response to UpdateCatalog and broadcast payload for CatalogUpdated)
struct AppManagerCatalogState {   // 256 bytes
    uint32_t updating_;           // 1 = download in progress
    uint32_t total_packages_;
    uint32_t last_updated_;       // Unix timestamp of last successful fetch
    uint32_t error_;              // 1 = last update failed
    char     error_msg_[80];
    char     catalog_url_[160];
};
static_assert(sizeof(AppManagerCatalogState) == 256);

// Remote install request
struct AppManagerRemoteInstallRequest {  // 256 bytes
    char identifier_[64];
    char download_url_[192];
};
static_assert(sizeof(AppManagerRemoteInstallRequest) == 256);

// ── MIME / File Association Service (com.freebsdx.MIME) ─────────────────────

enum class MimeOpcode : uint32_t {
    QueryHandler      = 0x01, // request→response: get default handler for a file/type
    SetDefaultHandler = 0x02, // request (no response): override the default handler
    ListHandlers      = 0x03, // request→response: list all capable apps for a type
};

// Request payload for QueryHandler and ListHandlers.
// Set is_path_=1 to detect the MIME type from the file extension in path_;
// set is_path_=0 to treat path_ as a literal MIME type string (e.g. "text/plain").
struct MimeQueryRequest {
    char     path_[252]; // file path or MIME type string
    uint32_t is_path_;   // 1 = detect from path extension, 0 = literal MIME type
}; // 256 bytes
static_assert(sizeof(MimeQueryRequest) <= 256);

// Describes one app that can handle a MIME type.
struct MimeHandlerInfo {
    char     app_id_[64];    // reverse-domain identifier, e.g. "com.freebsdx.texteditor"
    char     app_name_[48];  // display name, e.g. "Text Editor"
    char     mime_type_[48]; // MIME type, e.g. "text/plain"
    uint32_t is_default_;    // 1 = this is the current system default for this MIME type
}; // 164 bytes
static_assert(sizeof(MimeHandlerInfo) <= 256);

// Paginated response for ListHandlers (one handler per message).
// QueryHandler also returns this with count_=1 and the default handler.
struct MimeHandlerList {
    uint32_t      count_;    // entries in this response (0 or 1)
    uint32_t      total_;    // total handlers available for this type
    uint32_t      page_;     // 0-based page index
    uint32_t      has_more_; // 1 if more pages exist
    MimeHandlerInfo handler_;
}; // 180 bytes
static_assert(sizeof(MimeHandlerList) <= 256);

// Request payload for SetDefaultHandler.
struct MimeSetDefaultRequest {
    char mime_type_[128]; // MIME type to reassign, e.g. "text/plain"
    char app_id_[128];    // new default app identifier
}; // 256 bytes
static_assert(sizeof(MimeSetDefaultRequest) <= 256);

// ─── Drag and Drop ────────────────────────────────────────────────────────────
// Service: com.freebsdx.DnD
//
// Flow:
//   1. Source app detects drag gesture → sends StartDrag with MIME type + payload
//   2. Compositor tracks cursor, hit-tests surfaces
//   3. Compositor sends DragEnter / DragLeave to target as cursor moves
//   4. On button release: compositor sends Drop to target, DragEnd to source
//
// MIME types: "text/uri-list" (file paths, \n separated), "text/plain"

enum class DnDOpcode : uint32_t {
    StartDrag = 0x01,  // app → server:   I'm starting a drag
    DragEnter = 0x02,  // server → target: cursor entered your surface
    DragLeave = 0x03,  // server → target: cursor left your surface
    Drop      = 0x04,  // server → target: user released — here is the data
    DragEnd   = 0x05,  // server → source: drag finished (dropped or cancelled)
};

struct DnDStartRequest {
    uint32_t surface_id_;   //  4 — source surface identifier
    char     _pad[4];       //  4 — alignment
    char     mime_type_[64];//  64
    char     data_[184];    //  184 — payload (file path, text, …)
};                          // = 256 bytes
static_assert(sizeof(DnDStartRequest) == 256);

struct DnDEvent {           // used for DragEnter, DragLeave, Drop
    float x_;               //  4 — position within target surface (logical px)
    float y_;               //  4
    char  mime_type_[64];   //  64
    char  data_[184];       //  184 — populated only for Drop
};                          // = 256 bytes
static_assert(sizeof(DnDEvent) == 256);

// ─── Theme Service (com.freebsdx.Theme) ──────────────────────────────────────

enum class ThemeOpcode : uint32_t {
    GetTheme = 0x01, // request→response: query current dark_mode + accent_idx
    SetTheme = 0x02, // request (no response): set dark_mode + accent_idx; denied for sandboxed apps
};

struct ThemeSetRequest {
    uint32_t dark_mode_;   // 0 = light, 1 = dark
    uint32_t accent_idx_;  // 0 = Blue (#0A84FF), 1 = Purple (#BF5AF2), 2 = Orange (#FF9F0A)
    uint8_t  _pad[248];
}; // 256 bytes
static_assert(sizeof(ThemeSetRequest) == 256);

struct ThemeState {
    uint32_t dark_mode_;
    uint32_t accent_idx_;
    uint8_t  _pad[248];
}; // 256 bytes
static_assert(sizeof(ThemeState) == 256);

// ─── Wallpaper Service (com.freebsdx.Wallpaper) ──────────────────────────────

enum class WallpaperOpcode : uint32_t {
    SetWallpaper          = 0x01, // request + broadcast: set desktop wallpaper image path
    GetWallpaper          = 0x02, // request→response: query current wallpaper path
    SetWorkspaceWallpaper = 0x03, // request + broadcast: set per-workspace wallpaper
    GetWorkspaceWallpaper = 0x04, // request→response: query wallpaper for a workspace index
};

// fit_mode_ values match ImageFitMode: 0=Cover, 1=Contain, 2=Stretch, 3=Center
struct WallpaperSetRequest {
    char     path_[252];    // absolute file path to PNG / JPEG image; empty = clear
    uint32_t fit_mode_;     // 0=Cover (default), 1=Contain, 2=Stretch, 3=Center
}; // 256 bytes
static_assert(sizeof(WallpaperSetRequest) == 256);

struct WallpaperWorkspaceRequest {
    uint32_t workspace_idx_; //   4  workspace index (0-based, 0–8)
    uint32_t fit_mode_;      //   4  same fit values as WallpaperSetRequest
    char     path_[248];     // 248  absolute path; empty = no per-workspace override
};                           // = 256 bytes
static_assert(sizeof(WallpaperWorkspaceRequest) == 256);

// ─── Input Method Editor (com.freebsdx.IME) ──────────────────────────────────

enum class IMEOpcode : uint32_t {
    Activate   = 0x01, // app → server: text field gained focus; payload: IMEActivateRequest
    Deactivate = 0x02, // app → server: text field lost focus (no payload)
};

struct IMEActivateRequest {
    // 0 = passthrough (Latin / no conversion)
    // 1 = Japanese  (romaji → hiragana)
    // 2 = Chinese   (pinyin stub — reserved)
    // 3 = Korean    (stub — reserved)
    uint32_t locale_;
    uint8_t  _pad[252];
}; // 256 bytes
static_assert(sizeof(IMEActivateRequest) == 256);

// ─── Sandbox ──────────────────────────────────────────────────────────────────
// Service: com.freebsdx.Sandbox
//
// Apps call cap_enter() after connecting and rendering initialises, then
// immediately send EnterSandbox so the server can track capability-mode clients
// and apply tighter per-opcode policy.

enum class SandboxOpcode : uint32_t {
    EnterSandbox = 0x01,  // app → server: cap_enter() succeeded; payload: SandboxInfo
};

struct SandboxInfo {
    uint32_t api_version_{1};  // protocol version; currently always 1
    uint32_t flags_{0};        // reserved — set to 0
    uint32_t padding_[62]{};   // pad to 256 bytes
};
static_assert(sizeof(SandboxInfo) == 256);

// ─── Accessibility ────────────────────────────────────────────────────────────
// Service: com.freebsdx.Accessibility
//
// Flow:
//   Apps call XApplication::announce(text) to push spoken text to screen readers
//   via Announce opcode → compositor broadcasts to all subscribers.
//   Apps optionally send UpdateTree pages to give screen readers a navigable tree.
//   Screen reader apps use Subscribe to register, then receive broadcast XBusMessages
//   with service_name_="com.freebsdx.Accessibility" and opcode=Announce/Tree.
//

enum class AccessibilityOpcode : uint32_t {
    Subscribe   = 0x01,  // screen reader → server: register for broadcasts; no payload
    Unsubscribe = 0x02,  // screen reader → server: deregister
    Announce    = 0x03,  // app → server: UTF-8 text to speak; broadcast to subscribers
    UpdateTree  = 0x04,  // app → server: one page of the semantic tree (paged)
    QueryTree   = 0x05,  // screen reader → server: request stored tree for focused surface
};

// Announcement message: spoken text pushed by app or compositor on focus/state change.
// Fits in XBusMessage.payload_ (256 bytes).
struct AccessibilityAnnounce {
    uint32_t surface_id_;  // source surface (0 = compositor-generated)
    char     text_[252];   // UTF-8, NUL-terminated spoken text
};
static_assert(sizeof(AccessibilityAnnounce) == 256);

// One node in the serialised semantic tree.  64 bytes each.
// Trees are sent as AccessibilityTreePage messages (up to 3 nodes per message).
struct AccessibilityTreeNode {
    uint32_t id_;          // stable per-element id (lower 32 bits of pointer)
    uint32_t parent_id_;   // 0 for the root node
    char     role_[16];    // NUL-terminated: "Button","Label","TextInput","Checkbox",
                           //   "Switch","Slider","Radio","Image","ListItem","Container"
    char     label_[24];   // NUL-terminated, truncated display label
    float    x_, y_;       // absolute position (logical pixels)
    float    w_, h_;       // size (logical pixels)
    // total: 4+4+16+24+4+4+4+4 = 64 bytes
};
static_assert(sizeof(AccessibilityTreeNode) == 64);

// Paged tree payload — up to 3 nodes per XBusMessage.
struct AccessibilityTreePage {
    uint32_t surface_id_;    // which surface's tree
    uint32_t total_nodes_;   // total nodes in the full tree (all pages)
    uint32_t page_index_;    // 0-based page number
    uint32_t node_count_;    // number of valid nodes in nodes_[] (1..3)
    AccessibilityTreeNode nodes_[3];  // 3 × 64 = 192 bytes
    // total: 16 + 192 = 208 bytes ≤ 256
};
static_assert(sizeof(AccessibilityTreePage) <= 256);

// ─── Power Management ─────────────────────────────────────────────────────
// com.freebsdx.Power — suspend/resume/hibernate/reboot, lid policy, sleep timer
enum class PowerOpcode : uint32_t {
    GetPowerState   = 0x01,  // → PowerState (any app)
    Suspend         = 0x02,  // fire-and-forget (non-sandboxed)
    Hibernate       = 0x03,  // fire-and-forget (non-sandboxed)
    Reboot          = 0x04,  // fire-and-forget (non-sandboxed)
    SetLidPolicy    = 0x05,  // PowerLidPolicyRequest (non-sandboxed)
    GetLidPolicy    = 0x06,  // → PowerLidPolicyRequest (any app)
    SetSleepTimeout = 0x07,  // PowerSleepTimeoutRequest (non-sandboxed)
    GetSleepTimeout = 0x08,  // → PowerSleepTimeoutRequest (any app)
    PowerChanged    = 0x09,  // broadcast: AC↔battery state change
};

enum class PowerLidAction : uint32_t {
    DoNothing = 0,
    Suspend   = 1,
    Hibernate = 2,
    Shutdown  = 3,
};

struct PowerState {
    uint32_t on_ac_;          // 1 = plugged in, 0 = on battery
    uint32_t battery_life_;   // 0–100 %
    uint32_t battery_state_;  // 0=discharging, 1=charging, 2=full, 3=absent
    uint32_t time_remaining_; // minutes until empty; 0xFFFFFFFF = unknown
    uint8_t  pad_[240];
};
static_assert(sizeof(PowerState) <= 256);

struct PowerLidPolicyRequest {
    uint32_t action_;  // PowerLidAction
    uint8_t  pad_[252];
};
static_assert(sizeof(PowerLidPolicyRequest) <= 256);

struct PowerSleepTimeoutRequest {
    uint32_t seconds_;  // 0 = never sleep
    uint8_t  pad_[252];
};
static_assert(sizeof(PowerSleepTimeoutRequest) <= 256);

// ─── Backlight ────────────────────────────────────────────────────────────
// com.freebsdx.Backlight — hw.acpi.video.lcd0.brightness read/write +
// keyboard brightness-key routing (KEY_BRIGHTNESSUP / KEY_BRIGHTNESSDOWN).
enum class BacklightOpcode : uint32_t {
    GetBrightness = 0x01,  // → BacklightState (any app)
    SetBrightness = 0x02,  // BacklightState request (non-sandboxed)
    StepUp        = 0x03,  // BacklightStepRequest; step up by request.step_ % (non-sandboxed)
    StepDown      = 0x04,  // BacklightStepRequest; step down by request.step_ % (non-sandboxed)
};

struct BacklightState {
    uint32_t level_;    // 0–100 %
    uint8_t  pad_[252];
};
static_assert(sizeof(BacklightState) <= 256);

struct BacklightStepRequest {
    uint32_t step_;     // step size in percent (1–100)
    uint8_t  pad_[252];
};
static_assert(sizeof(BacklightStepRequest) <= 256);

// ─── Bluetooth ────────────────────────────────────────────────────────────
// com.freebsdx.Bluetooth — hccontrol / bthidd / hcsecd integration.
enum class BluetoothOpcode : uint32_t {
    GetState      = 0x01,  // → BluetoothState (adapter present/enabled)
    SetEnabled    = 0x02,  // BluetoothEnableRequest (non-sandboxed)
    Scan          = 0x03,  // fire-and-forget: begin 10s inquiry; results broadcast as DeviceFound
    StopScan      = 0x04,  // stop ongoing inquiry
    ListDevices   = 0x05,  // → paged BluetoothDevicePage (paired/known devices)
    Connect       = 0x06,  // BluetoothDeviceRequest (non-sandboxed)
    Disconnect    = 0x07,  // BluetoothDeviceRequest (non-sandboxed)
    Pair          = 0x08,  // BluetoothDeviceRequest (non-sandboxed)
    Forget        = 0x09,  // BluetoothDeviceRequest (non-sandboxed)
    DeviceFound   = 0x0A,  // broadcast: BluetoothDevice discovered during scan
    DeviceChanged = 0x0B,  // broadcast: connection/pair state changed
};

// Compact 6-byte BD_ADDR stored as 6 chars (MSB first, e.g. "00:11:22:33:44:55")
struct BluetoothAddr {
    char str_[18]; // "XX:XX:XX:XX:XX:XX\0"
};

struct BluetoothDevice {
    BluetoothAddr addr_;     // BD_ADDR string
    char          name_[64]; // human-readable name (NUL-terminated)
    uint32_t      paired_;   // 1 = in hcsecd.conf / known
    uint32_t      connected_; // 1 = active ACL link
    uint32_t      rssi_;     // signal strength 0–100 (0 if unknown)
    uint8_t       pad_[142]; // pad to 256 bytes
};
static_assert(sizeof(BluetoothDevice) <= 256);

struct BluetoothDevicePage {
    uint32_t       page_;         // 0-based page index
    uint32_t       total_;        // total known devices
    uint32_t       count_;        // devices in this page (0–3)
    uint32_t       has_more_;
    BluetoothDevice devices_[1];  // 1 device per page (256 bytes per device)
};
static_assert(sizeof(BluetoothDevicePage) <= 256);

struct BluetoothState {
    uint32_t present_;   // 1 = adapter detected (hci0 exists)
    uint32_t enabled_;   // 1 = adapter up
    uint32_t scanning_;  // 1 = inquiry in progress
    uint8_t  pad_[244];
};
static_assert(sizeof(BluetoothState) <= 256);

struct BluetoothEnableRequest {
    uint32_t enabled_; // 1 = up, 0 = down
    uint8_t  pad_[252];
};
static_assert(sizeof(BluetoothEnableRequest) <= 256);

struct BluetoothDeviceRequest {
    BluetoothAddr addr_;    // target device
    uint8_t       pad_[238];
};
static_assert(sizeof(BluetoothDeviceRequest) <= 256);

// ─── Security Context ──────────────────────────────────────────────────────
// Sent by native xkit apps (surface_id_=0) and by the Wayland bridge
// (surface_id_ = the bridge surface being tagged) to declare app identity.
enum class SecurityContextOpcode : uint32_t {
    SetContext = 0x01,  // app/bridge → server: announce identity
    GetContext = 0x02,  // → server: query; response echoes this struct
};

struct SecurityContextInfo {  // must be ≤ 256 bytes (XBusMessage payload)
    uint32_t surface_id_;          // 0 = socket-level (native apps); else bridge surface
    char     app_id_[64];          // e.g. "com.freebsdx.Files"
    char     sandbox_engine_[32];  // e.g. "org.freebsdx.Capsicum" / "org.freedesktop.Flatpak"
    char     instance_id_[32];     // unique per-launch (stringified PID for native apps)
    uint32_t flags_;               // reserved, set to 0
    uint8_t  padding_[120];        // pad to 256 bytes
};
static_assert(sizeof(SecurityContextInfo) == 256);

// ─── VPN (WireGuard) ──────────────────────────────────────────────────────
// com.freebsdx.VPN — wg(4) kernel module; wg-quick bring-up/tear-down.
enum class VpnOpcode : uint32_t {
    GetStatus    = 0x01,  // → VpnStatus (any app)
    Connect      = 0x02,  // VpnConnectRequest (non-sandboxed)
    Disconnect   = 0x03,  // VpnConnectRequest.interface_ names the tunnel (non-sandboxed)
    ImportConfig = 0x04,  // VpnImportRequest: copy .conf → /etc/wireguard/ (non-sandboxed)
    VpnChanged   = 0x05,  // broadcast: tunnel up/down state changed
};

struct VpnStatus {
    uint32_t connected_;       // 1 = tunnel is up
    char     interface_[16];   // active wg interface, e.g. "wg0" (empty if none)
    char     endpoint_[64];    // server address:port from wg show
    char     ip_address_[20];  // assigned tunnel IP from ifconfig
    uint64_t rx_bytes_;        // bytes received (from wg show transfer)
    uint64_t tx_bytes_;        // bytes sent
    uint8_t  pad_[136];
};
static_assert(sizeof(VpnStatus) <= 256);

struct VpnConnectRequest {
    char    interface_[16]; // wg interface, e.g. "wg0"; empty = first available .conf
    uint8_t pad_[240];
};
static_assert(sizeof(VpnConnectRequest) <= 256);

struct VpnImportRequest {
    char    path_[252]; // absolute path to .conf file
    uint8_t _pad[4];
};
static_assert(sizeof(VpnImportRequest) == 256);

// ─── Game Mode ───────────────────────────────────────────────────────────────
// com.freebsdx.GameMode — performance burst for the foreground game process.
// When enabled the compositor: raises GPU/CPU scheduler priority for the
// active surface, switches to the Performance power profile, and suppresses
// non-priority notification popups until disabled.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetEnabled(1) activates Game Mode for the current foreground surface.
//   3. StateChanged is broadcast when the active app changes or mode is toggled.

enum class GameModeOpcode : uint32_t {
    GetState    = 0x01, // → GameModeState
    SetEnabled  = 0x02, // payload: GameModeEnableRequest (no response)
    StateChanged = 0x03, // broadcast: GameModeState
};

struct GameModeState {
    uint32_t enabled_;          // 1 = Game Mode active
    char     active_app_[32];   // name of the boosted app (empty if disabled)
    uint32_t elapsed_secs_;     // seconds since Game Mode was last enabled
    uint8_t  pad_[216];
};
static_assert(sizeof(GameModeState) == 256);

struct GameModeEnableRequest {
    uint32_t enabled_; // 1 = on, 0 = off
    uint8_t  pad_[252];
};
static_assert(sizeof(GameModeEnableRequest) <= 256);

// ─── Accessibility Settings (com.freebsdx.AccessibilitySettings) ─────────────
// Compositor-level a11y adjustments distinct from the semantic tree service.
// Controls global font scale, cursor size, high-contrast mode, and magnifier.
//
// Flow:
//   1. xshell calls GetSettings on connect to populate all four controls.
//   2. Each Set* call takes effect immediately (no response).
//   3. SettingsChanged is broadcast when any setting changes (e.g. from another app).

enum class AccessibilitySettingsOpcode : uint32_t {
    GetSettings     = 0x01, // → AccessibilitySettings
    SetFontSize     = 0x02, // payload: AccessibilityFontRequest   (no response)
    SetCursorScale  = 0x03, // payload: AccessibilityCursorRequest (no response)
    SetHighContrast = 0x04, // payload: AccessibilityToggleRequest (no response)
    SetMagnifier    = 0x05, // payload: AccessibilityToggleRequest (no response)
    SettingsChanged = 0x06, // broadcast: AccessibilitySettings
};

struct AccessibilityQuickSettings {
    uint32_t font_size_preset_; // 0=S(11pt) 1=M(13pt) 2=L(16pt) 3=XL(20pt)
    float    cursor_scale_;     // 1.0–3.0
    uint32_t high_contrast_;    // 1 = enabled
    uint32_t magnifier_;        // 1 = enabled
    uint8_t  pad_[240];         // 4+4+4+4+240 = 256
};
static_assert(sizeof(AccessibilityQuickSettings) == 256);

struct AccessibilityFontRequest {
    uint32_t preset_; // 0–3
    uint8_t  pad_[252];
};
static_assert(sizeof(AccessibilityFontRequest) <= 256);

struct AccessibilityCursorRequest {
    float   scale_; // 1.0–3.0
    uint8_t pad_[252];
};
static_assert(sizeof(AccessibilityCursorRequest) <= 256);

struct AccessibilityToggleRequest {
    uint32_t enabled_; // 1 = on, 0 = off
    uint8_t  pad_[252];
};
static_assert(sizeof(AccessibilityToggleRequest) <= 256);

// ─── Lock Screen ─────────────────────────────────────────────────────────────
// com.freebsdx.Lock — screensaver/idle lock; PAM authentication in compositor.
//
// Flow (lock):
//   1. Idle timer fires OR PowerService triggers suspend/lid-close → Lock::Lock sent
//      (or any trusted app calls LockOpcode::Lock).
//   2. LockService spawns LockScreen.xapp, sets exclusive input mode.
//   3. LockScreen.xapp sends Register opcode so compositor knows its client socket.
//   4. All input (except media keys) is routed exclusively to the lock screen.
//
// Flow (unlock):
//   1. User types password → LockScreen.xapp sends LockOpcode::Unlock (username+pw).
//   2. LockService calls pam_authenticate(); on success: clears lock state,
//      restores normal input routing, sends LockStateChanged broadcast, kills app.

enum class LockOpcode : uint32_t {
    Lock             = 0x01, // trusted-only → ack; enters exclusive lock mode
    Unlock           = 0x02, // LockAuthRequest (username+pw) → LockState; PAM auth
    GetState         = 0x03, // → LockState (any app)
    SetTimeout       = 0x04, // LockTimeoutRequest: idle-before-lock seconds (non-sandboxed)
    GetTimeout       = 0x05, // → LockTimeoutRequest (any app)
    LockStateChanged = 0x06, // broadcast: lock state changed (LockState payload)
    Register         = 0x07, // LockScreen.xapp → compositor: "I am the lock screen"
};

struct LockState {
    uint32_t locked_;        // 1 = screen is currently locked
    uint32_t timeout_secs_;  // current idle-before-lock timeout; 0 = never
    uint8_t  pad_[248];
};
static_assert(sizeof(LockState) <= 256);

struct LockAuthRequest {
    char     username_[64];  // local username; empty = PAM uses current user
    char     password_[188]; // plaintext; server clears immediately after PAM call
    uint8_t  _pad[4];
};
static_assert(sizeof(LockAuthRequest) == 256);

struct LockTimeoutRequest {
    uint32_t seconds_;  // 0 = never lock automatically
    uint8_t  pad_[252];
};
static_assert(sizeof(LockTimeoutRequest) <= 256);

// ─── Screen Saver ─────────────────────────────────────────────────────────────
// com.freebsdx.ScreenSaver — visual idle screen saver (separate from lock screen).
// Activates before the lock screen when the user is idle; any input dismisses it.
// If the lock timeout also expired while the saver was showing, dismissal triggers
// LockService::lock(). Fullscreen video calls Inhibit to suppress the saver.
//
// Flow (activate):
//   1. ScreenSaverService::pollIdle() fires every 10 s → elapsed ≥ timeout_secs_ → activate().
//   2. Service forks ScreenSaver.xapp with mode as argv[1] ("blank"/"clock"/"matrix").
//   3. App runs with is_shell=true → fullscreen compositor layer over all windows.
//
// Flow (deactivate):
//   1. Any user input → main.cc calls resetIdleTimer() → if active_, deactivate().
//   2. deactivate() sends SIGTERM to saver process, broadcasts StateChanged.
//   3. If lock timeout also expired → LockService::lock() is triggered.
//
enum class ScreenSaverOpcode : uint32_t {
    GetState     = 0x01, // → ScreenSaverState (any app)
    SetConfig    = 0x02, // ScreenSaverConfig → ack (non-sandboxed)
    GetConfig    = 0x03, // → ScreenSaverConfig (any app)
    Inhibit      = 0x04, // → ack; increments inhibit counter; sandboxed apps OK
    Uninhibit    = 0x05, // → ack; decrements inhibit counter; sandboxed apps OK
    Register     = 0x06, // ScreenSaver.xapp → compositor: "I am the saver"
    StateChanged = 0x07, // broadcast: active_ / enabled_ changed (ScreenSaverState)
};

struct ScreenSaverState {
    uint32_t active_;       // 1 = saver currently showing
    uint32_t enabled_;      // 1 = timeout_secs_ > 0
    uint32_t inhibited_;    // 1 = at least one Inhibit call outstanding
    uint32_t timeout_secs_; // current configured timeout (0 = disabled)
    uint32_t mode_;         // 0=blank, 1=clock, 2=matrix
    uint8_t  pad_[236];
};
static_assert(sizeof(ScreenSaverState) <= 256);

struct ScreenSaverConfig {
    uint32_t timeout_secs_; // 0 = disabled; seconds of idle before activation
    uint32_t mode_;          // 0=blank, 1=clock, 2=matrix
    uint8_t  pad_[248];
};
static_assert(sizeof(ScreenSaverConfig) <= 256);

// ─── Login Manager ────────────────────────────────────────────────────────────
// com.freebsdx.Login — pre-session greeter (xlogin); authenticates via PAM and
// spawns the user session (xshell) on success.
//
// Flow:
//   1. Compositor starts → spawns xlogin (is_shell=true greeter).
//   2. xlogin sends Register so compositor stores its client socket.
//   3. xlogin sends ListUsers → UsersPage responses (one per user).
//   4. User selects an account + types password → Authenticate request.
//   5. LoginService calls pam_authenticate(); on success: kills xlogin,
//      spawns xshell; on failure: sends AuthResult{success_=0}.
//
enum class LoginOpcode : uint32_t {
    ListUsers    = 0x01, // greeter → compositor: request paged user list
    Authenticate = 0x02, // LoginAuthRequest (username+pw) → LoginAuthResult
    AuthResult   = 0x03, // compositor → greeter: result of Authenticate
    UsersPage    = 0x04, // compositor → greeter: one page of LoginUsersPage
    Register     = 0x05, // greeter → compositor: "I am the login greeter"
};

struct LoginUsersPage {
    uint32_t page_;           // 0-based page index
    uint32_t total_;          // total number of users
    uint32_t valid_;          // 1 if user entry below is valid
    char     username_[64];   // Unix login name
    char     display_name_[64]; // GECOS full name (falls back to username)
    uint8_t  pad_[116];
}; // 256 bytes (4+4+4+64+64+116)
static_assert(sizeof(LoginUsersPage) == 256);

struct LoginAuthRequest {
    char     username_[64];  // must match a real Unix account
    char     password_[188]; // cleared by server immediately after PAM call
    uint8_t  pad_[4];
}; // 256 bytes
static_assert(sizeof(LoginAuthRequest) == 256);

struct LoginAuthResult {
    uint32_t success_;       // 1 = authenticated, 0 = failed
    char     username_[64];  // echoed back on success
    uint32_t locked_out_;    // 1 = rejected by brute-force throttle (not a bad password)
    uint32_t retry_after_s_; // seconds to wait before retrying (when locked_out_ or backing off)
    uint8_t  pad_[180];
}; // 256 bytes — locked_out_/retry_after_s_ carved from pad (layout-compatible)
static_assert(sizeof(LoginAuthResult) == 256);

// ─── Screenshot & Screen Recording ────────────────────────────────────────────
// com.freebsdx.Screenshot — DRM/KMS readback via captureToShm(); PNG via libpng.
//
// Flow (screenshot):
//   1. Print Screen → compositor emits ScreenshotCapture InputEventType.
//   2. Compositor calls ScreenshotService::capture() → captureToShm → encode PNG → save.
//   3. ScreenshotTaken broadcast sent; shell notification pops up with path.
//
// Flow (region):
//   1. Shift+Print → compositor broadcasts ScreenshotCaptureRegion XBus event.
//   2. Shell creates region-selector layer surface; user drags to select.
//   3. Shell sends CaptureRegion opcode with ScreenshotRegionRequest to service.
//   4. ScreenshotTaken broadcast with saved path.
//
// Flow (recording):
//   1. App/shell sends StartRecording → service spawns capture thread at given fps.
//   2. Each frame: captureToShm → encode PNG into frames dir.
//   3. On StopRecording: thread exits; ffmpeg muxes frames → .mp4 in ~/Videos.

enum class ScreenshotOpcode : uint32_t {
    Capture          = 0x01, // ScreenshotRequest → ScreenshotResponse + ScreenshotTaken bcast
    CaptureRegion    = 0x02, // ScreenshotRegionRequest → ScreenshotResponse + ScreenshotTaken bcast
    StartRecording   = 0x03, // ScreenshotRecordingRequest → ack; begins frame capture thread
    StopRecording    = 0x04, // → ScreenshotRecordingState (path of output file)
    GetStatus        = 0x05, // → ScreenshotRecordingState
    ScreenshotTaken  = 0x06, // broadcast: ScreenshotResponse (path to saved PNG)
    RecordingChanged = 0x07, // broadcast: ScreenshotRecordingState
};

// InputEventType additions for keyboard shortcuts (values 35–36):
//   ScreenshotCapture       = 35  (Print Screen)
//   ScreenshotCaptureRegion = 36  (Shift+Print Screen)

struct ScreenshotRequest {
    uint32_t output_idx_;    // 0 = primary output
    char     save_path_[220]; // optional override; empty = auto ~/Pictures/Screenshots/
    uint8_t  _pad[32];
};
static_assert(sizeof(ScreenshotRequest) <= 256);

struct ScreenshotRegionRequest {
    uint32_t output_idx_;
    int32_t  x_;
    int32_t  y_;
    uint32_t width_;
    uint32_t height_;
    char     save_path_[200];
    uint8_t  _pad[28];
};
static_assert(sizeof(ScreenshotRegionRequest) <= 256);

struct ScreenshotResponse {
    uint32_t success_;  // 1 = PNG saved OK, 0 = failed
    char     path_[252]; // absolute path of saved PNG
};
static_assert(sizeof(ScreenshotResponse) == 256);

struct ScreenshotRecordingRequest {
    uint32_t output_idx_;
    uint32_t fps_;        // 0 = default (10 fps)
    char     save_dir_[248]; // directory for frames/output; empty = ~/Videos/
};
static_assert(sizeof(ScreenshotRecordingRequest) <= 256);

struct ScreenshotRecordingState {
    uint32_t recording_;    // 1 = active
    uint32_t frame_count_;
    uint32_t fps_;
    char     path_[244];    // output file path (filled on StopRecording)
};
static_assert(sizeof(ScreenshotRecordingState) <= 256);

// ─── XWayland ─────────────────────────────────────────────────────────────────
// com.freebsdx.XWayland — Xwayland process lifecycle and DISPLAY advertisement.
//
// Flow:
//   1. xwayland-bridge starts → spawns Xwayland → learns display number via displayfd.
//   2. Bridge sends XWaylandOpcode::StatusChanged XBus message (to compositor).
//   3. Compositor stores DISPLAY, re-broadcasts to all clients.
//   4. xshell / Terminal set DISPLAY= so X11 apps find Xwayland.
//
enum class XWaylandOpcode : uint32_t {
    GetStatus     = 0x01, // any → XWaylandStatus
    Enable        = 0x02, // (re)start Xwayland if stopped
    Disable       = 0x03, // stop Xwayland
    StatusChanged = 0x04, // broadcast: Xwayland started/stopped
};

struct XWaylandStatus {
    uint32_t running_;     // 1 = Xwayland is running
    int32_t  pid_;         // Xwayland process PID (-1 if not running)
    char     display_[16]; // e.g., ":1" (empty if not running)
    uint8_t  pad_[232];
};
static_assert(sizeof(XWaylandStatus) <= 256);

// ─── Remote Desktop (VNC/RFB) ─────────────────────────────────────────────────
// com.freebsdx.RemoteDesktop — built-in RFB 3.8 server on TCP port 5900.
// Screen captured via ScreenshotService; input injected back into compositor.
//
enum class RemoteDesktopOpcode : uint32_t {
    GetStatus     = 0x01, // → RemoteDesktopStatus
    Enable        = 0x02, // start RFB server
    Disable       = 0x03, // stop RFB server, disconnect all clients
    SetPassword   = 0x04, // RemoteDesktopPasswordRequest → ack
    StatusChanged = 0x05, // broadcast: server started/stopped or client connected/disconnected
};

struct RemoteDesktopStatus {
    uint32_t running_;       // 1 = RFB server is listening
    uint32_t port_;          // TCP port (default 5900)
    uint32_t client_count_;  // number of currently connected VNC clients
    uint32_t password_set_;  // 1 = password protection enabled
    uint8_t  pad_[240];
};
static_assert(sizeof(RemoteDesktopStatus) <= 256);

struct RemoteDesktopPasswordRequest {
    char    password_[128]; // empty string = disable password
    uint8_t pad_[128];
};
static_assert(sizeof(RemoteDesktopPasswordRequest) <= 256);

// ─── Bookmarks ────────────────────────────────────────────────────────────────
// com.freebsdx.Bookmarks — persistent bookmark store in ~/.freebsdx-bookmarks.
//
enum class BookmarkOpcode : uint32_t {
    Add          = 0x01, // BookmarkRequest → ack
    Remove       = 0x02, // BookmarkRequest (url_ used as key) → ack
    List         = 0x03, // page index in payload[0..3] → BookmarkPage responses
    BookmarkPage = 0x04, // compositor → client: one page of results
    Clear        = 0x05, // remove all bookmarks → ack
};

struct BookmarkPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;   // 1 if more pages exist after this one
    char     title_[60];
    char     url_[124];
    uint8_t  pad_[56];
};
static_assert(sizeof(BookmarkPage) == 256);

struct BookmarkRequest {
    char     title_[60];
    char     url_[192];
    uint8_t  pad_[4];
};
static_assert(sizeof(BookmarkRequest) == 256);

// ─── Calendar ─────────────────────────────────────────────────────────────────
// com.freebsdx.Calendar — per-user event store in ~/.freebsdx-calendar.
//
enum class CalendarOpcode : uint32_t {
    AddEvent    = 0x01, // CalendarEvent (id_=0) → ack (id_ filled in)
    UpdateEvent = 0x02, // CalendarEvent (id_ set) → ack
    DeleteEvent = 0x03, // CalendarDeleteRequest → ack
    ListEvents  = 0x04, // CalendarListRequest → CalendarEventPage stream
    EventPage   = 0x05, // compositor → client: one event per page
    ImportIcs   = 0x06, // CalendarImportRequest (path) → CalendarImportResult
    EventReminder = 0x07, // compositor → client broadcast: upcoming event
};

// Used for AddEvent and UpdateEvent requests.
struct CalendarEvent {
    uint32_t id_;            // 4  — 0 for new events; server fills on add
    uint16_t year_;          // 2
    uint8_t  month_;         // 1  (1–12)
    uint8_t  day_;           // 1  (1–31)
    uint8_t  hour_;          // 1  (0–23)
    uint8_t  minute_;        // 1  (0–59)
    uint8_t  end_hour_;      // 1  (0–23)
    uint8_t  end_minute_;    // 1  (0–59)
    uint16_t reminder_min_;  // 2  — minutes before event; 0 = no reminder
    uint8_t  all_day_;       // 1  — 1 if all-day event
    uint8_t  pad0_;          // 1
    char     title_[80];     // 80
    char     description_[80]; // 80
    char     location_[32];  // 32
    uint8_t  pad_[48];       // 48 → total 256
};
static_assert(sizeof(CalendarEvent) == 256);

// Returned by ListEvents (one page per event).
struct CalendarEventPage {
    uint32_t page_;          // 4
    uint32_t total_;         // 4
    uint32_t valid_;         // 4  — 1 if this page contains an event
    uint32_t event_id_;      // 4
    uint16_t year_;          // 2
    uint8_t  month_;         // 1
    uint8_t  day_;           // 1
    uint8_t  hour_;          // 1
    uint8_t  minute_;        // 1
    uint8_t  end_hour_;      // 1
    uint8_t  end_minute_;    // 1
    uint16_t reminder_min_;  // 2
    uint8_t  all_day_;       // 1
    uint8_t  pad0_;          // 1
    char     title_[60];     // 60
    char     description_[80]; // 80
    char     location_[32];  // 32
    uint8_t  pad_[56];       // 56 → total 256
};
static_assert(sizeof(CalendarEventPage) == 256);

struct CalendarDeleteRequest {
    uint32_t event_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(CalendarDeleteRequest) == 256);

struct CalendarListRequest {
    uint16_t year_;   // 0 = all years
    uint8_t  month_;  // 0 = all months
    uint8_t  pad0_;
    uint32_t page_;
    uint8_t  pad_[248];
};
static_assert(sizeof(CalendarListRequest) == 256);

struct CalendarImportRequest {
    char    path_[252];
    uint8_t pad_[4];
};
static_assert(sizeof(CalendarImportRequest) == 256);

struct CalendarImportResult {
    uint32_t imported_;  // number of events successfully imported
    uint8_t  pad_[252];
};
static_assert(sizeof(CalendarImportResult) == 256);

// ─── Power Profiles ──────────────────────────────────────────────────────────
// com.freebsdx.PowerProfile — CPU frequency and C-state profiles.

enum class PowerProfileOpcode : uint32_t {
    GetProfile     = 0x01, // → PowerProfileState
    SetProfile     = 0x02, // PowerProfileRequest → ack + broadcast
    ProfileChanged = 0x03, // broadcast compositor → clients
    ListProfiles   = 0x04, // page in payload[0..3] → PowerProfilePage
    ProfilePage    = 0x05, // compositor → client
};

enum class PowerProfileLevel : uint32_t {
    Performance = 0,
    Balanced    = 1,
    PowerSaver  = 2,
    Emergency   = 3,
};

struct PowerProfileState {
    uint32_t profile_;        // PowerProfileLevel
    uint32_t cpu_freq_mhz_;   // current CPU freq (0 if unknown)
    uint32_t cpu_max_mhz_;    // max available freq (0 if unknown)
    uint32_t cpu_min_mhz_;    // min available freq (0 if unknown)
    uint8_t  pad_[240];
};
static_assert(sizeof(PowerProfileState) == 256);

struct PowerProfileRequest {
    uint32_t profile_;        // PowerProfileLevel
    uint8_t  pad_[252];
};
static_assert(sizeof(PowerProfileRequest) == 256);

struct PowerProfilePage {
    uint32_t page_;
    uint32_t total_;          // always 4 (Performance/Balanced/PowerSaver/Emergency)
    uint32_t profile_;        // PowerProfileLevel
    char     name_[32];
    char     description_[64];
    uint8_t  pad_[148];
};
static_assert(sizeof(PowerProfilePage) == 256);

// ─── Keyboard Layout ─────────────────────────────────────────────────────────
// com.freebsdx.Keymap — switch the active XKB keyboard layout for seat0.

enum class KeymapOpcode : uint32_t {
    SetLayout     = 0x01, // shell → compositor: activate layout (KeymapRequest)
    LayoutChanged = 0x02, // broadcast: compositor confirms layout change (KeymapRequest)
};

struct KeymapRequest {   // 256 bytes
    char layout_[16];    // XKB layout code, e.g. "us", "fr", "de", "es", "jp"
    char variant_[16];   // XKB variant, e.g. "" or "dvorak"; may be empty
    uint8_t pad_[224];
};
static_assert(sizeof(KeymapRequest) == 256);

// ─── Multi-Seat ──────────────────────────────────────────────────────────────
// com.freebsdx.Seat — seat enumeration and assignment.

enum class SeatOpcode : uint32_t {
    ListSeats    = 0x01, // page in payload[0..3] → SeatPage stream
    SeatPage     = 0x02, // compositor → client: one seat per page
    GetSeatInfo  = 0x03, // SeatIdRequest → SeatPage (single seat)
    AssignOutput = 0x04, // SeatAssignRequest → ack (admin only, sandboxed denied)
    SeatChanged  = 0x05, // broadcast: seat added/removed/changed
};

struct SeatPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;        // 1 if more pages exist after this one
    uint32_t seat_id_;
    uint32_t has_keyboard_;
    uint32_t has_pointer_;
    uint32_t has_touch_;
    uint32_t output_id_;       // primary display output for this seat (0=unassigned)
    uint32_t active_surface_;  // currently focused surface on this seat (0=none)
    char     name_[32];        // e.g. "seat0", "seat1"
    char     user_[32];        // logged-in user on this seat (empty if unoccupied)
    uint8_t  pad_[152];
};
static_assert(sizeof(SeatPage) == 256);

struct SeatIdRequest {
    uint32_t seat_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(SeatIdRequest) == 256);

struct SeatAssignRequest {
    uint32_t seat_id_;
    uint32_t output_id_;
    uint8_t  pad_[248];
};
static_assert(sizeof(SeatAssignRequest) == 256);

// ─── Virtual Keyboard ──────────────────────────────────────────────────────
// com.freebsdx.VirtualKeyboard — on-screen keyboard key injection.
enum class VirtualKeyboardOpcode : uint32_t {
    InjectKey = 0x01,  // shell→server: inject a key press+release to focused surface
    Show      = 0x02,  // broadcast: show the on-screen keyboard (e.g. TextInput focused)
    Hide      = 0x03,  // broadcast: hide the on-screen keyboard
};

struct VirtualKeyInput {     // 256 bytes
    uint32_t keycode_;       // Linux KEY_* code (e.g. KEY_A=30, KEY_SPACE=57)
    uint32_t modifiers_;     // bitmask: bit0=Shift, bit1=Ctrl, bit2=Alt, bit3=CapsLock
    uint8_t  pad_[248];
};
static_assert(sizeof(VirtualKeyInput) == 256);

// ─── Camera ───────────────────────────────────────────────────────────────
// com.freebsdx.Camera — V4L2 webcam control and capture portal.
enum class CameraOpcode : uint32_t {
    ListDevices    = 0x01, // → paged CameraDevicePage
    Open           = 0x02, // CameraOpenRequest → CameraState
    Close          = 0x03, // → CameraState (open_=0)
    CaptureStill   = 0x04, // CameraStillRequest → CameraStillResult
    StartRecording = 0x05, // CameraRecordRequest → ok (CameraState recording_=1)
    StopRecording  = 0x06, // → CameraRecordResult
    DeviceChanged  = 0x07, // broadcast: device hotplug
};

struct CameraDevice {     // 256 bytes
    char     path_[32];   // "/dev/video0"
    char     name_[64];   // VIDIOC_QUERYCAP card name
    uint32_t width_;      // default/max width
    uint32_t height_;     // default/max height
    uint8_t  pad_[152];
};
static_assert(sizeof(CameraDevice) == 256);

struct CameraDevicePage {   // 256 bytes
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     valid_;     // 1 if device_ is populated
    uint32_t     has_more_;
    char         path_[32];  // device path (duplicate for quick scan)
    char         name_[64];  // device name
    uint32_t     width_;
    uint32_t     height_;
    uint8_t      pad_[136];
};
static_assert(sizeof(CameraDevicePage) == 256);

struct CameraOpenRequest {  // 256 bytes
    char     path_[32];     // "/dev/video0"
    uint32_t width_;        // 0 = use default (640)
    uint32_t height_;       // 0 = use default (480)
    uint8_t  pad_[216];
};
static_assert(sizeof(CameraOpenRequest) == 256);

struct CameraState {        // 256 bytes
    uint32_t open_;
    char     path_[32];
    uint32_t width_;
    uint32_t height_;
    uint32_t recording_;
    char     record_path_[184];
    uint8_t  pad_[24];
};
static_assert(sizeof(CameraState) == 256);

struct CameraStillRequest { // 256 bytes
    char     output_dir_[224]; // save directory; empty = ~/Pictures
    uint8_t  pad_[32];
};
static_assert(sizeof(CameraStillRequest) == 256);

struct CameraStillResult {  // 256 bytes
    uint32_t success_;
    char     path_[252];    // saved file path
};
static_assert(sizeof(CameraStillResult) == 256);

struct CameraRecordRequest { // 256 bytes
    char     output_dir_[224]; // save directory; empty = ~/Videos
    uint8_t  pad_[32];
};
static_assert(sizeof(CameraRecordRequest) == 256);

struct CameraRecordResult { // 256 bytes
    uint32_t success_;
    char     path_[252];
};
static_assert(sizeof(CameraRecordResult) == 256);

// ─── System Tray ──────────────────────────────────────────────────────────
// com.freebsdx.Tray — status icons in the xshell taskbar.
enum class TrayOpcode : uint32_t {
    Register    = 0x01, // TrayRegisterRequest → TrayEntry (with assigned id_)
    Unregister  = 0x02, // TrayIdRequest
    SetIcon     = 0x03, // TrayUpdateRequest (value_ = new icon_path or icon_text)
    SetTooltip  = 0x04, // TrayUpdateRequest (value_ = new tooltip)
    SetStatus   = 0x05, // TrayStatusRequest
    ListEntries = 0x06, // TrayPageRequest → TrayEntryPage (one per message)
    TrayChanged = 0x07, // broadcast: tray roster changed (TrayPageRequest{page_=0} re-fetch)
    Click       = 0x08, // server→registered_app: user clicked this app's icon; TrayIdRequest
};

enum class TrayStatus : uint32_t {
    Passive   = 0,
    Active    = 1,
    Attention = 2,
    Hidden    = 3,
};

// A single tray entry: 240 bytes
struct TrayEntry {
    uint32_t id_;             // 4  — assigned by server on Register
    char     app_id_[48];     // 48 — reverse-domain app identifier
    char     icon_path_[100]; // 100 — absolute path to icon image, or ""
    char     icon_text_[8];   // 8  — UTF-8 emoji/short text fallback (e.g. "🔔")
    char     tooltip_[48];    // 48 — tooltip shown on hover
    uint32_t status_;         // 4  — TrayStatus value
    uint8_t  pad_[28];        // 28
};
static_assert(sizeof(TrayEntry) == 240);

// Paginated list response: 256 bytes
struct TrayEntryPage {
    uint32_t  page_;     // 4
    uint32_t  total_;    // 4
    uint32_t  has_more_; // 4
    uint32_t  valid_;    // 4
    TrayEntry entry_;    // 240
};
static_assert(sizeof(TrayEntryPage) == 256);

// Register request: 256 bytes (same layout as TrayEntry minus id_)
struct TrayRegisterRequest {
    char     app_id_[48];     // 48
    char     icon_path_[100]; // 100
    char     icon_text_[8];   // 8
    char     tooltip_[48];    // 48
    uint32_t status_;         // 4
    uint8_t  pad_[48];        // 48
};
static_assert(sizeof(TrayRegisterRequest) == 256);

// Unregister / Click routing: 256 bytes
struct TrayIdRequest {
    uint32_t id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(TrayIdRequest) == 256);

// SetIcon or SetTooltip: 256 bytes
struct TrayUpdateRequest {
    uint32_t id_;
    char     value_[252]; // new icon_path, icon_text, or tooltip
};
static_assert(sizeof(TrayUpdateRequest) == 256);

// SetStatus: 256 bytes
struct TrayStatusRequest {
    uint32_t id_;
    uint32_t status_; // TrayStatus value
    uint8_t  pad_[248];
};
static_assert(sizeof(TrayStatusRequest) == 256);

// Re-fetch hint broadcast (no payload needed; xshell re-fetches page 0)
struct TrayPageRequest {
    uint32_t page_;
    uint8_t  pad_[252];
};
static_assert(sizeof(TrayPageRequest) == 256);

// ─── File Chooser Portal ──────────────────────────────────────────────────
// com.freebsdx.FileChooser — lets sandboxed apps request a native file-open
// or file-save dialog that the compositor (xshell) presents on their behalf.
// Flow: app → OpenFile/SaveFile → service queues + broadcasts Show → xshell
// presents picker → xshell sends Respond → service broadcasts Response.
enum class FileChooserOpcode : uint32_t {
    OpenFile  = 0x01,  // app → service: single-file open picker
    OpenFiles = 0x02,  // app → service: multi-file open picker
    SaveFile  = 0x03,  // app → service: save-as dialog
    Show      = 0x04,  // service → xshell broadcast: present native picker
    Respond   = 0x05,  // xshell → service: user finished selection
    Response  = 0x06,  // service broadcast → all: result (matched by request_id_)
};

// Sent by app for OpenFile/OpenFiles/SaveFile; reused as Show broadcast payload.
// 256 bytes.
struct FileChooserRequest {
    uint32_t opcode_;            // 4  — FileChooserOpcode value
    uint32_t request_id_;        // 4  — 0 when sent by app; assigned by service for Show
    char     title_[64];         // 64 — dialog title, e.g. "Open Image"
    char     filter_mime_[48];   // 48 — MIME filter ("image/*", "video/*", "" = all)
    char     initial_path_[128]; // 128 — starting directory; empty = $HOME
    uint32_t multi_select_;      // 4  — 1 for OpenFiles
    uint32_t save_mode_;         // 4  — 1 for SaveFile (enables filename input field)
};
static_assert(sizeof(FileChooserRequest) == 256);

// Sent by xshell for Respond; broadcast by service for Response.
// 256 bytes.
struct FileChooserResponse {
    uint32_t opcode_;     // 4  — FileChooserOpcode::Respond or ::Response
    uint32_t request_id_; // 4  — matches request_id_ from Show
    uint32_t cancelled_;  // 4  — 1 if user dismissed without selecting
    uint32_t count_;      // 4  — number of selected paths (1 for single, ≥1 for multi)
    char     path_[240];  // 240 — null-terminated absolute path (first path for multi)
};
static_assert(sizeof(FileChooserResponse) == 256);

// ─── Keychain / Secret Service (com.freebsdx.Keychain) ────────────────────────
// AES-256-CBC encrypted credential storage for sandboxed apps.
// Master-password-protected; session key held only in compositor memory.
//
// Typical flow:
//   app → Unlock(password) → KeychainStatusResponse(success, item_count)
//   app → Store(service, account, secret) → KeychainItemResponse(found=1)
//   app → Retrieve(service, account)      → KeychainItemResponse(found, secret)
//   app → Delete(service, account)        → KeychainStatusResponse(success)
//   app → List(page)                      → KeychainListPage
//   [any write op] → service broadcasts Changed to all subscribers

enum class KeychainOpcode : uint32_t {
    Unlock   = 0x01,  // KeychainUnlockRequest  → KeychainStatusResponse
    Lock     = 0x02,  // (no payload)           → KeychainStatusResponse
    Store    = 0x03,  // KeychainStoreRequest   → KeychainItemResponse
    Retrieve = 0x04,  // KeychainLookupRequest  → KeychainItemResponse
    Delete   = 0x05,  // KeychainLookupRequest  → KeychainStatusResponse
    List     = 0x06,  // KeychainPageRequest    → KeychainListPage
    Changed  = 0x07,  // broadcast: item added, updated, or removed
};

// Unlock: supply master password (PBKDF2-HMAC-SHA256 key derivation).
struct KeychainUnlockRequest {    // 256 bytes
    char    master_password_[252];
    uint8_t _pad[4];
};
static_assert(sizeof(KeychainUnlockRequest) == 256);

// Response to Unlock, Lock, and Delete.
struct KeychainStatusResponse {   // 256 bytes
    uint32_t success_;     // 1 = ok, 0 = failure (wrong password, not found, locked)
    uint32_t item_count_;  // total items currently stored (valid after Unlock)
    uint8_t  _pad[248];
};
static_assert(sizeof(KeychainStatusResponse) == 256);

// Store: write or overwrite a credential (requires unlocked keychain).
struct KeychainStoreRequest {     // 256 bytes
    char service_[80];   // reverse-DNS service id, e.g. "com.freebsdx.Browser"
    char account_[80];   // username / key label, e.g. "user@example.com"
    char secret_[96];    // plaintext secret; encrypted at rest with AES-256-CBC
};
static_assert(sizeof(KeychainStoreRequest) == 256);

// Lookup: retrieve or delete by (service, account) pair.
struct KeychainLookupRequest {    // 256 bytes
    char    service_[80];
    char    account_[80];
    uint8_t _pad[96];
};
static_assert(sizeof(KeychainLookupRequest) == 256);

// Response to Retrieve — also used as ack for Store.
struct KeychainItemResponse {     // 256 bytes
    uint32_t found_;       // 1 = found & unlocked, 0 = not found / keychain locked
    char     service_[80]; // echoed from request
    char     account_[80]; // echoed from request
    char     secret_[92];  // decrypted secret (valid only when found_==1)
    uint8_t  _pad[0];      // 4+80+80+92 = 256
};
static_assert(sizeof(KeychainItemResponse) == 256);

// One entry in a list page (no secret exposed).
struct KeychainEntry {            // 160 bytes
    char service_[80];
    char account_[80];
};
static_assert(sizeof(KeychainEntry) == 160);

// Paged list response — one entry per page (use page_ = 0, 1, 2 … total_-1).
struct KeychainListPage {         // 256 bytes
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     has_more_;  // 1 if more pages remain
    uint32_t     valid_;     // 1 = entry_ is populated
    KeychainEntry entry_;    // 160 bytes
    uint8_t      _pad[80];   // 16 + 160 + 80 = 256
};
static_assert(sizeof(KeychainListPage) == 256);

// Page request for List.
struct KeychainPageRequest {      // 256 bytes
    uint32_t page_;
    uint8_t  _pad[252];
};
static_assert(sizeof(KeychainPageRequest) == 256);

// ─── Contacts / Address Book (com.freebsdx.Contacts) ──────────────────────────
// Personal contact storage with vCard (.vcf) import.
// Persistence: ~/.freebsdx-contacts (pipe-separated rows).
//
// Flow:
//   app → Add(ContactEntry id_=0) → ContactEntry with assigned id_
//   app → Update(ContactEntry id_ set) → ContactEntry echoed
//   app → Delete(ContactsIdRequest) → ContactsStatusResponse
//   app → Get(ContactsIdRequest) → ContactEntry
//   app → List(ContactsPageRequest) → ContactsListPage
//   app → Search(ContactsSearchRequest page+query) → ContactsListPage
//   app → ImportVCard(ContactsImportRequest path) → ContactsStatusResponse(count)
//   [write] → service broadcasts ContactChanged

enum class ContactsOpcode : uint32_t {
    Add            = 0x01,  // ContactEntry (id_=0)         → ContactEntry (id_ assigned)
    Update         = 0x02,  // ContactEntry (id_ set)        → ContactEntry echoed
    Delete         = 0x03,  // ContactsIdRequest             → ContactsStatusResponse
    Get            = 0x04,  // ContactsIdRequest             → ContactEntry
    List           = 0x05,  // ContactsPageRequest           → ContactsListPage
    Search         = 0x06,  // ContactsSearchRequest         → ContactsListPage
    ImportVCard    = 0x07,  // ContactsImportRequest         → ContactsStatusResponse
    ContactChanged = 0x08,  // broadcast: added/updated/deleted
};

// One contact record — fits as a ListPage entry (240 bytes leaves 16 for header).
struct ContactEntry {             // 240 bytes
    uint32_t id_;                 // 4  — 0 = new; server assigns on Add
    char     first_name_[32];     // 32
    char     last_name_[32];      // 32
    char     email_[64];          // 64
    char     phone_[24];          // 24
    char     organization_[40];   // 40
    char     notes_[40];          // 40
    uint8_t  _pad[4];             // 4
};
static_assert(sizeof(ContactEntry) == 240);

// Paged list / search response (one contact per page).
struct ContactsListPage {         // 256 bytes
    uint32_t     page_;           // 4
    uint32_t     total_;          // 4
    uint32_t     has_more_;       // 4
    uint32_t     valid_;          // 4
    ContactEntry entry_;          // 240
};
static_assert(sizeof(ContactsListPage) == 256);

// Delete / Get by id.
struct ContactsIdRequest {        // 256 bytes
    uint32_t id_;
    uint8_t  _pad[252];
};
static_assert(sizeof(ContactsIdRequest) == 256);

// List page request.
struct ContactsPageRequest {      // 256 bytes
    uint32_t page_;
    uint8_t  _pad[252];
};
static_assert(sizeof(ContactsPageRequest) == 256);

// Search with optional pagination (re-send query with each page).
struct ContactsSearchRequest {    // 256 bytes
    uint32_t page_;               // 4
    char     query_[252];         // 252 — substring across name/email/org/phone
};
static_assert(sizeof(ContactsSearchRequest) == 256);

// Import contacts from a .vcf file path.
struct ContactsImportRequest {    // 256 bytes
    char path_[256];
};
static_assert(sizeof(ContactsImportRequest) == 256);

// Response to Add/Update/Delete/ImportVCard.
struct ContactsStatusResponse {   // 256 bytes
    uint32_t success_;    // 1 = ok
    uint32_t count_;      // imported count (ImportVCard) or remaining total
    uint8_t  _pad[248];
};
static_assert(sizeof(ContactsStatusResponse) == 256);

// ─── Clipboard History (com.freebsdx.ClipboardHistory) ────────────────────────
// Ring-buffer of the last 30 clipboard texts. Persisted to
// ~/.freebsdx-clipboard-history. Pinned entries are never evicted.
// Triggered from xshell via Super+V → ClipboardHistoryToggle shortcut.

enum class ClipboardHistoryOpcode : uint32_t {
    GetPage        = 0x01,  // ClipHistoryPageRequest  → ClipHistoryPage
    Clear          = 0x02,  // (no payload)            → ClipHistoryStatusResponse
    Pin            = 0x03,  // ClipHistoryIdRequest    → ClipHistoryStatusResponse
    Unpin          = 0x04,  // ClipHistoryIdRequest    → ClipHistoryStatusResponse
    HistoryChanged = 0x05,  // broadcast: entry added, pinned, or cleared
};

struct ClipHistoryEntry {         // 208 bytes
    uint32_t id_;                 // 4
    uint32_t pinned_;             // 4  — 1 = pinned (never evicted)
    uint32_t timestamp_;          // 4  — unix time
    uint32_t text_len_;           // 4  — full character count (may exceed text_[] capacity)
    char     text_[192];          // 192 — preview, null-terminated (truncated at 191)
};
static_assert(sizeof(ClipHistoryEntry) == 208);

struct ClipHistoryPage {          // 256 bytes
    uint32_t        page_;        // 4
    uint32_t        total_;       // 4
    uint32_t        has_more_;    // 4
    uint32_t        valid_;       // 4
    ClipHistoryEntry entry_;      // 208
    uint8_t         _pad[32];     // 32
};
static_assert(sizeof(ClipHistoryPage) == 256);

struct ClipHistoryPageRequest {   // 256 bytes
    uint32_t page_;
    uint8_t  _pad[252];
};
static_assert(sizeof(ClipHistoryPageRequest) == 256);

struct ClipHistoryIdRequest {     // 256 bytes
    uint32_t id_;
    uint8_t  _pad[252];
};
static_assert(sizeof(ClipHistoryIdRequest) == 256);

struct ClipHistoryStatusResponse { // 256 bytes
    uint32_t success_;
    uint32_t count_;
    uint8_t  _pad[248];
};
static_assert(sizeof(ClipHistoryStatusResponse) == 256);

// ─── Notes / Quick-note scratchpad ────────────────────────────────────────────
// Service name: "com.freebsdx.Notes"
// Body is limited to 164 bytes at the protocol level; the service persists
// exactly what the struct carries.  Titles are limited to 60 bytes.
// Colors: 0=default, 1=yellow, 2=green, 3=blue, 4=pink.

enum class NotesOpcode : uint32_t {
    Add         = 0x01,  // payload: NoteEntry (id_=0, service assigns)  → NotesStatusResponse
    Update      = 0x02,  // payload: NoteEntry                           → NotesStatusResponse
    Delete      = 0x03,  // payload: NotesIdRequest                      → NotesStatusResponse
    Get         = 0x04,  // payload: NotesIdRequest                      → NotesListPage (count=1)
    List        = 0x05,  // payload: NotesPageRequest                    → NotesListPage
    Search      = 0x06,  // payload: NotesSearchRequest                  → NotesListPage
    Pin         = 0x07,  // payload: NotesIdRequest                      → NotesStatusResponse
    Unpin       = 0x08,  // payload: NotesIdRequest                      → NotesStatusResponse
    NoteChanged = 0xFF   // broadcast: no payload
};

struct NoteEntry {       // 240 bytes — embedded in NotesListPage
    uint32_t id_;        //   4
    uint32_t created_;   //   4  (unix timestamp)
    uint32_t modified_;  //   4  (unix timestamp)
    uint8_t  pinned_;    //   1
    uint8_t  color_;     //   1  (0–4)
    uint8_t  _pad[2];    //   2
    char     title_[60]; //  60
    char     body_[164]; // 164
                         // = 240
};
static_assert(sizeof(NoteEntry) == 240);

struct NotesListPage {   // 256 bytes
    uint32_t  page_;
    uint32_t  has_more_;
    uint32_t  total_;
    uint32_t  count_;    // 0 or 1
    NoteEntry entry_;
};
static_assert(sizeof(NotesListPage) == 256);

struct NotesIdRequest {  // 256 bytes
    uint32_t id_;
    uint8_t  _pad[252];
};
static_assert(sizeof(NotesIdRequest) == 256);

struct NotesPageRequest { // 256 bytes
    uint32_t page_;
    uint8_t  _pad[252];
};
static_assert(sizeof(NotesPageRequest) == 256);

struct NotesSearchRequest { // 256 bytes
    uint32_t page_;
    char     query_[252];
};
static_assert(sizeof(NotesSearchRequest) == 256);

struct NotesStatusResponse { // 256 bytes
    uint32_t success_;
    uint32_t id_;    // assigned id on Add; 0 otherwise
    uint8_t  _pad[248];
};
static_assert(sizeof(NotesStatusResponse) == 256);

// ─── Archive Manager ──────────────────────────────────────────────────────────
// Service name: "com.freebsdx.Archive"
// Uses libarchive (FreeBSD base) for all format/compression support.
// Formats auto-detected on read; write format inferred from path extension
// (.zip → ZIP, .tar → plain tar, .tar.bz2 → bzip2, .tar.xz → xz,
//  default → tar.gz / .tar.gz).

enum class ArchiveOpcode : uint32_t {
    List          = 0x01, // payload: ArchiveListRequest  → ArchiveListPage (one entry per page)
    Extract       = 0x02, // payload: ArchiveExtractRequest → ArchiveStatusResponse (+ OperationDone broadcast)
    Create        = 0x03, // payload: ArchiveCreateRequest  → ArchiveStatusResponse (+ OperationDone broadcast)
    OperationDone = 0xFF  // broadcast: ArchiveStatusResponse in payload
};

struct ArchiveEntry {    // 240 bytes — embedded in ArchiveListPage
    char     path_[220]; // 220 — entry path within the archive
    uint32_t uncompressed_size_; //   4 — bytes (0 for directories)
    uint32_t compressed_size_;   //   4
    uint8_t  is_dir_;    //   1
    uint8_t  _pad[11];   //  11
                         // = 240
};
static_assert(sizeof(ArchiveEntry) == 240);

struct ArchiveListPage { // 256 bytes
    uint32_t     page_;
    uint32_t     has_more_;
    uint32_t     total_;
    uint32_t     count_;  // 0 or 1
    ArchiveEntry entry_;
};
static_assert(sizeof(ArchiveListPage) == 256);

struct ArchiveListRequest { // 256 bytes
    uint32_t page_;
    uint8_t  _pad[4];
    char     path_[248];
};
static_assert(sizeof(ArchiveListRequest) == 256);

struct ArchiveExtractRequest { // 256 bytes
    char archive_path_[128];
    char dest_path_[128];
};
static_assert(sizeof(ArchiveExtractRequest) == 256);

struct ArchiveCreateRequest { // 256 bytes
    char archive_path_[128];
    char source_path_[128];
};
static_assert(sizeof(ArchiveCreateRequest) == 256);

struct ArchiveStatusResponse { // 256 bytes
    uint32_t success_;
    uint32_t total_entries_;
    char     message_[248];
};
static_assert(sizeof(ArchiveStatusResponse) == 256);

// ── Media Player ─────────────────────────────────────────────────────────────
// Service name: "com.freebsdx.MediaPlayer"
//
// Client → compositor:
//   ReportState  — active player pushes its current state (compositor stores + broadcasts)
//   Play/Pause/Stop/Next/Previous/Seek/SetVolume — relayed as broadcasts to all clients
// Compositor → all clients (broadcasts):
//   StateChanged — current playback state (sent after ReportState or command)
//   MediaKey     — XF86 media key pressed (key_ field: 0=play/pause,1=stop,2=next,3=prev)

enum class MediaPlayerOpcode : uint32_t {
    Play        = 0x01,
    Pause       = 0x02,
    Stop        = 0x03,
    Next        = 0x04,
    Previous    = 0x05,
    Seek        = 0x06,
    SetVolume   = 0x07,
    ReportState = 0x08, // app → compositor: update stored state + broadcast
    MediaKey    = 0xFE, // broadcast: XF86 media key (key_ = 0/1/2/3)
    StateChanged= 0xFF, // broadcast: playback state changed
};

struct MediaPlayerState { // 256 bytes
    char     title_[80];       //  80
    char     artist_[60];      //  60
    char     album_[40];       //  40  → 180
    uint32_t position_ms_;     //   4
    uint32_t duration_ms_;     //   4
    uint32_t volume_;          //   4  → 192  (0-100)
    uint8_t  is_playing_;      //   1
    uint8_t  shuffle_;         //   1
    uint8_t  repeat_;          //   1
    uint8_t  _pad1;            //   1  → 196
    uint8_t  _pad[60];         //  60  → 256
};
static_assert(sizeof(MediaPlayerState) == 256);

struct MediaPlayerSeekRequest { // 256 bytes
    uint32_t position_ms_;
    uint8_t  _pad[252];
};
static_assert(sizeof(MediaPlayerSeekRequest) == 256);

struct MediaPlayerVolumeRequest { // 256 bytes
    uint32_t volume_; // 0-100
    uint8_t  _pad[252];
};
static_assert(sizeof(MediaPlayerVolumeRequest) == 256);

struct MediaPlayerKeyRequest { // 256 bytes
    uint32_t key_; // 0=play/pause, 1=stop, 2=next, 3=prev
    uint8_t  _pad[252];
};
static_assert(sizeof(MediaPlayerKeyRequest) == 256);

// ── Weather ───────────────────────────────────────────────────────────────────
// Service name: "com.freebsdx.Weather"
// Source: wttr.in JSON API (no API key required).

enum class WeatherOpcode : uint32_t {
    GetCurrent   = 0x01, // → compositor: respond with WeatherCondition
    GetForecast  = 0x02, // → compositor: respond with paged WeatherForecastPage
    SetLocation  = 0x03, // → compositor: change city, trigger re-fetch
    WeatherChanged = 0xFF, // broadcast: new conditions available
};

struct WeatherCondition { // 256 bytes
    char     city_[48];       //  48
    char     condition_[48];  //  48 → 96
    char     icon_[8];        //   8 → 104  (UTF-8 emoji, e.g. "⛅")
    char     wind_dir_[8];    //   8 → 112  (e.g. "NW")
    int32_t  temp_c_;         //   4 → 116
    int32_t  temp_f_;         //   4 → 120
    int32_t  feels_c_;        //   4 → 124
    uint32_t humidity_;       //   4 → 128  (percent)
    uint32_t wind_kph_;       //   4 → 132
    uint32_t uv_index_;       //   4 → 136
    uint8_t  _pad[120];       // 120 → 256
};
static_assert(sizeof(WeatherCondition) == 256);

struct WeatherForecastDay { // 240 bytes — embedded in WeatherForecastPage
    char    date_[16];        //  16
    char    condition_[48];   //  48 → 64
    char    icon_[8];         //   8 → 72
    int32_t max_c_;           //   4 → 76
    int32_t min_c_;           //   4 → 80
    int32_t max_f_;           //   4 → 84
    int32_t min_f_;           //   4 → 88
    uint8_t _pad[152];        // 152 → 240
};
static_assert(sizeof(WeatherForecastDay) == 240);

struct WeatherForecastPage { // 256 bytes
    uint32_t         page_;      // 0=today, 1=tomorrow, 2=day-after
    uint32_t         has_more_;
    uint32_t         total_;     // always 3
    uint32_t         count_;     // 0 or 1
    WeatherForecastDay day_;
};
static_assert(sizeof(WeatherForecastPage) == 256);

struct WeatherSetLocationRequest { // 256 bytes
    char city_[256];
};
static_assert(sizeof(WeatherSetLocationRequest) == 256);

struct WeatherStatusResponse { // 256 bytes
    uint32_t success_;
    char     message_[252];
};
static_assert(sizeof(WeatherStatusResponse) == 256);

// ── Disk Manager ─────────────────────────────────────────────────────────────
// Service name: "com.freebsdx.Disk"
// Uses geom/gpart/zpool/mount CLI tools; runs as compositor (outside Capsicum).

enum class DiskOpcode : uint32_t {
    ListDisks      = 0x01, // → paged DiskListPage
    ListPartitions = 0x02, // payload: DiskNameRequest → paged PartitionListPage
    Mount          = 0x03, // payload: DiskMountRequest → DiskStatusResponse
    Unmount        = 0x04, // payload: DiskNameRequest (device) → DiskStatusResponse
    Format         = 0x05, // payload: DiskFormatRequest → DiskStatusResponse (async)
    Eject          = 0x06, // payload: DiskNameRequest → DiskStatusResponse
    ListZpools     = 0x07, // → paged ZpoolListPage
    GetIoStats     = 0x08, // page in payload[0..3] → paged DiskIoPage (KB/s per disk)
    DiskChanged    = 0xFF, // broadcast: topology or mount state changed
};

struct DiskInfo { // 240 bytes
    char     name_[32];        //  32  e.g. "ada0"
    char     model_[64];       //  64  e.g. "Samsung SSD 860 EVO 500GB"
    char     serial_[32];      //  32  → 128
    uint64_t size_bytes_;      //   8  → 136
    uint32_t part_count_;      //   4  → 140
    uint8_t  is_removable_;    //   1  → 141
    uint8_t  _pad[99];         //  99  → 240
};
static_assert(sizeof(DiskInfo) == 240);

struct DiskListPage { // 256 bytes
    uint32_t page_, has_more_, total_, count_;
    DiskInfo disk_;
};
static_assert(sizeof(DiskListPage) == 256);

struct PartitionInfo { // 240 bytes
    char     disk_[16];        //  16  e.g. "ada0"
    char     name_[32];        //  32  e.g. "ada0p3"  → 48
    char     fs_type_[16];     //  16  e.g. "freebsd-ufs"  → 64
    char     mount_point_[64]; //  64  e.g. "/"  → 128
    char     label_[32];       //  32  → 160
    uint64_t size_bytes_;      //   8  → 168
    uint64_t used_bytes_;      //   8  → 176
    uint8_t  is_mounted_;      //   1  → 177
    uint8_t  _pad[63];         //  63  → 240
};
static_assert(sizeof(PartitionInfo) == 240);

struct PartitionListPage { // 256 bytes
    uint32_t      page_, has_more_, total_, count_;
    PartitionInfo partition_;
};
static_assert(sizeof(PartitionListPage) == 256);

struct ZpoolInfo { // 240 bytes
    char     name_[32];        //  32
    char     health_[16];      //  16  e.g. "ONLINE"  → 48
    char     state_[16];       //  16  → 64
    uint64_t size_bytes_;      //   8  → 72
    uint64_t used_bytes_;      //   8  → 80
    uint64_t free_bytes_;      //   8  → 88
    uint32_t dataset_count_;   //   4  → 92
    uint8_t  _pad[148];        // 148  → 240
};
static_assert(sizeof(ZpoolInfo) == 240);

struct ZpoolListPage { // 256 bytes
    uint32_t  page_, has_more_, total_, count_;
    ZpoolInfo pool_;
};
static_assert(sizeof(ZpoolListPage) == 256);

struct DiskNameRequest { // 256 bytes — used for ListPartitions, Unmount, Eject
    char name_[256];
};
static_assert(sizeof(DiskNameRequest) == 256);

struct DiskMountRequest { // 256 bytes
    char device_[128];
    char mount_point_[128];
};
static_assert(sizeof(DiskMountRequest) == 256);

struct DiskFormatRequest { // 256 bytes
    char    device_[128];
    char    fs_type_[32];   // "ufs", "msdos", "exfat"
    char    label_[64];
    uint8_t _pad[32];
};
static_assert(sizeof(DiskFormatRequest) == 256);

struct DiskStatusResponse { // 256 bytes
    uint32_t success_;
    char     message_[252];
};
static_assert(sizeof(DiskStatusResponse) == 256);

struct DiskIoEntry {
    char     name_[16];     // "ada0", "nvd0", "da0" …
    uint32_t read_kbps_;    // KB/s read  (1-second rolling sample)
    uint32_t write_kbps_;   // KB/s write
    uint32_t util_pct_;     // 0–100 utilisation
    uint8_t  pad_[4];
};
static_assert(sizeof(DiskIoEntry) == 32);

struct DiskIoPage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;        // 0–6 entries in this page
    uint32_t    has_more_;
    DiskIoEntry entries_[6];   // 6 × 32 = 192 bytes
};
static_assert(sizeof(DiskIoPage) <= 256);

// ─── SMART Disk Health (com.freebsdx.Smart) ──────────────────────────────────
// Per-drive SMART attribute summary via smartmontools / atactl(8).
//
// Flow:
//   1. xshell calls ListDrives(page=0) on connect.
//   2. No write operations; read-only panel.

enum class SmartOpcode : uint32_t {
    ListDrives = 0x01, // page in payload[0..3] → SmartPage
};

struct SmartDrive {
    char     name_[16];         // e.g. "ada0", "da1"
    char     health_[8];        // "PASSED", "FAILED", "CAUTION"
    int32_t  temp_c_;           // °C; INT32_MIN = unknown
    uint32_t reallocated_;      // reallocated sector count
    uint32_t power_on_hours_;   // total powered-on time
    uint8_t  pad_[28];
};
static_assert(sizeof(SmartDrive) == 64);

struct SmartPage {
    uint32_t   page_;
    uint32_t   total_;
    uint32_t   count_;    // 0–3
    uint32_t   has_more_;
    SmartDrive entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(SmartPage) <= 256);

// ─── Wi-Fi Hotspot ────────────────────────────────────────────────────────────
// com.freebsdx.Hotspot — hostapd(8) managed access-point on the wireless NIC.
//
// Flow:
//   1. xshell calls GetStatus on connect.
//   2. SetEnabled starts/stops hostapd; Configure writes hostapd.conf and
//      restarts the daemon.
//   3. StatusChanged fires when a client associates/deassociates or the AP
//      starts/stops.
enum class HotspotOpcode : uint32_t {
    GetStatus     = 0x01, // → HotspotStatus
    SetEnabled    = 0x02, // HotspotEnableRequest → ack + broadcast (non-sandboxed)
    Configure     = 0x03, // HotspotConfig → ack (non-sandboxed)
    StatusChanged = 0x04, // broadcast: HotspotStatus
};

struct HotspotStatus {
    uint32_t enabled_;        // 1 = AP active
    uint32_t client_count_;   // associated client count
    char     ssid_[32];       // current SSID
    uint8_t  pad_[216];
};
static_assert(sizeof(HotspotStatus) == 256);

struct HotspotEnableRequest {
    uint32_t enabled_;        // 1 = start AP, 0 = stop
    uint8_t  pad_[252];
};
static_assert(sizeof(HotspotEnableRequest) <= 256);

struct HotspotConfig {
    char    ssid_[32];        // desired SSID (NUL-terminated)
    char    passphrase_[64];  // WPA2 passphrase (empty = open network)
    uint8_t pad_[160];
};
static_assert(sizeof(HotspotConfig) == 256);

// ─── Screen Cast (com.freebsdx.Cast) ─────────────────────────────────────────
// Miracast / AirPlay-compatible wireless display mirroring to LAN receivers.
//
// Flow:
//   1. xshell calls Scan to populate the receiver list.
//   2. Connect starts the session; StateChanged is broadcast on connection.
//   3. Disconnect ends the session; StateChanged broadcast follows.

enum class CastOpcode : uint32_t {
    Scan         = 0x01, // no payload; page 0 → CastReceiverPage (repeated pages)
    Connect      = 0x02, // payload: CastConnectRequest (no response)
    Disconnect   = 0x03, // no payload (no response)
    GetState     = 0x04, // → CastState
    StateChanged = 0x05, // broadcast: CastState
};

struct CastReceiver {
    char    name_[40];    // display name e.g. "Living Room TV"
    uint8_t signal_;      // 0–100 signal strength
    uint8_t protocol_;    // 0=Miracast 1=AirPlay
    uint8_t pad_[6];
};
static_assert(sizeof(CastReceiver) == 48);

struct CastReceiverPage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;   // 0–4
    uint32_t     has_more_;
    CastReceiver entries_[4]; // 4 × 48 = 192 bytes
};
static_assert(sizeof(CastReceiverPage) <= 256);

struct CastState {
    uint32_t casting_;          // 1 = session active
    char     receiver_name_[40]; // name of current receiver
    uint32_t elapsed_secs_;     // seconds since session started
    uint8_t  pad_[208];
};
static_assert(sizeof(CastState) == 256);

struct CastConnectRequest {
    char    receiver_name_[40]; // name of receiver to connect
    uint8_t pad_[216];
};
static_assert(sizeof(CastConnectRequest) == 256);

// ─── Crash Reporter (com.freebsdx.CrashReporter) ────────────────────────────
// Tracks core dumps created by the system (via kern.corefile / devd(8) events).
// Each report records the faulting process, signal, and timestamp.
//
// Flow:
//   1. xshell calls ListReports on connect to populate the crash list.
//   2. DeleteReport removes a single entry; ClearAll removes all.
//   3. ReportAdded is broadcast when a new core dump arrives.

enum class CrashOpcode : uint32_t {
    ListReports  = 0x01, // page in payload[0..3] → CrashReportPage
    DeleteReport = 0x02, // payload: CrashDeleteRequest (no response)
    ClearAll     = 0x03, // no payload (no response)
    ReportAdded  = 0x04, // broadcast: CrashReport (single new entry)
};

struct CrashReport {
    char     app_[32];       // process name  e.g. "firefox"
    char     signal_[8];     // "SIGSEGV", "SIGABRT", …
    uint64_t timestamp_;     // Unix epoch seconds
    uint32_t report_id_;     // opaque handle for DeleteReport
    uint8_t  pad_[12];
};
static_assert(sizeof(CrashReport) == 64);

struct CrashReportPage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;      // 0–3 entries in this page
    uint32_t    has_more_;
    CrashReport entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(CrashReportPage) <= 256);

struct CrashDeleteRequest {
    uint32_t report_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(CrashDeleteRequest) <= 256);

// ─── Security Events ─────────────────────────────────────────────────────────
// com.freebsdx.Security — surfaces auth/audit events from utx(5) and the
// PAM stack (SSH logins, sudo grants, local login successes/failures).
//
// Flow:
//   1. xshell calls ListEvents on connect (page 0) to populate the log.
//   2. ClearEvents wipes the in-memory ring buffer.
//   3. EventAdded is broadcast whenever a new auth event is recorded.

enum class SecurityOpcode : uint32_t {
    ListEvents  = 0x01, // page in payload[0..3] → SecurityEventPage
    ClearEvents = 0x02, // no payload (no response)
    EventAdded  = 0x03, // broadcast: SecurityEvent (single new entry)
};

struct SecurityEvent {
    char     type_[8];      // "SSH", "sudo", "login", "fail"
    uint64_t timestamp_;    // Unix epoch seconds  (offset 8, aligned)
    char     user_[24];     // username            (offset 16)
    char     source_[20];   // IP address or "local" (offset 40)
    uint32_t success_;      // 1 = auth succeeded, 0 = failed (offset 60)
};                          // total = 8+8+24+20+4 = 64 bytes
static_assert(sizeof(SecurityEvent) == 64);

struct SecurityEventPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;    // 0–3 entries in this page
    uint32_t      has_more_;
    SecurityEvent entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(SecurityEventPage) <= 256);

// ─── Downloads (com.freebsdx.Downloads) ──────────────────────────────────────
// Tracks file downloads initiated by any sandboxed app that requested the
// Downloads capability.  Exposes paged listing, open-folder, and clear.
//
// Flow:
//   1. xshell calls ListDownloads(page=0) on connect.
//   2. DownloadProgress is broadcast every ~1 s while transfers are active.
//   3. DownloadCompleted / DownloadFailed are broadcast on terminal state.

enum class DownloadOpcode : uint32_t {
    ListDownloads    = 0x01, // page in payload[0..3] → DownloadPage
    OpenFile         = 0x02, // payload: DownloadFileRequest  (no response)
    ClearCompleted   = 0x03, // no payload (no response)
    DownloadProgress = 0x04, // broadcast: DownloadEntry (updated entry)
    DownloadComplete = 0x05, // broadcast: DownloadEntry (state_ = 1)
    DownloadFailed   = 0x06, // broadcast: DownloadEntry (state_ = 2)
};

struct DownloadEntry {
    uint64_t total_bytes_;     // 0 if unknown              (offset  0)
    uint64_t received_bytes_;  // bytes received so far     (offset  8)
    uint32_t id_;              // opaque handle for OpenFile (offset 16)
    uint32_t speed_kbps_;      // current transfer rate     (offset 20)
    uint32_t state_;           // 0=active 1=done 2=failed  (offset 24)
    uint8_t  pad_[4];          //                           (offset 28)
    char     filename_[64];    // display name (UTF-8)      (offset 32)
};                             // total = 32 + 64 = 96 bytes
static_assert(sizeof(DownloadEntry) == 96);

struct DownloadPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;     // 0–2 entries in this page
    uint32_t      has_more_;
    DownloadEntry entries_[2]; // 2 × 96 = 192 bytes
};
static_assert(sizeof(DownloadPage) <= 256);

struct DownloadFileRequest {
    uint32_t id_;      // download id to open
    uint8_t  pad_[252];
};
static_assert(sizeof(DownloadFileRequest) <= 256);

// ─── Package Updates (com.freebsdx.Packages) ─────────────────────────────────
// Surfaces pending pkg(8) upgrades; maps to `pkg upgrade --dry-run` output.
//
// Flow:
//   1. xshell calls ListUpdates(page=0) on connect to populate the panel.
//   2. UpdateAll triggers an async `pkg upgrade -y`; UpdateFinished broadcast
//      signals completion.
//   3. HoldPackage marks a package to be skipped by UpdateAll.

enum class PackageOpcode : uint32_t {
    ListUpdates    = 0x01, // page in payload[0..3] → PackagePage
    UpdateAll      = 0x02, // no payload; async — UpdateFinished broadcast on done
    HoldPackage    = 0x03, // payload: PackageHoldRequest (no response)
    UpdateStarted  = 0x04, // broadcast: no payload
    UpdateFinished = 0x05, // broadcast: no payload (re-poll ListUpdates)
};

struct PackageUpdate {
    uint64_t total_bytes_;   // download size in bytes   (offset  0)
    uint32_t pkg_id_;        // opaque handle for Hold   (offset  8)
    uint32_t size_kb_;       // installed size delta (KB) (offset 12)
    char     name_[32];      // package name             (offset 16)
    char     old_ver_[16];   // currently installed      (offset 48)
    char     new_ver_[16];   // available version        (offset 64)
    uint8_t  pad_[16];       //                          (offset 80)
};                           // total = 96 bytes
static_assert(sizeof(PackageUpdate) == 96);

struct PackagePage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;    // 0–2 entries
    uint32_t      has_more_;
    PackageUpdate entries_[2]; // 2 × 96 = 192 bytes
};
static_assert(sizeof(PackagePage) <= 256);

struct PackageHoldRequest {
    uint32_t pkg_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(PackageHoldRequest) <= 256);

// ─── System Services (com.freebsdx.Services) ─────────────────────────────────
// Exposes rc(8)-managed daemon status for key system services.
// Start/Stop map to service(8) start/stop under the hood.
//
// Flow:
//   1. xshell calls ListServices(page=0) on connect.
//   2. StartService / StopService take effect asynchronously; ServiceChanged
//      is broadcast when the daemon transitions state.

enum class ServiceOpcode : uint32_t {
    ListServices   = 0x01, // page in payload[0..3] → ServicePage
    StartService   = 0x02, // payload: ServiceRequest (no response)
    StopService    = 0x03, // payload: ServiceRequest (no response)
    ServiceChanged = 0x04, // broadcast: ServiceEntry (updated entry)
};

struct ServiceEntry {
    char     name_[32];    // e.g. "sshd", "ntpd", "cupsd"
    char     state_[8];    // "running", "stopped", "disabled"
    uint32_t pid_;         // PID when running, 0 otherwise
    uint32_t enabled_;     // 1 = rc.conf enabled (starts at boot)
    uint8_t  pad_[16];
};
static_assert(sizeof(ServiceEntry) == 64);

struct ServicePage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;   // 0–3 entries
    uint32_t     has_more_;
    ServiceEntry entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(ServicePage) <= 256);

struct ServiceRequest {
    char    name_[32];  // service name to start or stop
    uint8_t pad_[224];
};
static_assert(sizeof(ServiceRequest) <= 256);

// ─── Font Manager ──────────────────────────────────────────────────────────────

enum class FontManagerOpcode : uint32_t {
    ListFonts    = 0x01, // payload: FontPageRequest → FontListPage (paged, one font per page)
    InstallFont  = 0x02, // payload: FontInstallRequest → FontStatusResponse (async)
    RemoveFont   = 0x03, // payload: FontRemoveRequest  → FontStatusResponse (async)
    FontsChanged = 0xFF, // broadcast: font list changed after install/remove
};

struct FontInfo { // 240 bytes
    char     family_[48];            // e.g. "DejaVu Sans"
    char     style_[32];             // e.g. "Bold Italic"
    char     path_[128];             // e.g. "/usr/local/share/fonts/dejavu/DejaVuSans.ttf"
    char     format_[8];             // "TTF", "OTF", "TTC", "Type1", "BDF", "PCF"
    uint32_t is_user_installed_;     // 1 if under ~/.fonts, 0 = system font
    uint8_t  _pad[20];
};
static_assert(sizeof(FontInfo) == 240);

struct FontListPage { // 256 bytes
    uint32_t page_;
    uint32_t has_more_;
    uint32_t total_;
    uint32_t count_;    // always 1 when has entry
    FontInfo font_;
};
static_assert(sizeof(FontListPage) == 256);

struct FontPageRequest { // 256 bytes
    uint32_t page_;
    uint8_t  _pad[252];
};
static_assert(sizeof(FontPageRequest) == 256);

struct FontInstallRequest { // 256 bytes
    char path_[256];
};
static_assert(sizeof(FontInstallRequest) == 256);

struct FontRemoveRequest { // 256 bytes
    char path_[256];
};
static_assert(sizeof(FontRemoveRequest) == 256);

struct FontStatusResponse { // 256 bytes
    uint32_t success_;
    char     message_[252];
};
static_assert(sizeof(FontStatusResponse) == 256);

// ─── Terminal Multiplexer (com.freebsdx.Terminal) ─────────────────────────────
// tmux-style session management.  PTYs live in the compositor TerminalService.
//
// Typical flow:
//   app → CreateSession("name", cols, rows) → TerminalSessionReady(id, name)
//   app → AttachSession(id)                 → TerminalSessionReady(id)   (ACK)
//   service → OutputChunk(id, data) broadcast (to all clients attached to id)
//   app → SendInput(id, data)
//   app → ResizeSession(id, cols, rows)
//   app → RenameSession(id, "newname")
//   app → DetachSession(id)                 → keeps PTY running
//   app → DestroySession(id)                → kills PTY
//   app → ListSessions(page)                → TerminalListPage
//   service → SessionChanged broadcast      → any app refresh its session list

enum class TerminalOpcode : uint32_t {
    CreateSession  = 0x01,  // app→service: fork a new PTY session
    DestroySession = 0x02,  // app→service: kill PTY and remove session
    ListSessions   = 0x03,  // app→service: enumerate running sessions
    AttachSession  = 0x04,  // app→service: subscribe to OutputChunk for id
    DetachSession  = 0x05,  // app→service: unsubscribe (session keeps running)
    RenameSession  = 0x06,  // app→service: change session display name
    ResizeSession  = 0x07,  // app→service: update PTY dimensions
    SendInput      = 0x08,  // app→service: bytes to feed into PTY stdin
    OutputChunk    = 0x09,  // service→app broadcast: PTY stdout/stderr bytes
    SessionChanged = 0x0A,  // service broadcast: session list changed
    SessionReady   = 0x0B,  // service→app response to Create/Attach
};

// Request for CreateSession.  256 bytes.
struct TerminalCreateRequest {
    char     name_[60];     // 60 — display name; empty = auto "session-N"
    uint16_t cols_;         //  2 — initial PTY width  (default 80)
    uint16_t rows_;         //  2 — initial PTY height (default 24)
    uint8_t  pad_[192];     // 192
};
static_assert(sizeof(TerminalCreateRequest) == 256);

// Response to CreateSession / AttachSession.  256 bytes.
struct TerminalSessionReady {
    uint32_t id_;           //  4 — 0 = failure
    uint32_t success_;      //  4 — 1 = ok
    char     name_[60];     // 60
    uint8_t  pad_[188];     // 188
};
static_assert(sizeof(TerminalSessionReady) == 256);

// One entry in the session list.  64 bytes.
struct TerminalSessionEntry {
    uint32_t id_;           //  4
    char     name_[44];     // 44
    uint32_t created_;      //  4 — unix timestamp
    uint32_t last_active_;  //  4 — unix timestamp
    uint8_t  attached_;     //  1 — 1 if ≥1 client currently attached
    uint8_t  pad_[7];       //  7
};
static_assert(sizeof(TerminalSessionEntry) == 64);

// List page: up to 3 entries per message.  208 bytes ≤ 256.
struct TerminalListPage {
    uint32_t count_;        //  4 — entries in this page (0–3)
    uint32_t page_;         //  4 — zero-based page index
    uint32_t has_more_;     //  4 — 1 if more pages exist
    uint32_t total_;        //  4 — total session count
    TerminalSessionEntry entries_[3]; // 192
};
static_assert(sizeof(TerminalListPage) <= 256);

// Generic single-id request (AttachSession, DetachSession, DestroySession,
// ListSessions uses page in session_id_ field).  256 bytes.
struct TerminalSessionIdRequest {
    uint32_t session_id_;   //  4 — session id (or page number for ListSessions)
    uint8_t  pad_[252];     // 252
};
static_assert(sizeof(TerminalSessionIdRequest) == 256);

// Raw bytes to/from the PTY.  256 bytes (max 248 bytes of data per message).
struct TerminalDataChunk {
    uint32_t session_id_;   //  4
    uint32_t size_;         //  4 — number of valid bytes in data_
    uint8_t  data_[248];    // 248
};
static_assert(sizeof(TerminalDataChunk) == 256);

// Resize PTY.  256 bytes.
struct TerminalResizeRequest {
    uint32_t session_id_;   //  4
    uint16_t cols_;         //  2
    uint16_t rows_;         //  2
    uint8_t  pad_[248];     // 248
};
static_assert(sizeof(TerminalResizeRequest) == 256);

// Rename session.  256 bytes.
struct TerminalRenameRequest {
    uint32_t session_id_;   //  4
    char     name_[60];     // 60
    uint8_t  pad_[192];     // 192
};
static_assert(sizeof(TerminalRenameRequest) == 256);

// ─── Package Signing (com.freebsdx.PackageSigning) ──────────────────────────
// Trust store: ~/.freebsdx-pkg-trust/keys/  (one .pub / .asc file per trusted key)
// Policy file: ~/.freebsdx-pkg-trust/policy  (require_signed=0|1\nallow_unknown=0|1)
// Signature sidecar: <pkg>.xpkg.minisig  (minisign) or  <pkg>.xpkg.sig  (GPG detached)
//
// Sign:   minisign -S -s secret.key -m App.xpkg          → App.xpkg.minisig
// Verify: minisign -V -p pubkey.pub -m App.xpkg          (exit 0 = ok)
// Alt:    gpg --detach-sign App.xpkg                     → App.xpkg.sig
//         gpg --verify App.xpkg.sig App.xpkg
//
// AppManagerService checks signatures before installing .xpkg bundles.
// When require_signed=1 any package that fails verification is rejected.
// Sandboxed apps may not call ImportKey / RemoveKey / SetPolicy.

enum class PackageSigningOpcode : uint32_t {
    ImportKey      = 0x01,  // PkgKeyImportRequest  → PkgSigningStatusResponse
    RemoveKey      = 0x02,  // PkgKeyIdRequest      → PkgSigningStatusResponse
    ListKeys       = 0x03,  // PkgKeyPageRequest    → PkgKeyListPage
    SetPolicy      = 0x04,  // PkgSigningPolicy     → PkgSigningStatusResponse
    GetPolicy      = 0x05,  // (no payload)         → PkgSigningPolicy
    SigningChanged = 0x06,  // broadcast: trust store or policy changed
};

// One trusted public key entry.  64 bytes.
struct PkgKeyEntry {
    char fingerprint_[32];  // minisign key id or GPG fingerprint (hex prefix)
    char name_[28];         // label extracted from key comment
    char type_[4];          // "ms\0" = minisign, "gpg" = GPG
};
static_assert(sizeof(PkgKeyEntry) == 64);

// Paged key list response.  256 bytes.
struct PkgKeyListPage {
    uint32_t    count_;      //  4
    uint32_t    page_;       //  4
    uint32_t    has_more_;   //  4
    uint32_t    total_;      //  4
    PkgKeyEntry entries_[3]; // 192
    uint8_t     pad_[48];    //  48  → 256
};
static_assert(sizeof(PkgKeyListPage) == 256);

// Signing policy.  256 bytes.
struct PkgSigningPolicy {
    uint32_t require_signed_; // 1 = reject packages with no valid signature
    uint32_t allow_unknown_;  // 1 = silently allow packages with no sidecar (warn only)
    uint8_t  pad_[248];
};
static_assert(sizeof(PkgSigningPolicy) == 256);

// Page request for ListKeys.  256 bytes.
struct PkgKeyPageRequest {
    uint32_t page_;
    uint8_t  pad_[252];
};
static_assert(sizeof(PkgKeyPageRequest) == 256);

// Import a public key by local file path.  256 bytes.
struct PkgKeyImportRequest {
    char    path_[252];
    uint8_t pad_[4];
};
static_assert(sizeof(PkgKeyImportRequest) == 256);

// Identify a key by fingerprint for removal.  256 bytes.
struct PkgKeyIdRequest {
    char    fingerprint_[32];
    uint8_t pad_[224];
};
static_assert(sizeof(PkgKeyIdRequest) == 256);

// Generic status / result response.  256 bytes.
struct PkgSigningStatusResponse {
    uint32_t success_;       //  4 — 1 = ok, 0 = error
    char     message_[252];  // 252
};
static_assert(sizeof(PkgSigningStatusResponse) == 256);

// ─── System Update ───────────────────────────────────────────────────────────
// com.freebsdx.Update — checks for pkg(8) updates and applies them via ZFS BEs.
//
// Flow:
//   1. xshell calls CheckForUpdates on connect and every 60 min thereafter.
//   2. UpdateService runs `pkg version -l '<'` via popen to count outdated pkgs.
//   3. UpdateService responds with UpdateAvailableState and also broadcasts
//      UpdatesAvailable so any other client can react.
//   4. When count > 0 xshell shows a badge on the taskbar updates button and
//      posts a notification. Clicking the button launches AppManager.
//   5. ApplyUpdate: service creates a ZFS BE, fetches+extracts update.txz, then
//      activates the BE. Reboot is required; a notification prompts the user.
enum class UpdateOpcode : uint32_t {
    CheckForUpdates  = 0x01, // (no payload) → UpdateAvailableState; forks pkg check
    ApplyUpdate      = 0x02, // (no payload) fire-and-forget; ZFS BE update, reboot needed
    UpdatesAvailable = 0x03, // broadcast: UpdateAvailableState (emitted after each check)
};

struct UpdateAvailableState {
    uint32_t count_;     // packages with pending updates (0 = system is up-to-date)
    uint8_t  pad_[252];
};
static_assert(sizeof(UpdateAvailableState) <= 256);

// ─── Night Shift ─────────────────────────────────────────────────────────────
// com.freebsdx.NightShift — warm color-temperature filter applied to all outputs.
// The compositor multiplies pixel RGB by a warm matrix whose blend factor is
// driven by intensity_ (0 = no effect / 6500 K, 100 = max warmth / ~2700 K).
enum class NightShiftOpcode : uint32_t {
    GetState     = 0x01, // → NightShiftState (any app)
    SetConfig    = 0x02, // NightShiftConfig → ack + StateChanged broadcast
    StateChanged = 0x03, // broadcast: NightShiftState
};

struct NightShiftState {
    uint32_t enabled_;    // 1 = filter active
    uint32_t intensity_;  // 0–100 (warmth level; 50 is a mild warm tint)
    uint32_t auto_mode_;  // 1 = auto-enable at sunset, disable at sunrise
    uint8_t  pad_[244];
};
static_assert(sizeof(NightShiftState) <= 256);

struct NightShiftConfig {
    uint32_t enabled_;
    uint32_t intensity_;
    uint32_t auto_mode_;
    uint8_t  pad_[244];
};
static_assert(sizeof(NightShiftConfig) <= 256);

// ─── Privacy ─────────────────────────────────────────────────────────────────
// com.freebsdx.Privacy — kernel-level camera/microphone access control.
//
// Flow:
//   1. xshell calls GetStatus on connect.
//   2. Block/Allow calls toggle devd rules or device-node permissions.
//   3. DeviceStateChanged broadcast fires when access state or current user
//      changes (e.g. an app opens or releases the camera device).
enum class PrivacyOpcode : uint32_t {
    GetStatus          = 0x01, // → PrivacyStatus
    SetCameraBlocked   = 0x02, // PrivacyDeviceRequest → ack + broadcast (non-sandboxed)
    SetMicBlocked      = 0x03, // PrivacyDeviceRequest → ack + broadcast (non-sandboxed)
    DeviceStateChanged = 0x04, // broadcast: PrivacyStatus
};

struct PrivacyStatus {
    uint32_t camera_blocked_; // 1 = camera access globally denied
    uint32_t mic_blocked_;    // 1 = mic access globally denied
    char     camera_app_[48]; // name of app currently using camera ("" if none)
    char     mic_app_[48];    // name of app currently using mic   ("" if none)
    uint8_t  pad_[152];
};
static_assert(sizeof(PrivacyStatus) == 256);

struct PrivacyDeviceRequest {
    uint32_t blocked_;  // 1 = block, 0 = allow
    uint8_t  pad_[252];
};
static_assert(sizeof(PrivacyDeviceRequest) <= 256);

// ─── Process ──────────────────────────────────────────────────────────────────
// com.freebsdx.Process — top(1)-style process list and signal delivery.
//
// Flow:
//   1. xshell polls ListTop every 5 s alongside thermal/connections.
//   2. ProcessService reads /proc or sysctl kern.proc.all, sorts by CPU%.
//   3. Kill sends SIGTERM; if the process is still present after 2 s it
//      escalates to SIGKILL (non-sandboxed, uid-gated by the service).
enum class ProcessOpcode : uint32_t {
    ListTop = 0x01, // → paged ProcessPage sorted by CPU% descending
    Kill    = 0x02, // ProcessKillRequest → ack (non-sandboxed)
};

struct ProcessEntry {
    char     name_[32];    // process name (comm, NUL-terminated)
    uint32_t pid_;
    uint32_t cpu_pct10_;   // CPU% × 10  (e.g. 125 = 12.5 %)
    uint32_t rss_kb_;      // resident set size in KB
    uint8_t  pad_[20];
};
static_assert(sizeof(ProcessEntry) == 64);

struct ProcessPage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;       // entries in this page (0–3)
    uint32_t     has_more_;
    ProcessEntry entries_[3];  // 3 × 64 = 192 bytes
};
static_assert(sizeof(ProcessPage) <= 256);

struct ProcessKillRequest {
    uint32_t pid_;
    uint32_t signal_;   // 15 = SIGTERM (default), 9 = SIGKILL
    uint8_t  pad_[248];
};
static_assert(sizeof(ProcessKillRequest) <= 256);

// ─── Firewall ────────────────────────────────────────────────────────────────
// com.freebsdx.Firewall — pf(4) / ipfw(8) control.
//
// Flow:
//   1. xshell calls GetStatus on connect; FirewallService reads pf/ipfw state.
//   2. SetEnabled up/downs the firewall; BlockAllInbound inserts a catch-all
//      block rule before existing rules (emergency lock-down).
//   3. StatusChanged broadcast fires after any state change.
enum class FirewallOpcode : uint32_t {
    GetStatus       = 0x01, // → FirewallStatus
    SetEnabled      = 0x02, // FirewallEnableRequest → ack + broadcast (non-sandboxed)
    BlockAllInbound = 0x03, // (no payload) → ack (non-sandboxed)
    StatusChanged   = 0x04, // broadcast: FirewallStatus
};

struct FirewallStatus {
    uint32_t enabled_;      // 1 = firewall active
    uint32_t rule_count_;   // active rule count
    uint32_t block_all_;    // 1 = emergency block-all-inbound rule in effect
    uint8_t  pad_[244];
};
static_assert(sizeof(FirewallStatus) <= 256);

struct FirewallEnableRequest {
    uint32_t enabled_;      // 1 = enable, 0 = disable
    uint8_t  pad_[252];
};
static_assert(sizeof(FirewallEnableRequest) <= 256);

// ─── DNS Ad-Blocker (com.freebsdx.AdBlock) ───────────────────────────────────
// System-level DNS sinkhole via unbound(8) + regularly-updated blocklist.
// Counts reflect the number of domains currently in the active blocklist.
//
// Flow:
//   1. xshell calls GetStatus on connect.
//   2. SetEnabled(1/0) starts/stops the unbound instance.
//   3. UpdateList triggers an async blocklist refresh; StatusChanged is
//      broadcast when the new list is applied.

enum class AdBlockOpcode : uint32_t {
    GetStatus    = 0x01, // → AdBlockStatus
    SetEnabled   = 0x02, // payload: AdBlockEnableRequest (no response)
    UpdateList   = 0x03, // no payload; async — StatusChanged broadcast on completion
    StatusChanged = 0x04, // broadcast: AdBlockStatus
};

struct AdBlockStatus {
    uint32_t enabled_;          // 1 = sinkhole active
    uint32_t domain_count_;     // number of blocked domains
    char     list_updated_[24]; // human-readable date "Jan 14 2026\0"
    uint32_t updating_;         // 1 = list refresh in progress
    uint8_t  pad_[220];
};
static_assert(sizeof(AdBlockStatus) == 256);

struct AdBlockEnableRequest {
    uint32_t enabled_; // 1 = on, 0 = off
    uint8_t  pad_[252];
};
static_assert(sizeof(AdBlockEnableRequest) <= 256);

// ─── Netstat ──────────────────────────────────────────────────────────────────
// com.freebsdx.Netstat — established socket enumeration via sockstat(1).
//
// Flow:
//   1. xshell polls ListConnections every 5 s via the main loop tick.
//   2. NetstatService runs sockstat -c and parses established TCP/UDP rows.
//   3. KillConnection sends RST via setsockopt SO_LINGER + close on the fd
//      identified by conn_id_, or kills the owning PID as fallback.
enum class NetstatOpcode : uint32_t {
    ListConnections = 0x01, // → paged NetstatPage
    KillConnection  = 0x02, // NetstatKillRequest → ack (non-sandboxed)
};

struct NetstatConnection {
    char     app_[24];      // process name or "(unknown)"
    char     remote_[40];   // "addr:port"  e.g. "93.184.216.34:443"
    char     proto_[8];     // "tcp" / "udp"
    char     state_[16];    // "ESTABLISHED" / "CLOSE_WAIT" / "TIME_WAIT" …
    uint32_t pid_;          // owning PID
    uint32_t conn_id_;      // opaque id for KillConnection
};
static_assert(sizeof(NetstatConnection) == 96);

struct NetstatPage {
    uint32_t          page_;
    uint32_t          total_;
    uint32_t          count_;      // 0–2 connections in this page
    uint32_t          has_more_;
    NetstatConnection conns_[2];   // 2 × 96 = 192 bytes
};
static_assert(sizeof(NetstatPage) <= 256);

struct NetstatKillRequest {
    uint32_t conn_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(NetstatKillRequest) <= 256);

// ─── Thermal ──────────────────────────────────────────────────────────────────
// com.freebsdx.Thermal — sysctl coretemp / amdtemp / hw.acpi.thermal readings.
//
// Flow:
//   1. xshell calls GetReadings on connect and every 5 s in the main loop.
//   2. ThermalService reads sysctl hw.acpi.thermal.tz*.temperature and
//      dev.cpu.*.temperature (coretemp/amdtemp) plus hw.acpi.fan.*.speed.
//   3. Results arrive as paged ThermalPage messages (6 readings per page).
enum class ThermalSensorOpcode : uint32_t {
    GetReadings = 0x01, // → paged ThermalPage (temp zones + fan sensors)
};

struct ThermalReading {
    char     label_[24];  // e.g. "CPU Core 0", "GPU", "Chassis", "Fan 0"
    int32_t  temp_c_;     // °C; INT32_MIN if this row is fan-only
    uint32_t fan_rpm_;    // RPM; 0 if not a fan sensor
};
static_assert(sizeof(ThermalReading) == 32);

struct ThermalPage {
    uint32_t       page_;
    uint32_t       total_;
    uint32_t       count_;       // readings in this page (0–6)
    uint32_t       has_more_;
    ThermalReading readings_[6]; // 6 × 32 = 192 bytes
};
static_assert(sizeof(ThermalPage) <= 256);

// ─── USB Devices ──────────────────────────────────────────────────────────────
// com.freebsdx.Usb — usbconfig(8) / devd(8) integration for hot-plug events.
//
// Flow:
//   1. xshell calls ListDevices on connect and after each DeviceAdded/Removed.
//   2. UsbService enumerates attached devices via usbconfig -a list.
//   3. Storage devices carry mount_ (from geom/mount tables) and capacity_mb_.
//   4. Eject: unmount(8) + usbconfig power-off on the given addr_.
enum class UsbOpcode : uint32_t {
    ListDevices   = 0x01, // → paged UsbDevicePage
    Eject         = 0x02, // UsbEjectRequest → ack (non-sandboxed)
    DeviceAdded   = 0x03, // broadcast: UsbDevice
    DeviceRemoved = 0x04, // broadcast: UsbDevice (addr_ identifies device)
};

struct UsbDevice {
    char     name_[56];       // product string from USB descriptor
    char     mount_[56];      // mount point if storage device, else ""
    char     type_[12];       // "storage" / "hid" / "audio" / "hub" / "other"
    uint32_t capacity_mb_;    // 0 if not a mass-storage device
    uint32_t addr_;           // bus<<8 | devaddr (unique per session)
    uint8_t  pad_[108];
};
static_assert(sizeof(UsbDevice) <= 256);

struct UsbDevicePage {
    uint32_t  page_;          // 0-based
    uint32_t  total_;         // total device count
    uint32_t  count_;         // devices in this page (0 or 1)
    uint32_t  has_more_;      // 1 if another page follows
    UsbDevice device_;        // one device per page
};
static_assert(sizeof(UsbDevicePage) <= 256);

struct UsbEjectRequest {
    uint32_t addr_;           // addr_ from UsbDevice
    uint8_t  pad_[252];
};
static_assert(sizeof(UsbEjectRequest) <= 256);

// ─── Color Filter ─────────────────────────────────────────────────────────────
// com.freebsdx.ColorFilter — display color correction for color-blind users.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetEnabled / SetType / SetIntensity / SetMonochrome update compositor-side
//      color LUT applied to the whole display output.
//   3. StateChanged fires on any change.

enum class ColorFilterOpcode : uint32_t {
    GetState      = 0x01, // → ColorFilterState
    SetEnabled    = 0x02, // ColorFilterEnableRequest → ack
    SetType       = 0x03, // ColorFilterTypeRequest   → ack
    SetIntensity  = 0x04, // ColorFilterIntensityRequest → ack
    SetMonochrome = 0x05, // ColorFilterToggleRequest → ack
    StateChanged  = 0x06, // broadcast: ColorFilterState
};

enum class ColorFilterType : uint32_t {
    Normal       = 0,
    Deuteranopia = 1, // red-green (most common)
    Protanopia   = 2, // red-green (rare)
    Tritanopia   = 3, // blue-yellow
};

struct ColorFilterState {
    uint32_t enabled_;        // 1 = filter active
    uint32_t type_;           // ColorFilterType
    uint32_t intensity_;      // 0–100 (%)
    uint32_t monochrome_;     // 1 = full grayscale
    uint8_t  pad_[240];       // 4+4+4+4+240 = 256
};
static_assert(sizeof(ColorFilterState) == 256);

struct ColorFilterEnableRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ColorFilterEnableRequest) <= 256);

struct ColorFilterTypeRequest {
    uint32_t type_; // ColorFilterType
    uint8_t  pad_[252];
};
static_assert(sizeof(ColorFilterTypeRequest) <= 256);

struct ColorFilterIntensityRequest {
    uint32_t intensity_; // 0–100
    uint8_t  pad_[252];
};
static_assert(sizeof(ColorFilterIntensityRequest) <= 256);

struct ColorFilterToggleRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ColorFilterToggleRequest) <= 256);

// ─── Login Sessions ───────────────────────────────────────────────────────────
// com.freebsdx.Sessions — enumerate active login sessions (who(1) / utmpx(5)).
//
// Flow:
//   1. xshell calls ListSessions on connect and on SessionChanged broadcast.
//   2. KillSession sends SIGHUP to the session leader (requires CAP_KILL or
//      the caller owning the session).
//   3. SessionChanged is broadcast when a user logs in or out.

enum class SessionOpcode : uint32_t {
    ListSessions   = 0x01, // page (uint32) in payload → SessionPage
    KillSession    = 0x02, // SessionKillRequest → ack
    SessionChanged = 0x03, // broadcast: no payload (re-poll ListSessions)
};

struct SessionEntry {                // 64 B — uint64_t first to avoid padding
    uint64_t login_time_;            // Unix epoch (seconds)
    uint32_t session_id_;            // opaque compositor-assigned id
    char     type_[8];               // "graphical" / "ssh" / "tty"
    char     user_[20];              // login username
    char     source_[24];            // IP address or tty name
};                                   // 8+4+8+20+24 = 64
static_assert(sizeof(SessionEntry) == 64);

struct SessionPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;     // 0–3
    uint32_t      has_more_;
    SessionEntry  entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(SessionPage) <= 256);

struct SessionKillRequest {
    uint32_t session_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(SessionKillRequest) <= 256);

// ─── Keyboard Backlight ───────────────────────────────────────────────────────
// com.freebsdx.KeyboardBacklight — per-key or uniform backlight brightness.
//
// Flow:
//   1. xshell calls GetState on connect; compositor reads sysfs acpi_video/kbd_backlight.
//   2. SetEnabled turns the backlight on (max brightness) or fully off (0).
//   3. SetBrightness sets 0–100 %; compositor maps to sysfs hw_max range.
//   4. StateChanged fires whenever the brightness changes (e.g. Fn key press).

enum class KeyboardBacklightOpcode : uint32_t {
    GetState      = 0x01, // → KeyboardBacklightState
    SetEnabled    = 0x02, // KeyboardBacklightEnableRequest → ack
    SetBrightness = 0x03, // KeyboardBacklightBrightnessRequest → ack
    StateChanged  = 0x04, // broadcast: KeyboardBacklightState
};

struct KeyboardBacklightState {
    uint32_t enabled_;    // 1 = backlight on
    uint32_t brightness_; // 0–100 (%)
    uint8_t  pad_[248];   // 4+4+248 = 256
};
static_assert(sizeof(KeyboardBacklightState) == 256);

struct KeyboardBacklightEnableRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(KeyboardBacklightEnableRequest) <= 256);

struct KeyboardBacklightBrightnessRequest {
    uint32_t brightness_; // 0–100
    uint8_t  pad_[252];
};
static_assert(sizeof(KeyboardBacklightBrightnessRequest) <= 256);

// ─── Battery Drain ────────────────────────────────────────────────────────────
// com.freebsdx.BatteryDrain — per-app energy-impact breakdown via powerd(8).
//
// Flow:
//   1. xshell calls ListDrain on connect and on DrainChanged broadcast.
//   2. RefreshDrain triggers a re-sample of powerd accounting tables.
//   3. DrainChanged broadcast fires after each refresh cycle.

enum class BatteryDrainImpact : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
};

enum class BatteryDrainOpcode : uint32_t {
    ListDrain    = 0x01, // page (uint32) in payload → BatteryDrainPage
    RefreshDrain = 0x02, // → ack; triggers new powerd sample
    DrainChanged = 0x03, // broadcast: no payload (re-poll ListDrain)
};

struct BatteryDrainEntry {        // 64 B — no uint64_t, no alignment issues
    char     app_name_[32];       // application display name (null-terminated)
    uint32_t energy_pct_;         // 0–100: percentage of total battery drain
    uint32_t impact_;             // BatteryDrainImpact enum
    uint32_t pid_;                // process ID; 0 = unknown / not killable
    uint8_t  pad_[20];            // 32+4+4+4+20 = 64
};
static_assert(sizeof(BatteryDrainEntry) == 64);

struct BatteryDrainPage {
    uint32_t         page_;
    uint32_t         total_;
    uint32_t         count_;       // 0–3
    uint32_t         has_more_;
    BatteryDrainEntry entries_[3]; // 3 × 64 = 192 bytes
};
static_assert(sizeof(BatteryDrainPage) <= 256);

// ─── Airplane Mode ────────────────────────────────────────────────────────────
// com.freebsdx.AirplaneMode — master wireless kill-switch (Wi-Fi + Bluetooth).
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetEnabled=1: compositor snapshots current Wi-Fi+BT state into
//      wifi_was_on_ / bt_was_on_, then disables both radios.
//   3. SetEnabled=0: compositor restores wifi_was_on_ / bt_was_on_.
//   4. StateChanged broadcast fires after any toggle.

enum class AirplaneModeOpcode : uint32_t {
    GetState   = 0x01, // → AirplaneModeState
    SetEnabled = 0x02, // AirplaneModeEnableRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: AirplaneModeState
};

struct AirplaneModeState {
    uint32_t enabled_;      // 1 = airplane mode on (all radios off)
    uint32_t wifi_was_on_;  // pre-flight Wi-Fi state (restored on disable)
    uint32_t bt_was_on_;    // pre-flight Bluetooth state (restored on disable)
    uint8_t  pad_[244];     // 4+4+4+244 = 256
};
static_assert(sizeof(AirplaneModeState) == 256);

struct AirplaneModeEnableRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(AirplaneModeEnableRequest) <= 256);

// ─── Cron Jobs ────────────────────────────────────────────────────────────────
// com.freebsdx.CronJobs — read/enable/run crontab(5) entries for the current
// user.  Uses crontab -l / crontab -r / at(1) for RunNow semantics.
//
// Flow:
//   1. xshell calls ListJobs on connect and on JobChanged broadcast.
//   2. SetJobEnabled rewrites the crontab with the entry commented/uncommented.
//   3. RunNow runs the command immediately via sh -c in a detached subprocess.
//   4. JobChanged is broadcast after SetJobEnabled or external crontab edit.

enum class CronOpcode : uint32_t {
    ListJobs      = 0x01, // page (uint32) in payload → CronPage
    SetJobEnabled = 0x02, // CronJobToggleRequest → ack + broadcast
    RunNow        = 0x03, // CronRunRequest → ack
    JobChanged    = 0x04, // broadcast: no payload (re-poll ListJobs)
};

struct CronEntry {           // 64 B — all fields ≤ uint32_t, no alignment hazard
    char     schedule_[24];  // cron schedule string, e.g. "*/5 * * * *"
    char     command_[32];   // command (truncated, null-terminated)
    uint32_t job_id_;        // opaque compositor-assigned id
    uint32_t enabled_;       // 1 = active, 0 = commented-out in crontab
};                           // 24+32+4+4 = 64
static_assert(sizeof(CronEntry) == 64);

struct CronPage {
    uint32_t  page_;
    uint32_t  total_;
    uint32_t  count_;       // 0–3
    uint32_t  has_more_;
    CronEntry entries_[3];  // 3 × 64 = 192 bytes
};
static_assert(sizeof(CronPage) <= 256);

struct CronJobToggleRequest {
    uint32_t job_id_;
    uint32_t enabled_;  // 1 = enable, 0 = disable
    uint8_t  pad_[248];
};
static_assert(sizeof(CronJobToggleRequest) <= 256);

struct CronRunRequest {
    uint32_t job_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(CronRunRequest) <= 256);

// ─── Location Services ───────────────────────────────────────────────────────
// com.freebsdx.LocationServices — per-user location access toggle.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetEnabled=0: compositor revokes all pending location grants.
//   3. requesting_count_ reflects how many apps have an active location grant.
//   4. last_app_ is the display name of the most-recent requester.
//   5. StateChanged broadcast fires on enable/disable or grant changes.

enum class LocationOpcode : uint32_t {
    GetState      = 0x01, // → LocationState
    SetEnabled    = 0x02, // LocationEnableRequest → ack + broadcast
    StateChanged  = 0x03, // broadcast: LocationState
    ListApps      = 0x04, // uint32_t page → LocationAppPage
    SetAppPolicy  = 0x05, // LocationAppPolicyRequest → ack
    PolicyChanged = 0x06, // broadcast (no payload — shell re-fetches app list)
};

struct LocationState {
    uint32_t enabled_;           // 1 = location services on
    uint32_t requesting_count_;  // number of apps currently holding a grant
    char     last_app_[32];      // display name of most-recent requester
    uint8_t  pad_[216];          // 4+4+32+216 = 256
};
static_assert(sizeof(LocationState) == 256);

struct LocationEnableRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(LocationEnableRequest) <= 256);

// ─── GPU Statistics ───────────────────────────────────────────────────────────
// com.freebsdx.GpuStats — real-time GPU metrics polled by xshell every 2 s.
//
// Flow:
//   1. xshell sends GetStats every 2 seconds and updates the AC card.
//   2. compositor reads sysctl dev.drm / hwmon and fills GpuStats.
//   3. INT32_MIN for temp_c_ means sensor unavailable.

enum class GpuOpcode : uint32_t {
    GetStats = 0x01, // → GpuStats (no request payload)
};

struct GpuStats {
    uint32_t usage_pct_;     // 0–100 GPU engine utilisation
    uint32_t vram_used_mb_;  // VRAM in use (MB)
    uint32_t vram_total_mb_; // total VRAM (MB)
    int32_t  temp_c_;        // junction temperature °C; INT32_MIN = unavailable
    uint32_t clock_mhz_;     // core clock (MHz)
    char     name_[32];      // device name, e.g. "AMD Radeon RX 580"
    uint8_t  pad_[204];      // 4+4+4+4+4+32+204 = 256
};
static_assert(sizeof(GpuStats) == 256);

// ── Headless Mode ─────────────────────────────────────────────────────────────
// com.freebsdx.Headless — compositor introspection and frame capture for CI /
// automated testing when xserver is started with --headless (no DRM/KMS).
//
// Flow:
//   1. Client calls GetInfo to verify the compositor is headless and learn the
//      virtual output resolution.
//   2. Client calls CaptureFrame after any XBus round-trip to ensure the most
//      recent composite cycle has completed.  The compositor returns a
//      ScreencopyFrameInfo in the payload and the pixel data in an anonymous
//      memfd sent as an SCM_RIGHTS attachment (same wire format as Screencopy).
//
// Note: CaptureFrame is available on hardware builds too (for testing helpers)
// but GetInfo.headless_ == 0 indicates a real display is attached.

enum class HeadlessOpcode : uint32_t {
    GetInfo      = 0x01, // → HeadlessInfo
    CaptureFrame = 0x02, // → ScreencopyFrameInfo in payload + shm_fd via SCM_RIGHTS
    // Test harness: drive input without hardware and observe the result.
    InjectInput    = 0x03, // payload = InputMessage; fed into the compositor input path
    GetPointer     = 0x04, // → HeadlessPointer (current cursor position)
    GetCursorShape = 0x05, // → uint32 wp_cursor_shape value in payload[0..3]
};

// Response to HeadlessOpcode::GetPointer — the compositor's current cursor
// position in logical pixels.
struct HeadlessPointer {
    float   x_;
    float   y_;
    uint8_t pad_[248];  // 4+4+248 = 256
};

struct HeadlessInfo {
    uint32_t headless_;     // 1 when xserver was started with --headless
    uint32_t width_;        // virtual output width  (pixels)
    uint32_t height_;       // virtual output height (pixels)
    uint32_t frame_count_;  // total frames composited since start
    uint8_t  pad_[240];     // 4+4+4+4+240 = 256
};
static_assert(sizeof(HeadlessInfo) == 256);

// ── Microphone Mute ───────────────────────────────────────────────────────────
// com.freebsdx.MicMute — global hardware-level microphone kill-switch.
// Distinct from per-app Privacy mic access: this cuts the capture path in the
// compositor regardless of which app has a mic grant.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetMuted toggles the compositor's global capture gate.
//   3. StateChanged broadcast fires after any toggle (own or other clients).

enum class MicMuteOpcode : uint32_t {
    GetState   = 0x01, // → MicMuteState
    SetMuted   = 0x02, // MicMuteRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: MicMuteState
};

struct MicMuteState {
    uint32_t muted_;    // 1 = mic muted (capture gated off)
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(MicMuteState) == 256);

struct MicMuteRequest {
    uint32_t muted_;
    uint8_t  pad_[252];
};
static_assert(sizeof(MicMuteRequest) <= 256);

// ── Startup Items ─────────────────────────────────────────────────────────────
// com.freebsdx.StartupItems — manage login-time autostart entries.
// Reads ~/.config/autostart/*.desktop and the compositor's own registry.
//
// Flow:
//   1. xshell calls ListItems on connect and on ItemsChanged broadcast.
//   2. SetEnabled rewrites the Hidden= field in the .desktop entry.
//   3. RemoveItem deletes the .desktop file (builtin_ entries are immune).
//   4. ItemsChanged is broadcast after any mutation or external file change.

enum class StartupOpcode : uint32_t {
    ListItems    = 0x01, // page (uint32) in payload → StartupPage
    SetEnabled   = 0x02, // StartupToggleRequest → ack + broadcast
    RemoveItem   = 0x03, // StartupRemoveRequest → ack + broadcast
    ItemsChanged = 0x04, // broadcast: no payload (re-poll ListItems)
};

struct StartupEntry {           // 64 B — no uint64_t, no alignment hazard
    char     name_[32];         // display name (null-terminated)
    char     exec_[16];         // short executable name (null-terminated)
    uint32_t item_id_;          // opaque compositor-assigned id
    uint32_t enabled_;          // 1 = will launch at login
    uint32_t builtin_;          // 1 = system entry (remove not allowed)
    uint8_t  pad_[4];           // 32+16+4+4+4+4 = 64
};
static_assert(sizeof(StartupEntry) == 64);

struct StartupPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;       // 0–3
    uint32_t      has_more_;
    StartupEntry  entries_[3];  // 3 × 64 = 192 bytes
};                              // 16 + 192 = 208 ≤ 256
static_assert(sizeof(StartupPage) <= 256);

struct StartupToggleRequest {
    uint32_t item_id_;
    uint32_t enabled_;
    uint8_t  pad_[248];
};
static_assert(sizeof(StartupToggleRequest) <= 256);

struct StartupRemoveRequest {
    uint32_t item_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(StartupRemoveRequest) <= 256);

// ── Screen Rotation Lock ──────────────────────────────────────────────────────
// com.freebsdx.RotationLock — display orientation lock for tablet / convertible
// mode.  When unlocked, the compositor listens for accelerometer events and
// auto-rotates.  When locked, it holds the current orientation.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetLocked=1 freezes the current orientation.
//   3. SetOrientation overrides the orientation (even while locked).
//   4. StateChanged broadcast fires after any change (sensor or manual).

enum class RotationLockOpcode : uint32_t {
    GetState       = 0x01, // → RotationLockState
    SetLocked      = 0x02, // RotationLockRequest → ack + broadcast
    SetOrientation = 0x03, // RotationOrientationRequest → ack + broadcast
    StateChanged   = 0x04, // broadcast: RotationLockState
};

// orientation_: 0=landscape, 1=portrait, 2=landscape-flipped, 3=portrait-flipped
struct RotationLockState {
    uint32_t locked_;      // 1 = rotation locked
    uint32_t orientation_; // current display orientation (see above)
    uint8_t  pad_[248];    // 4+4+248 = 256
};
static_assert(sizeof(RotationLockState) == 256);

struct RotationLockRequest {
    uint32_t locked_;
    uint8_t  pad_[252];
};
static_assert(sizeof(RotationLockRequest) <= 256);

struct RotationOrientationRequest {
    uint32_t orientation_;
    uint8_t  pad_[252];
};
static_assert(sizeof(RotationOrientationRequest) <= 256);

// ── System Logs ───────────────────────────────────────────────────────────────
// com.freebsdx.SysLog — paged access to recent syslog / dmesg entries.
// Client filters by level client-side; server returns all levels per page.
//
// Flow:
//   1. xshell calls GetEntries (with page number) on connect and every 5 s.
//   2. Server tails /var/log/messages + dmesg, returns newest-first.
//   3. No broadcast — client polls on its own schedule.

enum class SyslogOpcode : uint32_t {
    GetEntries = 0x01, // page (uint32) in payload → SyslogPage
};

// level_: "ERROR", "WARN", "INFO", "DEBUG" (null-terminated, ≤7 chars + NUL)
struct SyslogEntry {            // 64 B — no uint64_t
    uint32_t timestamp_;        // unix seconds
    char     level_[8];         // "ERROR" / "WARN" / "INFO" / "DEBUG"
    char     source_[16];       // process/subsystem name
    char     message_[36];      // log line (truncated, null-terminated)
};                              // 4+8+16+36 = 64
static_assert(sizeof(SyslogEntry) == 64);

struct SyslogPage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;         // 0–3
    uint32_t    has_more_;
    SyslogEntry entries_[3];    // 3 × 64 = 192
};                              // 16 + 192 = 208 ≤ 256
static_assert(sizeof(SyslogPage) <= 256);

// ── Screen Timeout ────────────────────────────────────────────────────────────
// com.freebsdx.ScreenTimeout — quick selector for the display auto-lock delay.
// Complements the full autolock toggle already in QS; this lets the user pick
// a duration without opening Settings.  timeout_secs_==0 means "never".
//
// Flow:
//   1. xshell calls GetTimeout on connect.
//   2. SetTimeout sends the desired seconds; compositor updates LockService.
//   3. TimeoutChanged broadcast fires after any change (own or Settings app).

enum class ScreenTimeoutOpcode : uint32_t {
    GetTimeout     = 0x01, // → ScreenTimeoutState
    SetTimeout     = 0x02, // ScreenTimeoutRequest → ack + broadcast
    TimeoutChanged = 0x03, // broadcast: ScreenTimeoutState
};

struct ScreenTimeoutState {
    uint32_t timeout_secs_; // 0 = never, else auto-lock delay in seconds
    uint8_t  pad_[252];     // 4+252 = 256
};
static_assert(sizeof(ScreenTimeoutState) == 256);

struct ScreenTimeoutRequest {
    uint32_t timeout_secs_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ScreenTimeoutRequest) <= 256);

// ── Mounted Volumes ───────────────────────────────────────────────────────────
// com.freebsdx.MountedVolumes — df(1)-style snapshot of all mounted file
// systems with capacity and usage data.
//
// Flow:
//   1. xshell calls ListVolumes on connect and every 5 seconds.
//   2. Compositor reads getmntinfo(3) / statfs(2) and returns pages.
//   3. No broadcast — purely polled.

enum class MountedVolOpcode : uint32_t {
    ListVolumes = 0x01, // page (uint32) in payload → VolumePage
};

struct VolumeEntry {        // 64 B — uint64_t fields first
    uint64_t total_kb_;     // total capacity in kibibytes
    uint64_t used_kb_;      // used kibibytes
    char     mount_[32];    // mount point path (null-terminated)
    char     fstype_[12];   // filesystem type, e.g. "zfs", "ufs", "tmpfs"
    uint32_t readonly_;     // 1 = mounted read-only
};                          // 8+8+32+12+4 = 64
static_assert(sizeof(VolumeEntry) == 64);

struct VolumePage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;     // 0–3
    uint32_t    has_more_;
    VolumeEntry entries_[3]; // 3 × 64 = 192
};                           // 16 + 192 = 208 ≤ 256
static_assert(sizeof(VolumePage) <= 256);

// ── Audio Output ──────────────────────────────────────────────────────────────
// com.freebsdx.AudioOutput — enumerate and switch audio sink devices.
// Complements the volume slider already in QS; lets the user pick which output
// device is active (speakers, headphones, HDMI, Bluetooth sink, etc.).
//
// Flow:
//   1. xshell calls ListSinks on connect to populate the pill row.
//   2. SetActiveSink sends the desired sink id_[].
//   3. SinkChanged broadcast fires when the active sink changes externally
//      (e.g. headphones plugged in).

enum class AudioOutputOpcode : uint32_t {
    ListSinks     = 0x01, // page (uint32) → AudioSinkPage
    SetActiveSink = 0x02, // AudioSinkSelectRequest → ack + broadcast
    SinkChanged   = 0x03, // broadcast: AudioSinkPage (page 0, refreshed list)
};

struct AudioSinkEntry {     // 64 B
    char     name_[48];     // display name, e.g. "Built-in Speakers"
    char     id_[12];       // opaque sink identifier (null-terminated)
    uint32_t active_;       // 1 = currently selected output
};                          // 48+12+4 = 64
static_assert(sizeof(AudioSinkEntry) == 64);

struct AudioSinkPage {
    uint32_t       page_;
    uint32_t       total_;
    uint32_t       count_;     // 0–3
    uint32_t       has_more_;
    AudioSinkEntry entries_[3]; // 3 × 64 = 192
};                              // 16 + 192 = 208 ≤ 256
static_assert(sizeof(AudioSinkPage) <= 256);

struct AudioSinkSelectRequest {
    char    id_[12];        // sink id_ to activate
    uint8_t pad_[244];      // 12+244 = 256
};
static_assert(sizeof(AudioSinkSelectRequest) <= 256);

// ── Open Network Connections ──────────────────────────────────────────────────
// com.freebsdx.NetConns — live snapshot of active TCP/UDP sockets, equivalent
// to netstat -an.  Client polls every 5 seconds; no broadcast.
//
// Flow:
//   1. xshell calls ListConns (with page number) on connect and every 5 s.
//   2. Compositor reads /proc/net/tcp+udp (Linux) or sockstat(1) output
//      (FreeBSD) and returns pages newest-first.

enum class NetConnOpcode : uint32_t {
    ListConns = 0x01, // page (uint32) in payload → ConnPage
};

struct ConnEntry {          // 64 B — no uint64_t
    char     proto_[4];     // "TCP" or "UDP"
    char     local_[24];    // "addr:port" (null-terminated)
    char     remote_[24];   // "addr:port" or "-" for LISTEN
    char     state_[8];     // "ESTABL", "LISTEN", "TIMEWT", "CLOSWT", etc.
    uint32_t pid_;          // owning process PID (0 = unknown)
};                          // 4+24+24+8+4 = 64
static_assert(sizeof(ConnEntry) == 64);

struct ConnPage {
    uint32_t  page_;
    uint32_t  total_;
    uint32_t  count_;       // 0–3
    uint32_t  has_more_;
    ConnEntry entries_[3];  // 3 × 64 = 192
};                          // 16 + 192 = 208 ≤ 256
static_assert(sizeof(ConnPage) <= 256);

// ── Auto-Brightness ───────────────────────────────────────────────────────────
// com.freebsdx.AutoBrightness — ambient light sensor-driven brightness control.
// When enabled, the compositor reads the ALS sensor and adjusts BacklightService
// automatically.  lux_ == UINT32_MAX means the sensor is absent or unreadable.
//
// Flow:
//   1. xshell calls GetState on connect to seed the toggle and lux label.
//   2. SetEnabled=1 activates sensor-driven adjustment; SetEnabled=0 hands
//      control back to the manual brightness slider.
//   3. StateChanged broadcast fires on every toggle AND on each sensor update
//      so the live lux reading in QS stays current without polling.

enum class AutoBrightnessOpcode : uint32_t {
    GetState     = 0x01, // → AutoBrightnessState
    SetEnabled   = 0x02, // AutoBrightnessRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: AutoBrightnessState (toggle or sensor tick)
};

struct AutoBrightnessState {
    uint32_t enabled_;  // 1 = auto-brightness active
    uint32_t lux_;      // ambient illuminance in lux; UINT32_MAX = sensor unavailable
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(AutoBrightnessState) == 256);

struct AutoBrightnessRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(AutoBrightnessRequest) <= 256);

// ── SSH Keys ──────────────────────────────────────────────────────────────────
// com.freebsdx.SshKeys — manage ~/.ssh/authorized_keys entries.
// Reads the file line-by-line; each non-comment non-empty line is one entry.
// known_hosts_count_ in the page header is informational (~/.ssh/known_hosts).
//
// Flow:
//   1. xshell calls ListKeys on connect and on KeysChanged broadcast.
//   2. RemoveKey rewrites authorized_keys with the matching line removed.
//   3. KeysChanged is broadcast after any removal.

enum class SshKeyOpcode : uint32_t {
    ListKeys    = 0x01, // page (uint32) in payload → SshKeyPage
    RemoveKey   = 0x02, // SshKeyRemoveRequest → ack + broadcast
    KeysChanged = 0x03, // broadcast: no payload (re-poll ListKeys)
};

struct SshKeyEntry {        // 64 B — no uint64_t
    char     comment_[40];  // key comment field (null-terminated); "" if absent
    char     algo_[12];     // "ssh-ed25519", "ssh-rsa", "ecdsa-sha2-nistp256", …
    uint32_t key_id_;       // opaque compositor-assigned id for RemoveKey
    uint8_t  pad_[8];       // 40+12+4+8 = 64
};
static_assert(sizeof(SshKeyEntry) == 64);

struct SshKeyPage {
    uint32_t    page_;
    uint32_t    total_;             // total authorized_keys entries
    uint32_t    count_;             // 0–3 entries this page
    uint32_t    has_more_;
    uint32_t    known_hosts_count_; // ~/.ssh/known_hosts entry count (read-only)
    uint32_t    reserved_;          // future use; always 0
    SshKeyEntry entries_[3];        // 24 + 3×64 = 216 ≤ 256
};
static_assert(sizeof(SshKeyPage) <= 256);

struct SshKeyRemoveRequest {
    uint32_t key_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(SshKeyRemoveRequest) <= 256);

// ── Display Refresh Rate ──────────────────────────────────────────────────────
// com.freebsdx.RefreshRate — quick selector for the primary display's refresh
// rate.  Supported rates are signalled via a bitmask so the UI can dim buttons
// for rates the current display does not support.
//
// supported_mask_ bitmask:  bit 0 = 60 Hz
//                           bit 1 = 120 Hz
//                           bit 2 = 144 Hz
//                           bit 3 = 240 Hz
//
// Flow:
//   1. xshell calls GetRate on connect; compositor reads the DRM mode list.
//   2. SetRate sends the desired hz_; compositor calls drmModeSetCrtc.
//   3. RateChanged broadcast fires after any change (own or Settings / hotplug).

enum class RefreshRateOpcode : uint32_t {
    GetRate      = 0x01, // → RefreshRateState
    SetRate      = 0x02, // RefreshRateRequest → ack + broadcast
    RateChanged  = 0x03, // broadcast: RefreshRateState
};

struct RefreshRateState {
    uint32_t current_hz_;    // active refresh rate in Hz (e.g. 60, 120, 144, 240)
    uint32_t supported_mask_;// bitmask of supported rates (see above)
    uint8_t  pad_[248];      // 4+4+248 = 256
};
static_assert(sizeof(RefreshRateState) == 256);

struct RefreshRateRequest {
    uint32_t hz_;       // desired refresh rate in Hz
    uint8_t  pad_[252];
};
static_assert(sizeof(RefreshRateRequest) <= 256);

// ── Environment Variables ─────────────────────────────────────────────────────
// com.freebsdx.EnvVars — view and edit the compositor session's environment.
// System-inherited vars (PATH, HOME, USER, SHELL, …) are read-only; any var
// the user sets via SetVar is mutable and can be removed with UnsetVar.
//
// Flow:
//   1. xshell calls ListVars on connect to populate the AC list.
//   2. SetVar writes key=value into the session environ and broadcasts.
//   3. UnsetVar removes the key from environ and broadcasts.
//   4. EnvChanged broadcast fires after any set/unset — no polling.

enum class EnvVarOpcode : uint32_t {
    ListVars   = 0x01, // page (uint32) in payload → EnvVarPage
    SetVar     = 0x02, // EnvSetRequest → ack + broadcast
    UnsetVar   = 0x03, // EnvUnsetRequest → ack + broadcast
    EnvChanged = 0x04, // broadcast: no payload (re-poll ListVars)
};

struct EnvVarEntry {        // 64 B — no uint64_t
    char     key_[32];      // env var name  (null-terminated)
    char     value_[24];    // value snippet (truncated, null-terminated)
    uint32_t readonly_;     // 1 = system-inherited; UnsetVar not allowed
    uint32_t entry_id_;     // opaque id for targeted UnsetVar
};                          // 32+24+4+4 = 64
static_assert(sizeof(EnvVarEntry) == 64);

struct EnvVarPage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;      // 0–3
    uint32_t    has_more_;
    EnvVarEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(EnvVarPage) <= 256);

struct EnvSetRequest {
    char key_[64];    // var name (null-terminated)
    char value_[192]; // value (null-terminated); 64+192 = 256
};
static_assert(sizeof(EnvSetRequest) == 256);

struct EnvUnsetRequest {
    char    key_[64];    // var name (null-terminated)
    uint8_t pad_[192];   // 64+192 = 256
};
static_assert(sizeof(EnvUnsetRequest) == 256);

// ── Scroll Direction ──────────────────────────────────────────────────────────
// com.freebsdx.ScrollDir — toggle natural vs. traditional scroll direction.
// "Natural" (natural_==1): content follows finger direction (macOS-style).
// "Traditional" (natural_==0): wheel-up scrolls content down (classic).
//
// Flow:
//   1. xshell calls GetDir on connect.
//   2. SetNatural sends the desired mode; compositor updates the libinput config.
//   3. DirChanged broadcast fires after any change (own or Settings).

enum class ScrollDirOpcode : uint32_t {
    GetDir      = 0x01, // → ScrollDirState
    SetNatural  = 0x02, // ScrollDirRequest → ack + broadcast
    DirChanged  = 0x03, // broadcast: ScrollDirState
};

struct ScrollDirState {
    uint32_t natural_;  // 1 = natural scroll, 0 = traditional
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(ScrollDirState) == 256);

struct ScrollDirRequest {
    uint32_t natural_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ScrollDirRequest) <= 256);

// ── USB Devices ───────────────────────────────────────────────────────────────
// com.freebsdx.UsbDevices — live snapshot of connected USB devices.
// Compositor reads usbconfig(8) / libusb on FreeBSD, /sys/bus/usb on Linux.
// No management operations — read-only enumeration.
//
// Flow:
//   1. xshell calls ListDevices on connect and every 5 s.
//   2. Rescan requests an immediate re-enumeration before the next page fetch.
//   3. No broadcast — client polls on its own schedule.

enum class UsbDevOpcode : uint32_t {
    ListDevices = 0x01, // page (uint32) in payload → UsbDevPage
    Rescan      = 0x02, // no payload → ack (triggers fresh enumeration)
};

struct UsbDevEntry {       // 64 B — no uint64_t
    char     name_[32];    // device product name (null-terminated)
    char     class_[8];    // class string: "HID", "MSC", "CDC", "HUB", "AUD", …
    uint32_t vid_;         // USB Vendor ID  (e.g. 0x05AC = Apple)
    uint32_t pid_;         // USB Product ID
    uint32_t port_;        // bus/port number (informational)
    uint8_t  pad_[12];     // 32+8+4+4+4+12 = 64
};
static_assert(sizeof(UsbDevEntry) == 64);

struct UsbDevPage {
    uint32_t   page_;
    uint32_t   total_;
    uint32_t   count_;      // 0–3
    uint32_t   has_more_;
    UsbDevEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(UsbDevPage) <= 256);

// ── Presentation Mode ─────────────────────────────────────────────────────────
// com.freebsdx.Presentation — screen-friendly presentation toggle.
// When active: screen saver disabled, auto-lock suppressed, all notification
// pop-ups silenced.  Distinct from Focus Mode (app filtering) and DnD
// (allows lock).  Compositor holds a wakelock while active.
//
// Flow:
//   1. xshell calls GetState on connect.
//   2. SetActive toggles the mode; compositor sets the wakelock.
//   3. StateChanged broadcast fires on toggle.

enum class PresentationOpcode : uint32_t {
    GetState    = 0x01, // → PresentationState
    SetActive   = 0x02, // PresentationRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: PresentationState
};

struct PresentationState {
    uint32_t active_;   // 1 = presentation mode on
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(PresentationState) == 256);

struct PresentationRequest {
    uint32_t active_;
    uint8_t  pad_[252];
};
static_assert(sizeof(PresentationRequest) <= 256);

// ── Memory Pressure ───────────────────────────────────────────────────────────
// com.freebsdx.MemPressure — real-time RAM and swap snapshot.
// Compositor reads vm.stats.vm.* sysctls (FreeBSD) or /proc/meminfo (Linux).
// pressure_level_: 0=Normal, 1=Moderate, 2=Critical
// uint64_t fields placed first to satisfy 8-byte alignment with no padding.
//
// Flow:
//   1. xshell polls GetPressure every 5 s (no broadcast — purely pulled).

enum class MemPressureOpcode : uint32_t {
    GetPressure = 0x01, // → MemPressureState
};

struct MemPressureState {
    uint64_t total_kb_;        // total physical RAM in KiB
    uint64_t free_kb_;         // available (free + cache) RAM in KiB
    uint64_t swap_total_kb_;   // total swap space in KiB
    uint64_t swap_used_kb_;    // used swap in KiB
    uint32_t pressure_level_;  // 0=Normal, 1=Moderate, 2=Critical
    uint32_t page_faults_sec_; // major page faults per second
    uint8_t  pad_[216];        // 4×8 + 2×4 + 216 = 256
};
static_assert(sizeof(MemPressureState) == 256);

// ── Touchpad ──────────────────────────────────────────────────────────────────
// com.freebsdx.Touchpad — enable / disable the built-in touchpad.
// Compositor writes kern.evdev.input.* or synaptics(4) sysctl on FreeBSD.
//
// Flow:
//   1. xshell calls GetState on connect → TouchpadState.
//   2. User toggles → SetEnabled → compositor applies, broadcasts StateChanged.
//   3. xshell OnBroadcast StateChanged → sync QS switch.

enum class TouchpadOpcode : uint32_t {
    GetState     = 0x01, // → TouchpadState
    SetEnabled   = 0x02, // TouchpadRequest →
    StateChanged = 0x03, // broadcast → TouchpadState
};

struct TouchpadState {
    uint32_t enabled_; // 1 = touchpad active, 0 = disabled
    uint8_t  pad_[252];
};
static_assert(sizeof(TouchpadState) == 256);

struct TouchpadRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(TouchpadRequest) <= 256);

// ── Printer Queue ─────────────────────────────────────────────────────────────
// com.freebsdx.PrintQueue — live CUPS print job list.
// Compositor queries the CUPS IPP API for pending/active jobs.
//
// Flow:
//   1. xshell polls ListJobs every 5 s (paged, same pattern as processes).
//   2. User cancels a job → CancelJob → compositor calls cupsCancelJob2().
//   3. Compositor broadcasts QueueChanged when any job state changes.

enum class PrintOpcode : uint32_t {
    ListJobs     = 0x01, // uint32_t page → PrintJobPage
    CancelJob    = 0x02, // PrintCancelRequest →
    QueueChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct PrintJobEntry {
    char     printer_[32];  // printer display name
    char     document_[24]; // document / file name
    uint32_t job_id_;       // CUPS job id
    uint32_t pages_;        // total pages (0 = unknown)
    uint32_t status_;       // 0=pending, 1=held, 2=processing, 3=stopped
    uint32_t reserved_;
    // 32+24+4×4 = 72 bytes
};
static_assert(sizeof(PrintJobEntry) == 72);

struct PrintJobPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;
    uint32_t      has_more_;
    PrintJobEntry entries_[2]; // 16 + 2×72 = 160 ≤ 256
};
static_assert(sizeof(PrintJobPage) <= 256);

struct PrintCancelRequest {
    uint32_t job_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(PrintCancelRequest) <= 256);

// ── Reduce Motion ─────────────────────────────────────────────────────────────
// com.freebsdx.ReduceMotion — disables compositor easing and xkit transitions.
// When enabled, window open/close/move and panel slides cut instead of animate.
//
// Flow:
//   1. xshell calls GetState on connect → ReduceMotionState.
//   2. User toggles → SetEnabled → compositor sets global flag, broadcasts StateChanged.
//   3. xkit widgets read the flag before scheduling any easing step.

enum class ReduceMotionOpcode : uint32_t {
    GetState     = 0x01, // → ReduceMotionState
    SetEnabled   = 0x02, // ReduceMotionRequest →
    StateChanged = 0x03, // broadcast → ReduceMotionState
};

struct ReduceMotionState {
    uint32_t enabled_; // 1 = reduced motion active, 0 = standard animations
    uint8_t  pad_[252];
};
static_assert(sizeof(ReduceMotionState) == 256);

struct ReduceMotionRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ReduceMotionRequest) <= 256);

// ── Package Updates ───────────────────────────────────────────────────────────
// com.freebsdx.PkgUpdates — pending pkg(8) upgrades.
// Compositor runs `pkg version -l '<'` (rate-limited, cached 30 s).
// InstallPackage / InstallAll spawn `pkg upgrade [-y] <name>` in a PTY.
// Broadcast UpdatesChanged fires when the pkg database is refreshed.
//
// Flow:
//   1. xshell polls ListUpdates every 30 s (paged).
//   2. User clicks Install → InstallPackage or InstallAll → compositor runs upgrade.
//   3. Compositor broadcasts UpdatesChanged → xshell re-polls.
//   4. User clicks Check → CheckNow → compositor forces a pkg refresh.

enum class PkgOpcode : uint32_t {
    ListUpdates     = 0x01, // uint32_t page → PkgUpdatePage
    InstallPackage  = 0x02, // PkgInstallRequest →
    InstallAll      = 0x03, // (empty) →
    CheckNow        = 0x04, // (empty) → triggers UpdatesChanged broadcast when done
    UpdatesChanged  = 0x05, // broadcast (no payload — shell re-polls)
};

struct PkgUpdateEntry {
    char     name_[40];    // package name
    char     old_ver_[16]; // currently installed version
    char     new_ver_[16]; // available upgrade version
    uint8_t  pad_[8];      // 40+16+16+8 = 80 bytes
};
static_assert(sizeof(PkgUpdateEntry) == 80);

struct PkgUpdatePage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;
    uint32_t      has_more_;
    PkgUpdateEntry entries_[2]; // 16 + 2×80 = 176 ≤ 256
};
static_assert(sizeof(PkgUpdatePage) <= 256);

struct PkgInstallRequest {
    char    name_[64];   // package name to upgrade
    uint8_t pad_[192];
};
static_assert(sizeof(PkgInstallRequest) == 256);

// ── Cursor Size ───────────────────────────────────────────────────────────────
// com.freebsdx.CursorSize — system cursor scale preset.
// Compositor updates the Wayland cursor theme size and broadcasts to all clients.
// size_: 0=Small (24px), 1=Medium (32px), 2=Large (48px)
//
// Flow:
//   1. xshell calls GetSize on connect → CursorSizeState.
//   2. User picks preset → SetSize → compositor reloads cursor theme, broadcasts SizeChanged.

enum class CursorSizeOpcode : uint32_t {
    GetSize      = 0x01, // → CursorSizeState
    SetSize      = 0x02, // CursorSizeRequest →
    SizeChanged  = 0x03, // broadcast → CursorSizeState
};

struct CursorSizeState {
    uint32_t size_; // 0=Small, 1=Medium, 2=Large
    uint8_t  pad_[252];
};
static_assert(sizeof(CursorSizeState) == 256);

struct CursorSizeRequest {
    uint32_t size_;
    uint8_t  pad_[252];
};
static_assert(sizeof(CursorSizeRequest) <= 256);

// ── ZFS Pool Health ───────────────────────────────────────────────────────────
// com.freebsdx.ZfsPools — live ZFS pool status via libzfs / zpool(8).
// Compositor calls zpool_open_all() and reads pool state, capacity, and scrub info.
// StartScrub triggers `zpool scrub <name>`.
//
// Flow:
//   1. xshell polls ListPools every 30 s (paged).
//   2. User clicks Scrub → StartScrub → compositor runs zpool scrub.
//   3. Compositor broadcasts PoolsChanged when pool state changes (import/export/scrub done).
//
// ZfsPoolEntry: uint64_t fields first to satisfy 8-byte alignment.

enum class ZfsOpcode : uint32_t {
    ListPools    = 0x01, // uint32_t page → ZfsPoolPage
    StartScrub   = 0x02, // ZfsScrubRequest →
    PoolsChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct ZfsPoolEntry {
    uint64_t used_bytes_;   // bytes used
    uint64_t total_bytes_;  // total pool capacity in bytes
    char     name_[32];     // pool name
    char     health_[12];   // "ONLINE", "DEGRADED", "FAULTED", "UNAVAIL"
    uint32_t scrub_pct_;    // scrub progress 0-100 (valid when scrubbing_==1)
    uint32_t scrubbing_;    // 1 = scrub in progress
    uint8_t  pad_[4];       // 8+8+32+12+4+4+4 = 72 bytes
};
static_assert(sizeof(ZfsPoolEntry) == 72);

struct ZfsPoolPage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;
    uint32_t     has_more_;
    ZfsPoolEntry entries_[3]; // 16 + 3×72 = 232 ≤ 256
};
static_assert(sizeof(ZfsPoolPage) <= 256);

struct ZfsScrubRequest {
    char    name_[64]; // pool name to scrub
    uint8_t pad_[192];
};
static_assert(sizeof(ZfsScrubRequest) == 256);

// ── Hotspot ───────────────────────────────────────────────────────────────────
// com.freebsdx.Hotspot — WiFi software access point via hostapd(8).
// Compositor manages hostapd config and tracks connected DHCP leases.
// passphrase_hint_: masked form, e.g. "••••••••" (never the real key).
//
// Flow:
//   1. xshell calls GetState on connect → HotspotState.
//   2. User toggles → SetEnabled → compositor starts/stops hostapd, broadcasts StateChanged.
//   3. Compositor broadcasts StateChanged when client count changes.

enum class HotspotToggleOpcode : uint32_t {
    GetState     = 0x01, // → HotspotState
    SetEnabled   = 0x02, // HotspotRequest →
    StateChanged = 0x03, // broadcast → HotspotState
};

struct HotspotState {
    uint32_t enabled_;           // 1 = hotspot active
    uint32_t client_count_;      // number of connected clients
    char     ssid_[32];          // hotspot SSID
    char     passphrase_hint_[16]; // masked passphrase hint
    uint8_t  pad_[200];          // 4+4+32+16+200 = 256
};
static_assert(sizeof(HotspotState) == 256);

struct HotspotRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(HotspotRequest) <= 256);

// ── Active Jails ──────────────────────────────────────────────────────────────
// com.freebsdx.Jails — FreeBSD jail list via jls(8) / libjail.
// Compositor calls jail_getv() to enumerate running jails.
// StopJail sends SIGTERM to the jail's init process and calls jail_remove().
//
// Flow:
//   1. xshell polls ListJails every 5 s (paged, alongside processes).
//   2. User stops a jail → StopJail → compositor calls jail_remove().
//   3. Compositor broadcasts JailsChanged when a jail starts or stops.

enum class JailOpcode : uint32_t {
    ListJails    = 0x01, // uint32_t page → JailPage
    StopJail     = 0x02, // JailStopRequest →
    JailsChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct JailEntry {
    uint32_t jid_;         // numeric jail ID
    char     hostname_[32]; // jail hostname
    char     ip_[20];      // primary IPv4 address
    uint32_t running_;     // 1 = running, 0 = dying
    uint8_t  pad_[4];      // 4+32+20+4+4 = 64 bytes
};
static_assert(sizeof(JailEntry) == 64);

struct JailPage {
    uint32_t  page_;
    uint32_t  total_;
    uint32_t  count_;
    uint32_t  has_more_;
    JailEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(JailPage) <= 256);

struct JailStopRequest {
    uint32_t jid_;
    uint8_t  pad_[252];
};
static_assert(sizeof(JailStopRequest) <= 256);

// ── Text Size ─────────────────────────────────────────────────────────────────
// com.freebsdx.TextSize — system-wide font scale preset.
// Compositor propagates the scale factor to all running apps via FontScaleChanged.
// 0=Small(90%), 1=Normal(100%), 2=Large(115%).
//
// Flow:
//   1. xshell calls GetSize on connect → TextSizeState.
//   2. User picks preset → SetSize → compositor broadcasts SizeChanged.

enum class TextSizeOpcode : uint32_t {
    GetSize      = 0x01, // → TextSizeState
    SetSize      = 0x02, // TextSizeRequest →
    SizeChanged  = 0x03, // broadcast → TextSizeState
};

struct TextSizeState {
    uint32_t size_; // 0=Small, 1=Normal, 2=Large
    uint8_t  pad_[252];
};
static_assert(sizeof(TextSizeState) == 256);

struct TextSizeRequest {
    uint32_t size_;
    uint8_t  pad_[252];
};
static_assert(sizeof(TextSizeRequest) <= 256);

// ── Kernel Modules ────────────────────────────────────────────────────────────
// com.freebsdx.KernelModules — loaded kernel module list via kldstat(2).
// Compositor enumerates kld_file via kldnext()/kldstat().
// UnloadModule calls kldunload(2); only permitted when refs_ == 0.
//
// Flow:
//   1. xshell polls ListModules every 30 s (paged, 3 per page).
//   2. User clicks Unload → UnloadModule → compositor calls kldunload().
//   3. Compositor broadcasts ModulesChanged when a module loads or unloads.

enum class KmodOpcode : uint32_t {
    ListModules    = 0x01, // uint32_t page → KmodPage
    UnloadModule   = 0x02, // KmodUnloadRequest →
    ModulesChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct KmodEntry {
    char     name_[40];   // module name (e.g. "drm.ko")
    uint32_t size_kb_;    // file size in KB
    uint32_t refs_;       // reference count (0 = safe to unload)
    uint32_t flags_;      // kld flags bitmask
    uint8_t  pad_[12];    // 40+4+4+4+12 = 64 bytes
};
static_assert(sizeof(KmodEntry) == 64);

struct KmodPage {
    uint32_t  page_;
    uint32_t  total_;
    uint32_t  count_;
    uint32_t  has_more_;
    KmodEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(KmodPage) <= 256);

struct KmodUnloadRequest {
    char    name_[64]; // module name to unload
    uint8_t pad_[192];
};
static_assert(sizeof(KmodUnloadRequest) == 256);

// ── Update Channel ────────────────────────────────────────────────────────────
// com.freebsdx.UpdateChannel — system update ring selector.
// 0=Stable, 1=Beta. Compositor reads/writes /etc/freebsdx-update.conf.
// pending_count_ and pending_ver_ are populated by the update daemon on each
// channel change or scheduled check.
//
// Flow:
//   1. xshell calls GetChannel on connect → UpdateChannelState.
//   2. User picks Stable/Beta → SetChannel → compositor writes config, broadcasts
//      ChannelChanged (which also carries the new pending count).

enum class UpdateChannelOpcode : uint32_t {
    GetChannel     = 0x01, // → UpdateChannelState
    SetChannel     = 0x02, // UpdateChannelRequest →
    ChannelChanged = 0x03, // broadcast → UpdateChannelState
};

struct UpdateChannelState {
    uint32_t channel_;        // 0=Stable, 1=Beta
    uint32_t pending_count_;  // updates pending on current channel
    char     pending_ver_[32]; // newest pending version string, or ""
    char     channel_name_[16]; // "Stable" or "Beta"
    uint8_t  pad_[200];       // 4+4+32+16+200 = 256
};
static_assert(sizeof(UpdateChannelState) == 256);

struct UpdateChannelRequest {
    uint32_t channel_;
    uint8_t  pad_[252];
};
static_assert(sizeof(UpdateChannelRequest) <= 256);

// ── License & Support ─────────────────────────────────────────────────────────
// com.freebsdx.License — commercial license and support info.
// Compositor reads from /etc/freebsdx-license.conf (written by the installer).
// The compositor masks the full license key before sending — only last 4 chars
// are shown (XXXX-XXXX-XXXX-<last4>).
//
// Flow:
//   1. xshell calls GetInfo once on connect → LicenseState (no polling).
//   2. No mutation opcodes — changes go through the Settings app.

enum class LicenseOpcode : uint32_t {
    GetInfo = 0x01, // → LicenseState
};

struct LicenseState {
    char     license_key_[32]; // masked, e.g. "••••-••••-••••-A3F7"
    char     support_id_[16];  // short ID for support portal, e.g. "FBX-00123"
    char     tier_[16];        // "Standard", "Pro", "Enterprise"
    char     expiry_[12];      // ISO date "2027-12-31", or "Never"
    uint32_t seats_total_;     // licensed seat count (0 = unlimited)
    uint32_t seats_used_;      // currently activated seats
    uint8_t  pad_[172];        // 32+16+16+12+4+4+172 = 256
};
static_assert(sizeof(LicenseState) == 256);

// ── NTP / Time Sync ───────────────────────────────────────────────────────────
// com.freebsdx.NTP — network time synchronisation via ntpd(8) / chronyd(8).
// Compositor reads ntpq state via UNIX socket; toggling enabled_ starts/stops
// the ntpd service via rc.d.
//
// Flow:
//   1. xshell calls GetState on connect → NtpState.
//   2. User toggles → SetEnabled → compositor starts/stops ntpd, broadcasts StateChanged.
//   3. Compositor broadcasts StateChanged on stratum change or peer switch.

enum class NtpOpcode : uint32_t {
    GetState     = 0x01, // → NtpState
    SetEnabled   = 0x02, // NtpRequest →
    StateChanged = 0x03, // broadcast → NtpState
};

struct NtpState {
    uint32_t enabled_;       // 1 = ntpd running
    uint32_t synced_;        // 1 = clock synchronised (stratum reached)
    char     server_[48];    // active NTP server, e.g. "0.freebsd.pool.ntp.org"
    char     last_sync_[20]; // "2026-07-25 14:32:01" or ""
    int32_t  drift_ms_;      // signed drift in milliseconds
    uint8_t  pad_[176];      // 4+4+48+20+4+176 = 256
};
static_assert(sizeof(NtpState) == 256);

struct NtpRequest {
    uint32_t enabled_;
    uint8_t  pad_[252];
};
static_assert(sizeof(NtpRequest) <= 256);

// ── Boot Environments ─────────────────────────────────────────────────────────
// com.freebsdx.BootEnv — ZFS boot environment management via bectl(8).
// Compositor calls bectl_library functions (libbe) to list, activate, and
// delete boot environments.  The active BE cannot be deleted.
//
// Flow:
//   1. xshell polls ListBE every 30 s (paged, 3 per page).
//   2. User clicks Activate → ActivateBE → compositor calls bectl_activate().
//   3. User clicks Delete   → DeleteBE   → compositor calls bectl_destroy().
//   4. Compositor broadcasts BEChanged after any mutation.

enum class BeOpcode : uint32_t {
    ListBE     = 0x01, // uint32_t page → BePage
    ActivateBE = 0x02, // BeActionRequest →
    DeleteBE   = 0x03, // BeActionRequest →
    BEChanged  = 0x04, // broadcast (no payload — shell re-polls)
};

struct BeEntry {
    char     name_[40];    // BE name, e.g. "default", "14.2-RELEASE"
    char     created_[20]; // "2026-07-20 09:15"
    uint32_t active_;      // 1 = currently booted
    uint32_t next_boot_;   // 1 = activated for next boot
    uint8_t  pad_[4];      // 40+20+4+4+4 = 72 bytes
};
static_assert(sizeof(BeEntry) == 72);

struct BePage {
    uint32_t page_;
    uint32_t total_;
    uint32_t count_;
    uint32_t has_more_;
    BeEntry  entries_[3]; // 16 + 3×72 = 232 ≤ 256
};
static_assert(sizeof(BePage) <= 256);

struct BeActionRequest {
    char    name_[64]; // BE name to activate or delete
    uint8_t pad_[192];
};
static_assert(sizeof(BeActionRequest) == 256);

// ── Stylus Input ──────────────────────────────────────────────────────────────
// com.freebsdx.Stylus — pen/stylus input configuration.
// Compositor reads stylus presence via libinput; palm rejection and pressure
// curve are written to the libinput quirks file and applied immediately.
//
// Flow:
//   1. xshell calls GetState on connect → StylusState.
//   2. User toggles palm rejection or picks pressure curve → SetPalmReject /
//      SetPressure → compositor updates libinput config, broadcasts StateChanged.
//   3. Compositor broadcasts StateChanged when stylus is connected/disconnected.

enum class StylusOpcode : uint32_t {
    GetState      = 0x01, // → StylusState
    SetPalmReject = 0x02, // StylusRequest →
    SetPressure   = 0x03, // StylusRequest →
    StateChanged  = 0x04, // broadcast → StylusState
};

struct StylusState {
    uint32_t connected_;      // 1 = stylus detected (BT/USB/active digitiser)
    uint32_t palm_rejection_; // 1 = palm rejection enabled
    uint32_t pressure_curve_; // 0=Soft, 1=Normal, 2=Firm
    uint8_t  pad_[244];       // 4+4+4+244 = 256
};
static_assert(sizeof(StylusState) == 256);

struct StylusRequest {
    uint32_t palm_rejection_;
    uint32_t pressure_curve_;
    uint8_t  pad_[248];
};
static_assert(sizeof(StylusRequest) <= 256);

// ── Screenshot & Recordings ───────────────────────────────────────────────────
// com.freebsdx.Captures — screenshot and screen-recording file index.
// Compositor writes captures to ~/Pictures/Screenshots/ and ~/Videos/Captures/.
// TakeScreenshot triggers an immediate full-screen capture.
// DeleteCapture removes the file and broadcasts CapturesChanged.
//
// Flow:
//   1. xshell polls ListCaptures every 30 s (paged, newest first).
//   2. User clicks New Screenshot → TakeScreenshot → compositor captures,
//      broadcasts CapturesChanged → shell re-polls.
//   3. User clicks Delete → DeleteCapture → compositor removes file,
//      broadcasts CapturesChanged → shell re-polls.

enum class CaptureOpcode : uint32_t {
    ListCaptures    = 0x01, // uint32_t page → CapturePage
    TakeScreenshot  = 0x02, // (no payload) → CapturesChanged broadcast follows
    DeleteCapture   = 0x03, // CaptureDeleteRequest →
    CapturesChanged = 0x04, // broadcast (no payload — shell re-polls)
};

struct CaptureEntry {
    char     filename_[40]; // e.g. "Screenshot-2026-07-25-14-32.png"
    char     timestamp_[20]; // "2026-07-25 14:32"
    uint32_t type_;          // 0=Screenshot, 1=Recording
    uint32_t size_kb_;       // file size in KB
    uint8_t  pad_[4];        // 40+20+4+4+4 = 72 bytes
};
static_assert(sizeof(CaptureEntry) == 72);

struct CapturePage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;
    uint32_t     has_more_;
    CaptureEntry entries_[3]; // 16 + 3×72 = 232 ≤ 256
};
static_assert(sizeof(CapturePage) <= 256);

struct CaptureDeleteRequest {
    char    filename_[64];
    uint8_t pad_[192];
};
static_assert(sizeof(CaptureDeleteRequest) == 256);

// ── Color Profile ─────────────────────────────────────────────────────────────
// com.freebsdx.ColorProfile — ICC display color profile selection.
// Compositor applies the chosen profile to the DRM/KMS color pipeline (via
// a 3D LUT or CTM matrix).  Profile files live in /usr/share/color/icc/.
//
// Flow:
//   1. xshell calls GetProfile on connect → ColorProfileState.
//   2. User picks preset → SetProfile → compositor loads ICC LUT, broadcasts
//      ProfileChanged.

enum class ColorProfileOpcode : uint32_t {
    GetProfile     = 0x01, // → ColorProfileState
    SetProfile     = 0x02, // ColorProfileRequest →
    ProfileChanged = 0x03, // broadcast → ColorProfileState
};

struct ColorProfileState {
    uint32_t profile_;        // 0=sRGB, 1=DisplayP3, 2=AdobeRGB
    char     profile_name_[32]; // human-readable name, e.g. "Display P3"
    uint8_t  pad_[220];       // 4+32+220 = 256
};
static_assert(sizeof(ColorProfileState) == 256);

struct ColorProfileRequest {
    uint32_t profile_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ColorProfileRequest) <= 256);

// ── Input Methods ─────────────────────────────────────────────────────────────
// com.freebsdx.InputMethod — IME / input method selector.
// Compositor delegates to the active input framework (fcitx5, ibus) via D-Bus.
// MethodChanged is broadcast whenever the active IM changes (e.g. user uses a
// hotkey to switch language outside xshell).
//
// Flow:
//   1. xshell calls ListMethods on connect (paged).
//   2. User clicks Switch → SetActive → compositor signals the IM framework,
//      broadcasts MethodChanged → shell re-polls.

enum class ImOpcode : uint32_t {
    ListMethods   = 0x01, // uint32_t page → ImPage
    SetActive     = 0x02, // ImSetRequest →
    MethodChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct ImEntry {
    char     name_[40]; // e.g. "fcitx5-mozc", "ibus-hangul", "us-qwerty"
    char     lang_[16]; // BCP-47 tag, e.g. "ja", "ko", "zh-CN", "en"
    uint32_t active_;   // 1 = currently selected input method
    uint8_t  pad_[4];   // 40+16+4+4 = 64 bytes
};
static_assert(sizeof(ImEntry) == 64);

struct ImPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t count_;
    uint32_t has_more_;
    ImEntry  entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(ImPage) <= 256);

struct ImSetRequest {
    char    name_[64]; // input method name to activate
    uint8_t pad_[192];
};
static_assert(sizeof(ImSetRequest) == 256);

// ── Dev Overlay ───────────────────────────────────────────────────────────────
// com.freebsdx.DevOverlay — compositor debug visualisation mode.
// Off  : no overlay.
// FPS  : draws a real-time frame-time bar chart in the corner of every surface
//        (analogous to Flutter showPerformanceOverlay).
// Repaint: highlights repainted regions in amber each frame
//        (analogous to Flutter debugRepaintRainbowEnabled).
//
// Flow:
//   1. xshell calls GetState on connect → DevOverlayState.
//   2. User picks mode → SetMode → compositor applies overlay, broadcasts StateChanged.

enum class DevOverlayOpcode : uint32_t {
    GetState     = 0x01, // → DevOverlayState
    SetMode      = 0x02, // DevOverlayRequest →
    StateChanged = 0x03, // broadcast → DevOverlayState
};

struct DevOverlayState {
    uint32_t mode_; // 0=Off, 1=FPS, 2=Repaint
    uint8_t  pad_[252];
};
static_assert(sizeof(DevOverlayState) == 256);

struct DevOverlayRequest {
    uint32_t mode_;
    uint8_t  pad_[252];
};
static_assert(sizeof(DevOverlayRequest) <= 256);

// ── Frame Timing ──────────────────────────────────────────────────────────────
// com.freebsdx.FramePerf — compositor frame-timing statistics.
// Compositor accumulates stats over a 60-frame sliding window and exposes them
// via GetStats.  recent_[] holds the last 16 frame times in whole milliseconds
// (clamped to 255) for sparkline rendering — identical to Flutter's performance
// overlay chart data.
//
// Flow:
//   1. xshell polls GetStats every 2 s (alongside GPU stats).
//   2. No broadcast — pure pull model.

enum class FramePerfOpcode : uint32_t {
    GetStats = 0x01, // → FramePerfState
};

struct FramePerfState {
    uint64_t avg_frame_us_;  // mean frame time in µs
    uint64_t p99_frame_us_;  // 99th-percentile frame time in µs
    uint32_t jank_count_;    // frames >16.6 ms in last 60-frame window
    uint32_t vsync_misses_;  // vsync deadline misses (same window)
    uint32_t gpu_submit_us_; // mean GPU submit latency in µs
    uint32_t total_frames_;  // cumulative frames since compositor start
    uint8_t  recent_[16];    // last 16 frame durations in whole ms (0–255)
    uint8_t  pad_[208];      // 8+8+4+4+4+4+16+208 = 256
};
static_assert(sizeof(FramePerfState) == 256);

// ── Display Scaling ───────────────────────────────────────────────────────────
// com.freebsdx.DisplayScale — HiDPI fractional scaling via DRM viewport.
// scale_pct_: 100=1×, 150=1.5×, 200=2×, 300=3×.
// Compositor adjusts the KMS CRTC scaling matrix and reflows all surfaces.
//
// Flow:
//   1. xshell calls GetScale on connect → DisplayScaleState.
//   2. User picks preset → SetScale → compositor applies, broadcasts ScaleChanged.

enum class DisplayScaleOpcode : uint32_t {
    GetScale     = 0x01, // → DisplayScaleState
    SetScale     = 0x02, // DisplayScaleRequest →
    ScaleChanged = 0x03, // broadcast → DisplayScaleState
};

struct DisplayScaleState {
    uint32_t scale_pct_; // 100, 150, 200, or 300
    uint8_t  pad_[252];
};
static_assert(sizeof(DisplayScaleState) == 256);

struct DisplayScalePctRequest {
    uint32_t scale_pct_;
    uint8_t  pad_[252];
};
static_assert(sizeof(DisplayScalePctRequest) <= 256);

// ── App Permissions ───────────────────────────────────────────────────────────
// com.freebsdx.AppPerms — per-application camera/mic/location permission matrix.
// Compositor maintains a permission store in /var/db/freebsdx/permissions.db.
// PermsChanged is broadcast when any app requests a new permission at runtime.
//
// Flow:
//   1. xshell calls ListPerms on connect (paged).
//   2. User toggles a permission → SetPerm → compositor updates the store,
//      enforces via seccomp/Capsicum, broadcasts PermsChanged.
//   3. Compositor broadcasts PermsChanged when an app requests a new permission.

enum class AppPermOpcode : uint32_t {
    ListPerms    = 0x01, // uint32_t page → AppPermPage
    SetPerm      = 0x02, // AppPermSetRequest →
    PermsChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct AppPermEntry {
    char     app_id_[32]; // reverse-DNS bundle ID, e.g. "com.freebsdx.Camera"
    char     name_[20];   // display name, e.g. "Camera"
    uint32_t camera_;     // 1 = camera access granted
    uint32_t mic_;        // 1 = microphone access granted
    uint32_t location_;   // 1 = location access granted
    // 32+20+4+4+4 = 64 bytes
};
static_assert(sizeof(AppPermEntry) == 64);

struct AppPermPage {
    uint32_t     page_;
    uint32_t     total_;
    uint32_t     count_;
    uint32_t     has_more_;
    AppPermEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(AppPermPage) <= 256);

struct AppPermSetRequest {
    char     app_id_[32];
    uint32_t perm_;    // 0=camera, 1=mic, 2=location
    uint32_t value_;   // 0=deny, 1=grant
    uint8_t  pad_[216]; // 32+4+4+216 = 256
};
static_assert(sizeof(AppPermSetRequest) == 256);

// ── DNS Configuration ──────────────────────────────────────────────────────
// Flow:
//   1. Shell calls GetConfig on connect → DnsConfig.
//   2. User picks Auto/DoH/Custom → SetMode → compositor writes /etc/resolv.conf.
//   3. ConfigChanged broadcast carries updated DnsConfig.

enum class DnsOpcode : uint32_t {
    GetConfig     = 0x01, // → DnsConfig
    SetMode       = 0x02, // DnsSetRequest →
    ConfigChanged = 0x03, // broadcast → DnsConfig
};

struct DnsConfig {
    uint32_t mode_;          // 0=Auto (DHCP), 1=DoH (Cloudflare), 2=Custom
    char     resolver_[48];  // active resolver label shown in QS sub-label
    char     doh_url_[48];   // DoH endpoint, e.g. "https://1.1.1.1/dns-query"
    char     custom_ip_[20]; // user-specified custom resolver IP
    uint8_t  pad_[136];      // 4+48+48+20+136 = 256
};
static_assert(sizeof(DnsConfig) == 256);

struct DnsSetRequest {
    uint32_t mode_;          // new mode (0/1/2)
    char     custom_ip_[20]; // used only when mode_==2
    uint8_t  pad_[232];      // 4+20+232 = 256
};
static_assert(sizeof(DnsSetRequest) == 256);

// ── Audio Routing ──────────────────────────────────────────────────────────
// Flow:
//   1. Shell calls ListRoutes (paged) on connect → AudioRoutePage per page.
//   2. User clicks ▶ to cycle output device → SetRoute → compositor reroutes
//      PipeWire/PulseAudio sink-input to the next available device.
//   3. RoutesChanged broadcast (no payload) when apps start/stop or device
//      plugged/unplugged — shell re-polls.

enum class AudioRoutingOpcode : uint32_t {
    ListRoutes    = 0x01, // uint32_t page → AudioRoutePage
    SetRoute      = 0x02, // AudioRouteSetRequest →
    RoutesChanged = 0x03, // broadcast (no payload — shell re-polls)
};

struct AudioRouteEntry {
    char     app_id_[32];     // PipeWire/PulseAudio sink-input id
    char     app_name_[24];   // display name
    char     device_name_[32];// current output device label
    uint32_t device_idx_;     // current device index (0-based)
    uint32_t device_count_;   // total available output devices
    // 32+24+32+4+4 = 96 bytes
};
static_assert(sizeof(AudioRouteEntry) == 96);

struct AudioRoutePage {
    uint32_t        page_;
    uint32_t        total_;
    uint32_t        count_;
    uint32_t        has_more_;
    AudioRouteEntry entries_[2]; // 16 + 2×96 = 208 ≤ 256
};
static_assert(sizeof(AudioRoutePage) <= 256);

struct AudioRouteSetRequest {
    char     app_id_[32];   // target sink-input id
    uint32_t device_idx_;   // new device index (compositor wraps via modulo)
    uint8_t  pad_[220];     // 32+4+220 = 256
};
static_assert(sizeof(AudioRouteSetRequest) == 256);

// ── Wake Lock ──────────────────────────────────────────────────────────────
// Flow:
//   1. Shell calls GetState on connect → WakeLockState.
//   2. User picks duration and toggles on → SetLock → compositor inhibits
//      the idle-sleep and dpms-blank timers for the requested duration.
//   3. StateChanged broadcast fires when the lock expires or is cleared.

enum class WakeLockOpcode : uint32_t {
    GetState     = 0x01, // → WakeLockState
    SetLock      = 0x02, // WakeLockRequest →
    StateChanged = 0x03, // broadcast → WakeLockState
};

struct WakeLockState {
    uint32_t active_;          // 1 = keep-awake currently active
    uint32_t remaining_secs_;  // 0 = indefinite; otherwise seconds left
    uint8_t  pad_[248];        // 4+4+248 = 256
};
static_assert(sizeof(WakeLockState) == 256);

struct WakeLockRequest {
    uint32_t active_;          // 1 = enable, 0 = cancel
    uint32_t duration_secs_;   // 900=15 min, 3600=1 h, 0=indefinite
    uint8_t  pad_[248];        // 256
};
static_assert(sizeof(WakeLockRequest) == 256);

// ── Network Shares ─────────────────────────────────────────────────────────
// Flow:
//   1. Shell calls ListShares (paged) on connect → NetworkSharePage per page.
//   2. User clicks Mount/Unmount → MountShare/UnmountShare → compositor runs
//      mount_smbfs / umount (via rc script helper, no root prompt needed).
//   3. SharesChanged broadcast fires when mount state transitions — shell
//      re-polls to refresh status badges.

enum class NetworkShareOpcode : uint32_t {
    ListShares    = 0x01, // uint32_t page → NetworkSharePage
    MountShare    = 0x02, // ShareActionRequest →
    UnmountShare  = 0x03, // ShareActionRequest →
    SharesChanged = 0x04, // broadcast
};

struct NetworkShareEntry {
    char     name_[32];  // display name, e.g. "NAS Media"
    char     path_[48];  // UNC or NFS path, e.g. "//nas/media"
    char     type_[8];   // "SMB" or "NFS"
    uint32_t mounted_;   // 0=unmounted, 1=mounted, 2=error
    uint8_t  pad_[4];    // 32+48+8+4+4 = 96
};
static_assert(sizeof(NetworkShareEntry) == 96);

struct NetworkSharePage {
    uint32_t          page_;
    uint32_t          total_;
    uint32_t          count_;
    uint32_t          has_more_;
    NetworkShareEntry entries_[2]; // 16 + 2×96 = 208 ≤ 256
};
static_assert(sizeof(NetworkSharePage) <= 256);

struct ShareActionRequest {
    char    name_[64]; // share name to mount or unmount
    uint8_t pad_[192]; // 64+192 = 256
};
static_assert(sizeof(ShareActionRequest) == 256);

// ── Proxy Settings ─────────────────────────────────────────────────────────
// Flow:
//   1. Shell calls GetConfig on connect → ProxyConfig.
//   2. User picks Off/Auto/Manual → SetConfig → compositor writes
//      /etc/profile.d/proxy.sh and signals running sessions.
//   3. ConfigChanged broadcast fires so all panels stay in sync.

enum class ProxyOpcode : uint32_t {
    GetConfig     = 0x01, // → ProxyConfig
    SetConfig     = 0x02, // ProxyConfig →
    ConfigChanged = 0x03, // broadcast → ProxyConfig
};

struct ProxyConfig {
    uint32_t mode_;          // 0=Off, 1=Auto (PAC/WPAD), 2=Manual
    char     pac_url_[48];   // PAC URL used in Auto mode
    char     http_host_[32]; // proxy hostname for Manual mode
    uint16_t http_port_;     // HTTP proxy port
    uint16_t https_port_;    // HTTPS proxy port (same host)
    char     no_proxy_[64];  // comma-sep bypass list, e.g. "localhost,10.0.0.0/8"
    uint8_t  pad_[104];      // 4+48+32+2+2+64+104 = 256
};
static_assert(sizeof(ProxyConfig) == 256);

// ── Virtual Machine Manager ────────────────────────────────────────────────
// Flow:
//   1. Shell calls ListVMs (paged) on connect → VmPage per page.
//   2. User clicks Start/Stop → StartVM/StopVM → compositor delegates to
//      bhyvectl / VBoxManage via an rc-script helper.
//   3. VMChanged broadcast (no payload) fires on state transition; shell
//      re-polls to refresh state badges.

enum class VmOpcode : uint32_t {
    ListVMs   = 0x01, // uint32_t page → VmPage
    StartVM   = 0x02, // VmActionRequest →
    StopVM    = 0x03, // VmActionRequest →
    VMChanged = 0x04, // broadcast
};

struct VmEntry {
    char     name_[40];  // VM display name
    char     type_[8];   // "bhyve" or "vbox"
    uint32_t state_;     // 0=stopped, 1=running, 2=paused, 3=error
    uint32_t cpus_;      // vCPU count
    uint32_t ram_mb_;    // RAM in MB
    uint8_t  pad_[12];   // 40+8+4+4+4+12 = 72
};
static_assert(sizeof(VmEntry) == 72);

struct VmPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t count_;
    uint32_t has_more_;
    VmEntry  entries_[3]; // 16 + 3×72 = 232 ≤ 256
};
static_assert(sizeof(VmPage) <= 256);

struct VmActionRequest {
    char    name_[64]; // VM name to start or stop
    uint8_t pad_[192]; // 64+192 = 256
};
static_assert(sizeof(VmActionRequest) == 256);

// ── Accessibility Features ─────────────────────────────────────────────────
// Flow:
//   1. Shell calls GetState on connect → A11yState.
//   2. User toggles Reader/Contrast/Magnify → SetFeature → compositor wires
//      AT-SPI2 (screen reader), GLSL contrast shader, or xdg-desktop-portal
//      magnifier depending on feature index.
//   3. StateChanged broadcast keeps all clients in sync.

enum class A11yOpcode : uint32_t {
    GetState     = 0x01, // → A11yState
    SetFeature   = 0x02, // A11ySetRequest →
    StateChanged = 0x03, // broadcast → A11yState
};

struct A11yState {
    uint32_t reader_;   // 1 = screen reader active
    uint32_t contrast_; // 1 = high contrast mode active
    uint32_t magnify_;  // 1 = screen magnifier active
    uint8_t  pad_[244]; // 4+4+4+244 = 256
};
static_assert(sizeof(A11yState) == 256);

struct A11ySetRequest {
    uint32_t feature_;  // 0=reader, 1=contrast, 2=magnify
    uint32_t enabled_;  // 1=on, 0=off
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(A11ySetRequest) == 256);

// ── Container Manager ──────────────────────────────────────────────────────
// Flow:
//   1. Shell calls ListContainers (paged) on connect → ContainerPage per page.
//   2. User clicks Start/Stop → StartContainer/StopContainer → compositor
//      delegates to `podman start` / `podman stop` (rootless via user daemon).
//   3. ContainersChanged broadcast (no payload) fires on state change; shell
//      re-polls.

enum class ContainerOpcode : uint32_t {
    ListContainers    = 0x01, // uint32_t page → ContainerPage
    StartContainer    = 0x02, // ContainerActionRequest →
    StopContainer     = 0x03, // ContainerActionRequest →
    ContainersChanged = 0x04, // broadcast
};

struct ContainerEntry {
    char     id_[12];    // short container ID (first 12 chars)
    char     name_[24];  // container name
    char     image_[24]; // image name, truncated
    uint32_t state_;     // 0=stopped, 1=running, 2=paused, 3=error
    // 12+24+24+4 = 64 bytes
};
static_assert(sizeof(ContainerEntry) == 64);

struct ContainerPage {
    uint32_t       page_;
    uint32_t       total_;
    uint32_t       count_;
    uint32_t       has_more_;
    ContainerEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(ContainerPage) <= 256);

struct ContainerActionRequest {
    char    id_[16];   // container ID
    char    name_[24]; // container name (fallback)
    uint8_t pad_[216]; // 16+24+216 = 256
};
static_assert(sizeof(ContainerActionRequest) == 256);

// ── Remote Login (SSH daemon) ──────────────────────────────────────────────
// Flow:
//   1. Shell calls GetState on connect → RemoteLoginState.
//   2. User toggles → SetEnabled → compositor runs `service sshd start/stop`
//      via the rc-script helper (no root prompt).
//   3. StateChanged broadcast fires when sshd starts/stops or session count
//      changes — shell refreshes the sub-label.

enum class RemoteLoginOpcode : uint32_t {
    GetState     = 0x01, // → RemoteLoginState
    SetEnabled   = 0x02, // RemoteLoginRequest →
    StateChanged = 0x03, // broadcast → RemoteLoginState
};

struct RemoteLoginState {
    uint32_t enabled_;       // 1 = sshd running
    uint32_t port_;          // listening port (default 22)
    uint32_t sessions_;      // active SSH session count
    char     bind_addr_[16]; // e.g. "0.0.0.0" or "::"
    uint8_t  pad_[228];      // 4+4+4+16+228 = 256
};
static_assert(sizeof(RemoteLoginState) == 256);

struct RemoteLoginRequest {
    uint32_t enabled_;  // 1 = start sshd, 0 = stop sshd
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(RemoteLoginRequest) == 256);

// ── System Information ─────────────────────────────────────────────────────
// One-shot fetch on shell connect — no broadcast, no polling.
// Compositor reads from sysctl, DMI tables, and /etc/os-release.

enum class SysInfoOpcode : uint32_t {
    GetInfo = 0x01, // → SysInfoState
};

struct SysInfoState {
    char     hostname_[32];   // system hostname
    char     os_version_[32]; // "FreeBSD 14.2-RELEASE-p3"
    char     cpu_model_[48];  // "AMD Ryzen 9 7950X"
    char     gpu_model_[40];  // "AMD Radeon RX 7900 XTX" (may be "N/A")
    uint32_t ram_total_mb_;   // total installed RAM in MB
    uint32_t cpu_cores_;      // physical CPU core count
    uint32_t uptime_secs_;    // seconds since boot
    char     serial_[20];     // DMI chassis serial or "N/A"
    uint8_t  pad_[72];        // 32+32+48+40+4+4+4+20+72 = 256
};
static_assert(sizeof(SysInfoState) == 256);

// ── Time Zone ──────────────────────────────────────────────────────────────
// Flow:
//   1. Shell calls GetTZ on connect → TimezoneState.
//   2. User clicks ‹/› → SetTZ with the new POSIX zone name → compositor
//      symlinks /etc/localtime and sets TZ env for running sessions.
//   3. TZChanged broadcast fires → shell refreshes display and local time.
//   Shell also re-polls GetTZ every 30 s to keep the local_time_ sub-label live.

enum class TimezoneOpcode : uint32_t {
    GetTZ     = 0x01, // → TimezoneState
    SetTZ     = 0x02, // TimezoneSetRequest →
    TZChanged = 0x03, // broadcast → TimezoneState
};

struct TimezoneState {
    char    tz_name_[48];      // POSIX zone name, e.g. "America/New_York"
    char    display_name_[32]; // human label, e.g. "ET (UTC-5)"
    int32_t utc_offset_sec_;   // seconds east of UTC (negative = west)
    char    local_time_[12];   // current local time "HH:MM:SS"
    uint8_t pad_[160];         // 48+32+4+12+160 = 256
};
static_assert(sizeof(TimezoneState) == 256);

struct TimezoneSetRequest {
    char    tz_name_[48]; // POSIX zone name to activate
    uint8_t pad_[208];    // 48+208 = 256
};
static_assert(sizeof(TimezoneSetRequest) == 256);

// ── Energy Impact ──────────────────────────────────────────────────────────
// One-shot paged read — compositor samples powerd(8) / hwpmc data; no
// broadcast (polled by the shell every 5 s alongside GPU/thermal).

enum class EnergyOpcode : uint32_t {
    ListUsage = 0x01, // uint32_t page → EnergyPage (sorted by impact desc)
};

struct EnergyEntry {
    char     app_name_[32];  // process display name
    uint32_t impact_;        // 0=low, 1=medium, 2=high, 3=very high
    uint32_t wakeups_sec_;   // wakeups per second (avg over 10 s)
    uint32_t cpu_avg_pct_;   // CPU % over last 30 s
    uint8_t  pad_[20];       // 32+4+4+4+20 = 64
};
static_assert(sizeof(EnergyEntry) == 64);

struct EnergyPage {
    uint32_t    page_;
    uint32_t    total_;
    uint32_t    count_;
    uint32_t    has_more_;
    EnergyEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(EnergyPage) <= 256);

// ── Language / Locale ──────────────────────────────────────────────────────
// Compositor writes /etc/profile.d/locale.sh and re-exports LANG/LC_ALL
// so newly launched apps pick up the change without a restart.

enum class LocaleOpcode : uint32_t {
    GetLocale     = 0x01, // → LocaleState
    SetLocale     = 0x02, // LocaleSetRequest →
    LocaleChanged = 0x03, // broadcast → LocaleState
};

struct LocaleState {
    char    locale_[32];    // "en_US.UTF-8"
    char    lang_[16];      // "en"
    char    territory_[8];  // "US"
    char    encoding_[12];  // "UTF-8"
    uint8_t pad_[188];      // 68+188 = 256
};
static_assert(sizeof(LocaleState) == 256);

struct LocaleSetRequest {
    char    locale_[64];    // target locale string
    uint8_t pad_[192];      // 256
};
static_assert(sizeof(LocaleSetRequest) == 256);

// ── File Sync ──────────────────────────────────────────────────────────────
// Bridges Syncthing / rsync jobs.  Compositor monitors the Syncthing REST
// API or an rsync pid-file directory and translates state into FileSyncEntry.

enum class FileSyncOpcode : uint32_t {
    ListJobs    = 0x01, // uint32_t page → FileSyncPage
    PauseJob    = 0x02, // FileSyncActionRequest →
    ResumeJob   = 0x03, // FileSyncActionRequest →
    JobsChanged = 0x04, // broadcast (no payload)
};

struct FileSyncEntry {
    char     name_[28];    // job/folder display name
    char     path_[24];    // local root path
    uint32_t state_;       // 0=up-to-date, 1=syncing, 2=paused, 3=error
    uint32_t progress_;    // 0-100 when syncing
    uint8_t  pad_[4];      // 28+24+4+4+4 = 64
};
static_assert(sizeof(FileSyncEntry) == 64);

struct FileSyncPage {
    uint32_t      page_;
    uint32_t      total_;
    uint32_t      count_;
    uint32_t      has_more_;
    FileSyncEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(FileSyncPage) <= 256);

struct FileSyncActionRequest {
    char    name_[64];   // job name to pause / resume
    uint8_t pad_[192];   // 256
};
static_assert(sizeof(FileSyncActionRequest) == 256);

// ── Dock Layout ────────────────────────────────────────────────────────────
// Compositor repositions/resizes the shell panel; auto-hide is handled by the
// compositor input layer (hides after 2 s of no cursor near the dock edge).

enum class DockOpcode : uint32_t {
    GetSettings     = 0x01, // → DockSettings
    SetSettings     = 0x02, // DockSettings →
    SettingsChanged = 0x03, // broadcast → DockSettings
};

struct DockSettings {
    uint32_t position_;  // 0=Bottom, 1=Left, 2=Right
    uint32_t autohide_;  // 1=auto-hide enabled
    uint32_t icon_size_; // icon px: 32 / 48 / 64
    uint8_t  pad_[244];  // 4+4+4+244 = 256
};
static_assert(sizeof(DockSettings) == 256);

// ── Disk Usage ─────────────────────────────────────────────────────────────
// Compositor walks the filesystem with fts(3) in a worker thread.  The shell
// issues a Scan request and displays stale GetResults until ScanComplete fires.

enum class DiskUsageOpcode : uint32_t {
    Scan         = 0x01, // DiskUsageScanRequest → (async; ScanComplete when done)
    GetResults   = 0x02, // uint32_t page → DiskUsagePage (sorted by size desc)
    ScanComplete = 0x03, // broadcast (no payload)
};

struct DiskUsageEntry {
    char     path_[48];      // directory path (absolute, NUL-terminated)
    uint64_t size_bytes_;    // total size including subdirectories
    uint32_t pct_of_total_;  // percentage of scan root size (0-100)
    uint8_t  pad_[4];        // 48+8+4+4 = 64
};
static_assert(sizeof(DiskUsageEntry) == 64);

struct DiskUsagePage {
    uint32_t       page_;
    uint32_t       total_;
    uint32_t       count_;
    uint32_t       has_more_;
    DiskUsageEntry entries_[3]; // 16 + 3×64 = 208 ≤ 256
};
static_assert(sizeof(DiskUsagePage) <= 256);

struct DiskUsageScanRequest {
    char    path_[64];  // root path to scan (e.g. "/", "/home/user")
    uint8_t pad_[192];  // 64+192 = 256
};
static_assert(sizeof(DiskUsageScanRequest) == 256);

// ── GPU Monitor ────────────────────────────────────────────────────────────
// com.freebsdx.GpuMonitor — real-time GPU utilisation, VRAM, and temperature.

enum class GpuMonitorOpcode : uint32_t {
    GetStats     = 0x01, // → GpuStats
    StatsChanged = 0x02, // broadcast: GpuStats payload
};

struct GpuStatsDetail {
    char     name_[64];       // e.g. "AMD Radeon RX 6700"
    uint32_t util_pct_;       // 0–100
    uint32_t vram_used_mb_;
    uint32_t vram_total_mb_;
    uint32_t temp_c_;
    uint32_t fan_rpm_;        // 0 if passive / unknown
    uint8_t  pad_[172];       // 64+20+172 = 256
};
static_assert(sizeof(GpuStatsDetail) == 256);

// ── Screen Capture ─────────────────────────────────────────────────────────
// com.freebsdx.ScreenCapture — screenshots and screen recordings.

enum class ScreenCaptureOpcode : uint32_t {
    Capture       = 0x01, // ScreenCaptureRequest → ack
    ListCaptures  = 0x02, // uint32_t page → ScreenCapturePageFlat
    DeleteCapture = 0x03, // ScreenCaptureDeleteRequest → ack
    CaptureAdded  = 0x04, // broadcast: ScreenCapturePageFlat (valid_=1, page_=0)
};

struct ScreenCaptureRequest {
    uint32_t full_screen_;  // 1 = full screen; 0 = active window
    uint32_t with_cursor_;
    uint32_t delay_secs_;   // 0 = immediate
    uint8_t  pad_[244];
};
static_assert(sizeof(ScreenCaptureRequest) == 256);

struct ScreenCapturePageFlat {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     path_[168];
    uint64_t size_bytes_;
    uint32_t width_;
    uint32_t height_;
    uint32_t timestamp_;
    uint8_t  pad_[44];      // 16+168+8+4+4+4+44 = 248 ≤ 256
};
static_assert(sizeof(ScreenCapturePageFlat) <= 256);

struct ScreenCaptureDeleteRequest {
    char    path_[252];
    uint8_t pad_[4];
};
static_assert(sizeof(ScreenCaptureDeleteRequest) == 256);

// ── Display Arrange ────────────────────────────────────────────────────────
// com.freebsdx.DisplayArrange — multi-monitor layout, primary, and mirroring.

enum class DisplayArrangeOpcode : uint32_t {
    GetLayout     = 0x01, // uint32_t page → DisplayOutputPage
    SetPrimary    = 0x02, // DisplayOutputRequest{output_id_} → ack
    SetMirror     = 0x03, // DisplayMirrorRequest → ack
    LayoutChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct DisplayOutputPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t output_id_;
    uint32_t is_primary_;
    uint32_t mirroring_;    // 1 = mirrors primary
    uint32_t width_;
    uint32_t height_;
    uint32_t refresh_mhz_; // e.g. 60000 = 60 Hz
    char     name_[32];    // "HDMI-A-1"
    char     make_[32];
    char     model_[32];
    uint8_t  pad_[96];     // 40+32+32+32+96 = 232 ≤ 256
};
static_assert(sizeof(DisplayOutputPage) <= 256);

struct DisplayOutputRequest {
    uint32_t output_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(DisplayOutputRequest) == 256);

struct DisplayMirrorRequest {
    uint32_t enable_;      // 1 = enable mirroring
    uint32_t source_id_;
    uint32_t target_id_;
    uint8_t  pad_[244];
};
static_assert(sizeof(DisplayMirrorRequest) == 256);

// ── Clipboard History ──────────────────────────────────────────────────────
// com.freebsdx.ClipHistory — persistent clipboard entry ring.

enum class ClipHistoryOpcode : uint32_t {
    ListEntries = 0x01, // uint32_t page → ClipHistoryPageFlat
    PasteEntry  = 0x02, // ClipHistorySelectRequest{index_} → ack
    Clear       = 0x03, // → ack
    EntryAdded  = 0x04, // broadcast: ClipHistoryPageFlat (newest entry)
};

struct ClipHistoryPageFlat {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t index_;       // 0 = newest
    uint32_t timestamp_;
    uint32_t byte_len_;    // original byte length
    char     preview_[220]; // UTF-8 preview, NUL-terminated
    uint8_t  pad_[8];      // 28+220+8 = 256
};
static_assert(sizeof(ClipHistoryPageFlat) == 256);

struct ClipHistorySelectRequest {
    uint32_t index_;
    uint8_t  pad_[252];
};
static_assert(sizeof(ClipHistorySelectRequest) == 256);

// ── Gesture Config ─────────────────────────────────────────────────────────
// com.freebsdx.Gesture — trackpad / touchscreen gesture configuration.

enum class GestureOpcode : uint32_t {
    GetConfig     = 0x01, // → GestureConfig
    SetConfig     = 0x02, // GestureConfig → ack
    ConfigChanged = 0x03, // broadcast: GestureConfig
};

struct GestureConfig {
    uint32_t enabled_;
    uint32_t swipe3_sensitivity_; // 0–100
    uint32_t swipe4_sensitivity_;
    uint32_t pinch_sensitivity_;
    uint32_t tap_to_click_;
    uint32_t natural_scroll_;
    uint32_t two_finger_scroll_;
    uint32_t edge_scroll_;
    uint8_t  pad_[224];           // 32+224 = 256
};
static_assert(sizeof(GestureConfig) == 256);

// ── Input Devices ──────────────────────────────────────────────────────────
// com.freebsdx.InputDevice — enumerate and configure pointer/keyboard devices.

enum class InputDeviceOpcode : uint32_t {
    ListDevices    = 0x01, // uint32_t page → InputDevicePage
    SetSensitivity = 0x02, // InputSensitivityRequest → ack
    DeviceChanged  = 0x03, // broadcast (no payload — shell re-fetches)
};

struct InputDevicePage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t device_id_;
    uint32_t type_;          // 0=keyboard, 1=pointer, 2=touch, 3=tablet
    uint32_t sensitivity_;   // 0–100
    uint32_t accel_profile_; // 0=flat, 1=adaptive
    char     name_[64];
    char     vendor_[32];
    uint8_t  pad_[128];      // 32+64+32+128 = 256
};
static_assert(sizeof(InputDevicePage) == 256);

struct InputSensitivityRequest {
    uint32_t device_id_;
    uint32_t sensitivity_;
    uint32_t accel_profile_;
    uint8_t  pad_[244];
};
static_assert(sizeof(InputSensitivityRequest) == 256);

// ── Accessibility ──────────────────────────────────────────────────────────
// com.freebsdx.Accessibility — system-wide a11y feature toggles.

enum class AccessibilityPanelOpcode : uint32_t {
    GetSettings     = 0x01, // → AccessibilitySettings
    SetSettings     = 0x02, // AccessibilitySettings → ack
    SettingsChanged = 0x03, // broadcast: AccessibilitySettings
};

struct AccessibilitySettings {
    uint32_t high_contrast_;
    uint32_t large_text_;
    uint32_t magnifier_;
    uint32_t magnifier_scale_; // 100–500 percent
    uint32_t screen_reader_;
    uint32_t reduce_motion_;
    uint32_t sticky_keys_;
    uint32_t slow_keys_;
    uint32_t slow_keys_delay_ms_;
    uint32_t bounce_keys_;
    uint32_t cursor_size_;     // 1–4
    uint8_t  pad_[212];        // 44+212 = 256
};
static_assert(sizeof(AccessibilitySettings) == 256);

// ── Remote Desktop ─────────────────────────────────────────────────────────
// com.freebsdx.RemoteDesktop — inbound and outbound RDP/VNC sessions.

enum class RemoteDesktopSessionOpcode : uint32_t {
    ListSessions   = 0x01, // uint32_t page → RemoteDesktopPage
    Connect        = 0x02, // RemoteDesktopConnectRequest → ack
    Disconnect     = 0x03, // RemoteDesktopSessionRequest{session_id_} → ack
    SessionChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct RemoteDesktopPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t session_id_;
    uint32_t protocol_;   // 0=RDP, 1=VNC
    uint32_t direction_;  // 0=inbound, 1=outbound
    uint32_t state_;      // 0=connecting, 1=connected, 2=disconnected
    char     host_[64];
    char     user_[32];
    uint8_t  pad_[128];   // 32+64+32+128 = 256
};
static_assert(sizeof(RemoteDesktopPage) == 256);

struct RemoteDesktopConnectRequest {
    char     host_[128];
    char     user_[64];
    char     password_[48];
    uint32_t protocol_;
    uint32_t port_;       // 0 = default
    uint8_t  pad_[8];     // 128+64+48+4+4+8 = 256
};
static_assert(sizeof(RemoteDesktopConnectRequest) == 256);

struct RemoteDesktopSessionRequest {
    uint32_t session_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(RemoteDesktopSessionRequest) == 256);

// ── Wake-on-LAN ────────────────────────────────────────────────────────────
// com.freebsdx.WakeOnLan — send magic packets and manage saved targets.

enum class WakeOnLanOpcode : uint32_t {
    Send         = 0x01, // WakeOnLanRequest{mac_} → ack
    ListTargets  = 0x02, // uint32_t page → WakeOnLanPage
    AddTarget    = 0x03, // WakeOnLanTarget → ack
    RemoveTarget = 0x04, // WakeOnLanRemoveRequest{name_} → ack
};

struct WakeOnLanRequest {
    char    mac_[18];   // "AA:BB:CC:DD:EE:FF\0"
    uint8_t pad_[238];  // 18+238 = 256
};
static_assert(sizeof(WakeOnLanRequest) == 256);

struct WakeOnLanTarget {
    char    name_[64];
    char    mac_[18];
    uint8_t pad_[174];  // 64+18+174 = 256
};
static_assert(sizeof(WakeOnLanTarget) == 256);

struct WakeOnLanPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     name_[64];
    char     mac_[18];
    uint8_t  pad_[158]; // 16+64+18+158 = 256
};
static_assert(sizeof(WakeOnLanPage) == 256);

struct WakeOnLanRemoveRequest {
    char    name_[64];
    uint8_t pad_[192];  // 64+192 = 256
};
static_assert(sizeof(WakeOnLanRemoveRequest) == 256);

// ── System Log ─────────────────────────────────────────────────────────────
// com.freebsdx.SysLog — tail recent kernel and service log entries.

enum class SysLogOpcode : uint32_t {
    GetEntries = 0x01, // SysLogRequest → SysLogPageFlat stream
    LogAdded   = 0x02, // broadcast: SysLogPageFlat (newest entry)
};

struct SysLogRequest {
    uint32_t page_;
    uint32_t level_mask_; // bitmask: 1=emerg … 64=info; 0=all
    uint8_t  pad_[248];   // 8+248 = 256
};
static_assert(sizeof(SysLogRequest) == 256);

struct SysLogPageFlat {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t timestamp_;
    uint32_t level_;       // syslog severity 0–7
    char     facility_[24];
    char     message_[208]; // 24 (6×u32) + 24 + 208 = 256
};
static_assert(sizeof(SysLogPageFlat) == 256);

// ── DNS Config ─────────────────────────────────────────────────────────────
// com.freebsdx.DnsConfig — resolver settings and cache flush.

enum class DnsConfigOpcode : uint32_t {
    GetConfig     = 0x01, // → DnsConfig
    SetConfig     = 0x02, // DnsConfig → ack
    Flush         = 0x03, // → ack
    ConfigChanged = 0x04, // broadcast: DnsConfig
};

struct DnsResolverConfig {
    char     primary_[64];   // primary DNS IP or hostname
    char     secondary_[64];
    uint32_t doh_enabled_;   // 1 = DNS-over-HTTPS
    char     doh_url_[120];  // DoH server URL
    uint8_t  pad_[4];        // 64+64+4+120+4 = 256
};
static_assert(sizeof(DnsResolverConfig) == 256);

// ── Network Shares ─────────────────────────────────────────────────────────
// com.freebsdx.NetworkShare — SMB and NFS share management.

enum class NetworkShareMountOpcode : uint32_t {
    ListShares   = 0x01, // uint32_t page → NetworkSharePage
    Mount        = 0x02, // NetworkShareMountRequest → ack
    Unmount      = 0x03, // NetworkShareRequest{share_id_} → ack
    ShareChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct NetworkShareMountPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t share_id_;
    uint32_t protocol_;    // 0=SMB, 1=NFS
    uint32_t mounted_;
    char     host_[48];
    char     share_[48];
    char     mountpoint_[80];
    uint8_t  pad_[44];     // 28+48+48+80+44 = 248 ≤ 256
};
static_assert(sizeof(NetworkShareMountPage) <= 256);

struct NetworkShareMountRequest {
    char     host_[64];
    char     share_[64];
    char     mountpoint_[64];
    char     user_[32];
    char     password_[28];
    uint32_t protocol_;     // 0=SMB, 1=NFS
    uint8_t  pad_[0];       // 64+64+64+32+28+4 = 256
};
static_assert(sizeof(NetworkShareMountRequest) == 256);

struct NetworkShareRequest {
    uint32_t share_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(NetworkShareRequest) == 256);

// ── Thermal Monitor ────────────────────────────────────────────────────────
// com.freebsdx.Thermal — CPU/GPU/NVMe thermal zones and fan control.

enum class ThermalOpcode : uint32_t {
    GetZones     = 0x01, // uint32_t page → ThermalZonePage
    SetFanMode   = 0x02, // ThermalFanRequest → ack
    ZonesChanged = 0x03, // broadcast: ThermalZonePage (page=0)
};

struct ThermalZonePage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t zone_id_;
    uint32_t temp_c_;
    uint32_t fan_rpm_;     // 0 if no fan
    uint32_t fan_mode_;    // 0=auto, 1=manual
    uint32_t fan_pct_;     // 0–100 manual target
    char     label_[48];   // "CPU Package", "GPU", "NVMe 0"
    uint8_t  pad_[172];    // 36+48+172 = 256
};
static_assert(sizeof(ThermalZonePage) == 256);

struct ThermalFanRequest {
    uint32_t zone_id_;
    uint32_t fan_mode_;
    uint32_t fan_pct_;
    uint8_t  pad_[244];    // 12+244 = 256
};
static_assert(sizeof(ThermalFanRequest) == 256);

// ── USB Devices ────────────────────────────────────────────────────────────
// com.freebsdx.UsbDevice — enumerate attached USB devices and eject storage.

enum class UsbDeviceOpcode : uint32_t {
    ListDevices   = 0x01, // uint32_t page → UsbDevicePage
    Eject         = 0x02, // UsbDeviceRequest{device_id_} → ack
    DeviceChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct UsbDeviceDetailPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    uint32_t device_id_;
    uint32_t vendor_id_;
    uint32_t product_id_;
    uint32_t class_;       // USB device class (0x08=mass storage)
    uint32_t ejectable_;
    char     product_[64];
    char     manufacturer_[48];
    uint8_t  pad_[100];    // 36+64+48+100 = 248 ≤ 256
};
static_assert(sizeof(UsbDeviceDetailPage) <= 256);

struct UsbDeviceRequest {
    uint32_t device_id_;
    uint8_t  pad_[252];
};
static_assert(sizeof(UsbDeviceRequest) == 256);

// ── Notification Schedule ──────────────────────────────────────────────────
// com.freebsdx.NotifSchedule — quiet-hours window for Do Not Disturb.

enum class NotifScheduleOpcode : uint32_t {
    GetSchedule     = 0x01, // → NotifSchedule
    SetSchedule     = 0x02, // NotifSchedule → ack
    ScheduleChanged = 0x03, // broadcast: NotifSchedule
};

struct NotifSchedule {
    uint32_t enabled_;
    uint32_t start_hour_;
    uint32_t start_minute_;
    uint32_t end_hour_;
    uint32_t end_minute_;
    uint32_t allow_calls_;   // 1 = calls break through DND
    uint32_t allow_alarms_;
    uint32_t weekend_only_;
    uint8_t  pad_[224];      // 32+224 = 256
};
static_assert(sizeof(NotifSchedule) == 256);

// ── App Sandbox ────────────────────────────────────────────────────────────
// com.freebsdx.AppSandbox — per-app capability permission overrides.

enum class AppSandboxOpcode : uint32_t {
    ListApps      = 0x01, // uint32_t page → AppSandboxPage
    SetPermission = 0x02, // AppSandboxPermRequest → ack
    PermsChanged  = 0x03, // broadcast (no payload — shell re-fetches)
};

struct AppSandboxPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     identifier_[64];
    char     display_name_[48];
    uint32_t net_allowed_;
    uint32_t cam_allowed_;
    uint32_t mic_allowed_;
    uint32_t fs_allowed_;
    uint32_t gpu_allowed_;
    uint32_t screencap_allowed_;  // carved from pad (layout-compatible); 0 default-deny
    uint8_t  pad_[104];           // 16+64+48+24+104 = 256
};
static_assert(sizeof(AppSandboxPage) == 256);

struct AppSandboxPermRequest {
    char     identifier_[64];
    uint32_t permission_;  // 0=net,1=cam,2=mic,3=fs,4=gpu,5=screencapture
    uint32_t allowed_;     // 1=grant, 0=revoke
    uint8_t  pad_[184];    // 64+4+4+184 = 256
};
static_assert(sizeof(AppSandboxPermRequest) == 256);

// ── Screen Lock ────────────────────────────────────────────────────────────
// com.freebsdx.ScreenLock — auto-lock policy and manual lock trigger.

enum class ScreenLockOpcode : uint32_t {
    GetPolicy     = 0x01, // → ScreenLockPolicy
    SetPolicy     = 0x02, // ScreenLockPolicy → ack
    LockNow       = 0x03, // no payload → ack (locks immediately)
    PolicyChanged = 0x04, // broadcast: ScreenLockPolicy
};

struct ScreenLockPolicy {
    uint32_t enabled_;          // 1 = auto-lock active
    uint32_t timeout_seconds_;  // 0=never, otherwise idle timeout
    uint32_t require_password_; // 1 = ask password on wake
    uint32_t lock_on_sleep_;    // 1 = lock when lid closes / system sleeps
    uint8_t  pad_[240];         // 16+240 = 256
};
static_assert(sizeof(ScreenLockPolicy) == 256);

// ── Bluetooth Audio ────────────────────────────────────────────────────────
// com.freebsdx.BtAudio — paired BT audio device list and connection control.

enum class BtAudioOpcode : uint32_t {
    ListDevices   = 0x01, // uint32_t page → BtAudioPage
    Connect       = 0x02, // BtAudioRequest → ack
    Disconnect    = 0x03, // BtAudioRequest → ack
    DeviceChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct BtAudioPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     address_[18];     // "AA:BB:CC:DD:EE:FF\0"
    char     name_[46];        // product name
    uint32_t connected_;       // 1 = currently connected
    uint32_t battery_pct_;     // 0–100, 255 = unknown
    uint32_t codec_;           // 0=SBC, 1=AAC, 2=aptX, 3=aptX-HD, 4=LDAC
    uint8_t  pad_[164];        // 16+18+46+12+164 = 256
};
static_assert(sizeof(BtAudioPage) == 256);

struct BtAudioRequest {
    char     address_[18];
    uint8_t  pad_[238];
};
static_assert(sizeof(BtAudioRequest) == 256);

// ── Location Services (per-app precision extension) ───────────────────────
// Extends the existing com.freebsdx.Location service with per-app controls.

struct LocationAppPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     identifier_[64]; // bundle id
    char     name_[60];       // display name
    uint32_t precision_;      // 0=off, 1=approximate, 2=precise
    uint8_t  pad_[112];       // 16+64+60+4+112 = 256
};
static_assert(sizeof(LocationAppPage) == 256);

struct LocationAppPolicyRequest {
    char     identifier_[64]; // bundle id
    uint32_t precision_;      // 0=off, 1=approximate, 2=precise
    uint8_t  pad_[188];       // 64+4+188 = 256
};
static_assert(sizeof(LocationAppPolicyRequest) == 256);

// ── TOTP Authenticator ────────────────────────────────────────────────────
// com.freebsdx.Totp — list OTP accounts and fetch current codes.

enum class TotpOpcode : uint32_t {
    ListAccounts = 0x01, // uint32_t page → TotpAccountPage
    GetCode      = 0x02, // TotpCodeRequest → TotpCodeResponse
    AccountAdded = 0x03, // broadcast (no payload — shell re-fetches)
};

struct TotpAccountPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     id_[32];      // stable opaque id
    char     issuer_[48];  // e.g. "GitHub"
    char     account_[64]; // e.g. "user@example.com"
    uint8_t  pad_[96];     // 16+32+48+64+96 = 256
};
static_assert(sizeof(TotpAccountPage) == 256);

struct TotpCodeRequest {
    char     id_[32];    // matches TotpAccountPage::id_
    uint8_t  pad_[224];  // 32+224 = 256
};
static_assert(sizeof(TotpCodeRequest) == 256);

struct TotpCodeResponse {
    char     code_[8];         // "123456\0\0"
    uint32_t seconds_remaining_; // seconds until code rolls over
    uint8_t  pad_[244];        // 8+4+244 = 256
};
static_assert(sizeof(TotpCodeResponse) == 256);

// ── Audio EQ ──────────────────────────────────────────────────────────────
// com.freebsdx.AudioEq — 5-band parametric EQ with preset support.

enum class AudioEqOpcode : uint32_t {
    GetEq        = 0x01, // no payload → AudioEqConfig
    SetEq        = 0x02, // AudioEqConfig → ack
    EqChanged    = 0x03, // broadcast: AudioEqConfig
};

struct AudioEqConfig {
    // gains_[0..4]: Bass, Low-mid, Mid, High-mid, Treble; range −12 to +12 dB (int8)
    int8_t   gains_[5];
    uint8_t  preset_;    // 0=Flat,1=BassBoost,2=Vocal,3=Gaming,4=Custom
    uint8_t  pad_[250];  // 5+1+250 = 256
};
static_assert(sizeof(AudioEqConfig) == 256);

// ── Scheduled Backups ─────────────────────────────────────────────────────
// com.freebsdx.Backup — list backup jobs, query status, trigger manual run.

enum class BackupOpcode : uint32_t {
    ListJobs        = 0x01, // uint32_t page → BackupJobPage
    RunJob          = 0x02, // BackupJobRequest → ack
    JobStateChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct BackupJobPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     name_[48];        // job display name
    char     last_run_[24];    // "2026-07-31 14:32" or ""
    char     next_run_[24];    // "2026-08-01 02:00" or "Manual"
    uint32_t status_;          // 0=idle/ok, 1=running, 2=failed
    char     id_[32];          // stable opaque job id
    uint8_t  pad_[108];        // 16+48+24+24+4+32+108 = 256
};
static_assert(sizeof(BackupJobPage) == 256);

struct BackupJobRequest {
    char     id_[32];
    uint8_t  pad_[224];  // 32+224 = 256
};
static_assert(sizeof(BackupJobRequest) == 256);

// ── Mouse & Pointer ───────────────────────────────────────────────────────
// com.freebsdx.Pointer — pointer speed, scroll direction, accel, tap-to-click.

enum class PointerOpcode : uint32_t {
    GetConfig    = 0x01, // no payload → PointerConfig
    SetConfig    = 0x02, // PointerConfig → ack
    ConfigChanged = 0x03, // broadcast: PointerConfig
};

struct PointerConfig {
    uint32_t speed_;          // 1–10 (pointer speed steps)
    uint32_t natural_scroll_; // 1 = natural / reversed
    uint32_t acceleration_;   // 1 = adaptive acceleration on
    uint32_t tap_to_click_;   // 1 = trackpad tap acts as click
    uint8_t  pad_[240];       // 16+240 = 256
};
static_assert(sizeof(PointerConfig) == 256);

// ── Reminders ─────────────────────────────────────────────────────────────
// com.freebsdx.Reminder — list upcoming/overdue reminders and mark done.

enum class ReminderOpcode : uint32_t {
    ListReminders    = 0x01, // uint32_t page → ReminderPage
    CompleteReminder = 0x02, // ReminderActionRequest → ack
    ReminderChanged  = 0x03, // broadcast (no payload — shell re-fetches)
};

struct ReminderPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     id_[32];       // stable opaque id
    char     title_[80];    // reminder text
    char     due_[24];      // "2026-08-01 09:00" or ""
    uint32_t overdue_;      // 1 = past due time
    uint8_t  pad_[100];     // 16+32+80+24+4+100 = 256
};
static_assert(sizeof(ReminderPage) == 256);

struct ReminderActionRequest {
    char     id_[32];
    uint8_t  pad_[224]; // 32+224 = 256
};
static_assert(sizeof(ReminderActionRequest) == 256);

// ── Spatial Audio ─────────────────────────────────────────────────────────
// com.freebsdx.SpatialAudio — head-tracked / surround upmix config.

enum class SpatialAudioOpcode : uint32_t {
    GetConfig     = 0x01, // no payload → SpatialAudioConfig
    SetConfig     = 0x02, // SpatialAudioConfig → ack
    ConfigChanged = 0x03, // broadcast: SpatialAudioConfig
};

struct SpatialAudioConfig {
    uint32_t enabled_;      // 1 = spatial audio on
    uint32_t mode_;         // 0=Stereo, 1=Surround, 2=Adaptive
    char     device_[64];   // current output device name (display only)
    uint8_t  pad_[184];     // 4+4+64+184 = 256
};
static_assert(sizeof(SpatialAudioConfig) == 256);

// ── Active Transfers ──────────────────────────────────────────────────────
// com.freebsdx.Transfer — in-progress file transfer list and cancel control.

enum class TransferOpcode : uint32_t {
    ListActive       = 0x01, // uint32_t page → TransferPage
    CancelTransfer   = 0x02, // TransferActionRequest → ack
    TransferChanged  = 0x03, // broadcast (no payload — shell re-fetches)
};

struct TransferPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t has_more_;
    uint32_t valid_;
    char     id_[32];          // stable transfer id
    char     filename_[64];    // file being transferred
    uint64_t bytes_total_;     // total size in bytes
    uint64_t bytes_done_;      // bytes transferred so far
    uint32_t speed_kbps_;      // current transfer speed in KB/s
    uint32_t direction_;       // 0=download, 1=upload
    uint8_t  pad_[120];        // 16+32+64+8+8+4+4+120 = 256
};
static_assert(sizeof(TransferPage) == 256);

struct TransferActionRequest {
    char     id_[32];
    uint8_t  pad_[224]; // 32+224 = 256
};
static_assert(sizeof(TransferActionRequest) == 256);

// ── System Sounds ───────────────────────────────────────────────────────────
// com.freebsdx.SystemSounds — UI/notification sound-effect volume & mute.

enum class SystemSoundsOpcode : uint32_t {
    GetState     = 0x01, // → SystemSoundsState
    SetState     = 0x02, // SystemSoundsRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: SystemSoundsState
};

struct SystemSoundsState {
    uint32_t enabled_;     // 1 = UI/notification sounds on
    uint32_t volume_pct_;  // 0–100
    uint8_t  pad_[248];    // 4+4+248 = 256
};
static_assert(sizeof(SystemSoundsState) == 256);

struct SystemSoundsRequest {
    uint32_t enabled_;
    uint32_t volume_pct_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(SystemSoundsRequest) == 256);

// ── Game Controllers ────────────────────────────────────────────────────────
// com.freebsdx.GameController — connected gamepad/joystick management.
//
// Flow:
//   1. Shell calls ListControllers(page) → paged GameControllerPage.
//   2. User taps Calibrate → CalibrateController{controller_id_} → ack.
//   3. User taps Forget → ForgetController{controller_id_} → ack + broadcast.
//   4. Compositor broadcasts ControllerChanged on connect/disconnect
//      (no payload — shell re-fetches).

enum class GameControllerOpcode : uint32_t {
    ListControllers      = 0x01, // uint32_t page → GameControllerPage
    CalibrateController  = 0x02, // GameControllerRequest{controller_id_} → ack
    ForgetController     = 0x03, // GameControllerRequest{controller_id_} → ack
    ControllerChanged    = 0x04, // broadcast (no payload — shell re-fetches)
};

struct GameControllerPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    uint32_t controller_id_;
    uint32_t battery_pct_;   // 0–100; 0xFFFFFFFF if wired/unknown
    uint32_t connection_;    // 0=USB, 1=Bluetooth
    char     name_[64];      // e.g. "Xbox Wireless Controller"
    uint8_t  pad_[164];      // 28+64+164 = 256
};
static_assert(sizeof(GameControllerPage) == 256);

struct GameControllerRequest {
    uint32_t controller_id_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(GameControllerRequest) == 256);

// ── Live Captions ────────────────────────────────────────────────────────────
// com.freebsdx.LiveCaptions — system-wide auto-generated captions overlay
// for audio played through any app.

enum class LiveCaptionsOpcode : uint32_t {
    GetState     = 0x01, // → LiveCaptionsState
    SetState     = 0x02, // LiveCaptionsRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: LiveCaptionsState
};

struct LiveCaptionsState {
    uint32_t enabled_;   // 1 = captions overlay on
    uint32_t position_;  // 0=Bottom, 1=Top
    uint32_t font_size_; // 0=S, 1=M, 2=L
    uint8_t  pad_[244];  // 4+4+4+244 = 256
};
static_assert(sizeof(LiveCaptionsState) == 256);

struct LiveCaptionsRequest {
    uint32_t enabled_;
    uint32_t position_;
    uint32_t font_size_;
    uint8_t  pad_[244]; // 4+4+4+244 = 256
};
static_assert(sizeof(LiveCaptionsRequest) == 256);

// ── Saved Wi-Fi Networks ────────────────────────────────────────────────────
// com.freebsdx.SavedWifi — known/remembered network list, independent of the
// live scan+connect flow in NetworkOpcode.
//
// Flow:
//   1. Shell calls ListSaved(page) → paged SavedWifiPage.
//   2. User toggles auto-connect → SetAutoConnect(SavedWifiAutoConnectRequest) → ack.
//   3. User taps Forget → ForgetNetwork(SavedWifiForgetRequest) → ack + broadcast.
//   4. Compositor broadcasts SavedListChanged on save/forget elsewhere
//      (no payload — shell re-fetches).

enum class SavedWifiOpcode : uint32_t {
    ListSaved        = 0x01, // uint32_t page → SavedWifiPage
    SetAutoConnect   = 0x02, // SavedWifiAutoConnectRequest → ack
    ForgetNetwork    = 0x03, // SavedWifiForgetRequest → ack + broadcast
    SavedListChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct SavedWifiPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     ssid_[32];
    uint32_t security_type_;  // 0=open, 1=WPA2, 2=WPA3
    uint32_t auto_connect_;   // 1 = auto-connect enabled
    uint32_t last_connected_; // unix timestamp; 0 if never
    uint8_t  pad_[196];       // 16+32+12+196 = 256
};
static_assert(sizeof(SavedWifiPage) == 256);

struct SavedWifiAutoConnectRequest {
    char     ssid_[32];
    uint32_t auto_connect_;
    uint8_t  pad_[220]; // 32+4+220 = 256
};
static_assert(sizeof(SavedWifiAutoConnectRequest) == 256);

struct SavedWifiForgetRequest {
    char     ssid_[32];
    uint8_t  pad_[224]; // 32+224 = 256
};
static_assert(sizeof(SavedWifiForgetRequest) == 256);

// ── Data Saver ───────────────────────────────────────────────────────────────
// com.freebsdx.DataSaver — reduces background network usage system-wide.

enum class DataSaverOpcode : uint32_t {
    GetState     = 0x01, // → DataSaverState
    SetState     = 0x02, // DataSaverRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: DataSaverState
};

struct DataSaverState {
    uint32_t enabled_;         // 1 = data saver on
    uint32_t saved_mb_today_;  // estimated MB saved today (display only)
    uint8_t  pad_[248];        // 4+4+248 = 256
};
static_assert(sizeof(DataSaverState) == 256);

struct DataSaverRequest {
    uint32_t enabled_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(DataSaverRequest) == 256);

// ── Security Keys ───────────────────────────────────────────────────────────
// com.freebsdx.SecurityKey — registered FIDO2/U2F hardware security keys.
//
// Flow:
//   1. Shell calls ListKeys(page) → paged SecurityKeyPage.
//   2. User taps "Register" → RegisterKey → ack (device prompts for touch
//      out-of-band, outside this protocol).
//   3. User taps Remove → RemoveKey(SecurityKeyRequest) → ack + broadcast.
//   4. Compositor broadcasts KeysChanged on register/remove elsewhere
//      (no payload — shell re-fetches).

enum class SecurityKeyOpcode : uint32_t {
    ListKeys    = 0x01, // uint32_t page → SecurityKeyPage
    RegisterKey = 0x02, // no payload → ack (key added out-of-band)
    RemoveKey   = 0x03, // SecurityKeyRequest{key_id_} → ack + broadcast
    KeysChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct SecurityKeyPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    uint32_t key_id_;
    uint32_t added_ts_;    // unix timestamp when registered
    char     label_[64];   // user-assigned name, e.g. "YubiKey 5C"
    uint8_t  pad_[168];    // 24+64+168 = 256
};
static_assert(sizeof(SecurityKeyPage) == 256);

struct SecurityKeyRequest {
    uint32_t key_id_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(SecurityKeyRequest) == 256);

// ── Fast Charging ───────────────────────────────────────────────────────────
// com.freebsdx.FastCharging — battery fast-charge mode toggle.

enum class FastChargingOpcode : uint32_t {
    GetState     = 0x01, // → FastChargingState
    SetState     = 0x02, // FastChargingRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: FastChargingState
};

struct FastChargingState {
    uint32_t enabled_;       // 1 = fast charging on
    uint32_t charge_rate_w_; // current charge rate in watts (0 if not charging)
    uint8_t  pad_[248];      // 4+4+248 = 256
};
static_assert(sizeof(FastChargingState) == 256);

struct FastChargingRequest {
    uint32_t enabled_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(FastChargingRequest) == 256);

// ── Text Replacement ────────────────────────────────────────────────────────
// com.freebsdx.TextReplacement — system-wide shortcut → phrase expansions.
//
// Flow:
//   1. Shell calls ListShortcuts(page) → paged TextReplacementPage.
//   2. User fills shortcut+phrase, taps Add → AddShortcut(TextReplacementAddRequest)
//      → ack + broadcast.
//   3. User taps Delete → DeleteShortcut(TextReplacementDeleteRequest) → ack + broadcast.
//   4. Compositor broadcasts ShortcutsChanged (no payload — shell re-fetches).

enum class TextReplacementOpcode : uint32_t {
    ListShortcuts    = 0x01, // uint32_t page → TextReplacementPage
    AddShortcut      = 0x02, // TextReplacementAddRequest → ack + broadcast
    DeleteShortcut   = 0x03, // TextReplacementDeleteRequest → ack + broadcast
    ShortcutsChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct TextReplacementPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     shortcut_[32];
    char     phrase_[128];
    uint32_t entry_id_;   // stable id, used for delete
    uint8_t  pad_[76];    // 16+32+128+4+76 = 256
};
static_assert(sizeof(TextReplacementPage) == 256);

struct TextReplacementAddRequest {
    char     shortcut_[32];
    char     phrase_[128];
    uint8_t  pad_[96]; // 32+128+96 = 256
};
static_assert(sizeof(TextReplacementAddRequest) == 256);

struct TextReplacementDeleteRequest {
    uint32_t entry_id_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(TextReplacementDeleteRequest) == 256);

// ── Do Not Disturb ──────────────────────────────────────────────────────────
// com.freebsdx.DoNotDisturb — system-wide notification silencing.

enum class DoNotDisturbOpcode : uint32_t {
    GetState     = 0x01, // → DoNotDisturbState
    SetState     = 0x02, // DoNotDisturbRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: DoNotDisturbState
};

struct DoNotDisturbState {
    uint32_t enabled_;    // 1 = notifications silenced
    uint32_t duration_;   // 0=Until off, 1=1 hour, 2=8 hours, 3=Until tomorrow
    uint32_t ends_at_ts_; // unix timestamp when DND auto-expires (0 = indefinite)
    uint8_t  pad_[244];   // 4+4+4+244 = 256
};
static_assert(sizeof(DoNotDisturbState) == 256);

struct DoNotDisturbRequest {
    uint32_t enabled_;
    uint32_t duration_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(DoNotDisturbRequest) == 256);

// ── App Time Limits ─────────────────────────────────────────────────────────
// com.freebsdx.AppTimeLimit — per-app daily usage limits.
//
// Flow:
//   1. Shell calls ListLimits(page) → paged AppTimeLimitPage.
//   2. User fills app name + minutes, taps Add → SetLimit(AppTimeLimitSetRequest)
//      → ack + broadcast.
//   3. User taps Remove → RemoveLimit(AppTimeLimitRemoveRequest) → ack + broadcast.
//   4. Compositor broadcasts LimitsChanged (no payload — shell re-fetches).

enum class AppTimeLimitOpcode : uint32_t {
    ListLimits    = 0x01, // uint32_t page → AppTimeLimitPage
    SetLimit      = 0x02, // AppTimeLimitSetRequest → ack + broadcast
    RemoveLimit   = 0x03, // AppTimeLimitRemoveRequest → ack + broadcast
    LimitsChanged = 0x04, // broadcast (no payload — shell re-fetches)
};

struct AppTimeLimitPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     app_name_[64];
    uint32_t limit_minutes_;
    uint32_t used_minutes_today_;
    uint8_t  pad_[168];    // 16+64+8+168 = 256
};
static_assert(sizeof(AppTimeLimitPage) == 256);

struct AppTimeLimitSetRequest {
    char     app_name_[64];
    uint32_t limit_minutes_;
    uint8_t  pad_[188]; // 64+4+188 = 256
};
static_assert(sizeof(AppTimeLimitSetRequest) == 256);

struct AppTimeLimitRemoveRequest {
    char     app_name_[64];
    uint8_t  pad_[192]; // 64+192 = 256
};
static_assert(sizeof(AppTimeLimitRemoveRequest) == 256);

// ── Battery Health ───────────────────────────────────────────────────────────
// com.freebsdx.BatteryHealth — optimized charging + battery wear info.

enum class BatteryHealthOpcode : uint32_t {
    GetState     = 0x01, // → BatteryHealthState
    SetState     = 0x02, // BatteryHealthRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: BatteryHealthState
};

struct BatteryHealthState {
    uint32_t optimized_charging_; // 1 = optimized charging on (caps at ~80%)
    uint32_t health_pct_;         // 0-100, current max capacity vs. design capacity
    uint32_t cycle_count_;
    uint8_t  pad_[244];           // 4+4+4+244 = 256
};
static_assert(sizeof(BatteryHealthState) == 256);

struct BatteryHealthRequest {
    uint32_t optimized_charging_;
    uint8_t  pad_[252]; // 4+252 = 256
};
static_assert(sizeof(BatteryHealthRequest) == 256);

// ── Notification Sounds ─────────────────────────────────────────────────────
// com.freebsdx.NotifSound — per-app notification sound assignment.
//
// Flow:
//   1. Shell calls ListSounds(page) → paged NotifSoundPage.
//   2. User taps Cycle → SetSound(NotifSoundRequest) → ack + broadcast (advances
//      sound_id_ to the next preset, wrapping).
//   3. Compositor broadcasts SoundsChanged (no payload — shell re-fetches).

enum class NotifSoundOpcode : uint32_t {
    ListSounds    = 0x01, // uint32_t page → NotifSoundPage
    SetSound      = 0x02, // NotifSoundRequest → ack + broadcast
    SoundsChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct NotifSoundPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     app_name_[64];
    uint32_t sound_id_;   // 0=Default, 1=Chime, 2=Ping, 3=Silent
    uint8_t  pad_[172];   // 16+64+4+172 = 256
};
static_assert(sizeof(NotifSoundPage) == 256);

struct NotifSoundRequest {
    char     app_name_[64];
    uint32_t sound_id_;
    uint8_t  pad_[188]; // 64+4+188 = 256
};
static_assert(sizeof(NotifSoundRequest) == 256);

// ── True Tone ────────────────────────────────────────────────────────────────
// com.freebsdx.TrueTone — adaptive white-balance that warms the display to the
// ambient light. Toggle + strength, mirrors the BatteryHealth state pattern.

enum class TrueToneOpcode : uint32_t {
    GetState     = 0x01, // → TrueToneState
    SetState     = 0x02, // TrueToneRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: TrueToneState
};

struct TrueToneState {
    uint32_t enabled_;    // 1 = adapting white balance to ambient light
    uint32_t strength_;   // 0-100, how far to shift toward the ambient temperature
    uint32_t ambient_k_;  // measured ambient color temperature (Kelvin), display-only
    uint8_t  pad_[244];   // 4+4+4+244 = 256
};
static_assert(sizeof(TrueToneState) == 256);

struct TrueToneRequest {
    uint32_t enabled_;
    uint32_t strength_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(TrueToneRequest) == 256);

// ── Login Items ──────────────────────────────────────────────────────────────
// com.freebsdx.LoginItems — apps launched at login; each can be toggled on/off.
//
// Flow:
//   1. Shell calls ListItems(page) → paged LoginItemPage.
//   2. User taps a row → SetItem(LoginItemRequest) → ack + broadcast (flips
//      enabled_ for that app).
//   3. Compositor broadcasts ItemsChanged (no payload — shell re-fetches).

enum class LoginItemOpcode : uint32_t {
    ListItems    = 0x01, // uint32_t page → LoginItemPage
    SetItem      = 0x02, // LoginItemRequest → ack + broadcast
    ItemsChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct LoginItemPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     app_name_[64];
    uint32_t enabled_;    // 1 = launches at login
    uint8_t  pad_[172];   // 16+64+4+172 = 256
};
static_assert(sizeof(LoginItemPage) == 256);

struct LoginItemRequest {
    char     app_name_[64];
    uint32_t enabled_;
    uint8_t  pad_[188]; // 64+4+188 = 256
};
static_assert(sizeof(LoginItemRequest) == 256);

// ── Optimized Storage ────────────────────────────────────────────────────────
// com.freebsdx.OptStorage — auto-offload rarely-used files when disk gets tight.
// Toggle + free-space target, mirrors the TrueTone state pattern.

enum class OptStorageOpcode : uint32_t {
    GetState     = 0x01, // → OptStorageState
    SetState     = 0x02, // OptStorageRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: OptStorageState
};

struct OptStorageState {
    uint32_t enabled_;        // 1 = auto-offload active
    uint32_t target_pct_;     // 0-100, free-space target that triggers offload
    uint32_t reclaimable_mb_; // display-only: how much could be freed right now
    uint8_t  pad_[244];       // 4+4+4+244 = 256
};
static_assert(sizeof(OptStorageState) == 256);

struct OptStorageRequest {
    uint32_t enabled_;
    uint32_t target_pct_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(OptStorageRequest) == 256);

// ── Trusted Devices ──────────────────────────────────────────────────────────
// com.freebsdx.TrustedDevices — devices allowed to pair/sync without a prompt.
//
// Flow:
//   1. Shell calls ListDevices(page) → paged TrustedDevicePage.
//   2. User taps a row → SetTrust(TrustedDeviceRequest) → ack + broadcast
//      (flips trusted_ for that device).
//   3. Compositor broadcasts DevicesChanged (no payload — shell re-fetches).

enum class TrustedDeviceOpcode : uint32_t {
    ListDevices    = 0x01, // uint32_t page → TrustedDevicePage
    SetTrust       = 0x02, // TrustedDeviceRequest → ack + broadcast
    DevicesChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct TrustedDevicePage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     device_name_[64];
    uint32_t trusted_;    // 1 = trusted (no pairing prompt)
    uint8_t  pad_[172];   // 16+64+4+172 = 256
};
static_assert(sizeof(TrustedDevicePage) == 256);

struct TrustedDeviceRequest {
    char     device_name_[64];
    uint32_t trusted_;
    uint8_t  pad_[188]; // 64+4+188 = 256
};
static_assert(sizeof(TrustedDeviceRequest) == 256);

// ── Mono Audio ───────────────────────────────────────────────────────────────
// com.freebsdx.MonoAudio — accessibility: collapse stereo to mono and shift the
// left/right balance. Toggle + balance, mirrors the TrueTone state pattern.

enum class MonoAudioOpcode : uint32_t {
    GetState     = 0x01, // → MonoAudioState
    SetState     = 0x02, // MonoAudioRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: MonoAudioState
};

struct MonoAudioState {
    uint32_t enabled_;   // 1 = play the same audio in both channels
    uint32_t balance_;   // 0-100, L↔R balance (50 = centered)
    uint8_t  pad_[248];  // 4+4+248 = 256
};
static_assert(sizeof(MonoAudioState) == 256);

struct MonoAudioRequest {
    uint32_t enabled_;
    uint32_t balance_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(MonoAudioRequest) == 256);

// ── Keyboard Shortcuts ───────────────────────────────────────────────────────
// com.freebsdx.KeyboardShortcuts — system shortcut bindings, each toggleable.
//
// Flow:
//   1. Shell calls ListShortcuts(page) → paged KeyShortcutPage.
//   2. User taps a row → SetShortcut(KeyShortcutRequest) → ack + broadcast
//      (flips enabled_ for that action).
//   3. Compositor broadcasts ShortcutsChanged (no payload — shell re-fetches).

enum class KeyboardShortcutOpcode : uint32_t {
    ListShortcuts    = 0x01, // uint32_t page → KeyShortcutPage
    SetShortcut      = 0x02, // KeyShortcutRequest → ack + broadcast
    ShortcutsChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct KeyShortcutPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     action_[48];  // e.g. "Screenshot"
    char     binding_[16]; // e.g. "Cmd+Shift+4"
    uint32_t enabled_;     // 1 = binding active
    uint8_t  pad_[172];    // 16+48+16+4+172 = 256
};
static_assert(sizeof(KeyShortcutPage) == 256);

struct KeyShortcutRequest {
    char     action_[48];
    uint32_t enabled_;
    uint8_t  pad_[204]; // 48+4+204 = 256
};
static_assert(sizeof(KeyShortcutRequest) == 256);

// ── Stage Manager ────────────────────────────────────────────────────────────
// com.freebsdx.StageManager — group windows into a focused stage with a strip of
// recent apps. Toggle + recent-strip size, mirrors the TrueTone state pattern.

enum class StageManagerOpcode : uint32_t {
    GetState     = 0x01, // → StageManagerState
    SetState     = 0x02, // StageManagerRequest → ack + broadcast
    StateChanged = 0x03, // broadcast: StageManagerState
};

struct StageManagerState {
    uint32_t enabled_;      // 1 = stage manager active
    uint32_t recent_count_; // 1-6, how many recent apps to keep in the strip
    uint8_t  pad_[248];     // 4+4+248 = 256
};
static_assert(sizeof(StageManagerState) == 256);

struct StageManagerRequest {
    uint32_t enabled_;
    uint32_t recent_count_;
    uint8_t  pad_[248]; // 4+4+248 = 256
};
static_assert(sizeof(StageManagerRequest) == 256);

// ── Default Apps ─────────────────────────────────────────────────────────────
// com.freebsdx.DefaultApps — the handler app for each file-type category.
//
// Flow:
//   1. Shell calls ListDefaults(page) → paged DefaultAppPage.
//   2. User taps Cycle → CycleDefault(DefaultAppRequest) → ack + broadcast
//      (advances handler_ to the next available app for that category).
//   3. Compositor broadcasts DefaultsChanged (no payload — shell re-fetches).

enum class DefaultAppsOpcode : uint32_t {
    ListDefaults   = 0x01, // uint32_t page → DefaultAppPage
    CycleDefault   = 0x02, // DefaultAppRequest → ack + broadcast
    DefaultsChanged = 0x03, // broadcast (no payload — shell re-fetches)
};

struct DefaultAppPage {
    uint32_t page_;
    uint32_t total_;
    uint32_t valid_;
    uint32_t has_more_;
    char     category_[48]; // e.g. "Web Browser", "Email", "Music"
    char     handler_[64];  // e.g. "Browser.xapp"
    uint8_t  pad_[128];     // 16+48+64+128 = 256
};
static_assert(sizeof(DefaultAppPage) == 256);

struct DefaultAppRequest {
    char     category_[48];
    uint8_t  pad_[208]; // 48+208 = 256
};
static_assert(sizeof(DefaultAppRequest) == 256);

} // namespace fbx::shared
