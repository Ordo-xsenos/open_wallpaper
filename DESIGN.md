---
name: AetherWall Manager
description: A compact native Windows wallpaper utility in the classic Wallpaper Engine
  tradition.
colors:
  accent: '#85b7ff'
  accent-fill: '#316e98'
  accent-pressed: '#255d85'
  accent-border: '#528eb9'
  focus: '#a9d9ff'
  card-focus: '#d8edff'
  field-focus: '#83bde8'
  window: '#1b2430'
  header: '#273342'
  status: '#1d2733'
  inspector: '#222c39'
  card: '#1e232b'
  card-hover: '#2b323d'
  control: '#303945'
  control-hover: '#414b58'
  combo-hover: '#3c4857'
  field: '#171e28'
  border: '#495361'
  field-border: '#47515e'
  text: '#edf1f5'
  muted: '#b8c1ce'
  placeholder: '#a8b4c3'
  status-ready: '#9cceab'
  status-attention: '#e9b676'
typography:
  headline:
    fontFamily: Segoe UI, sans-serif
    fontSize: 21px
    fontWeight: 600
  title:
    fontFamily: Segoe UI, sans-serif
    fontSize: 20px
    fontWeight: 600
  app-header:
    fontFamily: Segoe UI, sans-serif
    fontSize: 18px
    fontWeight: 600
  body:
    fontFamily: Segoe UI, sans-serif
    fontSize: 13px
    fontWeight: 400
  metadata:
    fontFamily: Segoe UI, sans-serif
    fontSize: 12px
    fontWeight: 400
rounded:
  control: 3px
  checkbox: 2px
spacing:
  compact: 8px
  control-gap: 10px
  section-gap: 12px
  panel-inset: 14px
  header-inset: 18px
  settings-inset: 20px
components:
  button-primary:
    backgroundColor: '{colors.accent-fill}'
    textColor: '{colors.text}'
    typography: '{typography.body}'
    rounded: '{rounded.control}'
    padding: 7px 14px
    height: 34px
  button-primary-active:
    backgroundColor: '{colors.accent-pressed}'
  button-secondary:
    backgroundColor: '{colors.control}'
    textColor: '{colors.text}'
    typography: '{typography.body}'
    rounded: '{rounded.control}'
    padding: 7px 14px
    height: 34px
  button-secondary-hover:
    backgroundColor: '{colors.control-hover}'
  text-field:
    backgroundColor: '{colors.field}'
    textColor: '{colors.text}'
    typography: '{typography.body}'
    rounded: '{rounded.control}'
    padding: 0 10px
    height: 34px
  combo-box:
    backgroundColor: '{colors.control}'
    textColor: '{colors.text}'
    rounded: '{rounded.control}'
    padding: 0 28px 0 10px
    height: 34px
  scene-card:
    backgroundColor: '{colors.card}'
    textColor: '{colors.text}'
    rounded: '{rounded.control}'
  scene-card-hover:
    backgroundColor: '{colors.card-hover}'
---

# Design System: AetherWall Manager

## Overview

**Creative North Star: "The Classic Wallpaper Workbench"**

AetherWall is a native Windows 11 desktop utility in Operate mode. The user-approved direction closely follows the classic Wallpaper Engine experience: compact controls, graphite surfaces, blue selection, and original scene imagery as the visual focus. Segoe UI and modest corners belong to this established desktop world.

The Manager selects, inspects and explicitly applies scenes while the independent Core owns rendering. The interface makes connection and renderer state visible and keeps the primary action within reach as content scrolls. This documented implementation replaces the former sidebar/Fusion direction.

**Key Characteristics:**

- Compact, image-led desktop browsing.
- Flat graphite layers and restrained blue state cues.
- Persistent application controls and explicit Apply.
- Russian and English interface copy with native keyboard semantics.

Visual authority is the approved redesign and implemented QML in `apps/manager/qml/`, particularly `Main.qml`, `Discover.qml` and the Aw controls. The frontmatter is normative for extracted reusable tokens; QML remains the implementation source. This is native Qt Quick, not a website or an iOS interface.

## Colors

A cool graphite palette frames the scene artwork; blue marks actions, selection and focus.

### Primary

- **Selection Blue** (`accent`) outlines the chosen scene and supplies the inherited Qt highlight.
- **Action Blue** (`accent-fill`, `accent-pressed`, `accent-border`) identifies Apply and checked navigation, with a darker pressed fill.
- **Focus Ice** (`focus`, `card-focus`, `field-focus`) separates keyboard focus from selection, retaining control-specific outlines.

### Neutral

- **Graphite Workspace** (`window`) anchors the application; `header` and `status` distinguish persistent chrome.
- **Inspector Slate** (`inspector`) groups the right-hand controls and modal content.
- **Preview Charcoal** (`card`, `card-hover`) supports imagery without raised card decoration.
- **Control Steel** (`control`, `control-hover`, `combo-hover`, `border`) gives compact buttons and selectors a defined edge.
- **Inset Charcoal** (`field`, `field-border`) distinguishes editable text fields.
- **Reading White** (`text`), **Metadata Silver** (`muted`) and **Placeholder Steel** (`placeholder`) establish text roles.
- **Ready Sage** (`status-ready`) and **Attention Sand** (`status-attention`) accompany textual state. These are status roles, not additional brand accents.

**The State Truth Rule.** Connection, test transport and renderer readiness determine status copy; a green indicator alone never proves wallpaper rendering.

## Typography

