# Frame Groups

Frame groups organize the timeline (the strip below the canvas). They are a
grouping/labeling tool only - they never affect what plays when.

## Behavior

- **Organizational only.** Playback always walks frames in index order.
  Grouping a frame does not change when it appears in the animation.
- **No reordering, by design.** Frames keep their creation order, groups keep
  theirs, and neither the UI nor the document API exposes any way to move
  them. This is intentional.
- **Single membership.** A frame belongs to at most one group; adding it to a
  second group pulls it out of the first.
- **Groups are per-document**, one flat list, created in the timeline's
  `+ Group` button.

## Timeline controls

| Control | Action |
| --- | --- |
| `+ Frame` | Appends a new frame with its own blank canvas, layers, and layer groups |
| `- Frame` | Deletes the current frame |
| `Dup` | Duplicates the current frame (inserted after it) |
| `+ Group` | Creates an empty frame group |
| `Ctrl+Shift+G` | Dissolves the group containing the current frame (frames survive, ungrouped) |
| Click chip chevron / body | Collapse or expand the group section |
| Double-click chip | Rename the group inline (Enter commits, Escape cancels) |
| Click chip color swatch | Cycles the group color through the tag palette |
| Click chip `+` / `-` | Adds / removes the *current* frame to/from that group |
| Click chip `x` | Deletes the group (frames survive, ungrouped) |
| Click frame tile | Makes that frame active |
| Double-click frame tile | Rename the frame inline (unnamed frames show their number) |

Layout order in the strip is purely presentational: each expanded group shows
its member tiles (in frame-index order), followed by all ungrouped frames.

## Per-frame playback speed

Each frame carries a duration multiplier (`0.25x` - `4x`, default `1.0x`),
edited with the `Speed < x.xx >` stepper at the right end of the timeline.
During playback a frame is held for `multiplier / fps` seconds, so `2.0x`
stays twice as long and `0.5x` half as long as a default beat. Tiles show an
amber badge whenever a frame deviates from `1.0x`.

Grouping does not interact with speed: every frame keeps its own multiplier.

## Internals (for developers)

- `DrawingDocument::FrameGroup` - `{ name, color, frameIndices, collapsed }`
  living on the document, next to the frame vector.
- Membership bookkeeping mirrors layer groups: `addFrameToGroup`,
  `removeFrameFromGroup`, `findGroupForFrame`.
- Stored member indices are remapped when frames are inserted (`addFrame`,
  `duplicateFrame`) or removed (`removeFrame`), so groups stay valid across
  edits. Deleting a frame drops it from its group; deleting a group leaves
  its frames untouched and ungrouped.
- Timeline flattening lives in `buildTimelineItems` (MainWindow.cpp);
  `renderTimeline` and the click handler share the same geometry constants,
  so they must be updated together.
- Rename sessions reuse MainWindow's shared inline-edit state
  (`startFrameRename` / `startFrameGroupRename`); at most one edit session is
  active document-wide.
