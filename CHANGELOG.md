# Changelog

All notable changes to **iFancyZones** are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
No breaking data-schema change has shipped yet, so `MAJOR` stays at `1`.

Versions are set in `src/iFancyZones/CMakeLists.txt` via
`project(iFancyZones VERSION x.y.z)` and reach the running app through the
`IFZ_VERSION` compile definition, so CMake is the single source of truth.

---

## [Unreleased]

Planned work is tracked in the [roadmap](README.md#roadmap): app-zone history,
exclude-apps logic, snap animation, Developer ID notarization, per-screen layout
templates, Spaces awareness, layout import/export and auto-update.

---

## [1.7.0] - 2026-11-10

macOS 27 compatibility: a binary that runs on every supported macOS, working
launch at login, Qt 6.12 and a stricter hardened runtime.

### Added

- **Launch at login works.** The setting was saved but never applied. It now
  registers the app through `SMAppService` (`platform/mac/LoginItem`). The
  system state is the source of truth: a change made in System Settings >
  General > Login Items shows up in the app.

### Changed

- **Minimum macOS is now 14.4.** Qt 6.12 is built for macOS 14.4, so the app
  never ran on anything older. `CMAKE_OSX_DEPLOYMENT_TARGET`,
  `LSMinimumSystemVersion` and the README now say so.
- **Builds against Qt 6.12.0.** `package-dmg.sh` and the README pointed at
  Qt 6.11.1. The script also drops a build cache made with another Qt.
- **Fewer hardened-runtime exceptions.** The JIT, unsigned-memory and Apple
  Events entitlements were not needed and are gone.
  `disable-library-validation` stays: with an ad-hoc signature (no Team ID)
  library validation refuses the bundled Qt frameworks and dyld aborts at
  launch.
- Install notes describe **Open Anyway** in Privacy & Security, since the
  right-click Open shortcut stopped bypassing Gatekeeper in macOS 15.

### Fixed

- **Binary targeted the build machine's macOS.** `CMAKE_OSX_DEPLOYMENT_TARGET`
  was set after `project()` with a non-forcing `set(... CACHE)`, which never
  overrides the empty entry `project()` creates. A build on macOS 27.2 refused
  to launch on any older system. Existing build dirs are fixed on reconfigure.
- `QImage::mirrored` replaced with `flipped(Qt::Vertical)`.

---

## [1.6.0] - 2026-11-06

Liquid Glass on macOS 26 and later, an easier zone resize, and fixes for the
snap width, the app switcher and the popover beak.

### Added

- **Liquid Glass.** On macOS 26 and later, `AppKitBridge::setGlassBackdrop`
  puts an `NSGlassEffectView` behind the Qt content:
  - The popover is a floating glass panel 6 pt below the menu bar, with no
    beak, like the system's own menu-bar panels. Its 24 pt corner radius is
    concentric with the cards inside it.
  - The layout editor toolbar is a glass capsule with capsule buttons.
  - The app switcher has a glass platter behind the cards.
  - The popover's segmented control uses capsule shapes.

  Glass surfaces always render in Dark Mode, because the app paints light
  text. Older systems keep the vibrancy look.

### Changed

- **Zone resize is easier to grab.** The resize band ran 8 px in from the
  widget edge, but the visible border sits `gap` px further in. With a gap of
  8 px or more, the band was in the invisible gap and the border itself only
  moved the zone. The band now covers the gap plus 12 px inside the border,
  and corners catch 10 px further along each side. The edge under the cursor
  shows a white bar before you press.

### Fixed

- **A snapped window keeps the width you designed.** The editor measured zones
  against the whole screen, but a snap fills the available area, without the
  menu bar and the Dock. With the Dock on the left or right, every snapped
  window lost the Dock's width. The editor canvas is now the available area,
  and the menu bar and Dock are shaded. Layouts saved earlier show in the
  editor as they actually snap. Adjust and save them again where needed.
- **The app switcher raises one window.** It activated the chosen app with
  all its windows, so a Chrome window in another zone covered the app there.
  It now raises only the app's frontmost window, the way zone cycling does.
  An app with no window on the current space is unhidden and activated.
- **The popover beak joins the panel without a seam** (before macOS 26). The
  beak met the body at a corner, and the Qt tint and the AppKit blur mask
  were two different paths up to 1 px apart. The outline is now one closed
  contour with a smooth beak, and the blur mask is built from that same path.

---

## [1.5.0] - 2026-09-28

A configurable zone color, a new app icon, and fixes for zone cycling and
snapping the wrong window.

### Added

- **Configurable zone color.** Settings has a new `Zone color:` row with a
  swatch that opens the color picker and a `Reset` button that restores the
  default green. The color applies to the snap preview, the layout editor and
  the screen-number badges. `AppSettings::zoneColor` stores it as `zone-color`
  (`#RRGGBB`) in `data.json`. `theme::zoneColor()` gives each surface the color
  at its own alpha.

### Changed

- **New app icon.** A green squircle with three stacked windows replaces the
  old icon in `resources/icons/iFancyZones.png` (1024 px, transparent margin
  on the Apple icon grid) and in `packaging/iFancyZones.icns` (16 px to
  1024 px). `package-dmg.sh` takes the DMG volume icon and the DMG file icon
  from the same `.icns`.

### Fixed

- **The About card in Settings shows the app icon.** `qt_standard_project_setup()`
  turns on AUTOMOC and AUTOUIC only, so `iFancyZones.qrc` was never compiled
  and `:/icons/app.png` did not load. `CMakeLists.txt` now sets
  `CMAKE_AUTORCC`. The card scales the icon for the backing store, so it
  stays sharp on Retina.

- **Cycle within a zone no longer changes another zone.** `raiseWindow` raised
  the target window and then activated the whole app. Activation also brought
  the app's previous key window forward, so a second window of the same app in
  another zone moved to the front and took focus. Now the target becomes the
  main window first, and SkyLight fronts the process with only that window.
- **Snap moves the window you drag.** The drop handler used the AX focused
  window captured at mouse-down. That was the wrong window when a Chrome tab
  was torn off into a new window, or when an app (PhpStorm, WebStorm) still
  reported its previous window as focused. Now the drop handler moves the
  topmost window under the cursor at mouse-up, matched by `CGWindowID` or by
  frame. The mouse-down window stays as the fallback.

---

## [1.4.0] - 2026-09-17

Menu-bar popover reliability plus a full redesign of both tabs.

### Fixed

- **The popover no longer dismisses itself instantly.** It was a `Qt::Popup`,
  which installs a mouse grab and closes on the first click outside its rect. On
  macOS the status-item click that opens it is still in flight when the grab goes
  up, so that click's own mouse-up landed "outside" and hid the popover. macOS
  26/27 reworked `NSStatusItem` event delivery, turning a long-standing race into
  a guaranteed instant-hide.

  It is now a tool window that owns its dismissal: close on `WindowDeactivate`,
  with a grace period covering the opening click and a toggle guard so clicking
  the status item again closes instead of reopening. An accessory app is never
  frontmost, so the window is activated explicitly; without that it could never
  become key and would never report losing focus.

- **The vibrancy view no longer covered the whole UI.** It is now a sibling of
  the Qt view instead of its child, because a subview is always composited above
  whatever its superview draws.

### Added

- New `ui/PopoverTheme` and `ui/PopoverWidgets`: shared tokens and reusable
  controls behind the redesigned chrome.
- Popover chrome: a beak pointing at the status item, a rounded vibrancy panel, a
  segmented tab control and a Settings / Quit footer.
- **Displays tab**: an Accessibility banner, then one card per display carrying a
  monitor-framed layout preview, a `MAIN` tag, resolution, panel size and the
  layout binding.
- **Layouts tab**: live search, a five-entry template gallery (Focus, Columns,
  Rows, Grid, Priority) that seeds a new layout, and rows showing the zone count,
  which display uses the layout, and edit / duplicate / delete on hover.
- `Monitor::isMain` and `Monitor::physicalSizeMm` so the UI can label displays.
- `QuartzCore` linked for layer-backed overlay compositing.

### Changed

- `AppKitBridge` gained the window-chrome, vibrancy and beak plumbing
  (+118 lines); `MacScreenInfo` now reports the main-display flag and physical
  size.

---

## [1.1.5] - [1.3.9]

No per-version notes were kept for this range, and the DMGs were overwritten as
each build replaced the last. Only `1.3.8`, `1.3.9`, `1.4.0` and later still exist
locally under `releases/`, which is untracked.

What is verifiable is that the following feature was present in the source at
`1.3.9` but absent at `1.1.4`; the exact version that introduced it is not
recorded:

### Added

- **3D app-switcher carousel.** `app/AppSwitcher` coordinates a trigger chord
  (`HotkeyManager`) that opens `ui/CarouselOverlay`; `platform/mac/SwitcherInput`
  taps arrow keys and the modifier release to navigate and commit;
  `platform/mac/AppSwitcherBridge` enumerates and raises running applications.
  Default chord `Opt+Space`, configurable and switchable off via
  `switcherEnabled`. The event tap exists only while the carousel is open, so the
  input path costs nothing when the feature is idle.

---

## [1.1.4]

### Fixed

- **Identical monitors no longer share a binding.** Two DELL U2722D panels were
  producing the same `Monitor::stableKey()`: the main display reported
  `CGDisplayUnitNumber == 0` and fell back to its `displayId = 1`, while the
  second DELL had `unitNumber == 1`. Both collapsed to key `1`, so choosing a
  layout for one changed both.

  `stableKey()` now sets the high bit when falling back to `displayId`, putting
  fallback values in `0x80000000...0xFFFFFFFF` and real unit numbers in
  `0x00000001...0x7FFFFFFF`. Collision is impossible.
  `MacScreenInfo::unitNumberFor(QScreen*)` mirrors the same fallback.

---

## [1.1.3]

### Changed

- Preset auto-alignment moved from a bottom strip of 12 Unicode glyphs to a
  **4x3 grid of mini-thumbnails centered inside the zone**. Each tile is
  hand-painted with `QPainter` (no font dependency): dark chip, inner monitor
  outline, filled rectangle marking the preset's target area.
- Tile size raised from 28 px to 52 px so a layout is pickable at a glance. The
  grid shows only when the zone is wider than 270 px and taller than 256 px.

### Fixed

- `Save` in the editor now force-commits the in-progress layout name even if the
  user never pressed Enter. `EditorToolbar::currentName()` returns the field text
  immediately and `LayoutEditor::onSave` applies it before emitting `saved`.

---

## [1.1.2]

### Added

- First iteration of the `QPainter`-drawn preset thumbnails, still a 28 px bottom
  strip. Expanded into the centered 4x3 grid in 1.1.3.

---

## [1.1.1]

### Fixed

- **All monitors appear again.** `Monitor::isValid()` filtered on
  `unitNumber != 0`, which dropped the main display on macOS Sequoia and later
  where `CGDisplayUnitNumber` reports 0 for the menu-bar display. With three
  monitors attached only two rows showed up. It now checks `displayId != 0`,
  which is non-zero for any live display.

### Added

- Startup diagnostics: `MacScreenInfo::currentMonitors()` and
  `WindowManager::start()` log every `NSScreen` with its `displayId`,
  `unitNumber` and frame. Filter "iFancyZones" in Console.app.

---

## [1.1.0]

### Added

- **Cycle window focus inside a zone.** `platform/mac/HotkeyManager` wraps Carbon
  `RegisterEventHotKey` and fires on the Qt main thread.
  `AccessibilityBridge::raiseWindow(pid, CGWindowID)` resolves the AX window
  through the weak-imported private symbol `_AXUIElementGetWindow`, then applies
  `kAXRaiseAction`, `kAXMainAttribute` / `kAXFocusedAttribute` and
  `activateWithOptions:`. `WindowManager::onCycleHotkey` filters
  `CGWindowListCopyWindowInfo` by layer 0, size >= 50, center inside the current
  zone and not-self-pid, then raises the back-most candidate. Default chord
  `Opt+Tab`, since `Cmd+Tab` collides with the system app switcher. New
  `ui/ChordEdit` picker; re-registers live when the chord changes, no restart.
- **About / Author block** at the bottom of the Settings window: 64x64 app icon,
  app name, version, author link and copyright. The version comes from
  `qApp->applicationVersion()`, fed by the compile-time `IFZ_VERSION`.
- **Detect** button in the Screen & Layout tab: one overlay per physical display
  showing a giant green circle and white index at `NSScreenSaverWindowLevel`,
  auto-closing after 3 seconds. Each row also gained a `ScreenNumberBadge`.
- App icon as a 10-resolution `iFancyZones.icns`, the source PNG embedded at
  `:/icons/app.png`, and `.VolumeIcon.icns` in the DMG so Finder shows the
  branded volume.
- First-launch onboarding: `Application::bootstrap` calls `requestTrust()` when
  not yet trusted, and a 2 s Accessibility watchdog installs `DragDetector` the
  moment permission flips, with no restart. A yellow popover banner links
  straight to Privacy & Security -> Accessibility and hides itself once granted.
- Comprehensive `qDebug` and `NSLog` instrumentation across `DragDetector`,
  `WindowManager`, `MacScreenInfo` and `AccessibilityBridge`.

### Fixed

- **Multi-monitor identity.** `MacScreenInfo::currentMonitors()` now iterates
  `NSScreen.screens` directly instead of mapping QScreens to NSScreens by name,
  which collapsed identical monitors into one. `unitNumberFor(QScreen*)` uses
  center-point containment rather than frame equality, and the Qt-to-Cocoa
  conversion is explicit via `CocoaFrameToQt(frame, primaryHeight)`.
- **Snap robustness.** `onDragStarted` calls
  `AccessibilityBridge::captureFocusedWindow()` so the exact window grabbed at
  mouse-down is the one moved, surviving any focus shift mid-drag. `onDragEnded`
  issues the AX `setFrame` twice, immediately and again after 90 ms, to defeat
  the OS drag-finalize race. Hit-testing switched to
  `ZoneOverlay::pixelRectOfZoneOuter` so the full logical zone stays clickable
  instead of only the gap-inset area.

### Changed

- Editor backdrop locked: `setFixedSize(screen->geometry().size())` plus
  `AppKitBridge::configureAsEditor` setting `movable = NO` and stripping
  `NSWindowStyleMaskResizable`. Only zone widgets stay interactive.
- New `EditorToolbar` layout: `+ New Zone`, an inline editable layout name,
  `Gap -` / `Gap +`, and Save (green) / Cancel. Gap changes render immediately,
  each zone painting its content inset by `gap` on every side.
- Tray icon lost its right-click context menu; a click opens the popover
  directly. Quit moved to the popover footer, Settings to the right.
- Exclude-apps UI hidden, because `WindowManager` does not honour the list yet
  and showing it would mislead. `excludedBundles` still round-trips through JSON.
- Rebranded mid-cycle: bundle id `com.bliksund.ifancyzones` ->
  `com.tuanquynh.ifancyzones`, organization "Bliksund" -> "Jason", domain
  `bliksund.com` -> `tuanquynh.com`, copyright updated to match.

---

## [1.0.0]

First running build. Every file of the planned architecture exists and the core
loop works end to end.

### Added

- Menu-bar agent (`LSUIElement = true`) with CMake, `Info.plist` and
  entitlements.
- Two-tab popover: Screen & Layout, Layouts.
- Layout editor with a floating toolbar (New Zone, Add Gap, Reduce Gap, Save,
  Cancel), draggable and resizable zone widgets, a delete button and a 12-icon
  auto-align row.
- Runtime snap overlay per `QScreen`, white when inactive and green when active.
- `AccessibilityBridge`: `isTrusted`, `requestTrust`, `moveFrontmostWindow`,
  `moveWindowAtPoint`, `frameOfWindowAtPoint`.
- `DragDetector`: `CGEventTap` on the HID stream watching
  `LeftMouseDown/Dragged/Up`, `KeyDown/Up` and `FlagsChanged`. Default activation
  key `Space`.
- `MacScreenInfo` enumerating `QGuiApplication::screens()` mapped to NSScreens by
  name. Identical-monitor problems surfaced later and were fixed in 1.1.0.
- JSON persistence at
  `~/Library/Application Support/iFancyZones/data.json` with `schema-version = 1`.
- Universal binary targeting macOS 12+.
- Build pipeline: CMake -> Ninja -> `macdeployqt` -> `hdiutil` DMG.

Bundle id at this point was `com.bliksund.ifancyzones`, rebranded in 1.1.0.

---

## [0.1.0] - Pre-release scaffold

### Added

- Replaced the Qt Creator default blank project (`mainwindow.{h,cpp,ui}`) with the
  planned tree: `app/`, `core/`, `services/`, `platform/mac/`, `ui/`,
  `packaging/`, `resources/`.
- Initial stubbed files. No working features yet.