Segoe UI is the Windows family; the implementation uses the system sans-serif fallback on other platforms. Body and controls share a compact, regular text style. Section titles and the selected scene title use a modest semibold step. No separate display or monospace family is established.

### Hierarchy

- **Headline:** selected scene title in the inspector; wraps when necessary.
- **Title:** settings sections, empty states and display headings.
- **App header:** application identity and dialog headings.
- **Body:** actions, inputs, scene names and explanatory text.
- **Metadata:** card authors and secondary footer information.

Sizes and weights live in the frontmatter. Qt supplies native font metrics and line height; do not invent a web line-height or tracking scale. Use plain text for untrusted scene metadata, wrap descriptions, and keep actionable labels readable in both languages.

## Layout

The main window opens up to 1440 × 900 logical pixels, bounded by available desktop space. Its minimum is 1000 × 650, also bounded by available space. A 62-pixel header holds Installed and Discover tabs alongside Displays and Settings actions. A connection strip sits below it; a persistent footer explains that closing Manager leaves wallpaper running.

Installed uses a search/filter/import toolbar above a horizontal split: a compact image-led grid on the left and a resizable inspector on the right. The grid recalculates column count from available width and the selected thumbnail size (170, 220 or 280 logical pixels). Previews use a 16:9 image region. The inspector defaults to 340 pixels and resizes between 280 and 480; details scroll above the pinned full-width Apply button. The grid has a 380-pixel minimum width. The splitter handle is 8 pixels wide and changes color on hover.

Discover repeats the grid/inspector relationship with catalog filters, source disclosure, pagination and a pinned installation action. It can show the six bundled original examples without an external server. Settings and Performance live in a separate 720 × 720 window with scrollable sections. Displays uses a modal, scrollable dialog bounded by the parent. Narrow and scaled layouts reduce columns and scroll details while preserving Apply; these are desktop adaptations, not mobile navigation breakpoints.

The frontmatter records recurring spacing roles. Main content and inspector insets share `panel-inset`; settings use `settings-inset`. Capture evidence covers full-size, compact English and 150%/200% scaling in Linux offscreen Qt. Mixed-DPI Windows behavior remains unverified.

## Elevation & Depth

Depth comes from tonal layers, thin borders and modal separation. The implemented custom controls and cards use no decorative drop shadows. Hover changes the fill; selection and keyboard focus change the outline. Do not add raised cards or animated lifts. Any OS window chrome remains the platform's responsibility. No shared motion system is established; the temporary diagnostics-copy label uses a 2500 ms feedback timer.

## Shapes

Controls, dialogs and preview containers use the small control corner token. Checkboxes retain the smaller checkbox corner. Imagery remains rectangular and dominant; use subtle framing rather than large masks or pill controls. Standard controls have a thin border; scene selection/focus has a stronger two-pixel outline.

## Components

### Buttons

Compact, edged and direct. AwButton uses the primary and secondary variants in frontmatter, with a minimum width of 80 logical pixels. Highlighted or checked buttons use Action Blue; a press uses its darker state. Secondary hover uses Control Steel's hover fill. Focus changes the border to Focus Ice. Disabled buttons use 0.45 opacity and remain noninteractive. Top navigation increases button height to 38 pixels. Preserve Qt button activation and focus semantics.

### Inputs / Fields

AwTextField uses the inset field surface, horizontal padding and field-specific focus border. Placeholder text is distinct from entered text; selection uses Action Blue with white selected text. AwComboBox uses the standard control fill, a brighter hover fill and a reserved trailing chevron region. Retain Qt text editing, popup and keyboard behavior. The UI names search, sorting, preview size and target-display controls for assistive technology.

### Cards / Containers

Scene cards lead with original static preview images, followed by title and subdued author metadata. Installed and Discover cards share the charcoal surface and hover fill. A blue border means selected; a paler outline means keyboard focus. Installed exposes the selected accessibility state. Grid selection only updates the inspector; the preview never runs the scene animation.

### Navigation

Installed and Discover are top-level checked buttons in the header. Displays opens its modal; Settings opens the independent settings/performance window. Keep desktop text labels and native focus treatment. The old navigation rail is superseded.

### Inspector and Apply

The inspector puts a large static preview above title, attribution, target selector and secondary actions. Its content scrolls; Apply stays pinned at the bottom. Busy/disconnected/failed states gate actions using controller state, and Core confirms mutations.

**The Explicit Apply Rule.** Selecting or double-clicking a thumbnail never applies a scene. Only the pinned Apply action changes the wallpaper.

### Dialogs and status

Application-owned dialog headers and footer buttons use the active Russian or English setting. Displays prioritizes disconnected, failed, test-transport, recovering and preparing states before paused/active; Pause requires a connected, ready renderer. Fixture displays explicitly say they have no GPU. Status and errors remain readable when rendering is unavailable but the library connection survives.

## Do's and Don'ts

### Do:

- Do keep the native Windows utility character and the approved classic Wallpaper Engine resemblance.
- Do use original static scene previews and identify fixture transport as having no GPU.
- Do preserve the pinned Apply action, scrollable inspector and resizable split.
- Do distinguish selection from keyboard focus and expose accessible control names.
- Do localize application-owned dialogs and actions with the current RU/EN setting.

### Don't:

- Don't restore the superseded navigation rail, rounded preview tiles or Fusion visual direction.
- Don't add decorative display typography, oversized cards, floating shadows or promotional page treatments.
- Don't apply a scene from thumbnail selection or double-click.
- Don't imply Windows acceptance, GPU activity or working desktop attachment from Linux offscreen captures.
