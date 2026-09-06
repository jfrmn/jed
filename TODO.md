# BUGS

* delete line does not work with multiple cursors

* delete line messes up tree sitter highlighting (and i think pasting text too) (*FIXED?*)

* multicursor let's you remove all cursors - leading to a crash

* Opening both a searchbar and the tool panel blocks keyboard input

* using the file search bar from start screen and selecting a file with Ctrl+/Shift+Enter crashes

* multi cursor -> cut (maybe also other operation) only invalidate the first gylph run

* explorer rename doesn't work

* explorer pressing left in New-Item-dialog closes the dialog instead of moving the cursor

* multi line cursor -> Cut causes crash

* using alt+arrow with no open panels causes a crash

* sending notifications: log entry looks off

# MAJOR TASKS

* ~~refactor process so that it no longer spawns a thread; language server should spawn its own thread~~
* synatx highlighting in scrollbar preview and file preview
* open explorer at current file (maybe a command?)
* **STARTED** detect file changes -> watch settings and external files

# MINOR TASKS

* add LogDevVariable() to quickly log out the name + value of a local variable. Problem with that is that we need to know the format specifier. Possible solution: some typeid-magic hidden behind the makro
* refactor animation so that all animations use the same logic
* refactor statu-bar to utilize the new glyph run better (e.g. use draw partial at text pos)
* get rid of OnMouseWheel and OnResize etc.
* .clangd only really accepts absolute include paths. Relative paths are relative to eicher the current file or the compilation database. The compilation database has the msvc commands in it so that is not an option. Therefore generate .clangd from build.ps1
* Remove settings.GetBrushXXX() functions. Switch to UseColor(settings.xyz)
* Opening a new, unsaved file: should work with the language server. Document Uri should be untitled:// in that case

# BACKLOG

* editorcursorattached: animation for selection just like in prompt
* gotoline: add relatives with +/-
* gotoline: Ctrl + Enter = Scroll and close
* make panels resizeable
* langauge server capabilites could be encoded as bitmask
* own vector and string classes -> auto clean without capacity; clear without calling dtor on elems (*not sure if that is worth it*)
* implement backupFileBeforeSaving

# COMMAND IDEAS

* pretty screenshot of selected code
* ruler at cursor
* reset panels size (when panels are resizeable)
* align cursors (multicursor)
* close multiple tabs (to right, to left, others)
* Save all
* switch start and end of selection
* repear a character N times
* insert numbers at multicursor
* trim (left/right)
* rerender tree sitter tree/reopen file
