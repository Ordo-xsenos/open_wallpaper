# Manager design

Windows desktop utility; task-first, dark by specification. Use Qt Fusion controls
with a restrained blue accent, native keyboard behavior and Segoe UI/system sans.
A 208 px navigation rail contains Library, Monitors, Performance and Settings.
The first viewport leads with real static wallpaper previews, search and Import;
selection exposes metadata and an explicit Apply action beside a monitor selector.
A persistent connection strip identifies disconnected/reconnecting/Core-ready states.
No invented online content, GPU percentages or success messages before responses.

Base #15181e, secondary #1e232b, controls #2b323d, text #f0f2f6,
secondary text #b8c1ce, accent #85b7ff. Spacing unit 8 px; main margins 24 px;
preview corners 12 px. Minimum window 900×620, default 1180×780. Narrower widths
reduce preview columns; control labels wrap rather than vanish. Focus outlines and
accessible control names are required. Feedback stays visible until dismissed.

Signature interaction: selecting a scene reveals its static large preview and
application controls; applying waits for Core confirmation and updates the selected
monitor's assignment. Preview grids never run scenes. Closing the window explains
that Core continues only through persistent Settings copy, without a blocking modal.
